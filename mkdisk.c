/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Sun Aug 31 23:00:59 PDT 1997 by mann */

/*
 * mkdisk [-1] filename
 * mkdisk -h [-c cyl] [-s sec] [-g gran] filename
 *
 * Make a blank (unformatted) emulated floppy or hard drive in a file.
 *
 * By default or with the -3 flag, makes a JV3 floppy.  With the -1
 * flag, makes a JV1 floppy.  These floppy file formats were developed
 * by Jeff Vavasour for his MSDOS-based Model III/4 and Model I
 * emulators respectively.  xtrs uses them for compatibility.  Thanks
 * to Jeff for documenting them.
 *
 * With the -h flag, makes a hard drive with /cyl/ cylinders, /sec/
 * sectors per cylinder, and /gran/ granules (LDOS allocation units)
 * per cylinder.  The hard drive format was developed by Matthew Reed
 * for his MSDOS-based Model I/III emulator.  xtrs uses it for
 * compatibility.  Thanks to Matthew for providing documentation in an
 * email message.
 *
 * For cyl, default is 202, max is 202 (Model I/III) or 203 (Model 4),
 * min is 3.  (Model I/III LDOS can handle 203 cylinders except for a
 * minor bug in FORMAT/CMD that prevents such a large drive from being
 * formatted.)  For sec, default is 256, max is 256, min is 4.  For
 * gran, default is 8, max is 8, min is 1.  In addition, it is
 * required that (sec % gran == 0) && (sec / gran <= 32) && (sec >=
 * gran).
 */

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

void Usage(char *progname)
{
  fprintf(stderr, 
	  "Usage: %s [-1 | -3 | -h] [-c cyl] [-s sec] [-g gran] file\n",
	  progname);
  exit(2);
}

typedef unsigned char Uchar;

/* Matthew Reed's hard drive format.  No, I don't understand what the
   floppy-specific parameters like dparm are doing here.  The comments
   below are copied from his mail message, with additions in [ ]. */

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
		             [xtrs ignores]
		      bit 6: Must be 1
		      bit 5 - 0: reserved */
  Uchar flag2;     /* 8: Flags #2: reserved */
  Uchar flag3;     /* 9: Flags #3: reserved */
  Uchar crtr;      /* 10: Created by: 
		      14H = HDFORMAT
		      [42H = xtrs mkdisk] */
  Uchar dfmt;      /* 11: Disk format: 0 = LDOS/LS-DOS */
  Uchar mm;        /* 12: Creation month: mm */
  Uchar dd;        /* 13: Creation day: dd */
  Uchar yy;        /* 14: Creation year: yy (offset from 1900) */
  Uchar res1[12];  /* 15 - 26: reserved */
  Uchar dparm;     /* 27: Disk parameters:
		      [What are these floppy parameters doing here??]
		      bit 7: Density: 0 = double, 1 = single
		      bit 6: Sides: 0 = one side, 1 = 2 sides
		      bit 5: First sector: 0 if sector 0, 1 if sector 1
		      bit 4: DAM convention: 0 if normal (LDOS),
		      1 if reversed (TRSDOS 1.3)
		      bit 3 - 0: reserved */
  Uchar cyl;       /* 28: Number of cylinders per disk */
  Uchar sec;       /* 29: Number of sectors per track (one side only)
		      [One side?  Huh?  xtrs uses this as sectors/cylinder] */
  Uchar gran;      /* 30: Number of granules per track (one side only)
                      [One side?  Huh?  xtrs uses this as grans/cylinder] */
                   /* [Maybe the "one side" bit means that these are to be
                      doubled (using the DCT DBLBIT) if "2 sides" is set?] */
  Uchar dcyl;      /* 31: Directory cylinder [ignored by xtrs] */
  char label[32];  /* 32: Volume label: 31 bytes terminated by 0 */
  Uchar res2[192]; /* 64 - 255: reserved */
} ReedHardHeader;

ReedHardHeader rhh;

int
main(int argc, char *argv[])
{
  int jv1 = 0, jv3 = 0, hard = 0, cyl = 202, sec = 256, gran = 8;
  int i, c, hardopts = 0;
  char *fname;
  FILE *f;

  opterr = 0;
  for (;;) {
    c = getopt(argc, argv, "13hc:s:g:");
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
    fprintf(stderr, "%s: -c, -s, and -g are meaningful only with -h\n",
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
    Uchar *rhhp = (Uchar *) &rhh;
    int cksum = 0;

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

    rhh.id1 = 0x56;
    rhh.id2 = 0xcb;
    rhh.ver = 0x10;
    rhh.cksum = 0;  /* init for cksum computation */
    rhh.blks = 1;
    rhh.mb4 = 4;
    rhh.media = 0;
    rhh.flag1 = (1 << 6);
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
    rhh.dcyl = 1;   /* !! Needs to be correct for Reed emulator? */
    strcpy(rhh.label, "xtrshard");

    for (i=0; i<=31; i++) {
      cksum += *rhhp++;
    }
    rhh.cksum = cksum ^ 0x4c;

    f = fopen(fname, "w");
    if (f == NULL) {
      perror(fname);
      exit(1);
    }
    fwrite(&rhh, sizeof(rhh), 1, f);
    fclose(f);
  }
  return 0;
}
