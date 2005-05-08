/* Copyright (c) 1996-98, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Tue May 16 19:54:51 PDT 2000 by mann */

/*
 * mkdisk.c
 * Make a blank (unformatted) emulated floppy or hard drive in a file,
 * or write protect/unprotect an existing one.
 */

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

typedef unsigned char Uchar;
#include "reed.h"
ReedHardHeader rhh;

void Usage(char *progname)
{
  fprintf(stderr, 
	  "Usage:\t%s -1 file\n"
	  "\t%s [-3] file\n"
	  "\t%s -k [-s sides] [-d density] [-8] [-i] file\n"
	  "\t%s -h [-c cyl] [-s sec] [-g gran] file\n"
	  "\t%s {-p|-u} {-1|-3|-k|-h} file\n",
	  progname, progname, progname, progname, progname);
  exit(2);
}

int
main(int argc, char *argv[])
{
  int jv1 = 0, jv3 = 0, dmk = 0, hard = 0;
  int cyl = -1, sec = -1, gran = -1, dir = -1, eight = 0, ignden = 0;
  int writeprot = 0, unprot = 0;
  int i, c, oumask;
  char *fname;
  FILE *f;

  opterr = 0;
  for (;;) {
    c = getopt(argc, argv, "13khc:s:g:d:8ipu");
    if (c == -1) break;
    switch (c) {
    case '1':
      jv1 = 1;
      break;
    case '3':
      jv3 = 1;
      break;
    case 'k':
      dmk = 1;
      break;
    case 'h':
      hard = 1;
      break;
    case 'c':
      cyl = atoi(optarg);
      break;
    case 's':
      sec = atoi(optarg);
      break;
    case 'g':
      gran = atoi(optarg);
      break;
    case 'd':
      dir = atoi(optarg);
      break;
    case '8':
      eight = 1;
      break;
    case 'i':
      ignden = 1;
      break;
    case 'p':
      writeprot = 1;
      break;
    case 'u':
      unprot = 1;
      break;
    case '?':
    default:
      Usage(argv[0]);
      break;
    }
  }

  if (argc - optind != 1) Usage(argv[0]);

  fname = argv[optind];

  if (writeprot || unprot) {
    /* Completely different functionality here */
    struct stat st;
    int newmode;

    if (writeprot && unprot) {
      fprintf(stderr,
	      "%s: -p and -u are mutually exclusive\n", argv[0]);
      exit(2);
    }    

    if (jv1 + jv3 + dmk + hard != 1) {
      fprintf(stderr,
	      "%s: %s requires exactly one of -1, -3, -k, or -h\n",
	      argv[0], writeprot ? "-p" : "-u");
      exit(2);
    }

    if (stat(fname, &st) < 0) {
      perror(fname);
      exit(1);
    }

    /* Make writable so we can poke inside if need be */
    if (chmod(fname, st.st_mode | (S_IWUSR|S_IWGRP|S_IWOTH)) < 0) {
      perror(fname);
      exit(1);
    }

    f = fopen(fname, "r+");
    if (f == NULL) {
      perror(fname);
      exit(1);
    }

    /* Poke inside */
    if (jv1) {
      /* No indication inside; nothing to do here */

    } else if (jv3) {
      /* Set the magic byte */
      fseek(f, 256*34-1, 0);
      putc(writeprot ? 0 : 0xff, f);

    } else if (dmk) {
      /* Set the magic byte */
      putc(writeprot ? 0xff : 0, f);

    } else if (hard) {
      /* Set the magic bit */
      fseek(f, 7, 0);
      newmode = getc(f);
      if (newmode == EOF) {
	perror(fname);
	exit(1);
      }
      newmode = (newmode & 0x7f) | (writeprot ? 0x80 : 0);
      fseek(f, 7, 0);
      putc(newmode, f);

    }

    /* Finish by chmoding the file appropriately */
    if (writeprot) {
      newmode = st.st_mode & ~(S_IWUSR|S_IWGRP|S_IWOTH);
    } else {
      oumask = umask(0);
      umask(oumask);
      newmode = st.st_mode |
	(( (st.st_mode & (S_IRUSR|S_IRGRP|S_IROTH)) >> 1 ) & ~oumask);
    }
    if (fchmod(fileno(f), newmode)) {
      perror(fname);
      exit(1);
    }
    fclose(f);
    exit(0);
  }

  switch (jv1 + jv3 + dmk + hard) {
  case 0:
    jv3 = 1;
    break;
  case 1:
    break;
  default:
    fprintf(stderr,
	    "%s: -1, -3, -k, and -h are mutually exclusive\n", argv[0]);
    exit(2);
  }

  if ((jv1 || jv3) && (cyl >= 0 || sec >= 0 || gran >= 0 || dir >= 0)) {
    fprintf(stderr,
	    "%s: -c, -s, -g, -d are not meaningful with -1 or -3\n", argv[0]);
    exit(2);
  }

  if (dmk && (cyl >= 0 || gran >= 0)) {
    fprintf(stderr, "%s: -c and -g are not meaningful with -k\n", argv[0]);
    exit(2);
  }

  if (!dmk && (eight || ignden)) {
    fprintf(stderr, "%s: -8 and -i are only meaningful with -k\n", argv[0]);
    exit(2);
  }    

  if (jv1) {
    /* Unformatted JV1 disk - just an empty file! */
    f = fopen(fname, "w");
    if (f == NULL) {
      perror(fname);
      exit(1);
    }

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

  } else if (dmk) {
    /* Unformatted DMK disk */
    /* Reuse flag letters s, d */
#define sides sec
#define density dir
    if (sides == -1) sides = 2;
    if (density == -1) density = 2;

    if (sides != 1 && sides != 2) {
      fprintf(stderr, "%s error: sides must be 1 or 2\n", argv[0]);
      exit(2);
    }	    
    if (density < 1 || density > 2) {
      fprintf(stderr, "%s error: density must be 1 or 2\n", argv[0]);
      exit(2);
    }	    
    
    f = fopen(fname, "w");
    putc(0, f);           /* 0: not write protected */
    putc(0, f);           /* 1: initially zero tracks */
    if (eight) {
      if (density == 1)
	i = 0x14e0;
      else
	i = 0x2940;
    } else {
      if (density == 1)
	i = 0x0cc0;
      else
	i = 0x1900;
    }
    putc(i & 0xff, f);    /* 2: LSB of track length */
    putc(i >> 8, f);      /* 3: MSB of track length */
    i = 0;
    if (sides == 1)   i |= 0x10;
    if (density == 1) i |= 0x40;
    if (ignden)       i |= 0x80;
    putc(i, f);           /* 4: options */
    putc(0, f);           /* 5: reserved */
    putc(0, f);           /* 6: reserved */
    putc(0, f);           /* 7: reserved */
    putc(0, f);           /* 8: reserved */
    putc(0, f);           /* 9: reserved */
    putc(0, f);           /* a: reserved */
    putc(0, f);           /* b: reserved */
    putc(0, f);           /* c: MBZ */
    putc(0, f);           /* d: MBZ */
    putc(0, f);           /* e: MBZ */
    putc(0, f);           /* f: MBZ */

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

    if (cyl == -1) cyl = 202;
    if (sec == -1) sec = 256;
    if (gran == -1) gran = 8;
    if (dir == -1) dir = 1;

    if (cyl < 3) {
      fprintf(stderr, "%s error: cyl < 3\n", argv[0]);
      exit(2);
    }
    if (cyl > 256) {
      fprintf(stderr, "%s error: cyl > 256\n", argv[0]);
      exit(2);
    }
    if (cyl > 203) {
      fprintf(stderr,
	      "%s warning: cyl > 203 is incompatible with XTRSHARD/DCT\n",
	      argv[0]);
    }
    if (sec < 4) {
      fprintf(stderr, "%s error: sec < 4\n", argv[0]);
      exit(2);
    }
    if (sec > 256) {
      fprintf(stderr, "%s error: sec > 256\n", argv[0]);
      exit(2);
    }
    if ((sec % 32) != 0) {
      fprintf(stderr, "%s warning: %s\n", argv[0],
	      "(sec % 32) != 0 is incompatible with WD1000/1010 emulation");
      if (sec > 32) {
	fprintf(stderr, "%s warning: %s\n", argv[0],
		"(sec % 32) != 0 and sec > 32 "
		"is incompatible with Matthew Reed's emulators");
      }
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
  }
  fclose(f);
  return 0;
}



