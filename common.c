/* 
 * Copyright (c) 1999-2020, Timothy P. Mann
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

/* Common code used by xtrs and mkdisk */

#include "z80.h"
#include "trs_disk.h"
#include "trs_hard.h"
#include "trs_stringy.h"
#include "reed.h"
#include <errno.h>
#include <time.h>
#include <string.h>

/* Put a 2-byte quantity to a file in little-endian order */
/* Return -1 on error, 0 otherwise */
int
put_twobyte(Ushort n, FILE* f)
{
  int c;
  struct twobyte *p = (struct twobyte *) &n;
  c = putc(p->low, f);
  if (c == -1) return c;
  c = putc(p->high, f);
  if (c == -1) return c;
  return 0;
}

/* Put a 4-byte quantity to a file in little-endian order */
/* Return -1 on error, 0 otherwise */
int
put_fourbyte(Uint n, FILE* f)
{
  int c;
  struct fourbyte *p = (struct fourbyte *) &n;
  c = putc(p->byte0, f);
  if (c == -1) return c;
  c = putc(p->byte1, f);
  if (c == -1) return c;
  c = putc(p->byte2, f);
  if (c == -1) return c;
  c = putc(p->byte3, f);
  if (c == -1) return c;
  return 0;
}

/* Get a 2-byte quantity from a file in little-endian order */
/* Return -1 on error, 0 otherwise */
int
get_twobyte(Ushort *pp, FILE* f)
{
  int c;
  struct twobyte *p = (struct twobyte *) pp;
  c = getc(f);
  if (c == -1) return c;
  p->low = c;
  c = getc(f);
  if (c == -1) return c;
  p->high = c;
  return 0;
}

/* Get a 4-byte quantity from a file in little-endian order */
/* Return -1 on error, 0 otherwise */
int
get_fourbyte(Uint *pp, FILE* f)
{
  int c;
  struct fourbyte *p = (struct fourbyte *) pp;
  c = getc(f);
  if (c == -1) return c;
  p->byte0 = c;
  c = getc(f);
  if (c == -1) return c;
  p->byte1 = c;
  c = getc(f);
  if (c == -1) return c;
  p->byte2 = c;
  c = getc(f);
  if (c == -1) return c;
  p->byte3 = c;
  return 0;
}

/*
 * Initialize a blank virtual floppy disk with specified parameters.
 * -1 for a parameter chooses the default value.
 * Returns negative on error.  errno will have details.
 */
int
trs_disk_init_with(FILE *f,
		   int emutype,
		   int sides,    // emutype DMK only
		   int density,  // emutype DMK only
		   int eight,    // emutype DMK only
		   int ignden)   // emutype DMK only
{
  int i;

  if (emutype == -1) emutype = JV3;
  if (sides == -1) sides = 2;
  if (density == -1) density = 2;
  if (eight == -1) eight = 0;
  if (ignden == -1) ignden = 0;

  if (sides != 1 && sides != 2) {
    error("sides must be 1 or 2");
    errno = EINVAL;
    return -1;
  }
  if (density < 1 || density > 2) {
    error("density must be 1 or 2");
    errno = EINVAL;
    return -1;
  }

  switch (emutype) {
  case JV1:
    /* Unformatted JV1 disk - just an empty file! */
    break;

  case JV3:
    /* Unformatted JV3 disk. */
    for (i=0; i<(256*34); i++) {
      putc(0xff, f);
    }
    break;

  case DMK:
    /* Unformatted DMK disk */
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
    break;

  default:
    errno = EINVAL;
    return -1;
  }

  return 0;
}


/*
 * Set or clear a virtual floppy disk's internal write protect flag.
 * Returns negative on error.  errno will have details.
 */
int
trs_disk_set_write_prot(FILE *f,
			int emutype,
			int writeprot)
{
  switch (emutype) {
  case JV1:
    /* No indication inside; nothing to do here */
    return 0;

  case JV3:
    /* Set the magic byte */
    fseek(f, 256*34-1, 0);
    return putc(writeprot ? 0 : 0xff, f);

  case DMK:
    /* Set the magic byte */
    return putc(writeprot ? 0xff : 0, f);

  default:
    errno = EINVAL;
    return -1;
  }
}

/*
 * Initialize a blank virtual hard disk with specified parameters.
 * -1 for a parameter chooses the default value.
 * Returns negative on error.  errno will have details.
 */
