/*
 * Copyright (C) 1992 Clarendon Hill Software.
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software, provided this copyright notice is retained. 
 *
 * This software is provided "as is" without any expressed or implied
 * warranty.  If this software brings on any sort of damage -- physical,
 * monetary, emotional, or brain -- too bad.  You've got no one to blame
 * but yourself. 
 *
 * The software may be modified for your own purposes, but modified versions
 * must retain this notice.
 */

/* 
 * Portions copyright (c) 1996-2008, Timothy P. Mann
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

#include "z80.h"
#include <stdlib.h>

#define BUFFER_SIZE 256

extern void hex_transfer_address(int address);
extern void hex_data(int address, int value);

static int hex_byte(char *string)
{
    char buf[3];

    buf[0] = string[0];
    buf[1] = string[1];
    buf[2] = '\0';

    return(strtol(buf, (char **)NULL, 16));
}
    
int load_hex(FILE *file)
{
    char buffer[BUFFER_SIZE];
    char *b;
    int num_bytes;
    int address;
    int check;
    int value;
    int high = 0;

    while(fgets(buffer, BUFFER_SIZE, file))
    {
	if(buffer[0] == ':')
	{
	    /* colon */
	    b = buffer + 1;

	    /* number of bytes on the line */
	    num_bytes = hex_byte(b);  b += 2;
	    check = num_bytes;

	    /* the starting address */
	    address = hex_byte(b) << 8;  b += 2;
	    address |= hex_byte(b);  b+= 2;
	    check += (address >> 8) + (address & 0xff);

	    /* a zero? */
	    b += 2;

	    /* the data */
	    if(num_bytes == 0)
	    {
		/* Transfer address */
		hex_transfer_address(address);
	    } else {
		while(num_bytes--)
		{
		    value = hex_byte(b);  b += 2;
		    hex_data(address++, value);
		    check += value;
		}
		if (address > high) high = address;

		/* the checksum */
		value = hex_byte(b);
		if(((0x100 - check) & 0xff) != value)
		{
		    fatal("bad checksum from hex file");
		}
	    }
	}
    }
    return high;
}
