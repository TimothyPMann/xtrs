/* 
 * Copyright (c) 2011-2020, Timothy P. Mann
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
 * Common Stringy Floppy code used by xtrs and mkdisk.
 */
#ifndef _STRINGY_H
#define _STRINGY_H

#include "z80.h"

typedef long stringy_pos_t;

#define STRINGY_CELL_WIDTH 124 // in t-states

#define STRINGY_FMT_DEBUG 1
#define STRINGY_FMT_ESF 2
#define STRINGY_FMT_DEFAULT STRINGY_FMT_ESF

#define stringy_len_from_kb(kb) ((kb) * 1024 * 2 * 9 / 8) // allow gaps + leaders (?)

#define STRINGY_LEN_DEFAULT stringy_len_from_kb(64)
#define STRINGY_EOT_DEFAULT 60 // a good value per MKR

/*
 * .esf file format used by TRS32.
 */
static const char stringy_esf_magic[4] = "ESF\x1a";
static const Uchar stringy_esf_header_length = 12;
static const Uchar stringy_esf_write_protected = 1;
/*
struct {
  char magic[4] = stringy_esf_magic;
  Uchar headerLength = 12;  // length of this header, in bytes
  Uchar flags;              // bit 0: write protected; others reserved
  Ushort leaderLength = 60; // little endian, in bit cells
  Uint length;              // little endian, in bytes
  Uchar data[length];
}
 */

/*
 * Debug format
 */
static const char stringy_debug_header[] = "xtrs stringy debug %ld %ld %d\n";

int stringy_init_with(FILE *f,
		      int format,
		      Uint lengthBytes, // data length in bytes
		      Uint eotCells,    // leader length in bit cells
		      int writeProt);

int stringy_set_write_prot(FILE *f,
			   int format,
			   int writeProt);

#endif /* _STRINGY_H */
