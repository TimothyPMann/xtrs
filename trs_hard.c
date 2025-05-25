/* 
 * Copyright (c) 2000-2020, Timothy P. Mann
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
 * Emulation of the Radio Shack TRS-80 Model I/III/4/4P
 * hard disk controller.  This is a Western Digital WD1000/WD1010
 * mapped at ports 0xc8-0xcf, plus control registers at 0xc0-0xc1.
 */

#define _XOPEN_SOURCE 500 /* string.h: strdup() */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "trs.h"
#include "trs_hard.h"
#include "reed.h"

/*#define HARDDEBUG1 1*/  /* show detail on all port i/o */
/*#define HARDDEBUG2 1*/  /* show all commands */
/*#define HARDDEBUG3 1*/  /* show failure to open a drive */

/* Private types and data */

/* Structure describing one drive */
typedef struct {
  char *name;
  FILE *file;
  /* Values decoded from rhh */
  int writeprot;
  int cyls;  /* cyls per drive */
  int heads; /* tracks per cyl */
  int secs;  /* secs per track */
} Drive;

/* Structure describing controller state */
typedef struct {
  /* Controller present?  Yes if we have any drives, no if none */
  int present;

  /* Controller register images */
  Uchar control;
  Uchar data;
  Uchar error;
  Uchar seccnt;
  Uchar secnum;
  Ushort cyl;
  Uchar drive;
  Uchar head;
  Uchar status;
  Uchar command;

  /* Number of bytes already done in current read/write */
  int bytesdone;

  /* Drive geometries and files */
  Drive d[TRS_HARD_MAXDRIVES];
} State;

static State state;

/* Forward */
static int hard_data_in(void);
static void hard_data_out(int value);
static void hard_restore(int cmd);
static void hard_read(int cmd);
static void hard_write(int cmd);
static void hard_verify(int cmd);
static void hard_format(int cmd);
static void hard_init(int cmd);
static void hard_seek(int cmd);
static int open_drive(int drive);
static int reopen_drive(int drive);
static int find_sector(int newstatus);
static void set_dir_cyl(int cyl);

/* xtrs one-time initialization */
void trs_hard_init(void)
{
  int i;
  for (i=0; i<TRS_HARD_MAXDRIVES; i++) {
    Drive *d = &state.d[i];
    d->name = (char *) malloc(strlen(trs_disk_dir) + 10);
    if (trs_model == 5) {
      sprintf(d->name, "%s/hard4p-%d", trs_disk_dir, i);
    } else {
      sprintf(d->name, "%s/hard%d-%d", trs_disk_dir, trs_model, i);
    }
    state.d[i].file = NULL;
    state.d[i].writeprot = 0;
    state.d[i].cyls = 0;
    state.d[i].heads = 0;
    state.d[i].secs = 0;
  }
}

const char *
trs_hard_get_name(int drive)
{
  Drive *d = &state.d[drive];
  return d->name;
}

/* Returns 0 if OK, -1 if invalid header, errno value otherwise. */
int
trs_hard_set_name(int drive, const char *name)
{
  Drive *d = &state.d[drive];
  if (d->name) free(d->name);
  d->name = name ? strdup(name) : NULL;
  return reopen_drive(drive);
}

/* Returns 0 if OK, errno value otherwise. */
int
trs_hard_create(const char *name)
{
  FILE *f;
  int ires;

  f = fopen(name, "w");
  if (f == NULL) {
      return errno;
  }

  /*
   * Default parameters
   */
  ires = trs_hard_init_with(f, -1, -1, -1, -1);
  if (ires < 0) return errno;

  ires = fclose(f);
  if (ires < 0) return errno;
  return 0;
}

/* Powerup or reset button */
void trs_hard_reset(void)
{
  state.control = 0;
  state.data = 0;
  state.error = 0;
  state.seccnt = 0;
  state.secnum = 0;
  state.cyl = 0;
  state.drive = 0;
  state.head = 0;
  state.status = 0;
  state.command = 0;
}

/* Check for media change */
void trs_hard_change_all(void)
{
  int i;
  state.present = 0; // if no drives, emulate controller not present
  for (i=0; i<TRS_HARD_MAXDRIVES; i++) {
    if (reopen_drive(i) == 0) state.present = 1;
  }
}

