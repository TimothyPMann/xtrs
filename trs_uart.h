/* Copyright (c) 2000, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Thu May 18 00:42:46 PDT 2000 by mann */

/*
 * Emulation of the Radio Shack TRS-80 Model I/III/4/4P serial port.
 */

#include <errno.h>
#include "trs.h"
#include "trs_hard.h"

extern void trs_uart_init(int reset_button);
extern int trs_uart_check_avail();
extern int trs_uart_modem_in();
extern void trs_uart_reset_out(int value);
extern int trs_uart_switches_in();
extern void trs_uart_baud_out(int value);
extern int trs_uart_status_in();
extern void trs_uart_control_out(int value);
extern int trs_uart_data_in();
extern void trs_uart_data_out(int value);
extern char *trs_uart_name;
extern int trs_uart_switches;

#define TRS_UART_MODEM    0xE8 /* in */
#define TRS_UART_RESET    0xE8 /* out */
#define TRS_UART_SWITCHES 0xE9 /* in, model I only */
#define TRS_UART_BAUD     0xE9 /* out */
#define TRS_UART_STATUS   0xEA /* in */
#define TRS_UART_CONTROL  0xEA /* out */
#define TRS_UART_DATA     0xEB /* in/out */

/* Bits in TRS_UART_MODEM port */
#define TRS_UART_CTS      0x80
#define TRS_UART_DSR      0x40
#define TRS_UART_CD       0x20
#define TRS_UART_RI       0x10
#define TRS_UART_RCVIN    0x02

/* Bits in TRS_UART_BAUD port */
#define TRS_UART_SNDBAUD(v) (((v)&0xf0)>>4)
#define TRS_UART_RCVBAUD(v) (((v)&0x0f)>>0)
#define TRS_UART_50       0x00
#define TRS_UART_75       0x01
#define TRS_UART_110      0x02
#define TRS_UART_134      0x03
#define TRS_UART_150      0x04
#define TRS_UART_300      0x05
#define TRS_UART_600      0x06
#define TRS_UART_1200     0x07
#define TRS_UART_1800     0x08
#define TRS_UART_2000     0x09
#define TRS_UART_2400     0x0a
#define TRS_UART_3600     0x0b
#define TRS_UART_4800     0x0c
#define TRS_UART_7200     0x0d
#define TRS_UART_9600     0x0e
#define TRS_UART_19200    0x0f
#define TRS_UART_BAUD_TABLE \
  { 50.0, 75.0, 110.0, 134.5, 150.0, 300.0, 600.0, 1200.0, 1800.0, \
    2000.0, 2400.0, 3600.0, 4800.0, 7200.0, 9600.0, 19200.0 }

/* Bits in TRS_UART_STATUS port */
#define TRS_UART_RCVD     0x80
#define TRS_UART_SENT     0x40
#define TRS_UART_OVRERR   0x20
#define TRS_UART_FRMERR   0x10
#define TRS_UART_PARERR   0x08

/* Bits in TRS_UART_CONTROL port */
#define TRS_UART_EVENPAR  0x80
#define TRS_UART_WORDMASK 0x60
#define TRS_UART_WORD5    0x00
#define TRS_UART_WORD6    0x40
#define TRS_UART_WORD7    0x20
#define TRS_UART_WORD8    0x60
#define TRS_UART_WORDBITS(v) (((v)&TRS_UART_WORDMASK)>>5)
#define TRS_UART_WORDBITS_TABLE { 5, 7, 6, 8 }
#define TRS_UART_STOP2    0x10
#define TRS_UART_NOPAR    0x08
#define TRS_UART_NOTBREAK 0x04
#define TRS_UART_DTR      0x02 /* mislabelled as RTS in Model I manual */
#define TRS_UART_RTS      0x01 /* mislabelled as DTR in Model I manual */
