/* Routines to write a TRS-80 DOS "/cmd" file */
/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Wed Aug 13 19:46:49 PDT 1997 by mann */

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
cmd_end_of_file()
{
    if (!got_transfer) cmd_data(0, -3);
}
