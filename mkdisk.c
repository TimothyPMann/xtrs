/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Tue Dec 17 13:06:18 PST 1996 by mann */

/*
 * mkfloppy [-1] filename
 *
 * Make a blank (unformatted) emulated floppy in a file.
 *
 * By default, the emulated floppy is type JV3, compatible with Jeff
 * Vavasour's Model III/4 emulator.  It can be formatted with 256 byte
 * sectors, 1 or 2 sides, single or double density, with either FB or
 * F8 data address mark on any sector.  The total number of sectors on
 * the disk can be at most 2901, which is enough for an 80 track,
 * double-sided, double-density (18 sector) disk.  You cannot format a
 * sector with an incorrect track number or head number, or with
 * length other than 256 bytes.  It *is* possible to format a sector
 * with an intentional CRC error in the data field.
 * 
 * There is a funny kludge in my emulator for single density, to deal
 * with the fact that early Model I operating systems used an FA data
 * address mark for the directory (a nonstandard value supported only
 * by true 1771/1773 FDC chips), while others wrote F8 (for Model III
 * compatibility) but could read either.  On writing, any data address
 * mark other than FB is recorded as F8.  On reading in single
 * density, F8 is returned as FA.  This trick makes the different
 * operating systems perfectly compatible with each other -- which is
 * better than on a real Model I!
 *
 * With the -1 flag, the emulated floppy is type JV1, compatible with
 * Vavasour's shareware Model I emulator.  Type JV1 floppies can only
 * be single density, single sided, 10 sectors per track, 256 bytes
 * per sector, and have their directories on track 17.  With LDOS, use
 * FORMAT (DIR=17) to format.  */

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
