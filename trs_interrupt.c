/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Tue Dec 17 13:06:18 PST 1996 by mann */

/*
 * Emulate Model-I interrupts
 */

#include "z80.h"
#include "trs.h"
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>

static unsigned char interrupt_latch = 0;
#define TIMER_BIT 0x80  /* within interrupt latch */
#define DISK_BIT  0x40  /* not emulated, but could be */

#define TIMER_HZ  40    /* must be an integer */

/* Kludge: LDOS hides the date (not time) here over a reboot */
#define LDOS_MONTH 0x4306
#define LDOS_DAY   0x4307
#define LDOS_YEAR  0x4466

static int timer_on = 1;

void
trs_timer_event(signo)
{
  struct timeval tv;
  struct itimerval it;

  if (!timer_on) return;

  interrupt_latch |= TIMER_BIT;
  z80_state.irq = 1;

  /* Schedule next tick.  We do it this way because the host system
     probably didn't wake us up at exactly the right time.  For
     instance, on Linux i386 the real clock ticks at 10ms, but we want
     to tick at 25ms.  If we ask setitimer to wake us up in 25ms, it
     will really wake us up in 30ms.  The algorithm below compensates
     for such an error by making the next tick shorter. */
  gettimeofday(&tv, NULL);
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec =
    (1000000/TIMER_HZ) - (tv.tv_usec % (1000000/TIMER_HZ));
  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = 1000000/TIMER_HZ;  /* fail-safe */
  setitimer(ITIMER_REAL, &it, NULL);
}

void
trs_timer_init()
{
  struct sigaction sa;
  struct itimerval it;
  struct tm *lt;
  time_t tt;

  sa.sa_handler = trs_timer_event;
  sa.sa_mask = sigmask(SIGALRM);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGALRM, &sa, NULL);

  trs_timer_event(SIGALRM);

  /* Also initialize the clock in memory - hack */
  tt = time(NULL);
  lt = localtime(&tt);
  mem_write(LDOS_MONTH, (lt->tm_mon + 1) ^ 0x50);
  mem_write(LDOS_DAY, lt->tm_mday);
  mem_write(LDOS_YEAR, lt->tm_year - 80);
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
  } else {
    timer_on = 1;
  }
}

unsigned char
trs_interrupt_latch_read()
{
  unsigned char tmp = interrupt_latch;
  interrupt_latch &= ~TIMER_BIT;  /* only this one is cleared by
                                     reading latch */
  z80_state.irq = (interrupt_latch != 0);
  return tmp;
}
