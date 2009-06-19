/* Copyright (c) 2008, Timothy Mann */
/* $Id$ */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/*
 * Emulate Exatron stringy floppy.
 * XXX Needs lots of cleanup
 */

#include "z80.h"
#include "trs.h"
#include "trs_disk.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define STRINGYDEBUG_IN 0
#define STRINGYDEBUG_OUT 0
#define STRINGYDEBUG_OPEN 0

/* Input port bits */
#define STRINGY_WRITE_PROT  0x01
#define STRINGY_END_OF_TAPE 0x04
#define STRINGY_NO_WAFER    0x08  /* XXX? */
#define STRINGY_FLUX        0x80
#define STRINGY_NO_UNIT     0xff

/* Output port bits */
#define STRINGY_MOTOR_ON    0x01
#define STRINGY_WRITE_GATE  0x04
/*#define STRINGY_FLUX      0x80*/

#define STRINGY_EOT_WIDTH 24800 // seems to work; don't know how realistic it is
#define STRINGY_MAX_UNITS 8

typedef tstate_t stringy_pos_t; // XXX measure length/position in usec instead?

typedef struct {
  FILE *file;
  stringy_pos_t length;
  stringy_pos_t pos;
  tstate_t pos_time;
  stringy_pos_t flux_change_pos;
  int flux_change_to;
  Uchar in_port;
  Uchar out_port;
#if STRINGYDEBUG_IN
  int prev_in_port;
#endif
} stringy_info_t;

stringy_info_t stringy_info[STRINGY_MAX_UNITS];

#define STRINGY_STOPPED 0
#define STRINGY_READING 1
#define STRINGY_WRITING 2

static int
stringy_state(int out_port)
{
  if ((out_port & STRINGY_MOTOR_ON) == 0) return STRINGY_STOPPED;
  if ((out_port & STRINGY_WRITE_GATE) != 0) return STRINGY_WRITING;
  return STRINGY_READING;
}

static void
stringy_update_pos(stringy_info_t *s)
{
  if (!(s->out_port & STRINGY_MOTOR_ON)) {
    //XXX hack; rom seems to expect motor off/on to get past the splice??
    if (s->pos >= s->length - STRINGY_EOT_WIDTH) {
      goto wrap;  //XXX clean this up!
    }
  }

  s->pos += z80_state.t_count - s->pos_time;
  s->pos_time = z80_state.t_count;
  if (s->pos >= s->length) {
  wrap:
#if STRINGYDEBUG_WRAP
    debug("stringy wrap\n");
#endif
    //XXX probably should flush here if we were writing; could be
    // wrapping on a status read-back
    s->in_port = /*ZZZ STRINGY_FLUX*/ 0;
    s->pos = s->flux_change_pos = 0;
    s->flux_change_to = 1;
    rewind(s->file);
    //XXX skip header (if I decide to have one)
  } else if (s->pos >= s->length - STRINGY_EOT_WIDTH) {
    s->in_port |= STRINGY_END_OF_TAPE;
  }
}

static int
stringy_reopen(int unit)
{
  stringy_info_t *s = &stringy_info[unit];
  char name[1024];

  if (s->file) {
    fclose(s->file);
  }

  sprintf(name, "%s/stringy%d-%d", trs_disk_dir, trs_model, unit);
  s->file = fopen(name, "r+");
  if (s->file == NULL) {
    s->file = fopen(name, "r");
    if (s->file == NULL) {
#if STRINGYDEBUG_OPEN
      error("could not open wafer image %s: %s", name, strerror(errno));
#endif
      s->in_port = STRINGY_NO_WAFER; //XXX or STRINGY_NO_UNIT?
      return 0;
    }
    s->in_port = STRINGY_WRITE_PROT | /*ZZZ STRINGY_FLUX*/ 0;
#if STRINGYDEBUG_OPEN
    debug("opened write protected wafer image %s\n");
#endif
  } else {
    s->in_port = /*ZZZ STRINGY_FLUX*/ 0;
#if STRINGYDEBUG_OPEN
    debug("opened writeable wafer image %s\n", name);
#endif
  }  

  s->length = 2480000; //XXX get from length or header of file?
  s->pos = 0;
  s->pos_time = z80_state.t_count;
  s->flux_change_pos = 0;
  s->flux_change_to = 1;

  //XXX check for magic number at start of file, fail if not there?

  return 1;
}

