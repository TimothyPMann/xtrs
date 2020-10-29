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
#include "trs_stringy.h"
#include <errno.h>

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
 * Initialize a blank virtual stringy floppy wafer with specified parameters.
 * Returns 0 if OK, negative on error.
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
    return -1;
  }

  return 0;
}

int
stringy_set_write_prot(FILE *f,
		       int format,
		       int writeProt)
{
  int ires;
  int flags;

  switch (format) {
  case STRINGY_FMT_DEBUG:
    return ENOTSUP;

  case STRINGY_FMT_ESF:
    break;

  default:
    return ENOTSUP;
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
