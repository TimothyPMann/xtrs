/* 
 * Copyright (c) 1996-2020, Timothy P. Mann
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Emulate interrupts
 */

#define _XOPEN_SOURCE 500 /* signal.h: SA_RESTART */

#include "z80.h"
#include "trs.h"
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

/*#define IDEBUG 1*/
/*#define IDEBUG2 1*/

/* IRQs */
#define M1_TIMER_BIT    0x80
#define M1_DISK_BIT     0x40
#define M3_UART_ERR_BIT 0x40
#define M3_UART_RCV_BIT 0x20
#define M3_UART_SND_BIT 0x10
#define M3_IOBUS_BIT    0x80 /* not emulated */
#define M3_TIMER_BIT    0x04
#define M3_CASSFALL_BIT 0x02
#define M3_CASSRISE_BIT 0x01
static unsigned char interrupt_latch = 0;
static unsigned char interrupt_mask = 0;

/* NMIs (M3/4/4P only) */
#define M3_INTRQ_BIT    0x80  /* FDC chip INTRQ line */
#define M3_MOTOROFF_BIT 0x40  /* FDC motor timed out (stopped) */
#define M3_RESET_BIT    0x20  /* User pressed Reset button */
#define M3_CONSTANT_BIT 0x01  /* Hardwired to true; see Model III schematic */
static unsigned char nmi_latch = M3_CONSTANT_BIT;
static unsigned char nmi_mask = M3_RESET_BIT;

#define TIMER_HZ_1 40
#define TIMER_HZ_3 30
#define TIMER_HZ_4 60
static int timer_hz;

#define CLOCK_MHZ_1 1.77408
#define CLOCK_MHZ_3 2.02752
#define CLOCK_MHZ_4 4.05504

time_t trs_timeoffset;

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

/* Kludge, continued: On NEWDOS/80, both date and time are stored in memory
   across reboots, but a test is done on boot to decide whether to use the
   stored values.  Here's how it works: NEWDOS/80 writes a special byte value
   to the memory address right before the stored date and time.  On reboot,
   this address is checked, and if it contains that special byte, the stored
   date and time are considered valid and are therefore used.

   By putting this info in memory on powerup, NEWDOS/80 gets initialized
   with the system date and time.
 */
#define NEWDOS_DATETIME_VALID_BYTE  0xa5
// Model 1
#define NEWDOS_DATETIME_VALID_ADDR  0x43ab
#define NEWDOS_MONTH                0x43b1
#define NEWDOS_DAY                  0x43b0
#define NEWDOS_YEAR                 0x43af
#define NEWDOS_HOUR                 0x43ae
#define NEWDOS_MIN                  0x43ad
#define NEWDOS_SEC                  0x43ac
// Model 3
#define NEWDOS3_DATETIME_VALID_ADDR 0x42cb
#define NEWDOS3_MONTH               0x42d1
#define NEWDOS3_DAY                 0x42d0
#define NEWDOS3_YEAR                0x42cf
#define NEWDOS3_HOUR                0x42ce
#define NEWDOS3_MIN                 0x42cd
#define NEWDOS3_SEC                 0x42cc

static int timer_on = 1;
#ifdef IDEBUG
long lost_timer_interrupts = 0;
#endif

/* Note: the independent interrupt latch and mask model is not correct
   for all interrupts.  The cassette rise/fall interrupt enable is
   clocked into the interrupt latch when the event occurs (we get this
   right), and is *not* masked against the latch output (we get this
   wrong, but it doesn't really matter). */

void
trs_cassette_rise_interrupt(int dummy)
{
  interrupt_latch = (interrupt_latch & ~M3_CASSRISE_BIT) |
    (interrupt_mask & M3_CASSRISE_BIT);
  z80_state.irq = (interrupt_latch & interrupt_mask) != 0;
  trs_cassette_update(0);
}

void
trs_cassette_fall_interrupt(int dummy)
{
  interrupt_latch = (interrupt_latch & ~M3_CASSFALL_BIT) |
    (interrupt_mask & M3_CASSFALL_BIT);
  z80_state.irq = (interrupt_latch & interrupt_mask) != 0;
  trs_cassette_update(0);
}