/* Read from an I/O port mapped to the controller */
int trs_hard_in(int port)
{
  int v;
  if (state.present) {
    switch (port) {
    case TRS_HARD_WP: {
      int i;
      v = 0;
      for (i=0; i<TRS_HARD_MAXDRIVES; i++) {
	open_drive(i);
	if (state.d[i].writeprot) {
	  v |= TRS_HARD_WPBIT(i) | TRS_HARD_WPSOME;
	}
      }
      break; }
    case TRS_HARD_CONTROL:
      v = state.control;
      break;
    case TRS_HARD_DATA:
      v = hard_data_in();
      break;
    case TRS_HARD_ERROR:
      v = state.error;
      break;
    case TRS_HARD_SECCNT:
      v = state.seccnt;
      break;
    case TRS_HARD_SECNUM:
      v = state.secnum;
      break;
    case TRS_HARD_CYLLO:
      v = state.cyl & 0xff;
      break;
    case TRS_HARD_CYLHI:
      v = (state.cyl >> 8) & 0xff;
      break;
    case TRS_HARD_SDH:
      v = (state.drive << 3) | state.head;
      break;
    case TRS_HARD_STATUS:
      v = state.status;
      break;
    default:
      v = 0xff;
      break;
    }
  } else {
    v = 0xff;
  }
#if HARDDEBUG1
  debug("%02x -> %02x\n", port, v);
#endif  
  return v;
}

/* Write to an I/O port mapped to the controller */
void trs_hard_out(int port, int value)
{
#if HARDDEBUG1
  debug("%02x <- %02x\n", port, value);
#endif  
  switch (port) {
  case TRS_HARD_WP:
    break;
  case TRS_HARD_CONTROL:
    /* Do we really need to do anything here? */
    if (value & TRS_HARD_SOFTWARE_RESET) {
      trs_hard_reset();
    }
    if (value & TRS_HARD_DEVICE_ENABLE) {
      trs_hard_change_all();
    }
    state.control = value;
    break;
  case TRS_HARD_DATA:
    hard_data_out(value);
    break;
  case TRS_HARD_PRECOMP:
    break;
  case TRS_HARD_SECCNT:
    state.seccnt = value;
    break;
  case TRS_HARD_SECNUM:
    state.secnum = value;
    break;
  case TRS_HARD_CYLLO:
    state.cyl = (state.cyl & 0xff00) | (value & 0x00ff);
    break;
  case TRS_HARD_CYLHI:
    state.cyl = (state.cyl & 0x00ff) | ((value << 8) & 0xff00);
    break;
  case TRS_HARD_SDH:
    if (value & TRS_HARD_SIZEMASK) {
      error("trs_hard: size bits set to nonzero value (0x%02x)",
	    value & TRS_HARD_SIZEMASK);
    }
    state.drive = (value & TRS_HARD_DRIVEMASK) >> TRS_HARD_DRIVESHIFT;
    state.head = (value & TRS_HARD_HEADMASK) >> TRS_HARD_HEADSHIFT;
#if 0
    if (open_drive(state.drive) < 0) state.status &= ~TRS_HARD_READY;
#else
    /* Ready, but perhaps not able!  This way seems to work better; it
     * avoids a long delay in the Model 4P boot ROM when there is no
     * unit 0. */
    state.status = TRS_HARD_READY | TRS_HARD_SEEKDONE;
#endif
    break;

  case TRS_HARD_COMMAND:
    state.bytesdone = 0;
    state.command = value;
    switch (value & TRS_HARD_CMDMASK) {
    default:
      error("trs_hard: unknown command 0x%02x", value);
      break;
    case TRS_HARD_RESTORE:
      hard_restore(value);
      break;
    case TRS_HARD_READ:
      hard_read(value);
      break;
    case TRS_HARD_WRITE:
      hard_write(value);
      break;
    case TRS_HARD_VERIFY:
      hard_verify(value);
      break;
    case TRS_HARD_FORMAT:
      hard_format(value);
      break;
    case TRS_HARD_INIT:
      hard_init(value);
      break;
    case TRS_HARD_SEEK:
      hard_seek(value);
      break;
    }
    break;

  default:
    break;
  }
}

