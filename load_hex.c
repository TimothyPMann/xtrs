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

#include "z80.h"

#define BUFFER_SIZE 256

static int hex_byte(string)
    char *string;
{
    char buf[3];

    buf[0] = string[0];
    buf[1] = string[1];
    buf[2] = '\0';

    return(strtol(buf, (char **)NULL, 16));
}
    
void load_hex(file)
    FILE *file;
{
    char buffer[BUFFER_SIZE];
    char *b;
    int num_bytes;
    int address;
    int check;
    int value;

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
	    while(num_bytes--)
	    {
		value = hex_byte(b);  b += 2;
		mem_write_rom(address++, value);
		check += value;
	    }

	    /* the checksum */
	    value = hex_byte(b);
	    if(((0x100 - check) & 0xff) != value)
	    {
		error("bad checksum from hex file");
	    }
	}
    }
}
