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


static struct
{
    int address_bit;
    int data_bit;
    int shift_value;
} key_table[] = {
    -1, 0, -1, /* 0x0 */
    -1, 0, -1, /* 0x1 */
    -1, 0, -1, /* 0x2 */
    6, 2, -1, /* 0x3 */
    -1, 0, -1, /* 0x4 */
    -1, 0, -1, /* 0x5 */
    -1, 0, -1, /* 0x6 */
    -1, 0, -1, /* 0x7 */
    -1, 0, -1, /* 0x8 */
    -1, 0, -1, /* 0x9 */
    -1, 0, -1, /* 0xa */
    -1, 0, -1, /* 0xb */
    -1, 0, -1, /* 0xc */
    6, 0, -1, /* 0xd */
    -1, 0, -1, /* 0xe */
    -1, 0, -1, /* 0xf */
    -1, 0, -1, /* 0x10 */
    6, 3, -1, /* 0x11 */
    6, 4, -1, /* 0x12 */
    6, 5, -1, /* 0x13 */
    6, 6, -1, /* 0x14 */
    -1, 0, -1, /* 0x15 */
    -1, 0, -1, /* 0x16 */
    -1, 0, -1, /* 0x17 */
    -1, 0, -1, /* 0x18 */
    -1, 0, -1, /* 0x19 */
    -1, 0, -1, /* 0x1a */
    -1, 0, -1, /* 0x1b */
    -1, 0, -1, /* 0x1c */
    -1, 0, -1, /* 0x1d */
    -1, 0, -1, /* 0x1e */
    -1, 0, -1, /* 0x1f */
    6, 7, -1, /* 0x20 */
    4, 1, 1, /* 0x21 */
    4, 2, 1, /* 0x22 */
    4, 3, 1, /* 0x23 */
    4, 4, 1, /* 0x24 */
    4, 5, 1, /* 0x25 */
    4, 6, 1, /* 0x26 */
    4, 7, 1, /* 0x27 */
    5, 0, 1, /* 0x28 */
    5, 1, 1, /* 0x29 */
    5, 2, 1, /* 0x2a */
    5, 3, 1, /* 0x2b */
    5, 4, 0, /* 0x2c */
    5, 5, 0, /* 0x2d */
    5, 6, 0, /* 0x2e */
    5, 7, 0, /* 0x2f */
    4, 0, 0, /* 0x30 */
    4, 1, 0, /* 0x31 */
    4, 2, 0, /* 0x32 */
    4, 3, 0, /* 0x33 */
    4, 4, 0, /* 0x34 */
    4, 5, 0, /* 0x35 */
    4, 6, 0, /* 0x36 */
    4, 7, 0, /* 0x37 */
    5, 0, 0, /* 0x38 */
    5, 1, 0, /* 0x39 */
    5, 2, 0, /* 0x3a */
    5, 3, 0, /* 0x3b */
    5, 4, 1, /* 0x3c */
    5, 5, 1, /* 0x3d */
    5, 6, 1, /* 0x3e */
    5, 7, 1, /* 0x3f */
    0, 0, -1, /* 0x40 */
    0, 1, 1, /* 0x41 */
    0, 2, 1, /* 0x42 */
    0, 3, 1, /* 0x43 */
    0, 4, 1, /* 0x44 */
    0, 5, 1, /* 0x45 */
    0, 6, 1, /* 0x46 */
    0, 7, 1, /* 0x47 */
    1, 0, 1, /* 0x48 */
    1, 1, 1, /* 0x49 */
    1, 2, 1, /* 0x4a */
    1, 3, 1, /* 0x4b */
    1, 4, 1, /* 0x4c */
    1, 5, 1, /* 0x4d */
    1, 6, 1, /* 0x4e */
    1, 7, 1, /* 0x4f */
    2, 0, 1, /* 0x50 */
    2, 1, 1, /* 0x51 */
    2, 2, 1, /* 0x52 */
    2, 3, 1, /* 0x53 */
    2, 4, 1, /* 0x54 */
    2, 5, 1, /* 0x55 */
    2, 6, 1, /* 0x56 */
    2, 7, 1, /* 0x57 */
    3, 0, 1, /* 0x58 */
    3, 1, 1, /* 0x59 */
    3, 2, 1, /* 0x5a */
    -1, 0, -1, /* 0x5b */
    -1, 0, -1, /* 0x5c */
    -1, 0, -1, /* 0x5d */
    -1, 0, -1, /* 0x5e */
    -1, 0, -1, /* 0x5f */
    -1, 0, -1, /* 0x60 */
    0, 1, 0, /* 0x41 */
    0, 2, 0, /* 0x42 */
    0, 3, 0, /* 0x43 */
    0, 4, 0, /* 0x44 */
    0, 5, 0, /* 0x45 */
    0, 6, 0, /* 0x46 */
    0, 7, 0, /* 0x47 */
    1, 0, 0, /* 0x48 */
    1, 1, 0, /* 0x49 */
    1, 2, 0, /* 0x4a */
    1, 3, 0, /* 0x4b */
    1, 4, 0, /* 0x4c */
    1, 5, 0, /* 0x4d */
    1, 6, 0, /* 0x4e */
    1, 7, 0, /* 0x4f */
    2, 0, 0, /* 0x50 */
    2, 1, 0, /* 0x51 */
    2, 2, 0, /* 0x52 */
    2, 3, 0, /* 0x53 */
    2, 4, 0, /* 0x54 */
    2, 5, 0, /* 0x55 */
    2, 6, 0, /* 0x56 */
    2, 7, 0, /* 0x57 */
    3, 0, 0, /* 0x58 */
    3, 1, 0, /* 0x59 */
    3, 2, 0, /* 0x5a */
    -1, 0, -1, /* 0x7b */
    -1, 0, -1, /* 0x7c */
    -1, 0, -1, /* 0x7d */
    -1, 0, -1, /* 0x7e */
    6, 1, -1 /* 0x7f */ };