static void hard_restore(int cmd)
{
#if HARDDEBUG2
  debug("hard_restore drive %d\n", state.drive);
#endif
  state.cyl = 0;
  /*!! should anything else be zeroed? */
  state.status = TRS_HARD_READY | TRS_HARD_SEEKDONE;
}

static void hard_read(int cmd)
{
#if HARDDEBUG2
  debug("hard_read drive %d cyl %d hd %d sec %d\n",
	state.drive, state.cyl, state.head, state.secnum);
#endif
  if (cmd & TRS_HARD_MULTI) {
    error("trs_hard: multi-sector read not supported (0x%02x)", cmd);
    state.status = TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_ERR;
    state.error = TRS_HARD_ABRTERR;
    return;
  }
  find_sector(TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_DRQ);
}

static void hard_write(int cmd)
{
#if HARDDEBUG2
  debug("hard_write drive %d cyl %d hd %d sec %d\n",
	state.drive, state.cyl, state.head, state.secnum);
#endif
  if (cmd & TRS_HARD_MULTI) {
    error("trs_hard: multi-sector write not supported (0x%02x)", cmd);
    state.status = TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_ERR;
    state.error = TRS_HARD_ABRTERR;
    return;
  }
  find_sector(TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_DRQ);
}

static void hard_verify(int cmd)
{
#if HARDDEBUG2
  debug("hard_verify drive %d cyl %d hd %d sec %d\n",
	state.drive, state.cyl, state.head, state.secnum);
#endif
  find_sector(TRS_HARD_READY | TRS_HARD_SEEKDONE);
}

static void hard_format(int cmd)
{
#if HARDDEBUG2
  debug("hard_format drive %d cyl %d hd %d\n",
	state.drive, state.cyl, state.head);
#endif
  if (state.seccnt != TRS_HARD_SEC_PER_TRK) {
    error("trs_hard: can only do %d sectors/track, not %d",
	  TRS_HARD_SEC_PER_TRK, state.seccnt);
  }
  if (state.secnum != TRS_HARD_SECSIZE_CODE) {
    error("trs_hard: can only do %d bytes/sectors (code %d), not code %d",
	  TRS_HARD_SECSIZE, TRS_HARD_SECSIZE_CODE, state.secnum);
  }
  /* !!should probably set up to read skew table here */
  state.status = TRS_HARD_READY | TRS_HARD_SEEKDONE;
}

static void hard_init(int cmd)
{
#if HARDDEBUG2
  debug("hard_init drive %d cyl %d hd %d sec %d\n",
	state.drive, state.cyl, state.head, state.secnum);
#endif
  /* I don't know what this command does */
  error("trs_hard: init command (0x%02x) not implemented", cmd);
  state.status = TRS_HARD_READY | TRS_HARD_SEEKDONE;
}

static void hard_seek(int cmd)
{
#if HARDDEBUG2
  debug("hard_seek drive %d cyl %d hd %d sec %d\n",
	state.drive, state.cyl, state.head, state.secnum);
#endif
  find_sector(TRS_HARD_READY | TRS_HARD_SEEKDONE);
}

/*
 * Reopen the specified drive.
 *
 * 1) If already open, close the file for the drive.
 *
 * 2) Call open_drive and return the result.
 */
static int reopen_drive(int drive)
{
  Drive *d = &state.d[drive];

  if (d->file != NULL) {
    fclose(d->file);
    d->file = NULL;
  }

  return open_drive(drive);
}

/*
 * Open the specified drive if not already open.
 *
 * 1) Open the file.  If it cannot be opened, return 0 and set the
 * controller error status.
 *
 * 2) Set the hardware write protect status and geometry in the Drive
 * structure.
 *
 * 3) Return 0 if OK, -1 if invalid header, errno value otherwise.
 */
