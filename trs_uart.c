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
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include "trs.h"
#include "trs_uart.h"

#ifndef FNONBLOCK
#define FNONBLOCK O_NONBLOCK
#endif

#define BUFSIZE 256
/*#define UARTDEBUG 1*/
/*#define UARTDEBUG2 1*/

#if __linux
char *trs_uart_name = "/dev/ttyS0";
#else
char *trs_uart_name = "/dev/tty00";
#endif
int trs_uart_switches =
  0x7 | TRS_UART_NOPAR | TRS_UART_WORD8; /* Default: 9600 8N1 */

static int initialized = 0;

static struct {
  int modem;
  int switches;
  int baud;
  int status;
  int control;
  int idata;
  int odata;

  Uchar buf[BUFSIZE];
  Uchar* bufp;
  int bufleft;
  int tstates;

  int fd;
  int fdflags;
  struct termios t;
} uart;

static int trs_uart_wordbits[] = TRS_UART_WORDBITS_TABLE;
static float trs_uart_baud[] = TRS_UART_BAUD_TABLE;

static int
xlate_baud(int trs_baud)
{
  switch (trs_baud) {
  case TRS_UART_50:
    return B50;
  case TRS_UART_75:
    return B75;
  case TRS_UART_110:
    return B110;
  case TRS_UART_134:
    return B134;
  case TRS_UART_150:
    return B150;
  case TRS_UART_300:
    return B300;
  case TRS_UART_600:
    return B600;
  case TRS_UART_1200:
    return B1200;
  case TRS_UART_1800:
    return B1800;
  case TRS_UART_2000:
    error("unix does not support 2000 baud, using 38400");
    return B38400;
  case TRS_UART_2400:
    return B2400;
  case TRS_UART_3600:
#ifdef B57600
    error("unix does not support 3600 baud, using 57600");
    return B57600;
#else
    error("unix does not support 3600 baud");
    return B0;
#endif
  case TRS_UART_4800:
    return B4800;
  case TRS_UART_7200:
#ifdef B115200
    error("unix does not support 7200 baud, using 115200");
    return B115200;
#else
    error("unix does not support 7200 baud");
    return B0;
#endif
  case TRS_UART_9600:
    return B9600;
  case TRS_UART_19200:
    return B19200;
  }
  return B0;  /* not reached */
}

void
trs_uart_init(int reset_button)
{
  int err;
#if UARTDEBUG
  debug("trs_uart_init\n");
#endif
  if (initialized == 1 && uart.fd != -1) close(uart.fd);
  if (trs_uart_name == NULL || trs_uart_name[0] == '\000') {
    /* Emulate having no serial port */
    initialized = -1;
    return;
  }
  initialized = 1;
  uart.fd = open(trs_uart_name, O_RDWR|O_NOCTTY|O_NONBLOCK);
  if (uart.fd == -1) {
    error("can't open %s: %s", trs_uart_name, strerror(errno));
  } else {
    uart.fdflags = FNONBLOCK;
#if HAVE_SIGIO
    if (trs_model > 1) {
      uart.fdflags |= FASYNC;
      fcntl(uart.fd, F_SETOWN, getpid()); /* is this needed? */
      fcntl(uart.fd, F_SETFL, uart.fdflags);
    }
#endif
    err = tcgetattr(uart.fd, &uart.t);
    if (err < 0) {
      error("can't get attributes of %s: %s", trs_uart_name, strerror(errno));
    }
  }

  uart.t.c_iflag = 0;
  uart.t.c_oflag = 0;
  uart.t.c_lflag = 0;
  memset(uart.t.c_cc, 0, sizeof(uart.t.c_cc));

  /* Not readable from a user process on unix */
  uart.modem = TRS_UART_CTS | TRS_UART_DSR | TRS_UART_CD;

  uart.switches = (trs_model == 1) ? trs_uart_switches : 0xff;

  /* arbitrary default */
  uart.baud = -1;
  trs_uart_baud_out((TRS_UART_9600 << 4) + TRS_UART_9600);

  /* arbitrary default */
  uart.control = -1;
  trs_uart_control_out(TRS_UART_NOPAR | TRS_UART_WORD8 | TRS_UART_NOTBREAK |
		       TRS_UART_DTR | TRS_UART_RTS);

  uart.status = TRS_UART_SENT;
  trs_uart_snd_interrupt(1);

  uart.bufp = uart.buf;
  uart.bufleft = 0;
}

