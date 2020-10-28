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
 * Portions copyright (c) 1996-2020, Timothy P. Mann
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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "z80.h"
#include "trs.h"
#include "trs_disk.h"
#include "trs_hard.h"
#include "load_cmd.h"

int trs_model = 1;
int trs_paused = 1;
int trs_autodelay = 0;
char *program_name;
char *romfile = NULL;
char *romfile3 = NULL;
char *romfile4p = NULL;

static void check_endian(void)
{
    wordregister x;
    x.byte.low = 1;
    x.byte.high = 0;
    if(x.word != 1)
    {
	fatal("Program compiled with wrong ENDIAN value -- adjust the Makefile.local, type \"rm *.o\", recompile, and try again.");
    }
}

void trs_load_rom(char *filename)
{
    FILE *program;
    int c;

    if((program = fopen(filename, "r")) == NULL)
    {
	char message[100];
	sprintf(message, "could not read %s", filename);
	fatal(message);
    }
    c = getc(program);
    if (c == ':') {
        /* Assume Intel hex format */
        rewind(program);
        trs_rom_size = load_hex(program);
	fclose(program);
	return;
    } else if (c == 1 || c == 5) {
	/* Assume MODELA/III file */
	int res;
	extern Uchar *rom; /*!! fixme*/
	Uchar loadmap[Z80_ADDRESS_LIMIT];
	rewind(program);
	res = load_cmd(program, rom, loadmap, 0, NULL, -1, NULL, NULL, 1);
	if (res == LOAD_CMD_OK) {
	    trs_rom_size = Z80_ADDRESS_LIMIT;
	    while (trs_rom_size > 0) {
		if (loadmap[--trs_rom_size] != 0) {
		    trs_rom_size++;
		    break;
		}
	    }
	    fclose(program);
	    return;
	} else {
	    /* Guess it wasn't one */
	    rewind(program);
	    c = getc(program);
	}
    }
    trs_rom_size = 0;
    while (c != EOF) {
        mem_write_rom(trs_rom_size++, c);
	c = getc(program);
    }
}

void trs_load_compiled_rom(int size, unsigned char rom[])
{
    int i;
    
    trs_rom_size = size;
    for(i = 0; i < size; ++i)
    {
	mem_write_rom(i, rom[i]);
    }
}

void
trs_load_romfile(void)
{
  struct stat statbuf;

  switch (trs_model) {
  case 1:
#ifdef DEFAULT_ROM
    if (!romfile) {
      romfile = DEFAULT_ROM;
    }
#endif
    if (romfile != NULL && stat(romfile, &statbuf) == 0) {
      trs_load_rom(romfile);
    } else if (trs_rom1_size > 0) {
      trs_load_compiled_rom(trs_rom1_size, trs_rom1);
    } else {
      fatal("ROM file not specified!");
    }
    break;

  case 3: case 4:
#ifdef DEFAULT_ROM3
    if (!romfile3) {
      romfile3 = DEFAULT_ROM3;
    }
#endif
    if (romfile3 != NULL && stat(romfile3, &statbuf) == 0) {
      trs_load_rom(romfile3);
    } else if (trs_rom3_size > 0) {
      trs_load_compiled_rom(trs_rom3_size, trs_rom3);
    } else {
      fatal("ROM file not specified!");
    }
    break;

  default: /* 4P */
#ifdef DEFAULT_ROM4P
    if (!romfile4p) {
      romfile = DEFAULT_ROM4P;
    }
#endif
    if (romfile4p != NULL && stat(romfile4p, &statbuf) == 0) {
      trs_load_rom(romfile4p);
    } else if (trs_rom4p_size > 0) {
      trs_load_compiled_rom(trs_rom4p_size, trs_rom4p);
    } else {
      fatal("ROM file not specified!");
    }
    break;
  }
}

int main(int argc, char *argv[])
{
    int debug = FALSE;

    /* program_name must be set first because the error
     * printing routines use it. */
    program_name = strrchr(argv[0], '/');
    if (program_name == NULL) {
      program_name = argv[0];
    } else {
      program_name++;
    }

    check_endian();

    argc = trs_parse_command_line(argc, argv, &debug);
    if (argc > 1) {
      fatal("erroneous argument %s", argv[1]);
    }
    mem_init();
    trs_screen_init();
    trs_timer_init();
    trs_disk_init();
    trs_hard_init();
    stringy_init();

    trs_load_romfile();
    trs_reset(1);
    if (!debug) {
      /* Run continuously until exit or request to enter debugger */
      z80_run(TRUE);
    }
    printf("Entering debugger.\n");
    debug_init();
    debug_shell();
    printf("Quitting.\n");
    exit(0);
}

