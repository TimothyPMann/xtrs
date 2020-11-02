/*
 * Copyright (c) 1996-2020, Timothy P. Mann
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * mkdisk.c
 * Make a blank (unformatted) emulated floppy or hard drive in a file,
 * or write protect/unprotect an existing one.
 */

#define _XOPEN_SOURCE 500 /* unistd.h: getopt(), ...; sys/stat.h: fchmod() */

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include "trs_disk.h"
#include "trs_hard.h"
#include "trs_stringy.h"

char *program_name;
typedef unsigned char Uchar;

void Usage(char *progname)
{
  fprintf(stderr,
	  "Usage:\t%s -1 [-f] file\n"
	  "\t%s [-3] [-f] file\n"
	  "\t%s -k [-s sides] [-d density] [-8] [-i] [-f] file\n"
	  "\t%s -h [-c cyl] [-s sec] [-g gran] [-d dcyl] [-f] file\n"
	  "\t%s -w [-s size] [-f] file\n"
	  "\t%s {-p|-u} {-1|-3|-k|-h|-w} file\n",
	  progname, progname, progname, progname, progname, progname);
  exit(2);
}

/*
 * If overwrite, create or truncate fname and open for writing.  If
 * !overwrite, create and open fname only if it does not already
 * exist.  Could use fopen mode "wx" for the latter, but we're still
 * making a faint effort to be compatible with older systems that
 * don't have that C11 feature.
 */
FILE *
fopen_w(const char *fname, int overwrite)
{
  if (overwrite) {
    return fopen(fname, "w");
  } else {
    int fd;
    fd = open(fname, O_WRONLY|O_CREAT|O_EXCL, 0666);
    if (fd < 0) {
      return NULL;
    }
    return fdopen(fd, "w");
  }
}

int
main(int argc, char *argv[])
{
  int jv1 = 0, jv3 = 0, dmk = 0, hard = 0, wafer = 0;
  int cyl = -1, sec = -1, gran = -1, dir = -1, eight = 0, ignden = 0;
  int writeprot = 0, unprot = 0, overwrite = 0;
  int c, oumask, res;
  char *fname;
  FILE *f;

  /* program_name must be set first because the error
   * printing routines use it. */
  program_name = strrchr(argv[0], '/');
  if (program_name == NULL) {
    program_name = argv[0];
  } else {
    program_name++;
  }

  opterr = 0;
  for (;;) {
    c = getopt(argc, argv, "13khwc:s:g:d:8ipuf");
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
    case 'w':
      wafer = 1;
      break;
    case 'c':
      cyl = atoi(optarg);
      break;
    case 's':
      sec = atoi(optarg); // or sides or size
      break;
    case 'g':
      gran = atoi(optarg);
      break;
    case 'd':
      dir = atoi(optarg); // or density
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
    case 'f':
      overwrite = 1;
      break;
    case '?':
    default:
      Usage(argv[0]);
      break;
    }
  }

  if (argc - optind != 1) Usage(argv[0]);

  fname = argv[optind];

  /*
   * Media write protect/unprotect.
   */
  if (writeprot || unprot) {
    struct stat st;
    int newmode;

    if (writeprot && unprot) {
      error("-p and -u are mutually exclusive");
      exit(2);
    }

    if (jv1 + jv3 + dmk + hard + wafer != 1) {
      error("%s requires exactly one of -1, -3, -k, -h, or -w",
	      writeprot ? "-p" : "-u");
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
    if (jv1 || jv3 || dmk) {
      res = trs_disk_set_write_prot(f, jv1 ? JV1 : jv3 ? JV3 : DMK, writeprot);
      if (res < 0) {
	perror(fname);
	exit(1);
      }

    } else if (hard) {
      res = trs_hard_set_write_prot(f, writeprot);
      if (res < 0) {
	perror(fname);
	exit(1);
      }

    } else if (wafer) {
      /* Set the magic bit */
      res = stringy_set_write_prot(f, STRINGY_FMT_ESF, writeprot);
      if (res < 0) {
	perror(fname);
	exit(1);
      }
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

  /*
   * Media creation
   */
  switch (jv1 + jv3 + dmk + hard + wafer) {
  case 0:
    jv3 = 1;
    break;
  case 1:
    break;
  default:
    error("-1, -3, -k, -h, and -w are mutually exclusive");
    exit(2);
  }

  if ((jv1 || jv3) && (cyl >= 0 || sec >= 0 || gran >= 0 || dir >= 0)) {
    error("-c, -s, -g, -d are not meaningful with -1 or -3");
    exit(2);
  }

  if (dmk && (cyl >= 0 || gran >= 0)) {
    error("-c and -g are not meaningful with -k");
    exit(2);
  }

  if (!dmk && (eight || ignden)) {
    error("-8 and -i are only meaningful with -k");
    exit(2);
  }

  f = fopen_w(fname, overwrite);
  if (f == NULL) {
    perror(fname);
    exit(1);
  }

  if (jv1 || jv3 || dmk) {
    /* Reuse flag letters s, d */
#define sides sec
#define density dir
    res = trs_disk_init_with(f, jv1 ? JV1 : jv3 ? JV3 : DMK,
			     sides, density, eight, ignden);
  } else if (hard) {
    res = trs_hard_init_with(f, cyl, sec, gran, dir);

  } else /* wafer */ {
    /* Reuse flag letter s */
#define size sec
    if (size == -1) {
      size = STRINGY_LEN_DEFAULT;
    } else {
      size = stringy_len_from_kb(size);
    }
    res = stringy_init_with(f, STRINGY_FMT_DEFAULT, size,
			    STRINGY_EOT_DEFAULT, FALSE);
  }

  if (res < 0) {
    perror(fname);
    fclose(f);
    unlink(fname);
    exit(1);
  }

  fclose(f);

  return 0;
}