void
trs_cassette_clear_interrupts(void)
{
  interrupt_latch &= ~(M3_CASSRISE_BIT|M3_CASSFALL_BIT);
  z80_state.irq = (interrupt_latch & interrupt_mask) != 0;
}

int
trs_cassette_interrupts_enabled(void)
{
  return interrupt_mask & (M3_CASSRISE_BIT|M3_CASSFALL_BIT);
}

void
trs_timer_interrupt(int state)
{
  if (trs_model == 1) {
    if (state) {
#ifdef IDEBUG
      if (interrupt_latch & M1_TIMER_BIT) lost_timer_interrupts++;
#endif
      interrupt_latch |= M1_TIMER_BIT;
      z80_state.irq = 1;
    } else {
      interrupt_latch &= ~M1_TIMER_BIT;
    }
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
      z80_state.irq = 1;
    } else {
      interrupt_latch &= ~M1_DISK_BIT;
    }
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
  /* Drive motor timed out (stopped). */
  if (trs_model == 1) {
    /* no such interrupt */
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
trs_uart_err_interrupt(int state)
{
  if (trs_model > 1) {
    if (state) {
      interrupt_latch |= M3_UART_ERR_BIT;
    } else {
      interrupt_latch &= ~M3_UART_ERR_BIT;
    }
    z80_state.irq = (interrupt_latch & interrupt_mask) != 0;
  }
}

void
trs_uart_rcv_interrupt(int state)
{
  if (trs_model > 1) {
    if (state) {
      interrupt_latch |= M3_UART_RCV_BIT;
    } else {
      interrupt_latch &= ~M3_UART_RCV_BIT;
    }
    z80_state.irq = (interrupt_latch & interrupt_mask) != 0;
  }
}

void
trs_uart_snd_interrupt(int state)
{
  if (trs_model > 1) {
    if (state) {
      interrupt_latch |= M3_UART_SND_BIT;
    } else {
      interrupt_latch &= ~M3_UART_SND_BIT;
    }
    z80_state.irq = (interrupt_latch & interrupt_mask) != 0;
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
trs_interrupt_latch_read(void)
{
  unsigned char tmp = interrupt_latch;
  if (trs_model == 1) {
    trs_timer_interrupt(0); /* acknowledge this one (only) */
    z80_state.irq = (interrupt_latch != 0);
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
trs_nmi_latch_read(void)
{
  return ~nmi_latch;
}

void
trs_nmi_mask_write(unsigned char value)
{
  /*
   * On real Model III hardware, only bits 7 and 6 of the NMI mask
   * port exist, and only bits 7, 6, 5, and 0 of the NMI source port
   * exist.  Although you can read the state of the reset button in
   * bit 5 of the NMI source port, and bit 0 is hardwired to read as
   * true, those bits aren't latched in flipflops and don't have
   * mutable mask bits.  We emulate all bits in both ports as
   * existing, but prevent software from changing the immutable ones.
   */
  nmi_mask = (value & (M3_INTRQ_BIT|M3_MOTOROFF_BIT)) | M3_RESET_BIT;
  z80_state.nmi = (nmi_latch & nmi_mask) != 0;
#if IDEBUG2
  if (z80_state.nmi && !z80_state.nmi_seen) {
    debug("mask write caused nmi, mask %02x latch %02x\n",
	  nmi_mask, nmi_latch);
  }
#endif
  if (!z80_state.nmi) z80_state.nmi_seen = 0;
}

#if SUSPEND_DELAY
static int saved_delay;
/* Temporarily reduce the delay, until trs_restore_delay is called.
   Useful if we know we're about to do something that's emulated more
   slowly than most instructions, such as video or real-time sound.
   In case the boost is too big or too small, we allow the normal
   autodelay algorithm to continue to run and adjust the new delay.

   XXX This heuristic seems to have been doing more harm than good, at
   least on modern fast systems.  Disabled for now.  Would be nice to
   find a better way to do something like this.  Note: despite the
   above comment, the code is called only from sound start/stop, not
   anything to do with video.
 */
void
trs_suspend_delay(void)
{
  if (!saved_delay) {
    saved_delay = z80_state.delay;
    z80_state.delay /= 2;  /* dividing by 2 is arbitrary */
  }
}

/* Put back the saved delay */
void
trs_restore_delay(void)
{
  if (saved_delay) {
    z80_state.delay = saved_delay;
    saved_delay = 0;
    trs_paused = 1;
  }
}
#else
void trs_suspend_delay(void) { }
void trs_restore_delay(void) { }
#endif

#define UP_F   1.50
#define DOWN_F 0.50 

void
trs_timer_event(int signo)
{
  struct timeval tv;
  struct itimerval it;

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

  if (timer_on) {
    trs_timer_interrupt(1); /* generate */
    trs_disk_motoroff_interrupt(trs_disk_motoroff());
    trs_kb_heartbeat(); /* part of keyboard stretch kludge */
  }
  x_poll_count = 0; /* be sure to flush and check for X events */

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

/*
 * Initialize time offset.  This can useful for TRS-80 operating
 * systems that behave better when the year is within a limited range.
 */
void
trs_inityear(int year)
{
  time_t real, fake;
  struct tm tm;

  if (year == 0) {
    trs_timeoffset = 0;
  } else {
    real = time(NULL);
    localtime_r(&real, &tm);
    tm.tm_year = year - 1900;
    fake = mktime(&tm);
    trs_timeoffset = fake - real;
  }
}

void
trs_timer_init(void)
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
  tt = time(NULL) + trs_timeoffset;
  lt = localtime(&tt);
  if (trs_model == 1) {
      mem_write(LDOS_MONTH, (lt->tm_mon + 1) ^ 0x50);
      mem_write(LDOS_DAY, lt->tm_mday);
      mem_write(LDOS_YEAR, lt->tm_year - 80);

      mem_write(NEWDOS_DATETIME_VALID_ADDR, NEWDOS_DATETIME_VALID_BYTE);
      mem_write(NEWDOS_MONTH, lt->tm_mon + 1);
      mem_write(NEWDOS_DAY, lt->tm_mday);
      mem_write(NEWDOS_YEAR, lt->tm_year % 100);
      mem_write(NEWDOS_HOUR, lt->tm_hour);
      mem_write(NEWDOS_MIN, lt->tm_min);
      mem_write(NEWDOS_SEC, lt->tm_sec);
  } else {
      mem_write(LDOS3_MONTH, (lt->tm_mon + 1) ^ 0x50);
      mem_write(LDOS3_DAY, lt->tm_mday);
      mem_write(LDOS3_YEAR, lt->tm_year - 80);

      mem_write(NEWDOS3_DATETIME_VALID_ADDR, NEWDOS_DATETIME_VALID_BYTE);
      mem_write(NEWDOS3_MONTH, lt->tm_mon + 1);
      mem_write(NEWDOS3_DAY, lt->tm_mday);
      mem_write(NEWDOS3_YEAR, lt->tm_year % 100);
      mem_write(NEWDOS3_HOUR, lt->tm_hour);
      mem_write(NEWDOS3_MIN, lt->tm_min);
      mem_write(NEWDOS3_SEC, lt->tm_sec);

      if (trs_model >= 4) {
        extern Uchar memory[];
	memory[LDOS4_MONTH] = lt->tm_mon + 1;
	memory[LDOS4_DAY] = lt->tm_mday;
	memory[LDOS4_YEAR] = lt->tm_year;
      }
  }
}

void
trs_timer_off(void)
{
  timer_on = 0;
}

void
trs_timer_on(void)
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
    } else if (trs_model == 1) {
        /* Typical 2x clock speedup kit */
        z80_state.clockMHz = CLOCK_MHZ_1 * ((fast&1) + 1);
    }
}

static trs_event_func event_func = NULL;
static int event_arg;

/* Schedule an event to occur after "countdown" more t-states have
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
    z80_state.sched = z80_state.t_count + (tstate_t) countdown;
    if (z80_state.sched == 0) z80_state.sched--;
}

/*
 * If an event is scheduled, do it now.  (If the event function
 * schedules a new event, however, leave that one pending.)
 */
void
trs_do_event(void)
{
    trs_event_func f = event_func;
    if (f) {
	event_func = NULL;
	z80_state.sched = 0;
	f(event_arg);    
    }
}

/*
 * Cancel scheduled event, if any.
 */
void
trs_cancel_event(void)
{
    event_func = NULL;
    z80_state.sched = 0;
}

/*
 * Check event scheduled
 */
trs_event_func
trs_event_scheduled(void)
{
    return event_func;
}