void
stringy_reset(void)
{
  int i;
  for (i = 0; i < STRINGY_MAX_UNITS; i++) {
    stringy_info_t *s = &stringy_info[i];
    stringy_reopen(i);
    s->out_port = 0;
#if STRINGYDEBUG_IN
    s->prev_in_port = -1;
#endif
  }
}

static void
stringy_flux_write(stringy_info_t *s, int flux, stringy_pos_t delta)
{
  //XXX "debug"-style output only for the moment
  fprintf(s->file, "%u %" TSTATE_T_LEN "\n", flux, delta);
}

static int
stringy_flux_read(stringy_info_t *s, int *flux, stringy_pos_t *delta)
{
  //XXX "debug"-style output only for the moment
  return (fscanf(s->file, "%u %" TSTATE_T_LEN "\n", flux, delta) == 2);
}

int
stringy_in(int unit)
{
  stringy_info_t *s = &stringy_info[unit];
  int ret;

  if (s->in_port & STRINGY_NO_WAFER) {
    ret = s->in_port;
    goto done;
  }

  stringy_update_pos(s);

  if (stringy_state(s->out_port) == STRINGY_READING) {

    while (s->pos >= s->flux_change_pos) {
      int flux;
      stringy_pos_t delta;

      s->in_port = (s->in_port & ~STRINGY_FLUX) |
	/*ZZZ (s->flux_change_to ? STRINGY_FLUX : 0); */
	(s->flux_change_to ? 0 : STRINGY_FLUX);
      
      if (!stringy_flux_read(s, &flux, &delta)) {
	break;
      }

      s->flux_change_to = flux;
      s->flux_change_pos += delta;
    }
  }
  ret = s->in_port;

 done:
#if STRINGYDEBUG_IN
  if (ret != s->prev_in_port) {
    debug("stringy_in(%d) -> %d\n", unit, ret);
    s->prev_in_port = ret;
  }
#endif  
  return ret;
}

void
stringy_out(int unit, int value)
{
  stringy_info_t *s = &stringy_info[unit];
  int old_state, new_state;

  if (s->in_port & STRINGY_NO_WAFER) return;

  old_state = stringy_state(s->out_port);
  new_state = stringy_state(value);
  stringy_update_pos(s);

  if (old_state == STRINGY_STOPPED &&
      new_state != STRINGY_STOPPED) {
    s->pos_time = z80_state.t_count;
    s->in_port /* ZZZ |= ZZZ STRINGY_FLUX*/ &= ~STRINGY_FLUX;
    s->flux_change_pos = s->pos;
    s->flux_change_to = 1;
  }

  if (old_state != STRINGY_WRITING &&
      new_state == STRINGY_WRITING) {
    fflush(s->file);
    ftruncate(fileno(s->file), ftell(s->file));
    fseek(s->file, 0, SEEK_CUR);
    stringy_flux_write(s, 1, 0);
  }

  if (old_state == STRINGY_WRITING) {
    if (((s->out_port ^ value) & STRINGY_FLUX) != 0 ||
	new_state != STRINGY_WRITING) {
      stringy_flux_write(s, (value & STRINGY_FLUX) != 0,
			 s->pos - s->flux_change_pos);
      s->flux_change_pos = s->pos;
    }

    if (new_state != STRINGY_WRITING) {
      fflush(s->file);
    }

#if 0 //XXX 
    if (new_state == STRINGY_READING) {
      s->in_port /* ZZZ |= STRINGY_FLUX*/ 0; //XXX needed?
    }
#endif
  }
  s->out_port = value;

#if STRINGYDEBUG_OUT
  debug("stringy_out(%d, %d)\n", unit, value);
  s->prev_in_port = -1;
#endif  
}
