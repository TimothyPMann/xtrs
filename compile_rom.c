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
   Last modified on Tue Aug  5 19:44:52 PDT 1997 by mann
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

static void write_output()
{
    int address = 0;
    int i;
    
    highest_address++;

    printf("int trs_rom_size = %d;\n", highest_address);
    printf("unsigned char trs_rom[%d] = \n{\n", highest_address);
    
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

static void write_norom_output()
{
    printf("int trs_rom_size = -1;\n");
    printf("unsigned char trs_rom[1];\n");
}

main(argc, argv)
    int argc;
    char *argv[];
{
    if(argc == 1)
    {
	fprintf(stderr,
		"No specified ROM file, ROM will not be built into program.\n");
	write_norom_output();
    }
    else if(argc != 2)
    {
	error("usage: compile_rom hexfile");
    }
    else
    {
	load_rom(argv[1]);
	write_output();
    }
    exit(0);
}
