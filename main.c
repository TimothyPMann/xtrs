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
   Last modified on Tue Dec 17 12:59:51 PST 1996 by mann
*/

#include "z80.h"
#include "trs.h"
#include "trs_disk.h"

int trs_model = 1;

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
    int c;
    
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
	    mem_write_rom(trs_rom_size++, c);
	    c = getc(program);
	}
    }
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
	} else if (!strcmp(argv[i],"-spinfast")) {
	    /* Kludge -- make index hole go by at 10x normal frequency.
               This makes NEWDOS 80 somewhat happier.  NEWDOS 80
               apparently looks to see if a drive contains a disk by
               polling for some fixed number of iterations, looking
               for an index hole to go by.  But it seems we emulate
               the instructions in the loop too fast, so that sometimes
               no hole has gone by yet when NEWDOS 80 gives up.
	    */
	    trs_disk_spinfast = TRUE;
        } else if (!strcmp(argv[i],"-model1")) {
	    trs_model = 1;
        } else if (!strcmp(argv[i],"-model3")) {
	    trs_model = 3;  /* !!not fully implemented yet */
	}
    }
	
    mem_init();
    trs_disk_init(0);
#ifdef XTRASH
    argc = trs_screen_init(argc,argv,&debug);
#endif
    trs_timer_init();

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
	printf("Z-80 Halted.\n");  /* TPM: on a real Model I, executing a
				      HALT instruction is equivalent
				      to pressing the reset button */
	exit(0);
    }
}

