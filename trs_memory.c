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
 * trs_memory.c -- memory emulation functions for the TRS-80 emulator
 *
 * Routines in this file perform operations such as mem_read and mem_write,
 * and are the top level emulation points for memory-mapped devices such
 * as the screen and keyboard.
 */

#include "z80.h"
#include "trs.h"
#include <malloc.h>

uchar *memory;

#define MEMORY_SIZE	Z80_ADDRESS_LIMIT

#define ROM_START	(0x0000)
#define IO_START	(0x3000)
#define KEYBOARD_START	(0x3800)
#define MORE_IO_START	(0x3900)
#define VIDEO_START	(0x3c00)
#define RAM_START	(0x4000)
#define PRINTER_ADDRESS	(0x37E8)

/*
 * Macros to determine quickly if an address is writeable or readable
 * directly from the memory buffer.  Readable addresses are rom, video,
 * or ram.  Writable addresses are ram only.  Words are handled such that
 * they are guaranteed to be entirely within the right space.
 */

#define WRITEABLE(address) \
  ((address) >= RAM_START)
#define READABLE(address) \
  (((ushort) ((address) - IO_START)) >= (VIDEO_START - IO_START))

#define WRITEABLE_WORD(address) \
  (((ushort) ((address) + 1)) >= (RAM_START + 1))
#define READABLE_WORD(address) \
  (((ushort) ((address) + 1 - IO_START)) >= (VIDEO_START + 1 - IO_START))

/*SUPPRESS 53*/
/*SUPPRESS 112*/

#ifdef XTRASH

/*
 * The video cache hacks are for speed, so that we don't send and flush
 * an X packet every time that we write a character to the screen.
 */

#define VIDEO_CACHE_SIZE	(1024)
static int video_cache_locations[VIDEO_CACHE_SIZE];
static int video_cache_values[VIDEO_CACHE_SIZE];
static int video_cache_count;
static int video_cache_on;

static void video_write(location, value)
    int location;
    int value;
{
    if(video_cache_on)
    {
	if(video_cache_count == VIDEO_CACHE_SIZE)
	{
	    /* flush the cache if it is full */
	    trs_screen_write_chars(video_cache_locations,
				   video_cache_values,
				   video_cache_count);
	    video_cache_count = 0;
	}
	video_cache_locations[video_cache_count] = location;
	video_cache_values[video_cache_count] = value;
	video_cache_count++;
    }
    else
    {
	/* ordinary write, no caching */
	trs_screen_write_char(location, value, TRUE);
    }
}

static void video_scroll()
{
    /*
     * Fix here when scrolling works.
     */
    trs_screen_scroll();
#ifdef notdef
    int i;

    for(i = 0; i < 0x3c0; ++i)
    {
	video_write(i, memory[i + VIDEO_START + 0x40]);
    }
#endif
}

static void cache_video_writes()
{
    video_cache_count = 0;
    video_cache_on = 1;
}

static void uncache_video_writes()
{
    if(video_cache_count)
    {
	trs_screen_write_chars(video_cache_locations,
			       video_cache_values,
			       video_cache_count);
    }
    video_cache_on = 0;
}
#else

/* Versions of the video routines for non-X hacking */

/*ARGSUSED*/
static void video_write(location, value)
    int location;
    int value;
{
    /* video write */
    if(isprint(value))
    {
	printf("%c", value);
	if(value == 'M')
	  printf("");
    }
    else
    {
	printf(".");
    }
    fflush(stdout);
}

static void video_scroll()
{
}

static void cache_video_writes()
{
}

static void uncache_video_writes()
{
}
#endif

void trs_exit()
{
    exit(0);
}

void trs_reset()
{
    /* Signal an interrupt. */
    z80_state.interrupt = TRUE;
    printf("Interrupt.\n");
}

void mem_init()
{
    memory = (uchar *) malloc(MEMORY_SIZE);

    bzero(memory, MEMORY_SIZE);
#ifdef XTRASH
    video_cache_on = 0;
#endif
}

/*
 * hack to let us initialize the ROM memory
 */
void mem_write_rom(address, value)
    int address;
    int value;
{
    address &= 0xffff;

    memory[address] = value;
}

#ifdef INSTRUMENT
int mem_read(address)
    int address;
{
    int rval;

    address &= 0xffff;
    if(READABLE(address))
    {
	/* read from ROM, video, or RAM */
	rval = memory[address];
    }
    else if((address >= KEYBOARD_START) && (address < MORE_IO_START))
    {
	/* read from the keyboard */
	rval = trs_kb_mem_read(address);
    }
    else
    {
	/* read from any of the I/O space */
	rval = 0xFF;
    }

    instrument_mem_read(address, rval);

    return rval;
}