int
trs_hard_init_with(FILE *f,
		   int cyl, int sec, int gran, int dir)
{
  /* Blank hard disk */
  /* We don't care about most of this header, but we generate
     it just in case some user wants to exchange hard drives with
     Matthew Reed's emulator or with Pete Cervasio's port of
     xtrshard/dct to Jeff Vavasour's Model III/4 emulator.
  */
  time_t tt = time(0);
  struct tm *lt = localtime(&tt);
  Uchar *rhhp;
  int cksum;
  ReedHardHeader rhh;
  int i;

  if (cyl == -1) cyl = 202;
  if (sec == -1) sec = 256;
  if (gran == -1) gran = 8;
  if (dir == -1) dir = 1;

  if (cyl < 3) {
    error("cyl < 3");
    errno = EINVAL;
    return -1;
  }
  if (cyl > 256) {
    error("cyl > 256");
    errno = EINVAL;
    return -1;
  }
  if (cyl > 203) {
    warning("cyl > 203 is incompatible with XTRSHARD/DCT");
  }
  if (sec < 4) {
    error("sec < 4");
    errno = EINVAL;
    return -1;
  }
  if (sec > 256) {
    error("sec > 256");
    errno = EINVAL;
    return -1;
  }
  if ((sec % 32) != 0) {
    warning("(sec % 32) != 0 is incompatible with WD1000/1010 emulation");
    if (sec > 32) {
      warning("(sec % 32) != 0 and sec > 32 "
	      "is incompatible with Matthew Reed's emulators");
    }
  }
  if (gran < 1) {
    error("gran < 1");
    errno = EINVAL;
    return -1;
  }
  if (gran > 8) {
    error("gran > 8");
    errno = EINVAL;
    return -1;
  }
  if (sec < gran) {
    error("sec < gran");
    errno = EINVAL;
    return -1;
  }
  if (sec % gran != 0) {
    error("sec %% gran != 0");
    errno = EINVAL;
    return -1;
  }
  if (sec / gran > 32) {
    error("sec / gran > 32");
    errno = EINVAL;
    return -1;
  }
  if (dir < 1) {
    error("dir < 1");
    errno = EINVAL;
    return -1;
  }
  if (dir >= cyl) {
    error("dir >= cyl");
    errno = EINVAL;
    return -1;
  }

  memset(&rhh, 0, sizeof(rhh));
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

  cksum = 0;
  rhhp = (Uchar *) &rhh;
  for (i=0; i<=31; i++) {
    cksum += rhhp[i];
  }
  rhh.cksum = ((Uchar) cksum) ^ 0x4c;

  return fwrite(&rhh, sizeof(rhh), 1, f);
}

/*
 * Set or clear a virtual hard disk's internal write protect flag.
 * Returns negative on error.  errno will have details.
 */
int
trs_hard_set_write_prot(FILE *f, int writeprot)
{
  int newmode;

  /* Set the magic bit */
  fseek(f, 7, 0);
  newmode = getc(f);
  if (newmode == EOF) {
    return newmode;
  }
  newmode = (newmode & 0x7f) | (writeprot ? 0x80 : 0);
  fseek(f, 7, 0);
  return putc(newmode, f);
}


/*
 * Initialize a blank virtual stringy floppy wafer with specified parameters.
 * Returns negative on error.  errno will have details.
 */
int
stringy_init_with(FILE *f,
		  int format,
		  Uint lengthBytes, // data length in bytes
		  Uint eotCells,    // leader length in bit cells
		  int writeProt)
{
  int ires;
  size_t sres;

  switch (format) {
  case STRINGY_FMT_DEBUG:
    ires = fprintf(f, stringy_debug_header,
		   (stringy_pos_t)lengthBytes * STRINGY_CELL_WIDTH * 8,
		   (stringy_pos_t)eotCells * STRINGY_CELL_WIDTH,
		   writeProt);
    if (ires < 0) return ires;
    break;

  case STRINGY_FMT_ESF:
    sres = fwrite(stringy_esf_magic, sizeof(stringy_esf_magic), 1, f);
    if (sres < 1) return sres;
    ires = fputc(stringy_esf_header_length, f);
    if (ires < 0) return ires;
    ires = fputc(writeProt ? stringy_esf_write_protected : 0, f);
    if (ires < 0) return ires;
    ires = put_twobyte(eotCells, f);
    if (ires < 0) return ires;
    ires = put_fourbyte(lengthBytes, f);
    if (ires < 0) return ires;
    break;

  default:
    error("unknown wafer image type on write");
    errno = EINVAL;
    return -1;
  }

  return 0;
}

/*
 * Set or clear a virtual stringy floppy wafer's internal write protect flag.
 * Returns negative on error.  errno will have details.
 */
int
stringy_set_write_prot(FILE *f,
		       int format,
		       int writeProt)
{
  int ires;
  int flags;

  switch (format) {
  case STRINGY_FMT_DEBUG:
    errno = ENOTSUP;
    return -1;

  case STRINGY_FMT_ESF:
    break;

  default:
    errno = EINVAL;
    return -1;
  }

  ires = fseek(f, sizeof(stringy_esf_magic) + 
	       sizeof(stringy_esf_header_length), SEEK_SET);
  if (ires < 0) return ires;
  flags = fgetc(f);
  if (flags < 0) return flags;
  flags = (flags & ~stringy_esf_write_protected) | 
          (writeProt ? stringy_esf_write_protected : 0);
  ires = fseek(f, sizeof(stringy_esf_magic) + 
	       sizeof(stringy_esf_header_length), SEEK_SET);
  if (ires < 0) return ires;
  ires = fputc(flags, f);
  if (ires < 0) return ires;

  return 0;
}
