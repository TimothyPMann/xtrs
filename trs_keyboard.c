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
   Last modified on Tue Dec 17 13:06:20 PST 1996 by mann
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
    4, 0, -1, /* 0x30 */
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
#ifdef OLDWAY
    -1, 0, -1, /* 0x5b */
    -1, 0, -1, /* 0x5c */
    -1, 0, -1, /* 0x5d */
    -1, 0, -1, /* 0x5e */
    -1, 0, -1, /* 0x5f */
    -1, 0, -1, /* 0x60 */
#else
    /* Why not?  TRS-80 didn't have these keys, but there
       is a place in the matrix for them, and software decodes
       them as expected. */
    3, 3, -1, /* 0x5b */
    3, 4, -1, /* 0x5c */
    3, 5, -1, /* 0x5d */
    3, 6, -1, /* 0x5e */
    3, 7, -1, /* 0x5f */
    0, 0, -1, /* 0x60 */  /* causes ` to be shift-@, the pause key */
#endif
    0, 1, 0, /* 0x61 */
    0, 2, 0, /* 0x62 */
    0, 3, 0, /* 0x43 */
    0, 4, 0, /* 0x64 */
    0, 5, 0, /* 0x65 */
    0, 6, 0, /* 0x66 */
    0, 7, 0, /* 0x67 */
    1, 0, 0, /* 0x68 */
    1, 1, 0, /* 0x69 */
    1, 2, 0, /* 0x6a */
    1, 3, 0, /* 0x6b */
    1, 4, 0, /* 0x6c */
    1, 5, 0, /* 0x6d */
    1, 6, 0, /* 0x6e */
    1, 7, 0, /* 0x6f */
    2, 0, 0, /* 0x70 */
    2, 1, 0, /* 0x71 */
    2, 2, 0, /* 0x72 */
    2, 3, 0, /* 0x73 */
    2, 4, 0, /* 0x74 */
    2, 5, 0, /* 0x75 */
    2, 6, 0, /* 0x76 */
    2, 7, 0, /* 0x77 */
    3, 0, 0, /* 0x78 */
    3, 1, 0, /* 0x79 */
    3, 2, 0, /* 0x7a */
#ifdef OLDWAY
    -1, 0, -1, /* 0x7b */
    -1, 0, -1, /* 0x7c */
    -1, 0, -1, /* 0x7d */
    -1, 0, -1, /* 0x7e */
#else
    /* Why not?  TRS-80 didn't have these keys, but there
       is a place in the matrix for them, and software decodes
       them as expected. */
    3, 3, 1, /* 0x7b */
    3, 4, 1, /* 0x7c */
    3, 5, 1, /* 0x7d */
    3, 6, 1, /* 0x7e */
#endif
    6, 1, -1 /* 0x7f */ };

static int kb_mem_value(address, ascii_key, shift_key, clear_key)
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
    if((address & (1 << 6)) && clear_key)
    {
        rval |= 2;
    }
    return rval;
}

int trs_kb_mem_read(address)
    int address;
{
    int keystate;

#ifdef XTRASH
#if KBWAIT || KBQUEUE
    static int last_state = 0;

    /* Look for characteristic patterns, not specific PC values, so as
       to match both the ROM keyboard driver and also LDOS KI/DVR in
       high memory */

    if(REG_IX == 0x4015 && mem_read_word(REG_SP) == 0x03dd) 
    {
	/* In the system keyboard driver */
	if(mem_read_word(REG_PC) == 0xae5f && REG_BC == 0x3801)
	{
	    /* Scanning the first row */
	    if(mem_read_word(REG_SP + 10) == 0x004c)
            {
		/* Called from the wait-for-input routine, so wait for
                   a key. */
		last_state = trs_next_key();
		/* if (last_state)
		 *   printf("next_key pc 0x%04x ret (0x%02x) %c\n",
		 *	     REG_PC, last_state, last_state);
                 */
	    }
	    else
	    {
		/* Not waiting for a key */
		last_state = trs_next_key_nowait();
 		/*if (last_state)
 		 *   printf("next_key_nowait pc 0x%04x ret (0x%02x) %c\n",
 		 *	     REG_PC, last_state, last_state); 
                 */
	    }
	}
	/* If past scanning the first row, use the same state */
	keystate = last_state;
    }
    else
    {
	/* Reading the keyboard from outside the keyboard driver */
	keystate = trs_kb_poll();
	/*if (keystate)
	 *  printf("kb_poll pc 0x%04x ret (0x%02x) %c\n",
	 *	   REG_PC, keystate, keystate);
         */
    }
#else /* KBWAIT */
    keystate = trs_kb_poll();
#endif /* KBWAIT */

#else /* XTRASH */
    keystate = 0x0d;
#endif /* XTRASH */

    return kb_mem_value(address, keystate & 0x7f, keystate & 0x80,
			keystate & 0x100);
}
