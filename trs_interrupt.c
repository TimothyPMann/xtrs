/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Wed Aug 27 17:35:01 PDT 1997 by mann */

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
static unsigned char interrupt_mask = 0xFF;

/* NMIs (M3 only) */
#define M3_INTRQ_BIT 0x80
#define M3_DRQ_BIT   0x40
#define M3_RESET_BIT 0x20
static unsigned char nmi_latch = 0;
static unsigned char nmi_mask = M3_RESET_BIT;

#define TIMER_HZ  (trs_model == 1 ? 40 : 30)  /* must be an integer */

/* Kludge: LDOS hides the date (not time) here over a reboot */
#define LDOS_MONTH 0x4306
#define LDOS_DAY   0x4307
#define LDOS_YEAR  0x4466
#define LDOS3_MONTH 0x442f
#define LDOS3_DAY   0x4457
#define LDOS3_YEAR  0x4413

static int timer_on = 1;

void
trs_timer_interrupt(int state)
{
  if (trs_model == 1) {
    if (state) {
      interrupt_latch |= M1_TIMER_BIT;
    } else {
      interrupt_latch &= ~M1_TIMER_BIT;
    }
    z80_state.irq = (interrupt_latch != 0);
  } else {
    if (state) {
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
trs_disk_drq_interrupt(int state)
{
  if (trs_model == 1) {
    /* no effect */
  } else {
#if 0
/*!! seems to kill LDOS?? */
    if (state) {
      nmi_latch |= M3_DRQ_BIT;
    } else {
      nmi_latch &= ~M3_DRQ_BIT;
    }
    z80_state.nmi = (nmi_latch & nmi_mask) != 0;
    if (!z80_state.nmi) z80_state.nmi_seen = 0;
#endif
  }
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
  trs_timer_interrupt(0); /* acknowledge this one (only) */
  if (trs_model == 1) {
    return tmp;
  } else {
    return ~tmp;
  }
}

void
trs_interrupt_mask_write(unsigned char value)
{
  interrupt_mask = value;
  z80_state.irq = (interrupt_latch & interrupt_mask) != 0;
}

/* M3 only */
unsigned char
trs_nmi_latch_read()
{
  unsigned char tmp = ~nmi_latch;
  trs_reset_button_interrupt(0); /* acknowledge this one (only) !!? */
  return tmp;
}

void
trs_nmi_mask_write(unsigned char value)
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

