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
   Last modified on Sat Feb 17 17:32:18 PST 2001 by mann
*/

/*#define PORTDEBUG1 1*/
/*#define PORTDEBUG2 1*/

#include <time.h>

#include "z80.h"
#include "trs.h"
#include "trs_imp_exp.h"
#include "trs_disk.h"
#include "trs_hard.h"
#include "trs_uart.h"

static int modesel = 0;     /* Model I */
static int modeimage = 0x8; /* Model III/4/4p */
static int ctrlimage = 0;   /* Model 4/4p */
static int rominimage = 0;  /* Model 4p */

/*ARGSUSED*/
void z80_out(int port, int value)
{
#if PORTDEBUG1
  debug("out (0x%02x), 0x%02x; pc 0x%04x\n", port, value, z80_state.pc.word);
#endif
  /* First, ports common to all models */
  switch (port) {
  case TRS_HARD_WP:       /* 0xC0 */
  case TRS_HARD_CONTROL:  /* 0xC1 */
  case TRS_HARD_DATA:     /* 0xC8 */ 
  case TRS_HARD_ERROR:    /* 0xC9 */ /*=TRS_HARD_PRECOMP*/ 
  case TRS_HARD_SECCNT:   /* 0xCA */
  case TRS_HARD_SECNUM:   /* 0xCB */
  case TRS_HARD_CYLLO:    /* 0xCC */
  case TRS_HARD_CYLHI:    /* 0xCD */
  case TRS_HARD_SDH:      /* 0xCE */
  case TRS_HARD_STATUS:   /* 0xCF */ /*=TRS_HARD_COMMAND*/
    trs_hard_out(port, value);
    break;
  case IMPEXP_CMD:        /* 0xD0 */
    trs_impexp_cmd_write(value);
    break;
  case IMPEXP_DATA:       /* 0xD1 */
    trs_impexp_data_write(value);
    break;
  case TRS_UART_RESET:    /* 0xE8 */
    trs_uart_reset_out(value);
    break;
  case TRS_UART_BAUD:     /* 0xE9 */
    trs_uart_baud_out(value);
    break;
  case TRS_UART_CONTROL:  /* 0xEA */
    trs_uart_control_out(value);
    break;
  case TRS_UART_DATA:     /* 0xEB */
    trs_uart_data_out(value);
    break;
  }

  if (trs_model == 1) {
    /* Next, Model I only */
    switch (port) {
    case 0x00: /* HRG off */
    case 0x01: /* HRG on */
      hrg_onoff(port);
      break;
    case 0x02: /* HRG write address low byte */
      hrg_write_addr(value, 0xff);
      break;
    case 0x03: /* HRG write address high byte */
      hrg_write_addr(value << 8, 0x3f00);
      break;
    case 0x05: /* HRG write data byte */
      hrg_write_data(value);
      break;
    case 0xB9: /* Orchestra-85 left channel */
      trs_orch90_out(1, value);
      break;
    case 0xB5: /* Orchestra-85 right channel */
      trs_orch90_out(2, value);
      break;
    case 0xFD:
      /* GENIE location of printer port */
      trs_printer_write(value);
      break;
    case 0xFE:
      /* Typical location for clock speedup kits */
      trs_timer_speed(value);
      break;
    case 0xFF:
      /* screen mode select is on D3 line */
      modesel = (value >> 3) & 1;
      trs_screen_expanded(modesel);
      /* do cassette emulation */
      trs_cassette_motor((value >> 2) & 1);
      trs_cassette_out(value & 0x3);
      break;
    default:
      break;
    }

  } else {
    /* Next, Models III/4/4P only */
    switch (port) {
    case 0x79: /* Orchestra-90 left channel */
      trs_orch90_out(1, value);
      break;
    case 0x75: /* Orchestra-90 right channel */
      trs_orch90_out(2, value);
      break;
    case 0x80:
      if (trs_model >= 3) grafyx_write_x(value);
      break;
    case 0x81:
      if (trs_model >= 3) grafyx_write_y(value);
      break;
    case 0x82:
      if (trs_model >= 3) grafyx_write_data(value);
      break;
    case 0x83:
      if (trs_model >= 3) grafyx_write_mode(value);
      break;
    case 0x84:
    case 0x85:
    case 0x86:
    case 0x87:
      if (trs_model >= 4) {
	int changes = value ^ ctrlimage;
	if (changes & 0x80) {
	  mem_video_page((value & 0x80) >> 7);
	}
	if (changes & 0x70) {
	  mem_bank((value & 0x70) >> 4);
	}
	if (changes & 0x08) {
	  trs_screen_inverse((value & 0x08) >> 3);
	}
	if (changes & 0x04) {
	  trs_screen_80x24((value & 0x04) >> 2);
	}
	if (changes & 0x03) {
	  mem_map(value & 0x03);
	}
	ctrlimage = value;
      }
      break;
    case 0x8c:
      if (trs_model >= 4) grafyx_write_xoffset(value);
      break;
    case 0x8d:
      if (trs_model >= 4) grafyx_write_yoffset(value);
      break;
    case 0x8e:
      if (trs_model >= 4) grafyx_write_overlay(value);
      break;
    case 0x90:
    case 0x91:
    case 0x92:
    case 0x93:
      trs_sound_out(value & 1);
      break;
    case 0x9C:
    case 0x9D: /* !!? */
    case 0x9E: /* !!? */
    case 0x9F: /* !!? */
      if (trs_model == 5 /*4p*/) {
	rominimage = value & 1;
	mem_romin(rominimage);
      }
      break;
    case 0xE0:
      trs_interrupt_mask_write(value);
      break;
    case TRSDISK3_INTERRUPT: /* 0xE4 */
    case 0xE5:
    case 0xE6:
    case 0xE7:
      trs_nmi_mask_write(value);
      break;
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF:
      modeimage = value;
      /* cassette motor is on D1 */
      trs_cassette_motor((modeimage & 0x02) >> 1);
      /* screen mode select is on D2 */
      trs_screen_expanded((modeimage & 0x04) >> 2);
      /* alternate char set is on D3 */
      trs_screen_alternate(!((modeimage & 0x08) >> 3));
      /* clock speed is on D6; it affects timer HZ too */
      trs_timer_speed((modeimage & 0x40) >> 6);
      break;
    case TRSDISK3_COMMAND: /* 0xF0 */
      trs_disk_command_write(value);
      break;
    case TRSDISK3_TRACK: /* 0xF1 */
      trs_disk_track_write(value);
      break;
    case TRSDISK3_SECTOR: /* 0xF2 */
      trs_disk_sector_write(value);
      break;
    case TRSDISK3_DATA: /* 0xF3 */
      trs_disk_data_write(value);
      break;
    case TRSDISK3_SELECT: /* 0xF4 */
    case 0xF5:
    case 0xF6:
    case 0xF7:
      trs_disk_select_write(value);
      break;
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
      trs_printer_write(value);
      break;
    case 0xFC:
    case 0xFD:
    case 0xFE:
    case 0xFF:
      if (trs_model == 3 && (value & 0x20) && grafyx_get_microlabs()) {
	/* do Model III Micro Labs graphics card */
	grafyx_m3_write_mode(value);
      } else {
	/* do cassette emulation */
	trs_cassette_out(value & 3);
      }
      break;
    default:
      break;
    }
  }
  return;
}

