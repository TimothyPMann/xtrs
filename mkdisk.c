/* Copyright (c) 1996-98, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Fri Apr 24 21:18:06 PDT 1998 by mann */

/*
 * mkdisk.c
 * Make a blank (unformatted) emulated floppy or hard drive in a file.
 */

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

void Usage(char *progname)
{
  fprintf(stderr, 
	  "Usage: %s [-1|-3|-h] [-c cyl] [-s sec] [-g gran] [-d dir] file\n",
	  progname);
  exit(2);
}

typedef unsigned char Uchar;

/* Matthew Reed's hard drive format.  Thanks to Matthew for providing
   documentation.  The comments below are copied from his mail
   messages, with some additions. */

typedef struct {
  Uchar id1;       /* 0: Identifier #1: 56H */
  Uchar id2;       /* 1: Identifier #2: CBH */
  Uchar ver;       /* 2: Version of format: 10H = version 1.0 */
  Uchar cksum;     /* 3: Simple checksum: 
		      To calculate, add together bytes 0 to 31 of header
		      (excepting byte 3), then XOR result with 4CH */
  Uchar blks;      /* 4: Number of 256 byte blocks in header: should be 1 */
  Uchar mb4;       /* 5: Not used, but HDFORMAT sets to 4 */
  Uchar media;     /* 6: Media type: 0 for hard disk */
  Uchar flag1;     /* 7: Flags #1:
		      bit 7: Write protected: 0 for no, 1 for yes 
                             [xtrs ignores for now]
		      bit 6: Must be 0
		      bit 5 - 0: reserved */
  Uchar flag2;     /* 8: Flags #2: reserved */
  Uchar flag3;     /* 9: Flags #3: reserved */
  Uchar crtr;      /* 10: Created by: 
		      14H = HDFORMAT
		      42H = xtrs mkdisk
                      80H = Cervasio xtrshard port to Vavasour M4 emulator */
  Uchar dfmt;      /* 11: Disk format: 0 = LDOS/LS-DOS */
  Uchar mm;        /* 12: Creation month: mm */
  Uchar dd;        /* 13: Creation day: dd */
  Uchar yy;        /* 14: Creation year: yy (offset from 1900) */
  Uchar res1[12];  /* 15 - 26: reserved */
  Uchar dparm;     /* 27: Disk parameters: (unused with hard drives)
		      bit 7: Density: 0 = double, 1 = single
		      bit 6: Sides: 0 = one side, 1 = 2 sides
		      bit 5: First sector: 0 if sector 0, 1 if sector 1
		      bit 4: DAM convention: 0 if normal (LDOS),
		      1 if reversed (TRSDOS 1.3)
		      bit 3 - 0: reserved */
  Uchar cyl;       /* 28: Number of cylinders per disk */
  Uchar sec;       /* 29: Number of sectors per track (floppy); cyl (hard) */
  Uchar gran;      /* 30: Number of granules per track (floppy); cyl (hard)*/
  Uchar dcyl;      /* 31: Directory cylinder [mkdisk sets to 1; xtrs ignores]*/
  char label[32];  /* 32: Volume label: 31 bytes terminated by 0 */
  char filename[8];/* 64 - 71: 8 characters of filename (without extension)
		      [Cervasio addition.  xtrs actually doesn't limit this 
                       to 8 chars or strip the extension] */
  Uchar res2[184]; /* 72 - 255: reserved */
} ReedHardHeader;

ReedHardHeader rhh;