void mem_write(address, value)
    int address;
    int value;
{
    address &= 0xffff;

    instrument_mem_write(address, value);

    if(WRITEABLE(address))
    {
	/* write to RAM */
	memory[address] = value;
    }
    else if((address >= VIDEO_START) && (address < RAM_START))
    {
	/*
	 * Video write.  Hack here to make up for the missing bit 6
	 * video ram, emulating the gate in Z30.
	 */
	if(value & 0xa0)
	  value &= 0xbf;
	else
	  value |= 0x40;

	memory[address] = value;

	video_write(address - VIDEO_START, value);
    }
}

/*
 * Words are stored with the low-order byte in the lower address.
 */
int mem_read_word(address)
    int address;
{
    int rval;
    uchar *m;

    address &= 0xffff;
    rval = mem_read(address++);
    rval |= mem_read(address) << 8;
    return rval;
}

void mem_write_word(address, value)
    int address;
{
    uchar *m;

    address &= 0xffff;
    mem_write(address++, value & 0xff);
    mem_write(address, value >> 8);
}
#else    

int mem_read(address)
    int address;
{
    address &= 0xffff;
    if(READABLE(address))
    {
	/* read from ROM, video, or RAM */
	return(memory[address]);
    }
    else if((address >= KEYBOARD_START) && (address < MORE_IO_START))
    {
	/* read from the keyboard */
	return trs_kb_mem_read(address);
    }
    else if(address == PRINTER_ADDRESS)
    {
	return trs_printer_read();
    }
    else
    {
	/* read from any of the I/O space */
	return 0xFF;
    }
}

void mem_write(address, value)
    int address;
    int value;
{
    address &= 0xffff;

    if(WRITEABLE(address))
    {
	/* write to RAM */
	memory[address] = value;
    }
    else if((address >= VIDEO_START) && (address < RAM_START))
    {
	/*
	 * Video write.  Hack here to make up for the missing bit 6
	 * video ram, emulating the gate in Z30.
	 */
	if(value & 0xa0)
	  value &= 0xbf;
	else
	  value |= 0x40;

	/*
	 * Speed hack -- check to see if the character has actually changed.
	 * Only call the video emulator if it has.
	 */
	if(memory[address] != value)
	{
	    memory[address] = value;

	    video_write(address - VIDEO_START, value);
	}
    }
    else if(address == PRINTER_ADDRESS)
    {
	trs_printer_write(value);
    }
}

/*
 * Words are stored with the low-order byte in the lower address.
 */
int mem_read_word(address)
    int address;
{
    int rval;
    uchar *m;

    address &= 0xffff;

    if(READABLE_WORD(address))
    {
	m = memory + address;
	rval = *m++;
	rval |= *m << 8;
	return rval;
    }
    else
    {
	rval = mem_read(address++);
	rval |= mem_read(address) << 8;
	return rval;
    }
}

void mem_write_word(address, value)
    int address;
{
    uchar *m;

    address &= 0xffff;

    if(WRITEABLE_WORD(address))
    {
	m = memory + address;
	*m++ = value & 0xff;
	*m = value >> 8;
    }
    else
    {
	mem_write(address++, value & 0xff);
	mem_write(address, value >> 8);
    }
}
#endif

/*
 * Block move instructions, for LDIR and LDDR instructions.
 *
 * Direction is either +1 or -1.
 *
 * Note that a count of zero => move 64K bytes.
 *
 * These can be special cased to do fun stuff like fast
 * video scrolling.
 */
void mem_block_transfer(dest, source, direction, count)
    ushort dest, source;
    int direction;
    ushort count;
{
    /* special case for screen scroll */
    if((dest == VIDEO_START) && (source == VIDEO_START + 0x40) &&
       (count == 0x3c0) && (direction > 0))
    {
	/* scroll screen one line */
	video_scroll();

	do
	  memory[dest++] = memory[source++];
	while(count--);
    }
    else
    {
	cache_video_writes();
	
	if(direction > 0)
	{
	    do
	    {
		mem_write(dest++, mem_read(source++));
		count--;
	    }
	    while(count);
	}
	else
	{
	    do
	    {
		mem_write(dest--, mem_read(source--));
		count--;
	    }
	    while(count);
	}

	uncache_video_writes();
    }
}
