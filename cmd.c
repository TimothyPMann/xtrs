/* Routines to write a TRS-80 DOS "/cmd" file */

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

#include <stdio.h>

static int last_address, block_address;
static int block_size, got_transfer;
static unsigned char block[256];
static FILE* file;

void
cmd_init(FILE* f)
{
    last_address = block_address = -2;
    block_size = 0;
    got_transfer = 0;
    file = f;
}

void
cmd_data(int address, int value)
{
    unsigned char* p;

    if (value < 0 || address != last_address + 1 || block_size >= 256) {
	if (block_size > 0) {
	    /* close off current block */
	    putc(1, file);
	    putc((block_size + 2) & 0xff, file);
	    putc(block_address & 0xff, file);
	    putc((block_address >> 8) & 0xff, file);
	    p = block;
	    while (block_size) {
		putc(*p++, file);
		block_size--;
	    }
	}
	last_address = block_address = address;
	if (value == -2) {
	    /* transfer address */
	    putc(2, file);
	    putc(2, file);
	    putc(block_address & 0xff, file);
	    putc((block_address >> 8) & 0xff, file);
	    return;
	}
	if (value == -3) {
	    /* eof, no transfer address */
	    putc(3, file);
	    return;
	}
    }    
    /* continue current block */
    block[block_size++] = value;
    last_address = address;
    return;
}

void
cmd_transfer_address(int address)
{
    cmd_data(address, -2);
    got_transfer = 1;
}

void
cmd_end_of_file(void)
{
    if (!got_transfer) cmd_data(0, -3);
}
