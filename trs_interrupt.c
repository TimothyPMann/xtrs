/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Thu Sep 24 19:16:50 PDT 1998 by mann */

/*
 * Emulate interrupts
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

/* NMIs (M3/4/4P only) */
#define M3_INTRQ_BIT    0x80  /* FDC chip INTRQ line */
#define M3_MOTOROFF_BIT 0x40  /* FDC motor timed out (stopped) */
#define M3_RESET_BIT    0x20  /* User pressed Reset button */
static unsigned char nmi_latch = 1; /* ?? One diagnostic program needs this */
static unsigned char nmi_mask = M3_RESET_BIT;

#define TIMER_HZ_1 40
#define TIMER_HZ_3 30
#define TIMER_HZ_4 60
static int timer_hz;

#define CLOCK_MHZ_1 1.77408
#define CLOCK_MHZ_3 2.02752
#define CLOCK_MHZ_4 4.05504

/* Kludge: LDOS hides the date (not time) in a memory area across reboots. */
/* We put it there on powerup, so LDOS magically knows the date! */
#define LDOS_MONTH 0x4306
#define LDOS_DAY   0x4307
#define LDOS_YEAR  0x4466
#define LDOS3_MONTH 0x442f
#define LDOS3_DAY   0x4457
#define LDOS3_YEAR  0x4413
#define LDOS4_MONTH 0x0035
#define LDOS4_DAY   0x0034
#define LDOS4_YEAR  0x0033

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
trs_interrupt_mask_write(unsigned char value)
{
  interrupt_mask = value;
  z80_state.irq = (interrupt_latch & interrupt_mask) != 0;
}

/* M3 only */
unsigned char
trs_nmi_latch_read()
{
  return ~nmi_latch;
}

void
trs_nmi_mask_write(unsigned char value)
{
  nmi_mask = value | M3_RESET_BIT;
  z80_state.nmi = (nmi_latch & nmi_mask) != 0;
  if (!z80_state.nmi) z80_state.nmi_seen = 0;
}

#define UP_F   1.50
#define DOWN_F 0.50 

void
trs_timer_event(int signo)
{
  struct timeval tv;
  struct itimerval it;

  if (!timer_on) return;

  gettimeofday(&tv, NULL);
  if (trs_autodelay) {
      static struct timeval oldtv;
      static int increment = 1;
      static int oldtoofast = 0;
#if __GNUC__
      static unsigned long long oldtcount;
#else
      static unsigned long oldtcount;
#endif
      if (!trs_paused) {
	int toofast = (z80_state.t_count - oldtcount) >
	  ((tv.tv_sec*1000000 + tv.tv_usec) -
	   (oldtv.tv_sec*1000000 + oldtv.tv_usec))*z80_state.clockMHz;
	if (toofast == oldtoofast) {
	  increment = (int)(increment * UP_F + 0.5);
	} else {
	  increment = (int)(increment * DOWN_F + 0.5);
	}
	oldtoofast = toofast;
	if (increment < 1) increment = 1;
	if (toofast) {
	  z80_state.delay += increment;
	} else {
	  z80_state.delay -= increment;
	  if (z80_state.delay < 0) {
	    z80_state.delay = 0;
	    increment = 1;
	  }
	}
      }
      trs_paused = 0;
      oldtv = tv;
      oldtcount = z80_state.t_count;
  }

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
      z80_state.clockMHz = CLOCK_MHZ_1;
  } else {
      /* initially... */
      timer_hz = TIMER_HZ_3;  
      z80_state.clockMHz = CLOCK_MHZ_3;
  }

  sa.sa_handler = trs_timer_event;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGALRM);
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
      if (trs_model >= 4) {
        extern Uchar memory[];
	memory[LDOS4_MONTH] = lt->tm_mon + 1;
	memory[LDOS4_DAY] = lt->tm_mday;
	memory[LDOS4_YEAR] = lt->tm_year;
      }
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
trs_timer_speed(int fast)
{
    if (trs_model >= 4) {
	timer_hz = fast ? TIMER_HZ_4 : TIMER_HZ_3;
	z80_state.clockMHz = fast ? CLOCK_MHZ_4 : CLOCK_MHZ_3;
    }
}

static trs_event_func event_func = NULL;
static int event_arg;

/* Schedule an event to occur after "countdown" more instructions have
 *  executed.  0 makes the event happen immediately -- that is, at
 *  the end of the current instruction, but before the emulator checks
 *  for interrupts.  It is legal for an event function to call 
 *  trs_schedule_event.  
 *
 * Only one event can be buffered.  If you try to schedule a second
 *  event while one is still pending, the pending event (along with
 *  any further events that it schedules) is executed immediately.
 */
void
trs_schedule_event(trs_event_func f, int arg, int countdown)
{
    while (event_func) {
#if EDEBUG	
	error("warning: trying to schedule two events");
#endif
	trs_do_event();
    }
    event_func = f;
    event_arg = arg;
    z80_state.sched = countdown;
}

/*
 * If an event is scheduled, do it now.  (If the event function
 * schedules a new event, however, leave that one pending.)
 */
void
trs_do_event()
{
    trs_event_func f = event_func;
    if (f) {
	event_func = NULL;
	z80_state.sched = -1;
	f(event_arg);    
    }
}

/*
 * Cancel scheduled event, if any.
 */
void
trs_cancel_event()
{
    event_func = NULL;
    z80_state.sched = -1;
}

/*
 * Check if anything is scheduled.
 */
int
trs_is_event_scheduled()
{
    return event_func != NULL;
}
