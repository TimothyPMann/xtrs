/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Sun Aug 31 23:00:59 PDT 1997 by mann */

/*
 * mkfloppy [-1] filename
 *
 * Make a blank (unformatted) emulated floppy in a file.
 * By default, makes a JV3 floppy; with the -1 flag, makes a JV1 floppy.
 */

#include <stdio.h>

int
main(int argc, char *argv[])
{
  int i;
  FILE *f;
  if (argc == 2) {
    /* Unformatted JV3 disk. */
    f = fopen(argv[1], "w");
    if (f == NULL) {
      perror(argv[1]);
      exit(1);
    }
    for (i=0; i<(256*34); i++) {
      putc(0xff, f);
    }
    fclose(f);
  } else {
    if (argc != 3 || strcmp(argv[1], "-1")) {
      fprintf(stderr, "usage: %s [-1] filename\n", argv[0]);
      exit(2);
    }
    /* Unformatted JV1 disk - just an empty file! */
    f = fopen(argv[2], "w");
    fclose(f);
  }
  return 0;
}
