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
#include "trs_disk.h"

static int modesel = 0;

/*ARGSUSED*/
void z80_out(port, value)
    int port, value;
{
    int new_modesel;

    switch (port) {
    case 0xFF:
	/* screen mode select is on D3 line */
	new_modesel = (value >> 3) & 1;
	if(modesel != new_modesel)
	{
	    modesel = new_modesel;
	    trs_screen_mode_select(modesel ? EXPANDED : NORMAL);
	}

	/* do cassette emulation */
	trs_cassette_out(value & 0x7);
        break;
    case IMPEXP_CMD:
      trs_impexp_cmd_write(value);
      break;
    case IMPEXP_DATA:
      trs_impexp_data_write(value);
      break;
    case TRSDISK3_INTERRUPT:
      if (trs_model == 1) break;
      trs_nmi_mask_write(value);
      break;
    case TRSDISK3_COMMAND:
      if (trs_model == 1) break;
      trs_disk_command_write(value);
      break;
    case TRSDISK3_TRACK:
      if (trs_model == 1) break;
      trs_disk_track_write(value);
      break;
    case TRSDISK3_SECTOR:
      if (trs_model == 1) break;
      trs_disk_sector_write(value);
      break;
    case TRSDISK3_DATA:
      if (trs_model == 1) break;
      trs_disk_data_write(value);
      break;
    case TRSDISK3_SELECT:
      if (trs_model == 1) break;
      trs_disk_select_write(value);
      break;
    case 0xE0:
      if (trs_model == 1) break;
      trs_interrupt_mask_write(value);  /*!!?*/
      break;
    case 0xF8:
      if (trs_model == 1) break;
      trs_printer_write(value);
      break;
    default:
      break;
    }
    return;
}

/*ARGSUSED*/
int z80_in(port)
    int port;
{
    /* the cassette port -- return whatever the cassette hardware wants */
    switch (port) {
    case 0xFF:
      return trs_cassette_in(modesel);
    case IMPEXP_STATUS:
      return trs_impexp_status_read();
    case IMPEXP_DATA:
      return trs_impexp_data_read();
    case TRSDISK3_INTERRUPT:
      if (trs_model == 1) break;
      return trs_nmi_latch_read();
    case TRSDISK3_STATUS:
      if (trs_model == 1) break;
      return trs_disk_status_read();
    case TRSDISK3_TRACK:
      if (trs_model == 1) break;
      return trs_disk_track_read();
    case TRSDISK3_SECTOR:
      if (trs_model == 1) break;
      return trs_disk_sector_read();
    case TRSDISK3_DATA:
      if (trs_model == 1) break;
      return trs_disk_data_read();
    case 0xE0:
      if (trs_model == 1) break;
      return trs_interrupt_latch_read();  /*!!?*/
    case 0xF8:
      if (trs_model == 1) break;
      return trs_printer_read();
    default:
      break;
    }
    /* other ports -- unmapped */
    return 0xFF;
}

