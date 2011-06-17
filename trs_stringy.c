/* Copyright (c) 2011, Timothy Mann */
/* $Id$ */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/*
 * Emulate Exatron stringy floppy.
 *
 * XXX need to (re)test whether esf output can be read by TRS32.  the
 * esf output of TRS32 could be read at last test.
 *
 * XXX @NEW does not always write exactly the same bits and the length
 * seems to vary by +/-1 byte sometimes.  May only be when overwriting
 * a wafer.  Hmm, forgetting to clear the last partial byte from an
 * old write?
 *
 * XXX also see other XXX comments below.
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
#define STRINGY_NO_WAFER    0x08  /* is that what this bit means? */
#define STRINGY_FLUX        0x80
#define STRINGY_NO_UNIT     0xff

/* Output port bits */
#define STRINGY_MOTOR_ON    0x01
#define STRINGY_WRITE_GATE  0x04
/*#define STRINGY_FLUX      0x80*/

#define STRINGY_MAX_UNITS 8

#define STRINGY_CELL_WIDTH 124 // in t-states
//#define STRINGY_CELLS_DEFAULT (48 * 1024 * 2 / 8 * 9) // 48K incl. gaps/leaders
#define STRINGY_CELLS_DEFAULT 20000 // make super short while debugging XXX
#define STRINGY_EOT_DEFAULT 60 // a good value per MKR

#define STRINGY_FMT_DEBUG 1
#define STRINGY_FMT_ESF 2

/*#define STRINGY_FMT_DEFAULT STRINGY_FMT_DEBUG*/
#define STRINGY_FMT_DEFAULT STRINGY_FMT_ESF


typedef long stringy_pos_t;

typedef struct {
  FILE *file;
  stringy_pos_t length;
  stringy_pos_t eotWidth;
  stringy_pos_t pos;
  tstate_t pos_time;
  stringy_pos_t flux_change_pos;
  int flux_change_to;
  Uchar in_port;
  Uchar out_port;
  Uchar format;
  Uchar bytebuf; // for esf format
  Uchar bitpos;  // for esf format
#if STRINGYDEBUG_IN
  int prev_in_port;
#endif
} stringy_info_t;

stringy_info_t stringy_info[STRINGY_MAX_UNITS];

/*
 * .esf file format used by TRS32.  The exact definition is unknown at
 * this time.  In particular, the 4 bytes after "magic" might be
 * part of the magic number rather than parameters of unknown meaning.
 */
const char stringy_esf_magic[4] = "ESF\x1a";
const Uchar stringy_esf_header_length = 12;
const Uchar stringy_esf_write_protected = 1;
/*
struct {
  char magic[4] = stringy_esf_magic;
  Uchar headerLength = 12;  // length of this header, in bytes
  Uchar flags;              // bit 0: write protected; others reserved
  Ushort leaderLength = 60; // little endian, in bit cells
  Uint length;              // little endian, in bytes
  Uchar data[length];
}
 */

//XXX debug format should record leaderLength and writeProtected too
const char stringy_debug_header[] = "xtrs stringy debug %u\n";

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

static int
stringy_write_header(stringy_info_t *s)
{
  int ires;
  size_t sres;

  rewind(s->file);
  switch (s->format) {
  case STRINGY_FMT_DEBUG:
    ires = ftruncate(fileno(s->file), 0);
    if (ires < 0) return ires;
    ires = fprintf(s->file, stringy_debug_header, (Uint) s->length);
    if (ires < 0) return ires;
    break;
  case STRINGY_FMT_ESF:
    sres = fwrite(stringy_esf_magic, sizeof(stringy_esf_magic), 1, s->file);
    if (sres < 1) return -1;
    ires = fputc(stringy_esf_header_length, s->file);
    if (ires < 0) return ires;
    ires = fputc((s->in_port & STRINGY_WRITE_PROT) ?
		 stringy_esf_write_protected : 0, s->file);
    if (ires < 0) return ires;
    ires = put_twobyte(s->eotWidth / STRINGY_CELL_WIDTH, s->file);
    if (ires < 0) return ires;
    ires = put_fourbyte(s->length / (STRINGY_CELL_WIDTH * 8), s->file);
    if (ires < 0) return ires;
    break;
  default:
    error("unknown wafer image type on write");
    return -1;
  }
  fseek(s->file, 0, SEEK_CUR);
  return 0;
}

