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

void trs_printer_write(value)
{
    if(value == 0x0D)
    {
	putchar('\n');
    }
    else
    {
	putchar(value);
    }
}

int trs_printer_read()
{
    return 0x30;	/* printer selected, ready, with paper, not busy */
}