int
main(int argc, char *argv[])
{
  int jv1 = 0, jv3 = 0, hard = 0, cyl = 202, sec = 256, gran = 8;
  int i, c, hardopts = 0, dir = 1;
  char *fname;
  FILE *f;

  opterr = 0;
  for (;;) {
    c = getopt(argc, argv, "13hc:s:g:d:");
    if (c == -1) break;
    switch (c) {
    case '1':
      jv1 = 1;
      break;
    case '3':
      jv3 = 1;
      break;
    case 'h':
      hard = 1;
      break;
    case 'c':
      cyl = atoi(optarg);
      hardopts++;
      break;
    case 's':
      sec = atoi(optarg);
      hardopts++;
      break;
    case 'g':
      gran = atoi(optarg);
      hardopts++;
      break;
    case 'd':
      dir = atoi(optarg);
      hardopts++;
      break;
    case '?':
    default:
      Usage(argv[0]);
      break;
    }
  }

  if (argc - optind != 1) Usage(argv[0]);

  switch (jv1 + jv3 + hard) {
  case 0:
    jv3 = 1;
    break;
  case 1:
    break;
  default:
    fprintf(stderr, "%s: -1, -3, and -h are mutually exclusive\n", argv[0]);
    exit(2);
  }

  if (!hard && hardopts > 0) {
    fprintf(stderr, "%s: -c, -s, -g, and -d are meaningful only with -h\n",
	    argv[0]);
    exit(2);
  }

  fname = argv[optind];

  if (jv1) {
    /* Unformatted JV1 disk - just an empty file! */
    f = fopen(fname, "w");
    if (f == NULL) {
      perror(fname);
      exit(1);
    }
    fclose(f);

  } else if (jv3) {
    /* Unformatted JV3 disk. */
    f = fopen(fname, "w");
    if (f == NULL) {
      perror(fname);
      exit(1);
    }
    for (i=0; i<(256*34); i++) {
      putc(0xff, f);
    }
    fclose(f);

  } else /* hard */ {
    /* Unformatted hard disk */
    /* We don't care about most of this header, but we generate
       it just in case some user wants to exchange hard drives with
       Matthew Reed's emulator or with Pete Cervasio's port of
       xtrshard/dct to Jeff Vavasour's Model III/4 emulator.
    */
    time_t tt = time(0);
    struct tm *lt = localtime(&tt);
    Uchar *rhhp;
    int cksum;

    if (cyl < 3) {
      fprintf(stderr, "%s error: cyl < 3\n", argv[0]);
      exit(2);
    }
    if (cyl > 203) {
      fprintf(stderr, "%s error: cyl > 203\n", argv[0]);
      exit(2);
    }
    if (sec < 4) {
      fprintf(stderr, "%s error: sec < 4\n", argv[0]);
      exit(2);
    }
    if (sec > 256) {
      fprintf(stderr, "%s error: sec > 256\n", argv[0]);
      exit(2);
    }
    if (gran < 1) {
      fprintf(stderr, "%s error: gran < 1\n", argv[0]);
      exit(2);
    }
    if (gran > 8) {
      fprintf(stderr, "%s error: gran > 8\n", argv[0]);
      exit(2);
    }
    if (sec < gran) {
      fprintf(stderr, "%s error: sec < gran\n", argv[0]);
      exit(2);
    }
    if (sec % gran != 0) {
      fprintf(stderr, "%s error: sec %% gran != 0\n", argv[0]);
      exit(2);
    }
    if (sec / gran > 32) {
      fprintf(stderr, "%s error: sec / gran > 32\n", argv[0]);
      exit(2);
    }
    if (dir < 1) {
      fprintf(stderr, "%s error: dir < 1\n", argv[0]);
      exit(2);
    }
    if (dir >= cyl) {
      fprintf(stderr, "%s error: dir >= cyl\n", argv[0]);
      exit(2);
    }

    rhh.id1 = 0x56;
    rhh.id2 = 0xcb;
    rhh.ver = 0x10;
    rhh.cksum = 0;  /* init for cksum computation */
    rhh.blks = 1;
    rhh.mb4 = 4;
    rhh.media = 0;
    rhh.flag1 = 0;
    rhh.flag2 = rhh.flag3 = 0;
    rhh.crtr = 0x42;
    rhh.dfmt = 0;
    rhh.mm = lt->tm_mon + 1;
    rhh.dd = lt->tm_mday;
    rhh.yy = lt->tm_year;
    rhh.dparm = 0;
    rhh.cyl = cyl;
    rhh.sec = sec;
    rhh.gran = gran;
    rhh.dcyl = dir;
    strcpy(rhh.label, "xtrshard");
    strcpy(rhh.filename, fname); /* note we don't limit to 8 chars */

    cksum = 0;
    rhhp = (Uchar *) &rhh;
    for (i=0; i<=31; i++) {
      cksum += rhhp[i];
    }
    rhh.cksum = ((Uchar) cksum) ^ 0x4c;

    f = fopen(fname, "w");
    if (f == NULL) {
      perror(fname);
      exit(1);
    }
    fwrite(&rhh, sizeof(rhh), 1, f);
    fclose(f);

    printf("%s: Be sure to format this drive with FORMAT (DIR=%d)\n",
           argv[0], dir);
  }
  return 0;
}