static int
stringy_read_esf_header(stringy_info_t *s)
{
  char magic[sizeof(stringy_esf_magic)];
  Uchar headerLength, flags;
  Ushort eotWidth;
  size_t sres;
  int ires;
  Uint len;
  
  rewind(s->file);
  sres = fread(magic, sizeof(stringy_esf_magic), 1, s->file);
  if (sres < 1) return -1;
  if (memcmp(magic, stringy_esf_magic,
	     sizeof(stringy_esf_magic)) != 0) return -1;
  ires = fgetc(s->file);
  if (ires < 0) return ires;
  headerLength = ires;
  if (headerLength != stringy_esf_header_length) return -1;
  ires = fgetc(s->file);
  if (ires < 0) return ires;
  flags = ires;
  ires = get_twobyte(&eotWidth, s->file);
  if (ires < 0) return ires;
  ires = get_fourbyte(&len, s->file);
  if (ires < 0) return ires;

  s->format = STRINGY_FMT_ESF;
  s->length = len * STRINGY_CELL_WIDTH * 8;
  s->eotWidth = eotWidth * STRINGY_CELL_WIDTH;
  s->in_port = (s->in_port & ~STRINGY_WRITE_PROT) |
    ((flags & stringy_esf_write_protected) ? STRINGY_WRITE_PROT : 0);
  fseek(s->file, 0, SEEK_CUR);
  return 0;
}

static int
stringy_read_debug_header(stringy_info_t *s)
{
  int ires;
  Uint len;
  
  rewind(s->file);
  ires = fscanf(s->file, stringy_debug_header, &len);
  if (ires < 1) return -1;
  s->format = STRINGY_FMT_DEBUG;
  s->length = len;
  s->eotWidth = STRINGY_EOT_DEFAULT * STRINGY_CELL_WIDTH; //XXX add to header
  fseek(s->file, 0, SEEK_CUR);
  return 0;
}

static int
stringy_read_header(stringy_info_t *s)
{
  int ires;

  ires = stringy_read_esf_header(s);
  if (ires >= 0) return ires;

  ires = stringy_read_debug_header(s);
  return ires;
}

static void
stringy_update_pos(stringy_info_t *s)
{
  if (stringy_state(s->out_port) == STRINGY_STOPPED) {
#if 1
    /*
     * XXX Always start at the beginning after motor off/on.  This
     * surely doesn't emulate real hardware accurately, but I am
     * getting "tape too short" errors without it.  Maybe it is
     * working around a bug elsewhere?  Would like to get rid of this
     * for that reason.  Also wondering if it is causing problems.
     *
     * If we do need this, factor out the common code from the wrap
     * cases.
     */
    s->pos = 0;
    s->in_port &= ~STRINGY_END_OF_TAPE;
    stringy_read_header(s);
    //XXX this can't be quite right XXX
    // there should not be a glitch at the wrap point
    s->flux_change_pos = 0;
    s->flux_change_to = 1;
#endif
    return;
  }

  s->pos += z80_state.t_count - s->pos_time;
  s->pos_time = z80_state.t_count;

  /*
   * XXX Wrapping this way on write still doesn't exactly duplicate
   * TRS32 output (TRS32 seems to drop one bit, while mine looks
   * worse).  Maybe I'm trying to wrap when not on a byte boundary?
   * Maybe missing a stringy_byte_flush or doing it at the wrong spot?
   */
  if (s->pos >= s->length) {
    // Debug format can't handle overwriting
    if (s->format != STRINGY_FMT_DEBUG ||
	stringy_state(s->out_port) == STRINGY_READING) {
      s->pos = 0;
      s->in_port &= ~STRINGY_END_OF_TAPE;
      stringy_read_header(s);
      //XXX this can't be quite right XXX
      // there should not be a glitch at the wrap point
      s->flux_change_pos = 0;
      s->flux_change_to = 1;
    }
  } else if (s->pos >= s->length - s->eotWidth) {
    s->in_port |= STRINGY_END_OF_TAPE;
  }
}

static int
stringy_reopen(int unit)
{
  stringy_info_t *s = &stringy_info[unit];
  char name[1024]; //XXX buffer overflow?
  int ires;

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
      return -1;
    }
    s->in_port = STRINGY_WRITE_PROT;
#if STRINGYDEBUG_OPEN
    debug("opened write protected wafer image %s\n");
#endif
  } else {
    s->in_port = 0;
#if STRINGYDEBUG_OPEN
    debug("opened writeable wafer image %s\n", name);
#endif
  }  

  ires = stringy_read_header(s);
  if (ires == -1) {
    // If the file exists but has no header, write one.
    // This lets you create a blank wafer with "touch"
    // XXX Have a better way to create a stringy and set the parameters.
    s->format = STRINGY_FMT_DEFAULT;
    s->length = STRINGY_CELLS_DEFAULT * STRINGY_CELL_WIDTH;
    s->eotWidth = STRINGY_EOT_DEFAULT * STRINGY_CELL_WIDTH;
    ires = stringy_write_header(s);
  }

  s->pos = 0;
  s->pos_time = z80_state.t_count;
  s->flux_change_pos = 0;
  s->flux_change_to = 1;

  return ires;
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
stringy_byte_flush(stringy_info_t *s)
{
  int ires;

  if (s->format != STRINGY_FMT_ESF ||
      stringy_state(s->out_port) != STRINGY_WRITING ||
      s->bitpos == 0) return;

  fseek(s->file, 0, SEEK_CUR);
  ires = fgetc(s->file);
  if (ires == EOF) {
    ires = 0;
  }
  fseek(s->file, -1, SEEK_CUR);
  fputc((ires & (0xff << s->bitpos)) | s->bytebuf, s->file); //XXX handle errors
  fseek(s->file, -1, SEEK_CUR);
}

