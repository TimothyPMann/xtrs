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
   Last modified on Tue Dec 17 13:06:21 PST 1996 by mann
*/

#include "z80.h"
#include "trs.h"
#include "trs_imp_exp.h"

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
    else if(port == IMPEXP_CMD)
    {
        trs_impexp_cmd_write(value);
    }
    else if(port == IMPEXP_DATA)
    {
        trs_impexp_data_write(value);
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
    else if(port == IMPEXP_STATUS)
    {
        return trs_impexp_status_read();
    }
    else if(port == IMPEXP_DATA)
    {
        return trs_impexp_data_read();
    }

    /* other ports -- unmapped */
    return 0xFF;
}

