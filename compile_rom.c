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
   Modified by Timothy Mann, 1996
   Last modified on Sat Aug 23 14:21:45 PDT 1997 by mann
*/

#include "z80.h"

static int highest_address = 0;
static Uchar memory[Z80_ADDRESS_LIMIT];

/* Called by load_hex */
void hex_data(address, value)
    int address;
    int value;
{
    address &= 0xffff;

    memory[address] = value;
    if(highest_address < address)
      highest_address = address;
}

void hex_transfer_address(address)
     int address;
{
    /* Ignore */
}

static void load_rom(filename)
    char *filename;
{
    FILE *program;
    int c, trs_rom_size;
    
    if((program = fopen(filename, "r")) == NULL)
    {
	char message[100];
	sprintf(message, "could not read %s", filename);
	error(message);
    }
    c = getc(program);
    if (c == ':') {
        rewind(program);
        trs_rom_size = load_hex(program);
    } else {
        trs_rom_size = 0;
        while (c != EOF) {
	    hex_data(trs_rom_size++, c);
	    c = getc(program);
	}
    }
    fclose(program);
}

void error(string)
    char *string;
{
    fprintf(stderr, "compile_rom error: %s\n", string);
    exit(1);
}

static void write_output(which)
     char* which;
{
    int address = 0;
    int i;
    
    highest_address++;

    printf("int trs_rom%s_size = %d;\n", which, highest_address);
    printf("unsigned char trs_rom%s[%d] = \n{\n", which, highest_address);
    
    while(address < highest_address) 
    {
	printf("    ");
	for(i = 0; i < 8; ++i)
	{
	    printf("0x%.2x,", memory[address++]);

	    if(address == highest_address)
	      break;
	}
	printf("\n");
    }
    printf("};\n");
}

static void write_norom_output(which)
     char* which;
{
    printf("int trs_rom%s_size = -1;\n", which);
    printf("unsigned char trs_rom%s[1];\n", which);
}

main(argc, argv)
    int argc;
    char *argv[];
{
    if(argc == 2)
    {
	fprintf(stderr,
		"No specified ROM file, ROM %s will not be built into program.\n", argv[1]);
	write_norom_output(argv[1]);
    }
    else if(argc != 3)
    {
	error("usage: compile_rom model hexfile");
    }
    else
    {
	load_rom(argv[2]);
	write_output(argv[1]);
    }
    exit(0);
}