static void
stringy_bit_write(stringy_info_t *s, int flux)
{
  s->bytebuf |= flux << s->bitpos;
  s->bitpos++;
  if (s->bitpos == 8) {
    fputc(s->bytebuf, s->file); //XXX handle errors
    s->bitpos = 0;
    s->bytebuf = 0;
  }
}

/*
 * flux = new flux state
 * delta = time in *previous* state
 */
static void
stringy_flux_write(stringy_info_t *s, int flux, stringy_pos_t delta)
{
  int cells;
  stringy_pos_t adjustment;

  switch (s->format) {
  case STRINGY_FMT_DEBUG:
    fprintf(s->file, "%u %" TSTATE_T_LEN "\n", flux, delta);
    break;
  case STRINGY_FMT_ESF:
    cells = (delta + 1) / STRINGY_CELL_WIDTH;
    if (cells > 3) {
      /*
       * XXX Why is this needed?  Otherwise we get a huge run of cells
       * right at the start, and some later too -- see .debug format
       * output.  Did the real hardware/firmware leave gaps there
       * (maybe for motor startup?), or is this a bug in xtrs
       * emulation?  TRS32 shows no such gaps in its output format, so
       * we avoid them too by crushing them out here.
       */
      cells = 3;
    }
    adjustment = delta - cells * STRINGY_CELL_WIDTH;
    while (cells--) {
      stringy_bit_write(s, flux);
    }
    /*
     * Adjust the position to exactly match the nominal size of the
     * number of cells written.  This is necessary so that the end of
     * tape mark will be seen at exactly the same place when reading
     * the tape back as it was seen when writing the tape out.
     */
    s->pos -= adjustment;
    break;
  }
}

static int
stringy_bit_read(stringy_info_t *s, int *bit)
{
  int ires;

  if (s->bitpos == 0) {
    ires = fgetc(s->file); //XXX handle errors differently from EOF
    if (ires < 0) return FALSE;
    s->bytebuf = ires;
  }
  *bit = (s->bytebuf & (1 << s->bitpos)) != 0;
  s->bitpos = (s->bitpos + 1) % 8;
  return TRUE;
}

static int
stringy_flux_read(stringy_info_t *s, int *flux, stringy_pos_t *delta)
{
  int bres;
  int bit;

  switch(s->format) {
  case STRINGY_FMT_DEBUG:
    return (fscanf(s->file, "%u %" TSTATE_T_LEN "\n", flux, delta) == 2);
  case STRINGY_FMT_ESF:
    bres = stringy_bit_read(s, &bit);
    if (!bres) return bres;
    /*
     * This calls for some explanation.  Our caller wants the delta to
     * the next flux change and the resulting flux value, as in "xtrs
     * stringy debug" format.  We don't really know either; we only
     * know the current flux value and that it will continue for
     * STRINGY_CELL_WIDTH.  So we fib and say that the value will
     * change to the opposite of the current value after
     * STRINGY_CELL_WIDTH.  This is actually good enough for our
     * caller, which will request enough additional samples to get
     * correct information before using the information.
     */
    *flux = !bit;
    *delta = STRINGY_CELL_WIDTH;
    return TRUE;
  }
  return FALSE;
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

// XXX the state change stuff is a bit suspect, and not too clean
void
stringy_out(int unit, int value)
{
  stringy_info_t *s = &stringy_info[unit];
  int old_state, new_state;
  int ignore;

  if (s->in_port & STRINGY_NO_WAFER) return;

  old_state = stringy_state(s->out_port);
  new_state = stringy_state(value);
  stringy_update_pos(s);

  if (old_state == STRINGY_STOPPED &&
      new_state != STRINGY_STOPPED) {
    s->pos_time = z80_state.t_count;
    s->in_port &= ~STRINGY_FLUX;
    s->flux_change_pos = s->pos;
    s->flux_change_to = 1;
  }

  if (old_state != STRINGY_WRITING &&
      new_state == STRINGY_WRITING) {
    fflush(s->file);
    if (s->format == STRINGY_FMT_DEBUG) {
      // Debug format can't handle overwriting
      ignore = ftruncate(fileno(s->file), ftell(s->file));
    }
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

    if (old_state == STRINGY_WRITING &&
	new_state != STRINGY_WRITING) {
      stringy_byte_flush(s);
      fflush(s->file);
    }
  }
  s->out_port = value;

#if STRINGYDEBUG_OUT
  debug("stringy_out(%d, %d)\n", unit, value);
  s->prev_in_port = -1;
#endif  
}
