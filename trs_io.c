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

static int modesel = 0;

/*ARGSUSED*/
void z80_out(port, value)
    int port, value;
{
    int new_modesel;

    if(port == 0xFF)
    {
	/* screen mode select is on D3 line */
	new_modesel = (value >> 3) & 1;
	if(modesel != new_modesel)
	{
	    modesel = new_modesel;
	    trs_screen_mode_select(modesel ? EXPANDED : NORMAL);
	}

	/* do cassette emulation */
	trs_cassette_out(value & 0x7);
    }
    return;
}

/*ARGSUSED*/
int z80_in(port)
    int port;
{
    /* the cassette port -- return whatever the cassette hardware wants */
    if(port == 0xFF)
    {
	return trs_cassette_in(modesel);
    }

    /* other ports -- unmapped */
    return 0xFF;
}