static int open_drive(int drive)
{
  Drive *d = &state.d[drive];
  ReedHardHeader rhh;
  size_t res;
  int err = 0;

  if (d->file != NULL) {
    return 0;
  }
  if (d->name == NULL) {
    goto fail;
  }

  /* First try opening for reading and writing */
  d->file = fopen(d->name, "r+");
  if (d->file == NULL) {
    if (errno == EACCES || errno == EROFS) {
      /* No luck, try for reading only */
      d->file = fopen(d->name, "r");
    }
    if (d->file == NULL) {
      err = errno;
      goto fail;
    }
    d->writeprot = 1;
  } else {
    d->writeprot = 0;
  }

  /* Read in the Reed header and check some basic magic numbers (not all) */
  res = fread(&rhh, sizeof(rhh), 1, d->file);
  if (res != 1 || rhh.id1 != 0x56 || rhh.id2 != 0xcb || rhh.ver >= 0x1f) {
    err = -1;
    goto fail;
  }
  if (rhh.flag1 & 0x80) d->writeprot = 1;

  /* Use the number of cylinders specified in the header */
  d->cyls = rhh.cyl ? rhh.cyl : 256;

  /* Use the number of secs/track that RSHARD requires */
  d->secs = TRS_HARD_SEC_PER_TRK;

  /* Header gives only secs/cyl.  Compute number of heads from 
     this and the assumed number of secs/track. */
  d->heads = (rhh.sec ? rhh.sec : 256) / d->secs;

  if ((rhh.sec % d->secs) != 0 ||
      d->heads <= 0 || d->heads > TRS_HARD_MAXHEADS) {
    error("trs_hard: unusable geometry in image %s", d->name);
    err = -1;
    goto fail;
  }

  state.status = TRS_HARD_READY | TRS_HARD_SEEKDONE;
  return 0;

 fail:
  if (d->file) fclose(d->file);
  d->file = NULL;
  state.status = TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_ERR;
  state.error = TRS_HARD_NFERR;
  return err;
}    

/*
 * Check whether the current position is in bounds for the geometry.
 * If not, return 0 and set the controller error status.  If so, fseek
 * the file to the start of the current sector, return 1, and set
 * the controller status to newstatus.
 */
static int find_sector(int newstatus)
{
  Drive *d = &state.d[state.drive];
  if (open_drive(state.drive) < 0) return 0;
  if (/**state.cyl >= d->cyls ||**/ /* ignore this limit */
      state.head >= d->heads ||
      state.secnum > d->secs /* allow 0-origin or 1-origin */ ) {
    error("trs_hard: requested cyl %d hd %d sec %d; max cyl %d hd %d sec %d\n",
	  state.cyl, state.head, state.secnum, d->cyls, d->heads, d->secs);
    state.status = TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_ERR;
    state.error = TRS_HARD_NFERR;
    return 0;
  }
  fseek(d->file,
	sizeof(ReedHardHeader) +
	TRS_HARD_SECSIZE * (state.cyl * d->heads * d->secs +
			    state.head * d->secs +
			    (state.secnum % d->secs)),
	0);
  state.status = newstatus;
  return 1;
}

static int hard_data_in(void)
{
  Drive *d = &state.d[state.drive];
  if ((state.command & TRS_HARD_CMDMASK) == TRS_HARD_READ &&
      (state.status & TRS_HARD_ERR) == 0) {
    if (state.bytesdone < TRS_HARD_SECSIZE) {
      state.data = getc(d->file);
      state.bytesdone++;
    }
  }
  return state.data;
}

static void hard_data_out(int value)
{
  Drive *d = &state.d[state.drive];
  int res = 0;
  state.data = value;
  if ((state.command & TRS_HARD_CMDMASK) == TRS_HARD_WRITE &&
      (state.status & TRS_HARD_ERR) == 0) {
    if (state.bytesdone < TRS_HARD_SECSIZE) {
      if (state.cyl == 0 && state.head == 0 &&
	  state.secnum == 0 && state.bytesdone == 2) {
	set_dir_cyl(value);
      }
      res = putc(state.data, d->file);
      state.bytesdone++;
      if (res != EOF && state.bytesdone == TRS_HARD_SECSIZE) {
	res = fflush(d->file);
      }
    }
  }
  if (res == EOF) {
    error("trs_hard: errno %d while writing drive %d", errno, state.drive);
    state.status = TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_ERR;
    state.error = TRS_HARD_DATAERR; /* arbitrary choice */
  }
}

/* Sleazy trick to update the "directory cylinder" byte in the Reed
   header.  This value is only needed by the Reed emulator itself, and
   we would like xtrs to set it automatically so that the user doesn't
   have to know about it. */
static void set_dir_cyl(int cyl)
{
  Drive *d = &state.d[state.drive];
  long where = ftell(d->file);
  fseek(d->file, 31, 0);
  putc(cyl, d->file);
  fseek(d->file, where, 0);
}
