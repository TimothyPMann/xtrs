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
   Last modified on Wed Aug 27 16:53:33 PDT 1997 by mann
*/

#include "trs.h"
#include "z80.h"
#include <string.h>

#define CLOSE		0
#define READ		1
#define WRITE		2

#define CONTROL_FILENAME	".cassette.ctl"
#define DEFAULT_FILENAME	"cassette.bin"

static char cassette_filename[256];
static int cassette_position;
static int cassette_state = CLOSE;
static int cassette_motor = 0;
static FILE *cassette_file;

static void get_control()
{
    FILE *f;

    f = fopen(CONTROL_FILENAME, "r");
    if((!f) ||
       (fscanf(f, "%s %d", cassette_filename, &cassette_position) != 2))
    {
	fprintf(stderr, "Error reading %s, cassette file will be: %s\n",
		CONTROL_FILENAME, DEFAULT_FILENAME);
	strcpy(cassette_filename, DEFAULT_FILENAME);
	cassette_position = 0;
    }

    if(f)
    {
	fclose(f);
    }
}

static void put_control()
{
    FILE *f;

    f = fopen(CONTROL_FILENAME, "w");

    if(f)
    {
	fprintf(f, "%s %d", cassette_filename, cassette_position);
	fclose(f);
    }
}

static int assert_state(state)
    int state;
{
    if(cassette_state == state)
    {
	return 1;
    }

    if(cassette_state != CLOSE)
    {
	cassette_position = ftell(cassette_file);
	fclose(cassette_file);
	put_control();
    }

    switch(state)
    {
      case READ:
	get_control();
	cassette_file = fopen(cassette_filename, "r");
	if(cassette_file == NULL)
	{
	    fprintf(stderr, "Couldn't read %s\n", cassette_filename);
	    return 1;
	}
	fseek(cassette_file, cassette_position, 0);
	break;
      case WRITE:
	get_control();
	cassette_file = fopen(cassette_filename, "r+");
        if(cassette_file == NULL)
	{
	    cassette_file = fopen(cassette_filename, "w");
	}
	if(cassette_file == NULL)
	{
	    fprintf(stderr, "Couldn't write %s\n", cassette_filename);
	    return 1;
	}
	fseek(cassette_file, cassette_position, 0);
	break;
    }
    
    cassette_state = state;
    return 0;
}

void trs_cassette_out(value)
{
#ifdef CASSDEBUG
    fprintf(stderr, 
    "cass out %02x, pc %04x, stack %04x %04x %04x %04 x%04x %04x %04x %04x\n",
	    value, REG_PC, mem_read_word(REG_SP), mem_read_word(REG_SP + 2),
	    mem_read_word(REG_SP + 4), mem_read_word(REG_SP + 6),
	    mem_read_word(REG_SP + 8), mem_read_word(REG_SP + 10),
	    mem_read_word(REG_SP + 12), mem_read_word(REG_SP + 14));
#endif
    if (trs_model == 3) return; /* not implemented */

    if(value & 4)
    {
	/* motor on */
	if(!cassette_motor)
	{
	    cassette_motor = 1;
	}
    }
    else
    {
	/* motor off */
	if(cassette_motor)
	{
	    assert_state(CLOSE);
	    cassette_motor = 0;
	}
    }

    if((REG_PC == 0x0228) &&  /* writing bit to cassette */
       (mem_read_word(REG_SP) == 0x01df) &&  /* called from write-bit */
       (mem_read_word(REG_SP + 2) == 0x026e)) /* called from write-byte */
    {
	if((mem_read_word(REG_SP + 12) == 0x028d) && /* from write-leader */
	   (mem_read(REG_SP + 9) == 0xff)) /* start of leader */
	{
	    if(assert_state(WRITE))
	    {
		return;
	    }
	}

	if(cassette_state == WRITE)
	{
	    /* write that byte! */
	    fputc(REG_D, cassette_file);
	}

	mem_write(0x40d3, REG_A); /* take care of instr at 0228 */

	/* jump to the end of the write-byte routine */
	REG_SP += 4;
	REG_PC = 0x0279;
    }
}

int trs_cassette_in(modesel)
    int modesel;
{
#ifdef CASSDEBUG
    fprintf(stderr, 
    "cass in, pc %04x, stack %04x %04x %04x %04x %04x %04x %04x %04x\n",
	    REG_PC, mem_read_word(REG_SP), mem_read_word(REG_SP + 2),
	    mem_read_word(REG_SP + 4), mem_read_word(REG_SP + 6),
	    mem_read_word(REG_SP + 8), mem_read_word(REG_SP + 10),
	    mem_read_word(REG_SP + 12), mem_read_word(REG_SP + 14));
#endif
    if (trs_model == 3) return 0xff;

    if(REG_PC == 0x0245)
    {
	if(mem_read_word(REG_SP + 4) == 0x029b)
	{
	    /* this is a read from the code which reads the header */
	    
	    if(!assert_state(READ))
	    {
		int nextbyte;
		
		do
		{
		    if (z80_state.nmi) return 0; /* allow reboot */
		    nextbyte = fgetc(cassette_file);
		}
		while(nextbyte != 0xa5);

		/* do the pop af instruction */
		REG_AF = mem_read_word(REG_SP);
		REG_SP += 2;

		/* skip to the end of the function */
		REG_PC = 0x025a;

		/* this will cause the A register to be loaded
		   with the return value */
		return 0xa5;
	    }
	}
	else if(mem_read_word(REG_SP + 4) == 0x023c)
	{
	    /* reading a byte from the cassette */

	    int nextbyte;
	    
	    nextbyte = fgetc(cassette_file);
	    
	    /* spoof caller into thinking we are at end of loop */
	    mem_write(REG_SP + 3, 1);
	    
	    /* do the pop af instruction */
	    REG_AF = mem_read_word(REG_SP);
	    REG_SP += 2;
	    
	    /* skip to the end of the function */
	    REG_PC = 0x025a;
	    
	    /* this will cause the A register to be loaded
	       with the return value */
	    return nextbyte;
	}
    }

    /* default -- return all 1's plus the modesel mask */
    return modesel ? 0xff : 0xbf;
}
