/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Tue Sep 30 17:45:39 PDT 1997 by mann */

/*
 * Emulate Model-I interrupts
 */

#include "z80.h"
#include "trs.h"
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>

/* IRQs */
#define M1_TIMER_BIT 0x80
#define M1_DISK_BIT  0x40
#define M3_TIMER_BIT 0x04
static unsigned char interrupt_latch = 0;
static unsigned char interrupt_mask = 0;

/* NMIs (M3 only) */
#define M3_INTRQ_BIT    0x80  /* FDC chip INTRQ line */
#define M3_MOTOROFF_BIT 0x40  /* FDC motor timed out (stopped) */
#define M3_RESET_BIT    0x20  /* User pressed Reset button */
static unsigned char nmi_latch = 0;
static unsigned char nmi_mask = M3_RESET_BIT;

#define TIMER_HZ_1 40
#define TIMER_HZ_3 30
#define TIMER_HZ_4 60
static int timer_hz;

/* Kludge: LDOS hides the date (not time) here over a reboot */
#define LDOS_MONTH 0x4306
#define LDOS_DAY   0x4307
#define LDOS_YEAR  0x4466
#define LDOS3_MONTH 0x442f
#define LDOS3_DAY   0x4457
#define LDOS3_YEAR  0x4413

static int timer_on = 1;
#ifdef IDEBUG
long lost_timer_interrupts = 0;
#endif

void
trs_timer_interrupt(int state)
{
  if (trs_model == 1) {
    if (state) {
#ifdef IDEBUG
      if (interrupt_latch & M1_TIMER_BIT) lost_timer_interrupts++;
#endif
      interrupt_latch |= M1_TIMER_BIT;
    } else {
      interrupt_latch &= ~M1_TIMER_BIT;
    }
    z80_state.irq = (interrupt_latch != 0);
  } else {
    if (state) {
#ifdef IDEBUG
      if (interrupt_latch & M3_TIMER_BIT) lost_timer_interrupts++;
#endif
      interrupt_latch |= M3_TIMER_BIT;
    } else {
      interrupt_latch &= ~M3_TIMER_BIT;
    }
    z80_state.irq = (interrupt_latch & interrupt_mask) != 0;
  }
}

void
trs_disk_intrq_interrupt(int state)
{
  if (trs_model == 1) {
    if (state) {
      interrupt_latch |= M1_DISK_BIT;
    } else {
      interrupt_latch &= ~M1_DISK_BIT;
    }
    z80_state.irq = (interrupt_latch != 0);
  } else {
    if (state) {
      nmi_latch |= M3_INTRQ_BIT;
    } else {
      nmi_latch &= ~M3_INTRQ_BIT;
    }
    z80_state.nmi = (nmi_latch & nmi_mask) != 0;
    if (!z80_state.nmi) z80_state.nmi_seen = 0;
  }
}

void
trs_disk_motoroff_interrupt(int state)
{
  /* Drive motor timed out (stopped).
     Not emulated; this routine is never called.
   */
  if (trs_model == 1) {
    /* no effect */
  } else {
    if (state) {
      nmi_latch |= M3_MOTOROFF_BIT;
    } else {
      nmi_latch &= ~M3_MOTOROFF_BIT;
    }
    z80_state.nmi = (nmi_latch & nmi_mask) != 0;
    if (!z80_state.nmi) z80_state.nmi_seen = 0;
  }
}

void
trs_disk_drq_interrupt(int state)
{
  /* no effect */
}

void
trs_reset_button_interrupt(int state)
{
  if (trs_model == 1) {
    z80_state.nmi = state;
  } else {  
    if (state) {
      nmi_latch |= M3_RESET_BIT;
    } else {
      nmi_latch &= ~M3_RESET_BIT;
    }
    z80_state.nmi = (nmi_latch & nmi_mask) != 0;
  }
  if (!z80_state.nmi) z80_state.nmi_seen = 0;
}

unsigned char
trs_interrupt_latch_read()
{
  unsigned char tmp = interrupt_latch;
  if (trs_model == 1) {
    trs_timer_interrupt(0); /* acknowledge this one (only) */
    return tmp;
  } else {
    return ~tmp;
  }
}

void
trs_interrupt_mask_write(value)
     unsigned char value;
{
  interrupt_mask = value;
  z80_state.irq = (interrupt_latch & interrupt_mask) != 0;
}

/* M3 only */
unsigned char
trs_nmi_latch_read()
{
  unsigned char tmp = ~nmi_latch;

  /* !!Kludge: On a real machine, the reset button interrupt signal
     goes away when the user releases the button.  Here, we leave
     it active until software has read the NMI latch. */
  trs_reset_button_interrupt(0);

  return tmp;
}

void
trs_nmi_mask_write(value)
     unsigned char value;
{
  nmi_mask = value | M3_RESET_BIT;
  z80_state.nmi = (nmi_latch & nmi_mask) != 0;
  if (!z80_state.nmi) z80_state.nmi_seen = 0;
}

void
trs_timer_event(signo)
{
  struct timeval tv;
  struct itimerval it;

  if (!timer_on) return;

  trs_timer_interrupt(1); /* generate */
  trs_kb_heartbeat(); /* part of keyboard stretch kludge */
#if HAVE_SIGIO
  x_poll_count = 0; /* be sure to flush X events */
#endif

  /* Schedule next tick.  We do it this way because the host system
     probably didn't wake us up at exactly the right time.  For
     instance, on Linux i386 the real clock ticks at 10ms, but we want
     to tick at 25ms.  If we ask setitimer to wake us up in 25ms, it
     will really wake us up in 30ms.  The algorithm below compensates
     for such an error by making the next tick shorter. */
  gettimeofday(&tv, NULL);
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec =
    (1000000/timer_hz) - (tv.tv_usec % (1000000/timer_hz));
  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = 1000000/timer_hz;  /* fail-safe */
  setitimer(ITIMER_REAL, &it, NULL);
}

void
trs_timer_init()
{
  struct sigaction sa;
  struct tm *lt;
  time_t tt;

  if (trs_model == 1) {
      timer_hz = TIMER_HZ_1;
  } else {
      timer_hz = TIMER_HZ_3;  /* initially */
  }

  sa.sa_handler = trs_timer_event;
  sa.sa_mask = sigmask(SIGALRM);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGALRM, &sa, NULL);

  trs_timer_event(SIGALRM);

  /* Also initialize the clock in memory - hack */
  tt = time(NULL);
  lt = localtime(&tt);
  if (trs_model == 1) {
      mem_write(LDOS_MONTH, (lt->tm_mon + 1) ^ 0x50);
      mem_write(LDOS_DAY, lt->tm_mday);
      mem_write(LDOS_YEAR, lt->tm_year - 80);
  } else {
      mem_write(LDOS3_MONTH, (lt->tm_mon + 1) ^ 0x50);
      mem_write(LDOS3_DAY, lt->tm_mday);
      mem_write(LDOS3_YEAR, lt->tm_year - 80);
  }
}

void
trs_timer_off()
{
  timer_on = 0;
}

void
trs_timer_on()
{
  if (!timer_on) {
    timer_on = 1;
    trs_timer_event(SIGALRM);
  }
}

void
trs_timer_speed(fast)
     int fast;
{
    if (trs_model == 4) {
	timer_hz = fast ? TIMER_HZ_4 : TIMER_HZ_3;
    }
}