static int kb_mem_value(address, ascii_key, shift_key)
{
    int rval;
    int address_bit = -1;
    int data_bit;

    address_bit = key_table[ascii_key].address_bit;
    data_bit = key_table[ascii_key].data_bit;
    if(key_table[ascii_key].shift_value >= 0)
      shift_key = key_table[ascii_key].shift_value;

    rval = 0;
    if(address_bit >= 0)
    {
	if(address & (1 << address_bit))
	{
	    rval |= (1 << data_bit);
	}
    }
    if((address & (1 << 7)) && shift_key)
    {
	rval |= 1;
    }
    return rval;
}

#define KBWAIT

int trs_kb_mem_read(address)
    int address;
{
    int keystate;

#ifdef XTRASH
#ifdef KBWAIT
    static int last_state = 0;

    if(REG_PC == 0x03ec)
    {
	/* scanning keys in the keyboard driver */

	if(REG_BC == 0x3801)
	{
	    /* scanning the first row */

	    if((mem_read_word(REG_SP) == 0x03dd) &&
	       (mem_read_word(REG_SP + 10) == 0x004c))
	    {
		/* At the wait-for-input routine, so wait for a key. */
		if(last_state)
		{
		    /* make sure we see the keys go up */
		    last_state = 0;
		}
		else
		{
		    last_state = trs_next_key();

		    /*
		     * if(last_state)
		     * printf("Waited for key, returned %c\n", last_state);
		     */
		}
	    }
	    else
	    {
		/* In the beginning of the kb decoding routine, but
		   not waiting for a key */
		last_state = trs_kb_poll();

		/*
		 * if(last_state)
		 * printf("Read not waiting returned %c\n", last_state);
		 */
	    }
	}
	keystate = last_state;
    }
    /* Note that these addresses are not at the end of the entire ld
       instruction; this is because the implementation in z80.c does not
       increment the PC until after it calls this function.  Very kludgy. */
    else if((REG_PC == 0x040c) || (REG_PC == 0x041e))
    {
	/* reading other locations from the keyboard driver */
	keystate  = last_state;
    }
    else
    {
	keystate = trs_kb_poll();

	/*
	 * if(keystate)
	 *  printf("Unknown read returned %c\n", keystate);
	 */
    }
#else /* KBWAIT */
    keystate = trs_kb_poll();
#endif /* KBWAIT */

#else /* XTRASH */
    keystate = 0x0d;
#endif /* XTRASH */

    return kb_mem_value(address, keystate & 0x7f, keystate & 0x80);
}

#ifdef NEVER
static int kb_mem_value(address, ascii_key, shift_key)
{
    int rval;
    int offset;
    int address_bit = -1;
    int data_bit;

    if(ascii_key)
    {
	if(isupper(ascii_key))
	{
	    shift_key = 1;
	}
	else if(islower(ascii_key))
	{
	    ascii_key = toupper(ascii_key);
	    shift_key = 0;
	}
	
	if((ascii_key >= '@') && (ascii_key <= 'Z'))
	{
	    offset = ascii_key - '@';
	    address_bit = offset >> 3;
	    data_bit = offset & 0x7;
	}
	else if((ascii_key >= '0') && (ascii_key <= ';'))
	{
	    offset = ascii_key - '0';
	    address_bit = (offset >> 3) + 4;
	    data_bit = offset & 0x7;
	    shift_key = 0;
	}
	else if((ascii_key >= '!') && (ascii_key <= '+'))
	{
	    offset = ascii_key - '!';
	    address_bit = (offset >> 3) + 4;
	    data_bit = offset & 0x7;
	    shift_key = 1;
	}
	else
	{
	    switch(ascii_key)
	    {
	      case '<':
		address_bit = 5;
		data_bit = 4;
		shift_key = 1;
		break;
	      case '=':
		address_bit = 5;
		data_bit = 5;
		shift_key = 1;
		break;
	      case '>':
		address_bit = 5;
		data_bit = 6;
		shift_key = 1;
		break;
	      case '?':
		address_bit = 5;
		data_bit = 7;
		shift_key = 1;
		break;
	      case ',':
		address_bit = 5;
		data_bit = 4;
		shift_key = 0;
		break;
	      case '-':
		address_bit = 5;
		data_bit = 5;
		shift_key = 0;
		break;
	      case '.':
		address_bit = 5;
		data_bit = 6;
		shift_key = 0;
		break;
	      case '/':
		address_bit = 5;
		data_bit = 7;
		shift_key = 0;
		break;
	      case ' ':
		address_bit = 6;
		data_bit = 7;
		break;
	      case 0x3:	/* break key */
		address_bit = 6;
		data_bit = 2;
		break;
	      case 0x11:	/* up arrow */
		address_bit = 6;
		data_bit = 3;
		break;
	      case 0x12:	/* down arrow */
		address_bit = 6;
		data_bit = 4;
		break;
	      case 0x13:	/* left arrow */
		address_bit = 6;
		data_bit = 5;
		break;
	      case 0x14:	/* right arrow */
		address_bit = 6;
		data_bit = 6;
		break;
	      case 0x0D:	/* enter */
		address_bit = 6;
		data_bit = 0;
		break;
	      case 0x7F:	/* clear ? */
		address_bit = 6;
		data_bit = 1;
		break;
	    }
	}
    }
    rval = 0;
    if(address_bit >= 0)
    {
	if(address & (1 << address_bit))
	{
	    rval |= (1 << data_bit);
	}
    }
    if((address & (1 << 7)) && shift_key)
    {
	rval |= 1;
    }
    return rval;
}
#endif