/*ARGSUSED*/
int z80_in(int port)
{
  /* First, ports common to all models */
#if PORTDEBUG2
  debug("in (0x%02x); pc %04x\n", port, z80_state.pc.word);
#endif

  /* Support for a special HW real-time clock (TimeDate80?)
   * I used to have.  It was a small card-edge unit with a
   * battery that held the time/date with power off.
   * - Joe Peterson (joe@skyrush.com)
   *
   * According to the LDOS Quarterly 1-6, TChron1, TRSWatch, and
   * TimeDate80 are accessible at high ports 0xB0-0xBC, while
   * T-Timer is accessible at high ports 0xC0-0xCC.  It does
   * not say where the low ports were; Joe's code had 0x70-0x7C,
   * so I presume that's correct at least for the TimeDate80.
   * Note: 0xC0-0xCC conflicts with Radio Shack hard disk, so
   * clock access at these ports is disabled starting in xtrs 4.1.
   *
   * These devices were based on the MSM5832 chip, which returns only
   * a 2-digit year.  It's not clear what software will do with the
   * date in years beyond 1999.
   */

  if ((port >= 0x70 && port <= 0x7C)
      || (port >= 0xB0 && port <= 0xBC)
      /*|| (port >= 0xC0 && port <= 0xCC)*/) {
    struct tm *time_info;
    time_t time_secs;

    time_secs = time(NULL);
    time_info = localtime(&time_secs);

    switch (port & 0x0F) {
    case 0xC: /* year (high) */
      return (time_info->tm_year / 10) % 10;
    case 0xB: /* year (low) */
      return (time_info->tm_year % 10);
    case 0xA: /* month (high) */
      return ((time_info->tm_mon + 1) / 10);
    case 0x9: /* month (low) */
      return ((time_info->tm_mon + 1) % 10);
    case 0x8: /* date (high) and leap year (bit 2) */
      return ((time_info->tm_mday / 10) | ((time_info->tm_year % 4) ? 0 : 4));
    case 0x7: /* date (low) */
      return (time_info->tm_mday % 10);
    case 0x6: /* day-of-week */
      return time_info->tm_wday;
    case 0x5: /* hours (high) and PM (bit 2) and 24hr (bit 3) */
      return ((time_info->tm_hour / 10) | 8);
    case 0x4: /* hours (low) */
      return (time_info->tm_hour % 10);
    case 0x3: /* minutes (high) */
      return (time_info->tm_min / 10);
    case 0x2: /* minutes (low) */
      return (time_info->tm_min % 10);
    case 0x1: /* seconds (high) */
      return (time_info->tm_sec / 10);
    case 0x0: /* seconds (low) */
      return (time_info->tm_sec % 10);
    }
  }

  switch (port) {
  case 0x00:
    return trs_joystick_in();
  case TRS_HARD_WP:       /* 0xC0 */
  case TRS_HARD_CONTROL:  /* 0xC1 */
  case TRS_HARD_DATA:     /* 0xC8 */ 
  case TRS_HARD_ERROR:    /* 0xC9 */ /*=TRS_HARD_PRECOMP*/ 
  case TRS_HARD_SECCNT:   /* 0xCA */
  case TRS_HARD_SECNUM:   /* 0xCB */
  case TRS_HARD_CYLLO:    /* 0xCC */
  case TRS_HARD_CYLHI:    /* 0xCD */
  case TRS_HARD_SDH:      /* 0xCE */
  case TRS_HARD_STATUS:   /* 0xCF */ /*=TRS_HARD_COMMAND*/
    return trs_hard_in(port);
  case IMPEXP_STATUS:     /* 0xD0 */
    return trs_impexp_status_read();
  case IMPEXP_DATA:       /* 0xD1 */
    return trs_impexp_data_read();
  case TRS_UART_MODEM:    /* 0xE8 */
    return trs_uart_modem_in();
  case TRS_UART_SWITCHES: /* 0xE9 */
    return trs_uart_switches_in();
  case TRS_UART_STATUS:   /* 0xEA */
    return trs_uart_status_in();
  case TRS_UART_DATA:     /* 0xEB */
    return trs_uart_data_in();
  default:
    break;
  }

  if (trs_model == 1) {
    /* Model I only */
    switch (port) {
    case 0x00: /* HRG off (undocumented) */
    case 0x01: /* HRG on (undocumented) */
      hrg_onoff(port);
      break;
    case 0x04: /* HRG read data byte */
      return hrg_read_data();
    case 0xFD:
      /* GENIE location of printer port */
      return trs_printer_read();
    case 0xFF:
      return (modesel ? 0x7f : 0x3f) | trs_cassette_in();
    default:
      break;
    }

  } else {
    /* Models III/4/4P only */
    switch (port) {
    case 0x82:
      if (trs_model >= 3) {
	return grafyx_read_data();
      }
      break;
    case 0x9C: /* !!? */
    case 0x9D: /* !!? */
    case 0x9E: /* !!? */
    case 0x9F: /* !!? */
      if (trs_model == 5 /*4p*/) {
	return rominimage;
      }
      break;
    case TRS_HARD_WP:      /* 0xC0 */
    case TRS_HARD_CONTROL: /* 0xC1 */
    case TRS_HARD_DATA:    /* 0xC8 */ 
    case TRS_HARD_ERROR:   /* 0xC9 */ /*=TRS_HARD_PRECOMP*/ 
    case TRS_HARD_SECCNT:  /* 0xCA */
    case TRS_HARD_SECNUM:  /* 0xCB */
    case TRS_HARD_CYLLO:   /* 0xCC */
    case TRS_HARD_CYLHI:   /* 0xCD */
    case TRS_HARD_SDH:     /* 0xCE */
    case TRS_HARD_STATUS:  /* 0xCF */ /*=TRS_HARD_COMMAND*/
      return trs_hard_in(port);
      break;
    case IMPEXP_STATUS: /* 0xD0 */
      return trs_impexp_status_read();
    case IMPEXP_DATA: /* 0xD1 */
      return trs_impexp_data_read();
    case 0xE0:
      return trs_interrupt_latch_read();
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF:
      trs_timer_interrupt(0); /* acknowledge */
      return 0xFF;
    case TRSDISK3_INTERRUPT: /* 0xE4 */
      return trs_nmi_latch_read();
    case TRSDISK3_STATUS: /* 0xF0 */
      return trs_disk_status_read();
    case TRSDISK3_TRACK: /* 0xF1 */
      return trs_disk_track_read();
    case TRSDISK3_SECTOR: /* 0xF2 */
      return trs_disk_sector_read();
    case TRSDISK3_DATA: /* 0xF3 */
      return trs_disk_data_read();
    case 0xF8:
      return trs_printer_read();
    case 0xFF:
      return (modeimage & 0x7e) | trs_cassette_in();
    default:
      break;
    }
  }

  /* other ports -- unmapped */
  return 0xFF;
}