int
trs_uart_modem_in()
{
  /* should poll hardware here, if we could */
  if (initialized == 0) trs_uart_init(0);
  if (initialized == -1) return 0xff;
#if UARTDEBUG2
  debug("trs_uart_modem_in returns 0x%02x\n", uart.modem);
#endif
  return uart.modem;
}

void
trs_uart_reset_out(int value)
{
#if UARTDEBUG
  debug("trs_uart_reset_out\n");
#endif
  if (initialized == 0) trs_uart_init(0);
  if (initialized == -1) {
    error("serial port emulation is not enabled");
    return;
  }
}

int
trs_uart_switches_in()
{
  if (initialized == 0) trs_uart_init(0);
  if (initialized == -1) return 0xff;
#if UARTDEBUG
  debug("trs_uart_switches_in returns 0x%02x\n", uart.switches);
#endif
  return uart.switches;
}

void
trs_uart_baud_out(int value)
{
  int err;
  int bits;
#if UARTDEBUG
  debug("trs_uart_baud_out 0x%02x\n", value);
#endif

  if (initialized == 1 && uart.baud == value) return;
  if (initialized == 0) trs_uart_init(0);
  if (initialized == -1) return;
  uart.baud = value;

  cfsetispeed(&uart.t, xlate_baud(TRS_UART_RCVBAUD(value)));
  cfsetospeed(&uart.t, xlate_baud(TRS_UART_SNDBAUD(value)));

  bits = 1 + trs_uart_wordbits[TRS_UART_WORDBITS(uart.control)] +
    ((uart.control & TRS_UART_NOPAR) ? 0 : 1) +
    ((uart.control & TRS_UART_STOP2) ? 2 : 1);
  uart.tstates = (z80_state.clockMHz * 1000000.0 * bits)
    / trs_uart_baud[TRS_UART_SNDBAUD(value)];
#if UARTDEBUG
  debug("total bits %d; tstates per word %d\n", bits, uart.tstates);
#endif

  if (uart.fd != -1) {
    err = tcsetattr(uart.fd, TCSADRAIN, &uart.t);
    if (err == -1) {
      error("can't set attributes of %s: %s", trs_uart_name, strerror(errno));
    }
  }
}

void
trs_uart_set_avail(int dummy)
{
  uart.status |= TRS_UART_RCVD;
  trs_uart_rcv_interrupt(1);
}

void
trs_uart_set_empty(int dummy)
{
  uart.status |= TRS_UART_SENT;
  trs_uart_snd_interrupt(1);
}

int
trs_uart_check_avail()
{
  if (initialized == 1 && uart.bufleft == 0 && uart.fd != -1) {
    /* check for data available */
    int rc;
    if (!(uart.fdflags & FNONBLOCK)) {
#if UARTDEBUG
      debug("trs_uart nonblocking\n");
#endif
      uart.fdflags |= FNONBLOCK;
      fcntl(uart.fd, F_SETFL, uart.fdflags);
    }
    do {
      rc = read(uart.fd, uart.buf, BUFSIZE);
    } while (rc < 0 && errno == EINTR);
#if UARTDEBUG
#if !UARTDEBUG2
    if (rc >= 0 || errno != EAGAIN)
#endif
      debug("trs_uart read returns %d, errno %d\n", rc, errno);
#endif
    if (rc < 0) {
      if (errno != EAGAIN) {
	error("can't read from %s: %s", trs_uart_name, strerror(errno));
      }
      rc = 0;
    }
    uart.bufp = uart.buf;
    uart.bufleft = rc;
    if (rc > 0) {
      /* be sure events don't happen too fast */
      trs_schedule_event(trs_uart_set_avail, 1, uart.tstates);
    }
  }
#if UARTDEBUG2
  debug("trs_uart_check_avail returns %d\n", uart.bufleft);
#endif
  return uart.bufleft;
}

