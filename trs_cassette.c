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
   Last modified on Wed Apr 15 21:33:37 PDT 1998 by mann
*/

#include "trs.h"
#include "z80.h"
#include <string.h>

#if __linux
#include <sys/io.h>
#include <asm/io.h>
#endif

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

/* ioport of the SoundBlaster command register. 0 means none */
static unsigned int sb_address=0;
static unsigned char sb_cass_volume[4];
static unsigned char sb_sound_volume[2];

/* try to initialize SoundBlaster. Usual ioport is 0x220 */
void trs_sound_init(ioport, vol)
    int ioport; int vol;
{
#if __linux
    if(sb_address != 0) return;
    if((ioport & 0xFFFFFF0F) != 0x200)
    {
        fprintf(stderr, "Error in the SoundBlaster IOPort\n");
	return;
    }
    sb_address = ioport + 0xC; /* COMMAND address */
    if(ioperm(ioport, 0x10, 1))
    {
        perror("Unable to access SoundBlaster");
	sb_address = 0;
    }
    setuid(getuid());
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    /* Values in comments from Model I technical manual.  Model III/4 used
       a different value for one resistor in the network, so these
       voltages are not exactly right.  In particular 3 and 0 are no
       longer almost identical, but as far as I know, 3 is still unused.
    */
    sb_cass_volume[0] = (vol*255)/200;  /* 0.46 V */
    sb_cass_volume[1] = (vol*255)/100;  /* 0.85 V */
    sb_cass_volume[2] = 0;              /* 0.00 V */
    sb_cass_volume[3] = (vol*255)/200;  /* unused, but about 0.46 V */
    sb_sound_volume[0] = 0;
    sb_sound_volume[1] = (vol*255)/100;
#endif
}

void trs_cassette_motor(value)
{
    if(value)
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

    if((trs_model == 1) &&
       (REG_PC == 0x0228) &&  /* writing bit to cassette */
       (mem_read_word(REG_SP) == 0x01df) &&  /* called from write-bit */
       (mem_read_word(REG_SP + 2) == 0x026e)) /* called from write-byte */
    {
	if((mem_read_word(REG_SP + 12) == 0x028d) && /* from write-leader */
	   (mem_read((REG_SP + 9) & 0xffff) == 0xff)) /* start of leader */
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

#if __linux
    /* Do sound emulation */
    if((cassette_motor == 0) && sb_address)
    {
	while (inb(sb_address) & 0x80)
	    ;
	outb(0x10, sb_address);
	while (inb(sb_address) & 0x80)
	    ;
	outb(sb_cass_volume[value], sb_address);
    }
#endif /* linux */
}


/* Model 4 sound port */
void
trs_sound_out(value)
{
#if __linux
    /* Do sound emulation */
    if(sb_address)
    {
	while (inb(sb_address) & 0x80)
	    ;
	outb(0x10, sb_address);
	while (inb(sb_address) & 0x80)
	    ;
	outb(sb_sound_volume[value], sb_address);
    }
#endif /* linux */
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
    if (trs_model != 1) return 0xff; /* not implemented */

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

