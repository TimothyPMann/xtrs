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
#include "trs.h"

static void check_endian()
{
    wordregister x;
    x.byte.low = 1;
    x.byte.high = 0;
    if(x.word != 1)
    {
	error("Program compiled with wrong ENDIAN value -- adjust the Makefile.local, recompile, and try again.");
    }
}

void trs_load_rom(filename)
    char *filename;
{
    FILE *program;
    
    if((program = fopen(filename, "r")) == NULL)
    {
	char message[100];
	sprintf(message, "could not read %s", filename);
	error(message);
    }
    load_hex(program);
    fclose(program);
}

void trs_load_compiled_rom()
{
    int i;

    for(i = 0; i < trs_rom_size; ++i)
    {
	mem_write_rom(i, trs_rom[i]);
    }
}

main(argc, argv)
    int argc;
    char *argv[];
{
    int debug = FALSE;
    int instrument = FALSE;
    int i = 0;

    check_endian();

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-debug")) {
	    debug = TRUE;
	} else if (!strcmp(argv[i],"-instrument")) {
	    instrument = TRUE;
	}
    }
	
    mem_init();
#ifdef XTRASH
    argc = trs_screen_init(argc,argv,&debug);
#endif

    if(instrument)
    {
#ifdef INSTRUMENT
	printf("Running in instrument mode.\n");
	instrument_run(1);
#else
	error("instrument mode is not supported in this version");
#endif
    }
    else if(debug)
    {
	printf("Entering debugger.\n");
	debug_init();
	debug_shell();
	printf("Quitting.\n");
    }
    else
    {
	z80_reset();
	z80_run(TRUE);	/* run continuously */
	printf("Z-80 Halted.\n");
	exit(0);
    }
}