int
trs_uart_status_in()
{
#if UARTDEBUG
  static int oldstatus = -1;
#endif
  if (initialized == 0) trs_uart_init(0);
  if (initialized == -1) return 0xff;
  trs_uart_check_avail();
#if UARTDEBUG
  if (uart.status != oldstatus) {
    debug("trs_uart_status_in returns 0x%02x\n", uart.status);
    oldstatus = uart.status;
  }
#endif
  return uart.status;
}

void
trs_uart_control_out(int value)
{
  int err;
  int cflag = HUPCL|CREAD|CLOCAL;
#if UARTDEBUG
  debug("trs_uart_control_out 0x%02x\n", value);
#endif
  if (initialized == 1 && uart.control == value) return;
  if (initialized == 0) trs_uart_init(0);
  if (initialized == -1) return;
  uart.control = value;
  if (!(value & TRS_UART_EVENPAR)) cflag |= PARODD;
  switch (value & TRS_UART_WORDMASK) {
  case TRS_UART_WORD5:
    cflag |= CS5;
    break;
  case TRS_UART_WORD6:
    cflag |= CS6;
    break;
  case TRS_UART_WORD7:
    cflag |= CS7;
    break;
  case TRS_UART_WORD8:
    cflag |= CS8;
    break;
  }
  if (value & TRS_UART_STOP2) cflag |= CSTOPB;
  if (!(value & TRS_UART_NOPAR)) cflag |= PARENB;
  uart.t.c_cflag = cflag;
  if (uart.fd != -1) {
    err = tcsetattr(uart.fd, TCSADRAIN, &uart.t);
    if (err == -1) {
      error("can't set attributes of %s: %s", trs_uart_name, strerror(errno));
    }
  }

  if (!(value & TRS_UART_NOTBREAK) && uart.fd != -1) {
    sigset_t set, oldset;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGIO);
    sigprocmask(SIG_BLOCK, &set, &oldset);
    err = tcsendbreak(uart.fd, 0);
    sigprocmask(SIG_SETMASK, &oldset, NULL);
    if (err == -1) {
      error("can't send break on %s: %s", trs_uart_name, strerror(errno));
    }
  }
}

int
trs_uart_data_in()
{
  if (initialized == 0) trs_uart_init(0);
  if (initialized == -1) return 0xff;
  trs_uart_check_avail();
  if (uart.status & TRS_UART_RCVD) {
    uart.status &= ~TRS_UART_RCVD;
    trs_uart_rcv_interrupt(0);
    uart.bufleft--;
    uart.idata = *uart.bufp++;
    if (uart.bufleft) {
      trs_schedule_event(trs_uart_set_avail, 1, uart.tstates);
    }
  }
#if UARTDEBUG
  debug("trs_uart_data_in returns 0x%02x\n", uart.idata);
#endif
  return uart.idata;
}

void
trs_uart_data_out(int value)
{
  int err;

#if UARTDEBUG
  debug("trs_uart_data_out 0x%02x\n", value);
#endif
  if (initialized == 0) trs_uart_init(0);
  if (initialized == -1) return;
  uart.odata = value;
  if (uart.fd != -1) {
    for (;;) {
      err = write(uart.fd, &uart.odata, 1);
      if (err >= 0) return;
      if (errno != EAGAIN) {
	error("can't read from %s: %s", trs_uart_name, strerror(errno));
	return;
      }
      /* Oops, here we didn't really want nonblocking i/o */
#if UARTDEBUG
      debug("trs_uart blocking\n");
#endif
      uart.fdflags &= ~FNONBLOCK;
      fcntl(uart.fd, F_SETFL, uart.fdflags);
    }
    trs_uart_snd_interrupt(0);
    trs_schedule_event(trs_uart_set_empty, 1, uart.tstates);
  }    
}
