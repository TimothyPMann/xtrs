/* Copyright (c) 1996-97, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Sat Apr 17 20:33:49 PDT 1999 by mann */

/*
 * Emulate Model I or III/4 disk controller
 */

/*#define DISKDEBUG  1*/  /* FDC register reads and writes */
/*#define DISKDEBUG2 1*/  /* VTOS 3.0 kludges */
/*#define DISKDEBUG3 1*/  /* Gaps and real_writetrk */
/*#define DISKDEBUG4 1*/  /* REAL sector size detection */
/*#define DISKDEBUG5 1*/  /* Read Address timing */
#define TSTATEREV 1       /* Index holes timed by T-states, not real time */
#define SIZERETRY 1       /* Retry in different sizes on real_read */
#define REALRADLY 0       /* Read Address timing for real disks; nonworking */

#include "z80.h"
#include "trs.h"
#include "trs_disk.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if __linux
#include <sys/types.h>
#include <fcntl.h>
#include <linux/fd.h>
#include <linux/fdreg.h>
#include <sys/ioctl.h>
#include <signal.h>
#endif

#define NDRIVES 8

int trs_disk_nocontroller = 0;
int trs_disk_doubler = TRSDISK_BOTH;
char *trs_disk_dir = DISKDIR;
unsigned short trs_disk_changecount = 0;
float trs_disk_holewidth = 0.01;
int trs_disk_truedam = 0;

typedef struct {
  /* Registers */
  unsigned char status;
  unsigned char track;
  unsigned char sector;
  unsigned char data;
  /* Other state */
  unsigned char currcommand;
  int lastdirection;
  int bytecount;		/* bytes left to transfer this command */
  int format;			/* write track state machine */
  int format_bytecount;         /* bytes left in this part of write track */
  int format_sec;               /* current sector number or id_index */
  int format_gapcnt;            /* measure requested gaps */
  int format_gap[5];
  unsigned curdrive;
  unsigned curside;
  unsigned density;		/* sden=0, dden=1 */
  unsigned char controller;	/* TRSDISK_P1771 or TRSDISK_P1791 */
  int last_readadr;             /* id index found by last readadr */
  tstate_t motor_timeout;       /* 0 if stopped, else time when it stops */
} FDCState;

FDCState state, other_state;

/* Format states - what is expected next? */
#define FMT_GAP0    0
#define FMT_IAM     1
#define FMT_GAP1    2
#define FMT_IDAM    3
#define FMT_TRACKID 4
#define FMT_HEADID  5
#define FMT_SECID   6
#define FMT_SIZEID  7
#define FMT_IDCRC   8
#define FMT_GAP2    9
#define FMT_DAM     10
#define FMT_DATA    11
#define FMT_DCRC    12
#define FMT_GAP3    13
#define FMT_GAP4    14
#define FMT_DONE    15


/* Gap 0+1 and gap 4 angular size, used in Read Address timing emulation.
   Units: fraction of a complete circle. */
#define GAP1ANGLE 0.020 
#define GAP4ANGLE 0.050

/* How long does emulated motor stay on after drive selected? (us of
   emulated time) */
#define MOTOR_USEC 2000000

/* Heuristic: how often are we willing to check whether real drive
   has a disk in it?  (seconds of real time) */
#define EMPTY_TIMEOUT 3

/*
 * The following rather quirky data structure is designed to be
 * compatible with what Jeff Vavasour's Model III/4 emulator uses to
 * represent disk formatting information.  My interpretation is based
 * on reading his documentation, looking at some disk images, and
 * experimenting with his emulator to generate odd cases, so the
 * compatibility should be excellent.
 *
 * I have compatibly extended the format to allow for more sectors, so
 * that 8" DSDD drives can be supported, by adding a second block of
 * ids after the block of data sectors that is described by the first
 * block.  JV himself says that sounds like a good idea.  Matthew
 * Reed's emulators now support this extension.
 *
 * I've further extended the format to add a flag bit for non-IBM
 * sectors. Only a subset of the non-IBM functionality is supported,
 * rigged to make the VTOS 3.0 copy-protection system work.  Non-IBM
 * sectors were a feature of the 1771 only. Using this feature, you
 * could have a sector of any length from 16 to 4096 bytes in
 * multiples of 16. xtrs supports only 16-byte non-IBM sectors (with
 * their data stored in a 256-byte field), and has some special kludges
 * to detect and support VTOS's trick of formatting these sectors with
 * inadequate gaps between so that writing to one would smash the
 * index block of the next one.
 *
 * Finally, I've extended the format to support (IBM) sector lengths
 * other than 256 bytes. The standard lengths 128, 256, 512, and 1024
 * are supported, using the last two available bits in the header
 * flags byte. The data area of a floppy image thus becomes an array
 * of *variable length* sectors, making it more complicated to find
 * the sector data corresponding to a given header and to manage freed
 * sectors.
 *
 * NB: JV's model I emulator uses no auxiliary data structure for disk
 * format.  It simply assumes that all disks are single density, 256
 * bytes/sector, 10 sectors/track, single sided, with nonstandard FA
 * data address mark on all sectors on track 17.  */

/* Values for flags below */
#define JV3_DENSITY     0x80  /* 1=dden, 0=sden */
#define JV3_DAM         0x60  /* data address mark; values follow */
#define JV3_DAMSDFB     0x00
#define JV3_DAMSDFA     0x20
#define JV3_DAMSDF9     0x40
#define JV3_DAMSDF8     0x60
#define JV3_DAMDDFB     0x00
#define JV3_DAMDDF8     0x20
#define JV3_SIDE        0x10  /* 0=side 0, 1=side 1 */
#define JV3_ERROR       0x08  /* 0=ok, 1=CRC error */
#define JV3_NONIBM      0x04  /* 0=normal, 1=short (for VTOS 3.0, xtrs only) */
#define JV3_SIZE        0x03  /* in used sectors: 0=256,1=128,2=1024,3=512
				 in free sectors: 0=512,1=1024,2=128,3=256 */

#define JV3_FREE        0xff  /* in track/sector fields */
#define JV3_FREEF       0xfc  /* in flags field, or'd with size code */

typedef struct {
  unsigned char track;
  unsigned char sector;
  unsigned char flags;
} SectorId;

/* Arbitrary maximum on tracks, chosen because LDOS can use at most 95 */
#define MAXTRACKS    96
#define JV1_SECSIZE 256
#define MAXSECSIZE 1024

/* Max bytes per unformatted track. */
/* Select codes 1, 2, 4, 8 are emulated 5" drives, disk?-0 to disk?-3 */
#define TRKSIZE_SD    3125  /* 250kHz / 5 Hz [300rpm] / (2 * 8) */
                         /* or 300kHz / 6 Hz [360rpm] / (2 * 8) */
#define TRKSIZE_DD    6250  /* 250kHz / 5 Hz [300rpm] / 8 */
                         /* or 300kHz / 6 Hz [360rpm] / 8 */
/* Select codes 3, 5, 6, 7 are emulated 8" drives, disk?-4 to disk?-7 */
#define TRKSIZE_8SD   5208  /* 500kHz / 6 Hz [360rpm] / (2 * 8) */
#define TRKSIZE_8DD  10416  /* 500kHz / 6 Hz [360rpm] / 8 */
/* TRS-80 software has no concept of HD, so these constants are unused,
 * but expanded JV3 would be big enough even for 3.5" HD. */
#define TRKSIZE_5HD  10416  /* 500kHz / 6 Hz [360rpm] / 8 */
#define TRKSIZE_3HD  12500  /* 500kHz / 5 Hz [300rpm] / 8 */

#define JV3_SIDES      2
#define JV3_IDSTART    0
#define JV3_SECSTART   (34*256) /* start of sectors within file */
#define JV3_SECSPERBLK ((int)(JV3_SECSTART/3))
#define JV3_SECSMAX    (2*JV3_SECSPERBLK)

#define JV1_SECPERTRK 10

/* Values for emulated disk image type (emutype) below */
#define JV1 1 /* compatible with Vavasour Model I emulator */
#define JV3 3 /* compatible with Vavasour Model III/4 emulator */
#define REAL 100
#define NONE 0

typedef struct {
  int writeprot;		  /* emulated write protect tab */
  int phytrack;			  /* where are we really? */
  int emutype;
  int inches;                     /* 5 or 8, as seen by TRS-80 */
  int real_rps;                   /* phys rotations/sec; emutype REAL only */
  int real_size_code;             /* most recent sector size; REAL only */
  int real_step;                  /* 1=normal, 2=double-step; REAL only */
  int real_empty;                 /* 1=emulate empty drive */
  time_t empty_timeout;           /* real_empty valid until this time */
  FILE* file;
  int free_id[4];		  /* first free id, if any, of each size */
  int last_used_id;		  /* last used index */
  int nblocks;                    /* number of blocks of ids, 1 or 2 */
  int sorted_valid;               /* sorted_id array valid */
  SectorId id[JV3_SECSMAX + 1];   /* extra one is a loop sentinel */
  int offset[JV3_SECSMAX + 1];    /* offset into file for each id */
  short sorted_id[JV3_SECSMAX + 1];
  short track_start[MAXTRACKS][JV3_SIDES];
  unsigned char buf[MAXSECSIZE];
} DiskState;

/* Reuse some fields */
#define realfmt_nbytes nblocks      /* number of PC format command bytes */
#define realfmt_fill sorted_valid   /* fill byte for data sectors */

DiskState disk[NDRIVES];

/* Emulate interleave in JV1 mode */
unsigned char jv1_interleave[10] = {0, 5, 1, 6, 2, 7, 3, 8, 4, 9};

/* Forward */
void real_verify();
void real_restore(int curdrive);
void real_seek();
void real_read();
void real_write();
void real_readadr();
void real_readtrk();
void real_writetrk();
int real_check_empty(DiskState *d);

void
trs_disk_setsize(int unit, int value)
{
  if (unit < 0 || unit > 7) return;
  disk[unit].inches = (value == 8) ? 8 : 5;
}

void
trs_disk_setstep(int unit, int value)
{
  if (unit < 0 || unit > 7) return;
  disk[unit].real_step = (value == 2) ? 2 : 1;
}

int
trs_disk_getsize(int unit)
{
  if (unit < 0 || unit > 7) return 0;
  return disk[unit].inches;
}

int
trs_disk_getstep(int unit)
{
  if (unit < 0 || unit > 7) return 0;
  return disk[unit].real_step;
}

void
trs_disk_init(int reset_button)
{
  int i;

  state.status = TRSDISK_NOTRDY|TRSDISK_TRKZERO;
  state.track = 0;
  state.sector = 0;
  state.data = 0;
  state.currcommand = TRSDISK_RESTORE;
  state.lastdirection = 1;
  state.bytecount = 0;
  state.format = FMT_DONE;
  state.format_bytecount = 0;
  state.format_sec = 0;
  state.curdrive = state.curside = 0;
  state.density = 0;
  state.controller = (trs_model == 1) ? TRSDISK_P1771 : TRSDISK_P1791;
  state.last_readadr = -1;
  state.motor_timeout = 0;
  if (!reset_button) {
    for (i=0; i<NDRIVES; i++) {
      disk[i].phytrack = 0;
      disk[i].emutype = NONE;
    }
  }
  trs_disk_change_all();
  trs_cancel_event();

  trs_disk_nocontroller = (disk[0].file == NULL);
}

void
trs_disk_change_all()
{
  int i;
  for (i=0; i<NDRIVES; i++) {
    trs_disk_change(i);
  }
  trs_disk_changecount++;
}


static void
trs_disk_firstdrq(int restoredelay)
{
  if (restoredelay > 0) {
    z80_state.delay = restoredelay;
  }
  state.status |= TRSDISK_DRQ;
  trs_disk_drq_interrupt(1);
}

static void
trs_disk_done(int bits)
{
  state.status &= ~TRSDISK_BUSY;
  state.status |= bits;
  trs_disk_intrq_interrupt(1);
}


static void
trs_disk_unimpl(unsigned char cmd, char* more)
{
  char msg[2048];
  state.status = TRSDISK_NOTRDY|TRSDISK_WRITEFLT|TRSDISK_NOTFOUND;
  state.bytecount = state.format_bytecount = 0;
  state.format = FMT_DONE;
  trs_disk_drq_interrupt(0);
  trs_schedule_event(trs_disk_done, 0, 0);
  sprintf(msg, "trs_disk_command(0x%02x) not implemented - %s",
	  cmd, more);
  error(msg);
}

/* Sort first by track, second by side, third by position in emulated-disk
   sector array (i.e., physical sector order on track).
*/
static int
id_compare(const void* p1, const void* p2)
{
  DiskState *d = &disk[state.curdrive];
  int i1 = *(short*)p1;
  int i2 = *(short*)p2;
  int r = d->id[i1].track - d->id[i2].track;
  if (r != 0) return r;
  r = (d->id[i1].flags & JV3_SIDE) - (d->id[i2].flags & JV3_SIDE);
  if (r != 0) return r;
  return i1 - i2;
}

/* (Re-)create the sorted_id data structure for the given drive */
void
sort_ids(int drive)
{
  DiskState *d = &disk[drive];
  int olddrive = state.curdrive;
  int i, track, side;

  for (i=0; i<=JV3_SECSMAX; i++) {
    d->sorted_id[i] = i;
  }
  state.curdrive = drive;
  qsort((void*) d->sorted_id, JV3_SECSMAX, sizeof(short), id_compare);
  state.curdrive = olddrive;

  for (track=0; track<MAXTRACKS; track++) {
    d->track_start[track][0] = -1;
    d->track_start[track][1] = -1;
  }    
  track = side = -1;
  for (i=0; i<JV3_SECSMAX; i++) {
    SectorId *sid = &d->id[d->sorted_id[i]];
    if (sid->track != track ||
	(sid->flags & JV3_SIDE ? 1 : 0) != side) {
      track = sid->track;
      if (track == JV3_FREE) break;
      side = sid->flags & JV3_SIDE ? 1 : 0;
      d->track_start[track][side] = i;
    }
  }

  d->sorted_valid = 1;  
}

int
id_index_to_size_code(DiskState *d, int id_index)
{
  return (d->id[id_index].flags & JV3_SIZE) ^
    ((d->id[id_index].track == JV3_FREE) ? 2 : 1);
}

int
size_code_to_size(int code)
{
  return 128 << code;
}

int
id_index_to_size(DiskState *d, int id_index)
{
  return 128 << id_index_to_size_code(d, id_index);
}

/* Return the offset of the data block for the id_index'th sector
   in an emulated-disk file. */
static off_t
offset(DiskState *d, int id_index)
{
  if (d->emutype == JV1) {
    return id_index * JV1_SECSIZE;
  } else {
    return d->offset[id_index];
  }
}

/* Return the offset of the id block for the id_index'th sector
   in an emulated-disk file.  Initialize a new block if needed. */
static off_t
idoffset(DiskState *d, int id_index)
{
  if (d->emutype == JV1) {
    trs_disk_unimpl(state.currcommand, "JV1 id (internal error)");
    return -1;
  } else {
    if (id_index < JV3_SECSPERBLK) {
      return JV3_IDSTART + id_index * sizeof(SectorId);
    } else {
      int idstart2 = d->offset[JV3_SECSPERBLK-1] +
	id_index_to_size(d, JV3_SECSPERBLK-1);

      if (d->nblocks == 1) {
        /* Initialize new block of ids */
	int c;
	fseek(d->file, idstart2, 0);
        c = fwrite((void*)&d->id[JV3_SECSPERBLK], JV3_SECSTART, 1, d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	c = fflush(d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	d->nblocks = 2;
      }	
      return idstart2 + (id_index - JV3_SECSPERBLK) * sizeof(SectorId);
    }
  }
}

int
alloc_sector(DiskState *d, int size_code)
{
  int maybe = d->free_id[size_code];
  d->sorted_valid = 0;
  while (maybe <= d->last_used_id) {
    if (d->id[maybe].track == JV3_FREE &&
	id_index_to_size_code(d, maybe) == size_code) {
      d->free_id[size_code] = maybe + 1;
      return maybe;
    }
    maybe++;
  }
  d->free_id[size_code] = JV3_SECSMAX; /* none are free */
  if (d->last_used_id >= JV3_SECSMAX-1) {
    return -1;
  }
  d->last_used_id++;
  d->offset[d->last_used_id + 1] =
    d->offset[d->last_used_id] + size_code_to_size(size_code);
  if (d->last_used_id + 1 == JV3_SECSPERBLK) {
      d->offset[d->last_used_id + 1] += JV3_SECSTART;
  }
  return d->last_used_id;
}

void
free_sector(DiskState *d, int id_index)
{
  int c;
  int size_code = (d->id[id_index].flags & JV3_SIZE) ^ 1;
  if (d->free_id[size_code] > id_index) d->free_id[size_code] = id_index;
  d->sorted_valid = 0;
  d->id[id_index].track = JV3_FREE;
  d->id[id_index].sector = JV3_FREE;
  d->id[id_index].flags = (d->id[id_index].flags | JV3_FREEF) ^ JV3_SIZE;
  fseek(d->file, idoffset(d, id_index), 0);
  c = fwrite(&d->id[id_index], sizeof(SectorId), 1, d->file);
  if (c == EOF) state.status |= TRSDISK_WRITEFLT;

  if (id_index == d->last_used_id) {
    int newlen;
    while (d->id[d->last_used_id].track == JV3_FREE) {
      d->last_used_id--;
    }
    fflush(d->file);
    rewind(d->file);
    if (d->last_used_id >= 0) {
      newlen = offset(d, d->last_used_id) +
	id_index_to_size(d, d->last_used_id);
    } else {
      newlen = offset(d, 0);
    }
    ftruncate(fileno(d->file), newlen);
  }
}

void
trs_disk_change(int drive)
{
  char diskname[1024];
  DiskState *d = &disk[drive];  
  struct stat st;
  int c, res;

  if (d->file != NULL) {
    c = fclose(d->file);
    if (c == EOF) state.status |= TRSDISK_WRITEFLT;
  }
  if (trs_model == 5) {
    sprintf(diskname, "%s/disk4p-%d", trs_disk_dir, drive);
  } else {
    sprintf(diskname, "%s/disk%d-%d", trs_disk_dir, trs_model, drive);
  }
  res = stat(diskname, &st);
  if (res == -1) {
    d->file = NULL;
    return;
  }
#if __linux
  if (S_ISBLK(st.st_mode)) {
    /* Real floppy drive */
    int fd;
    int reset_now = 0;
    struct floppy_drive_params fdp;
    fd = open(diskname, O_ACCMODE|O_NDELAY);
    if (fd == -1) {
      perror(diskname);
      d->file = NULL;
      d->emutype = JV3;
      return;
    }
    d->file = fdopen(fd, "r+");
    if (d->file == NULL) {
      perror(diskname);
      d->emutype = JV3;
      return;
    }
    d->writeprot = 0;
    ioctl(fileno(d->file), FDRESET, &reset_now);
    ioctl(fileno(d->file), FDGETDRVPRM, &fdp);
    d->real_rps = fdp.rps;
    d->real_size_code = 1; /* initial guess: 256 bytes */
    d->empty_timeout = 0;
    if (d->emutype != REAL) {
      d->emutype = REAL;
      d->phytrack = 0;
      real_restore(drive);
    }
  } else
#endif
  {
    d->file = fopen(diskname, "r+");
    if (d->file == NULL) {
      d->file = fopen(diskname, "r");
      if (d->file == NULL) return;
      d->writeprot = 1;
    } else {
      d->writeprot = 0;
    }
    /* Heuristic to decide what file format we have */
    if (st.st_size < JV3_SECSTART) {
      d->emutype = JV1;
    } else {
      fseek(d->file, 0, 0);
      if (getc(d->file) == 0 && getc(d->file) == 0xfe) {
	d->emutype = JV1;
      } else {
	int c;
	d->emutype = JV3;
	fseek(d->file, JV3_SECSPERBLK*sizeof(SectorId), 0);
	c = getc(d->file);
	switch (c) {
	case 0xff:
	  /* Normal */
	  break;
	case 0x00:
	  /* Write-protected internally (new feature JV is planning) */
	  d->writeprot = 1;  /* !!Hmm, no way to unprotect */
	  break;
	default:
	  d->emutype = JV1;
	  break;
	}
      }
    }
  }

  if (d->emutype == JV3) {
    int id_index, n;
    int ofst;

    memset((void*)d->id, JV3_FREE, sizeof(d->id));

    /* Read first block of ids */
    fseek(d->file, JV3_IDSTART, 0);
    fread((void*)&d->id[0], 3, JV3_SECSPERBLK, d->file);

    /* Scan to find their offsets */
    ofst = JV3_SECSTART;
    for (id_index=0; id_index<JV3_SECSPERBLK; id_index++) {
      d->offset[id_index] = ofst;
      ofst += id_index_to_size(d, id_index);
    }

    /* Read second block of ids, if any */
    fseek(d->file, ofst, 0);
    n = fread((void*)&d->id[JV3_SECSPERBLK], 3, JV3_SECSPERBLK, d->file);
    d->nblocks = n > 0 ? 2 : 1;

    /* Scan to find their offsets */
    ofst += JV3_SECSTART;
    for (id_index=JV3_SECSPERBLK; id_index<JV3_SECSPERBLK*2; id_index++) {
      d->offset[id_index] = ofst;
      ofst += id_index_to_size(d, id_index);
    }

    /* Find last_used_id value and free_id hints */
    for (n=0; n<4; n++) {
      d->free_id[n] = JV3_SECSMAX;
    }
    d->last_used_id = -1;
    for (id_index=0; id_index<JV3_SECSMAX; id_index++) {
      if (d->id[id_index].track == JV3_FREE) {
	int size_code = id_index_to_size_code(d, id_index);
	if (d->free_id[size_code] == JV3_SECSMAX) {
	  d->free_id[size_code] = id_index;
	}
      } else {
	d->last_used_id = id_index;
      }
    }
    sort_ids(drive);
  }
}

static int
cmd_type(unsigned char cmd)
{
  switch (cmd & TRSDISK_CMDMASK) {
  case TRSDISK_RESTORE:
  case TRSDISK_SEEK:
  case TRSDISK_STEP:
  case TRSDISK_STEPU:
  case TRSDISK_STEPIN:
  case TRSDISK_STEPINU:
  case TRSDISK_STEPOUT:
  case TRSDISK_STEPOUTU:
    return 1;
  case TRSDISK_READ:
  case TRSDISK_READM:
  case TRSDISK_WRITE:
  case TRSDISK_WRITEM:
    return 2;
  case TRSDISK_READADR:
  case TRSDISK_READTRK:
  case TRSDISK_WRITETRK:
    return 3;
  case TRSDISK_FORCEINT:
    return 4;
  }
  return -1;			/* not reached */
}


/* Search for a sector on the current physical track and return its
   index within the emulated disk's array of sectors.  Set status and
   return -1 if there is no such sector.  If sector == -1, 
   the first sector found if any. */
static int
search(int sector)
{
  DiskState *d = &disk[state.curdrive];
  if (d->file == NULL) {
    state.status |= TRSDISK_NOTFOUND;
    return -1;
  }
  if (d->emutype == JV1) { 
    if (d->phytrack < 0 || d->phytrack >= MAXTRACKS ||
	state.curside > 0 || sector >= JV1_SECPERTRK || d->file == NULL ||
	d->phytrack != state.track || state.density == 1) {
      state.status |= TRSDISK_NOTFOUND;
      return -1;
    }
    return JV1_SECPERTRK * d->phytrack + (sector < 0 ? 0 : sector);
  } else {
    int i;
    SectorId *sid;
    if (d->phytrack < 0 || d->phytrack >= MAXTRACKS ||
	state.curside >= JV3_SIDES ||
	d->phytrack != state.track || d->file == NULL) {
      state.status |= TRSDISK_NOTFOUND;
      return -1;
    }
    if (!d->sorted_valid) sort_ids(state.curdrive);
    i = d->track_start[d->phytrack][state.curside];
    if (i != -1) {
      for (;;) {
	sid = &d->id[d->sorted_id[i]];
	if (sid->track != d->phytrack ||
	    (sid->flags & JV3_SIDE ? 1 : 0) != state.curside) break;
	if ((sector == -1 || sid->sector == sector) &&
	    ((sid->flags & JV3_DENSITY) ? 1 : 0) == state.density) {
	  return d->sorted_id[i];
	}
	i++;
      }
    }
    state.status |= TRSDISK_NOTFOUND;
    return -1;
  }
}


/* Search for the first sector on the current physical track (in
   either density) and return its index within the sorted index array
   (JV3), or index within the sector array (JV1).  Return -1 if there
   is no such sector, or if reading JV1 in double density.  Don't set
   TRSDISK_NOTFOUND; leave the caller to do that. */
static int
search_adr()
{
  DiskState *d = &disk[state.curdrive];
  if (d->file == NULL) {
    return -1;
  }
  if (d->emutype == JV1) { 
    if (d->phytrack < 0 || d->phytrack >= MAXTRACKS ||
	state.curside > 0 || d->file == NULL || state.density == 1) {
      return -1;
    }
    return JV1_SECPERTRK * d->phytrack;
  } else {
    if (d->phytrack < 0 || d->phytrack >= MAXTRACKS ||
	state.curside >= JV3_SIDES || d->file == NULL) {
      return -1;
    }
    if (!d->sorted_valid) sort_ids(state.curdrive);
    return d->track_start[d->phytrack][state.curside];
  }
}


void
verify()
{
  /* Verify that head is on the expected track */
  DiskState *d = &disk[state.curdrive];
  if (d->emutype == REAL) {
    real_verify();
  } else if (d->emutype == JV1) {
    if (d->file == NULL) {
      state.status |= TRSDISK_NOTFOUND;
    } if (state.density == 1) {
      state.status |= TRSDISK_NOTFOUND;
    } else if (state.track != d->phytrack) {
      state.status |= TRSDISK_SEEKERR;
    }
  } else {
    search(-1);	/* TRSDISK_SEEKERR == TRSDISK_NOTFOUND */
  }
}

/* Return a value in [0,1) indicating how far we've rotated
 * from the leading edge of the index hole */
float
angle()
{
  DiskState *d = &disk[state.curdrive];
  float a;
  /* Set revus to number of microseconds per revolution */
  int revus = d->inches == 5 ? 200000 /* 300 RPM */ : 166666 /* 360 RPM */;
#if TSTATEREV
  /* Lock revolution rate to emulated time measured in T-states */
  /* Minor bug: there will be a glitch when t_count wraps around on
     a 32-bit machine */
  int revt = (int)(revus * z80_state.clockMHz);
  a = ((float)(z80_state.t_count % revt)) / ((float)revt);
#else
  /* Old way: lock revolution rate to real time */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  /* Ignore the seconds field; this is OK if there are a round number
     of revolutions per second */
  a = ((float)(tv.tv_usec % revus)) / ((float)revus);
#endif
  return a;
}

static void
type1_status()
{
  DiskState *d = &disk[state.curdrive];

  switch (cmd_type(state.currcommand)) {
  case 1:
  case 4:
    break;
  default:
    return;
  }

  if (d->file == NULL || (d->emutype == REAL && d->real_empty)) {
    state.status |= TRSDISK_INDEX;
  } else {
    if (angle() < trs_disk_holewidth) {
      state.status |= TRSDISK_INDEX;
    } else {
      state.status &= ~TRSDISK_INDEX;
    }
    if (d->writeprot) {
      state.status |= TRSDISK_WRITEPRT;
    } else {
      state.status &= ~TRSDISK_WRITEPRT;
    }
  }
  if (d->phytrack == 0) {
    state.status |= TRSDISK_TRKZERO;
  } else {
    state.status &= ~TRSDISK_TRKZERO;
  }
}

void
trs_disk_select_write(unsigned char data)
{
#ifdef DISKDEBUG
  static int old_data = -1;
  if (data != old_data) {
    fprintf(stderr, "select_write(0x%02x)\n", data);
    old_data = data;
  }
#endif
  state.status &= ~TRSDISK_NOTRDY;
  if (trs_model == 1) {
    /* Disk 3 and side select share a bit.  You can't have a drive :3
       on a real Model I if any drive is two-sided.  Here we are more
       generous and just forbid drive :3 from being 2-sided. */
    state.curside = ( (data & (TRSDISK_0|TRSDISK_1|TRSDISK_2)) != 0 &&
		      (data & TRSDISK_SIDE) != 0 );
    if (state.curside) data &= ~TRSDISK_SIDE;

  } else {
    state.curside = (data & TRSDISK3_SIDE) != 0;
    state.density = (data & TRSDISK3_MFM) != 0;
    if (data & TRSDISK3_WAIT) {
	/* If there was an event pending, deliver it right now */
	trs_do_event();
    }
  }
  switch (data & (TRSDISK_0|TRSDISK_1|TRSDISK_2|TRSDISK_3)) {
  case 0:
    state.status |= TRSDISK_NOTRDY;
    break;
  case TRSDISK_0:
    state.curdrive = 0;
    break;
  case TRSDISK_1:
    state.curdrive = 1;
    break;
  case TRSDISK_2:
    state.curdrive = 2;
    break;
  case TRSDISK_3:
    state.curdrive = 3;
    break;
  case TRSDISK_4:
    /* fake value for emulator only */
    state.curdrive = 4;
    break;
  case TRSDISK_5:
    /* fake value for emulator only */
    state.curdrive = 5;
    break;
  case TRSDISK_6:
    /* fake value for emulator only */
    state.curdrive = 6;
    break;
  case TRSDISK_7:
    /* fake value for emulator only */
    state.curdrive = 7;
    break;
  default:
    trs_disk_unimpl(data, "bogus drive select");
    state.status |= TRSDISK_NOTRDY;
    break;
  }
  if (!(state.status & TRSDISK_NOTRDY)) {
    DiskState *d = &disk[state.curdrive];
    /* A drive was selected; retrigger emulated motor timeout */
    state.motor_timeout = z80_state.t_count +
      MOTOR_USEC * z80_state.clockMHz;
    /* Also update our knowledge of whether there is a disk present */
    if (d->emutype == REAL) real_check_empty(d);
  }
}

unsigned char
trs_disk_track_read(void)
{
#ifdef DISKDEBUG
  fprintf(stderr, "track_read() => 0x%02x\n", state.track);
#endif
  return state.track;
}

void
trs_disk_track_write(unsigned char data)
{
#ifdef DISKDEBUG
  fprintf(stderr, "track_write(0x%02x)\n", data);
#endif
  state.track = data;
}

unsigned char
trs_disk_sector_read(void)
{
#ifdef DISKDEBUG
  fprintf(stderr, "sector_read() => 0x%02x\n", state.sector);
#endif
  return state.sector;
}

void
trs_disk_set_controller(int controller)
{
  /* Support for more accurate Doubler emulation */
  FDCState tmp_state;
  if (state.controller == controller) return;
  tmp_state.status = state.status;
  tmp_state.track = state.track;
  tmp_state.sector = state.sector;
  tmp_state.data = state.data;
  tmp_state.lastdirection = state.lastdirection;
  state.controller = controller;
  state.status = other_state.status;
  state.track = other_state.track;
  state.sector = other_state.sector;
  state.data = other_state.data;
  state.lastdirection = other_state.lastdirection;
  other_state.status = tmp_state.status;
  other_state.track = tmp_state.track;
  other_state.sector = tmp_state.sector;
  other_state.data = tmp_state.data;
  other_state.lastdirection = tmp_state.lastdirection;
}

void
trs_disk_sector_write(unsigned char data)
{
#ifdef DISKDEBUG
  fprintf(stderr, "sector_write(0x%02x)\n", data);
#endif
  if (trs_model == 1 && (trs_disk_doubler & TRSDISK_TANDY)) {
    switch (data) {
      /* Emulate Radio Shack doubler */
    case TRSDISK_R1791:
      trs_disk_set_controller(TRSDISK_P1791);
      state.density = 1;
      break;
    case TRSDISK_R1771:
      trs_disk_set_controller(TRSDISK_P1771);
      state.density = 0;
      break;
    case TRSDISK_NOPRECMP:
    case TRSDISK_PRECMP:
      /* Nothing for emulator to do */
      break;
    default:
      state.sector = data;
      break;
    }
  } else {
    state.sector = data;
  }
}

unsigned char
trs_disk_data_read(void)
{
  DiskState *d = &disk[state.curdrive];
  SectorId *sid;
  switch (state.currcommand & TRSDISK_CMDMASK) {
  case TRSDISK_READ:
    if (state.bytecount > 0) {
      int c;
      if (d->emutype == REAL) { 
	c = d->buf[size_code_to_size(d->real_size_code) - state.bytecount];
      } else {
	c = getc(disk[state.curdrive].file);
	if (c == EOF) {
	  /*state.status |= TRSDISK_NOTFOUND;*/
	  c = 0xe5;
	  if (d->emutype == JV1) {
	    state.status &= ~TRSDISK_RECTYPE;
	    state.status |= (state.controller == TRSDISK_P1771) ?
	      TRSDISK_1771_FB : TRSDISK_1791_FB;
	  }
	}
      }
      state.data = c;
      state.bytecount--;
      if (state.bytecount <= 0) {
	state.bytecount = 0;
	state.status &= ~TRSDISK_DRQ;
        trs_disk_drq_interrupt(0);
	trs_schedule_event(trs_disk_done, 0, 32);
      }
    }
    break;
  case TRSDISK_READADR:
    if (state.bytecount <= 0) break;
    if (d->emutype == REAL) {
      state.sector = d->phytrack; /*sic*/
      state.data = d->buf[6 - state.bytecount];

    } else if (state.last_readadr >= 0) {
      if (d->emutype == JV1) {
	switch (state.bytecount) {
	case 6:
	  state.data = d->phytrack;
	  state.sector = d->phytrack; /*sic*/
	  break;
	case 5:
	  state.data = 0;
	  break;
	case 4:
	  state.data = jv1_interleave[state.last_readadr % JV1_SECPERTRK];
	  break;
	case 3:
	  state.data = 0x01;  /* 256 bytes always */
	  break;
	case 2:
	case 1:
	  state.data = 0;     /* CRC not emulated */
	  break;
	}
      } else {
	sid = &d->id[d->sorted_id[state.last_readadr]];
	switch (state.bytecount) {
	case 6:
	  state.data = sid->track;
	  state.sector = sid->track;  /*sic*/
	  break;
	case 5:
	  state.data = (sid->flags & JV3_SIDE) != 0;
	  break;
	case 4:
	  state.data = sid->sector;
	  break;
	case 3:
	  state.data =
	    id_index_to_size_code(d, d->sorted_id[state.last_readadr]);
	  break;
	case 2:
	case 1:
	  state.data = 0;     /* CRC not emulated */
	  break;
	}
      }
    }
    state.bytecount--;
    if (state.bytecount <= 0) {
      state.bytecount = 0;
      state.status &= ~TRSDISK_DRQ;
      trs_disk_drq_interrupt(0);
      trs_schedule_event(trs_disk_done, 0, 32);
    }
    break;
  default:
    break;
  }
#ifdef DISKDEBUG
  fprintf(stderr, "data_read() => 0x%02x\n", state.data);
#endif
  return state.data;
}

void
trs_disk_data_write(unsigned char data)
{
  DiskState *d = &disk[state.curdrive];
  int c;

#ifdef DISKDEBUG
  fprintf(stderr, "data_write(0x%02x)\n", data);
#endif
  switch (state.currcommand & TRSDISK_CMDMASK) {
  case TRSDISK_WRITE:
    if (state.bytecount > 0) {
      if (d->emutype == REAL) {
	d->buf[size_code_to_size(d->real_size_code) - state.bytecount] = data;
	state.bytecount--;
	if (state.bytecount <= 0) {
	  real_write();
	}
	break;
      }
      c = putc(data, d->file);
      if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      state.bytecount--;
      if (state.bytecount <= 0) {
	state.bytecount = 0;
	state.status &= ~TRSDISK_DRQ;
        trs_disk_drq_interrupt(0);
	trs_schedule_event(trs_disk_done, 0, 32);
	c = fflush(d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      }
    }
    break;
  case TRSDISK_WRITETRK:
    state.bytecount = state.bytecount - 2 + state.density;
    if (state.bytecount <= 0) {
      if (state.format == FMT_DONE) break;
      if (state.format == FMT_GAP2) {
	/* False ID: there was no DAM for following data */
	if (d->emutype != JV3) {
	  trs_disk_unimpl(state.currcommand, "false sector ID (no data)");
	} else {
	  /* We do not have a flag for this; try using CRC error */
	  d->id[state.format_sec].flags |= JV3_ERROR;
	  error("warning: recording false sector ID as CRC error");

	  /* Write the sector id */
	  fseek(d->file, idoffset(d, state.format_sec), 0);
	  c = fwrite(&d->id[state.format_sec], sizeof(SectorId), 1, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	}
      } else if (state.format != FMT_GAP3) {
	/* If not in FMT_GAP3 state, format data was either too long,
	   had extra garbage following, or was intentionally non-
	   standard.  SuperUtility does a few tricks like "software
	   bulk erase" and duplication of protected disks, so we
	   do not complain about this any more. */
#if BOGUS
	error("format data end is not in gap4");
#endif
	state.format_gap[4] = 0;
      } else {
	/* This was really GAP4 */
	state.format_gap[4] = state.format_gapcnt;
	state.format_gapcnt = 0;
      }
#if DISKDEBUG3
      fprintf(stderr,
	      "trk %d side %d gap0 %d gap1 %d gap2 %d gap3 %d gap4 %d\n",
	      d->phytrack, state.curside,
	      state.format_gap[0], state.format_gap[1], state.format_gap[2], 
	      state.format_gap[3], state.format_gap[4]);
#endif
      state.format = FMT_DONE;
      state.status &= ~TRSDISK_DRQ;
      if (d->emutype == REAL) {
	real_writetrk();
      } else {
	c = fflush(d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      }
      trs_disk_drq_interrupt(0);
      trs_schedule_event(trs_disk_done, 0, 32);
    }
    switch (state.format) {
    case FMT_GAP0:
      if (data == 0xfc) {
	state.format = FMT_GAP1;
	state.format_gap[0] = state.format_gapcnt;
	state.format_gapcnt = 0;
      } else if (data == 0xfe) {
	/* There wasn't a gap 0; we were really in gap 1 */
	state.format_gap[0] = 0;
	goto got_idam;
      } else {
	state.format_gapcnt++;
      }
      break;
    case FMT_GAP1:
      if (data == 0xfe) {
      got_idam:
	/* We've received the first ID address mark */
	state.format_gap[1] = state.format_gapcnt;
	state.format_gapcnt = 0;
	state.format = FMT_TRACKID;
      } else {
	state.format_gapcnt++;
      }
      break;
    case FMT_GAP3:
      if (data == 0xfe) {
      got_idam2:
	/* We've received an ID address mark */
	state.format_gap[3] = state.format_gapcnt;
	state.format_gapcnt = 0;
	state.format = FMT_TRACKID;
      } else {
	state.format_gapcnt++;
      }
      break;
    case FMT_TRACKID:
      if (d->emutype == REAL) {
	if (d->realfmt_nbytes >= sizeof(d->buf)) {
	  /* Data structure full */
	  state.status |= TRSDISK_WRITEFLT;
	  state.bytecount = 0;
	  state.format_bytecount = 0;
	  state.format = FMT_DONE;
	} else {
	  d->buf[d->realfmt_nbytes++] = data;
	  state.format = FMT_HEADID;
	}
      } else {
	if (data != d->phytrack) {
	  trs_disk_unimpl(state.currcommand, "false track number");
	}
	state.format = FMT_HEADID;
      }
      break;
    case FMT_HEADID:
      if (d->emutype == REAL) {
	d->buf[d->realfmt_nbytes++] = data;
      } else if (d->emutype == JV1) {
	if (data != 0) {
	  trs_disk_unimpl(state.currcommand, "JV1 double sided");
	}
	if (state.density) { 
	  trs_disk_unimpl(state.currcommand, "JV1 double density");
	}
      } else {
	if (data != state.curside) {
	  trs_disk_unimpl(state.currcommand, "false head number");
	}
      }
      state.format = FMT_SECID;
      break;
    case FMT_SECID:
      if (d->emutype == REAL) {
	d->buf[d->realfmt_nbytes++] = data;
      } else if (d->emutype == JV1) {
	if (data >= JV1_SECPERTRK) {
	  trs_disk_unimpl(state.currcommand, "JV1 sector number >= 10");
	}
      } else {
	state.format_sec = data;
      }
      state.format = FMT_SIZEID;
      break;
    case FMT_SIZEID:
      if (data > 0x03) {
	trs_disk_unimpl(state.currcommand, "invalid sector size");
      }
      if (d->emutype == JV3) {
	int id_index;
	id_index = alloc_sector(d, data);
	if (id_index == -1) {
	  /* Data structure full */
	  state.status |= TRSDISK_WRITEFLT;
	  state.bytecount = 0;
	  state.format_bytecount = 0;
	  state.format = FMT_DONE;
	  break;
	}
	d->sorted_valid = 0;
	d->id[id_index].track = d->phytrack;
	d->id[id_index].sector = state.format_sec;
	d->id[id_index].flags =
	  (state.curside ? JV3_SIDE : 0) | (state.density ? JV3_DENSITY : 0) |
	  ((data & 3) ^ 1);
	state.format_sec = id_index;

      } else if (d->emutype == REAL) {
	d->buf[d->realfmt_nbytes++] = data;
	if (d->real_size_code != -1 && d->real_size_code != data) {
	  trs_disk_unimpl(state.currcommand,
			  "varying sector size on same track on real floppy");
	}
	d->real_size_code = data;
      } else {
	if (data != 0x01) {
	  trs_disk_unimpl(state.currcommand, "sector size != 256");
	}
      }
      state.format = FMT_GAP2;
      break;
    case FMT_GAP2:
      if ((data & 0xfc) == 0xf8) {
	/* Found a DAM */
        if (d->emutype == REAL) {
	  switch (data) {
	  case 0xfb:  /* Standard DAM */
	    break;
	  case 0xfa:
	    if (state.density) {
	      /* This DAM is illegal, but SuperUtility uses it, so
		 ignore the error.  This seems to be a bug in SU for
		 Model I, where it meant to use F8 instead.  I think
		 the WD controller would read back the FA as FB, so we
		 treat it as FB here.  */
	    } else {
	      if (trs_disk_truedam) {
		error("format DAM FA on real floppy");
	      }
	    }
	    break;
	  case 0xf9:
	    if (trs_disk_truedam) {
	      error("format DAM F9 on real floppy");
	    }
	    break;
	  case 0xf8:
	    /* This is probably needed by Model III TRSDOS, but it is
	       a pain to implement.  We would have to remember to do a
	       Write Deleted after the format is complete to change
	       the DAM.
	    */
	    error("format DAM F8 on real floppy");
	    break;
	  }
	} else if (d->emutype == JV1) {
          switch (data) {
	  case 0xf9:
	    trs_disk_unimpl(state.currcommand, "JV1 DAM cannot be F9");
	    break;
	  case 0xf8:
	  case 0xfa:
	    if (d->phytrack != 17) 
	      trs_disk_unimpl(state.currcommand,
			      "JV1 directory track must be 17");
	    break;
	  default: /* impossible */
	  case 0xfb:
	    break;
	  }
	} else /* JV3 */ {
	  if (state.density) {
	    /* Double density */
	    switch (data) {
	    case 0xf8: /* Standard deleted DAM */
	    case 0xf9: /* Illegal, probably never used; ignore error. */
	      d->id[state.format_sec].flags |= JV3_DAMDDF8;
	      break;
	    case 0xfb: /* Standard DAM */
	    case 0xfa: /* Illegal, but SuperUtility uses it! */
	    default:   /* Impossible */
	      d->id[state.format_sec].flags |= JV3_DAMDDFB;
	      break;
	    }
	  } else {
	    /* Single density */
	    switch (data) {
	    case 0xf8:
	      if (trs_disk_truedam) {
		d->id[state.format_sec].flags |= JV3_DAMSDF8;
	      } else {
		d->id[state.format_sec].flags |= JV3_DAMSDFA;
	      }
	      break;
	    case 0xf9:
	      d->id[state.format_sec].flags |= JV3_DAMSDF9;
	      break;
	    case 0xfa:
	      d->id[state.format_sec].flags |= JV3_DAMSDFA;
	      break;
	    default: /* impossible */
	    case 0xfb:
	      d->id[state.format_sec].flags |= JV3_DAMDDFB;
	      break;
	    }
	  }
	}
	if (d->emutype == JV3) {
	  /* Prepare to write the data */
	  fseek(d->file, offset(d, state.format_sec), 0);
	  state.format_bytecount = id_index_to_size(d, state.format_sec);
	} else if (d->emutype == JV1) {
	  state.format_bytecount = JV1_SECSIZE;
	} else if (d->emutype == REAL) {
	  state.format_bytecount = size_code_to_size(d->real_size_code);
	}
	state.format_gap[2] = state.format_gapcnt;
	state.format_gapcnt = 0;
	state.format = FMT_DATA;
      } else if (data == 0xfe) {
	/* False ID: there was no DAM for following data */
	if (d->emutype != JV3) {
	  trs_disk_unimpl(state.currcommand, "false sector ID (no data)");
	} else {
	  /* We do not have a flag for this; try using CRC error */
	  error("warning: recording false sector ID as CRC error");
	  d->id[state.format_sec].flags |= JV3_ERROR;

	  /* Write the sector id */
	  fseek(d->file, idoffset(d, state.format_sec), 0);
	  c = fwrite(&d->id[state.format_sec], sizeof(SectorId), 1, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	}
	goto got_idam2;
      } else {
	state.format_gapcnt++;
      }
      break;
    case FMT_DATA:
      if (data == 0xfe) {
	/* Short sector with intentional CRC error */
	if (d->emutype == JV3) {
	  d->id[state.format_sec].flags |= JV3_NONIBM | JV3_ERROR;
#if DISKDEBUG2
	  fprintf(stderr,
		  "non-IBM sector: drv %02x, sid %d, trk %02x, sec %02x\n",
		  state.curdrive, state.curside,
		  d->id[state.format_sec].track, 
		  d->id[state.format_sec].sector);
#endif
	  /* Write the sector id */
	  fseek(d->file, idoffset(d, state.format_sec), 0);
	  c = fwrite(&d->id[state.format_sec], sizeof(SectorId), 1, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  goto got_idam;
	} else {
	  trs_disk_unimpl(state.currcommand, "JV1 non-IBM sector");
	}
      }
      if (d->emutype == JV3) {
	c = putc(data, d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      } else if (d->emutype == REAL) {
	d->realfmt_fill = data;
      }
      if (--state.format_bytecount <= 0) {
	state.format = FMT_DCRC;
      }
      break;
    case FMT_DCRC:
      if (data == 0xf7) {
	state.bytecount--;  /* two bytes are written */
      } else {
	/* Intentional CRC error */
	if (d->emutype != JV3) {
	  trs_disk_unimpl(state.currcommand, "intentional CRC error");
	} else {
	  d->id[state.format_sec].flags |= JV3_ERROR;
	}
      }
      if (d->emutype == JV3) {
	/* Write the sector id */
	fseek(d->file, idoffset(d, state.format_sec), 0);
	c = fwrite(&d->id[state.format_sec], sizeof(SectorId), 1, d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      }
      state.format = FMT_GAP3;
      break;
    case FMT_DONE:
      break;
    case FMT_GAP4:
    case FMT_IAM:
    case FMT_IDAM:
    case FMT_IDCRC:
    case FMT_DAM:
    default:
      error("error in format state machine");
      break;
    }      
  default:
    break;
  }
  state.data = data;
  return;
}

unsigned char
trs_disk_status_read(void)
{
#ifdef DISKDEBUG
  static int last_status = -1;
#endif
  if (trs_disk_nocontroller) return 0xff;
  type1_status();
  if (!(state.status & TRSDISK_NOTRDY)) {
    if (state.motor_timeout - z80_state.t_count > TSTATE_T_MID) {
      /* Subtraction wrapped; motor stopped */
      state.status |= TRSDISK_NOTRDY;
    }
  }
#ifdef DISKDEBUG
  if (state.status != last_status) {
    fprintf(stderr, "status_read() => 0x%02x\n", state.status);
    last_status = state.status;
  }
#endif

#if BOGUS
  /* Clear intrq unless user did a Force Interrupt with immediate interrupt. */
  /* The 17xx data sheets say this is how it is supposed to work, but it
   * makes Model I SuperUtility hang due to the interrupt routine failing
   * to clear the interrupt.
   */
  if (!(((state.currcommand & TRSDISK_CMDMASK) == TRSDISK_FORCEINT) &&
	((state.currcommand & 0x08) != 0)))
#else
  /* Clear intrq always */
#endif
    {
      /* Don't call trs_schedule_event, which could cancel a pending
       *  interrupt that should occur later and prevent it from ever
       *  happening; just clear the interrupt right now.
       */
      trs_disk_intrq_interrupt(0);
    }

  return state.status;
}

void
trs_disk_command_write(unsigned char cmd)
{
  int id_index, non_ibm;
  DiskState *d = &disk[state.curdrive];

#ifdef DISKDEBUG
  fprintf(stderr, "command_write(0x%02x)\n", cmd);
#endif
  /* Cancel any ongoing command */
  trs_schedule_event(trs_disk_intrq_interrupt, 0, 0);
  state.bytecount = 0;
  state.currcommand = cmd;
  switch (cmd & TRSDISK_CMDMASK) {
  case TRSDISK_RESTORE:
    state.last_readadr = -1;
    d->phytrack = 0;
    state.track = 0;
    state.status = TRSDISK_TRKZERO|TRSDISK_BUSY;
    if (d->emutype == REAL) real_restore(state.curdrive);
    /* Should this set lastdirection? */
    if (cmd & TRSDISK_VBIT) verify();
    trs_schedule_event(trs_disk_done, 0, 512);
    break;
  case TRSDISK_SEEK:
    state.last_readadr = -1;
    d->phytrack += (state.data - state.track);
    state.track = state.data;
    if (d->phytrack <= 0) {
      d->phytrack = 0;		/* state.track too? */
      state.status = TRSDISK_TRKZERO|TRSDISK_BUSY;
    } else {
      state.status = TRSDISK_BUSY;
    }
    if (d->emutype == REAL) real_seek();
    /* Should this set lastdirection? */
    if (cmd & TRSDISK_VBIT) verify();
    trs_schedule_event(trs_disk_done, 0, 256);
    break;
  case TRSDISK_STEP:
  case TRSDISK_STEPU:
  step:
    state.last_readadr = -1;
    d->phytrack += state.lastdirection;
    if (cmd & TRSDISK_UBIT) {
      state.track += state.lastdirection;
    }
    if (d->phytrack <= 0) {
      d->phytrack = 0;		/* state.track too? */
      state.status = TRSDISK_TRKZERO|TRSDISK_BUSY;
    } else {
      state.status = TRSDISK_BUSY;
    }
    if (d->emutype == REAL) real_seek();
    if (cmd & TRSDISK_VBIT) verify();
    trs_schedule_event(trs_disk_done, 0, 64);
    break;
  case TRSDISK_STEPIN:
  case TRSDISK_STEPINU:
    state.lastdirection = 1;
    goto step;
  case TRSDISK_STEPOUT:
  case TRSDISK_STEPOUTU:
    state.lastdirection = -1;
    goto step;
  case TRSDISK_READ:
    state.last_readadr = -1;
    state.status = 0;
    non_ibm = 0;
    if (state.controller == TRSDISK_P1771) {
      if (!(cmd & TRSDISK_BBIT)) {
#if DISKDEBUG2
	fprintf(stderr, 
		"non-IBM read: drv %02x, sid %d, trk %02x, sec %02x\n",
		state.curdrive, state.curside, state.track, state.sector);
#endif
	if (d->emutype == REAL) {
	  trs_disk_unimpl(cmd, "non-IBM read on real floppy");
	}
	non_ibm = 1;
      } else {
#if DISKDEBUG2
	if (state.sector >= 0x7c) {
	  fprintf(stderr, 
		  "IBM read: drv %02x, sid %d, trk %02x, sec %02x\n",
		  state.curdrive, state.curside, state.track, state.sector);
	}
#endif
      }
    }
    if (d->emutype == REAL) {
      real_read();
      break;
    }
    id_index = search(state.sector);
    if (id_index == -1) {
      state.status |= TRSDISK_BUSY;
      trs_schedule_event(trs_disk_done, 0, 512);
    } else {
      if (d->emutype == JV1) {
	if (d->phytrack == 17) {
	  if (state.controller == TRSDISK_P1771) {
	    state.status |= TRSDISK_1771_FA;
	  } else {
	    state.status |= TRSDISK_1791_F8;
	  }
	}
	state.bytecount = JV1_SECSIZE;
      } else {
	if (state.controller == TRSDISK_P1771) {
	  switch (d->id[id_index].flags & JV3_DAM) {
	  case JV3_DAMSDFB:
	    state.status |= TRSDISK_1771_FB;
	    break;
	  case JV3_DAMSDFA:
	    state.status |= TRSDISK_1771_FA;
	    break;
	  case JV3_DAMSDF9:
	    state.status |= TRSDISK_1771_F9;
	    break;
	  case JV3_DAMSDF8:
	    state.status |= TRSDISK_1771_F8;
	    break;
	  }
	} else if ((d->id[id_index].flags & JV3_DENSITY) == 0) {
	  /* single density 179x */
	  switch (d->id[id_index].flags & JV3_DAM) {
	  case JV3_DAMSDFB:
	    state.status |= TRSDISK_1791_FB;
	    break;
	  case JV3_DAMSDFA:
	    if (trs_disk_truedam) {
	      state.status |= TRSDISK_1791_FB;
	    } else {
	      state.status |= TRSDISK_1791_F8;
	    }
	    break;
	  case JV3_DAMSDF9:
	    state.status |= TRSDISK_1791_F8;
	    break;
	  case JV3_DAMSDF8:
	    state.status |= TRSDISK_1791_F8;
	    break;
	  }
	} else {
	  /* double density 179x */
	  switch (d->id[id_index].flags & JV3_DAM) {
	  default: /*impossible*/
	  case JV3_DAMDDFB:
	    state.status |= TRSDISK_1791_FB;
	    break;
	  case JV3_DAMDDF8:
	    state.status |= TRSDISK_1791_F8;
	    break;
	  }
	}
	if (d->id[id_index].flags & JV3_ERROR) {
	  state.status |= TRSDISK_CRCERR;
	}
	if (non_ibm) {
	  state.bytecount = 16;
	} else {
	  state.bytecount = id_index_to_size(d, id_index);
	}
      }
      state.status |= TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      fseek(d->file, offset(d, id_index), 0);
    }
    break;
  case TRSDISK_READM:
    state.last_readadr = -1;
    trs_disk_unimpl(cmd, "read multiple");
    break;
  case TRSDISK_WRITE:
    state.last_readadr = -1;
    state.status = 0;
    non_ibm = 0;
    if (state.controller == TRSDISK_P1771) {
      if (!(cmd & TRSDISK_BBIT)) {
#if DISKDEBUG2
	fprintf(stderr,
		"non-IBM write drv %02x, sid %d, trk %02x, sec %02x\n",
		state.curdrive, state.curside, state.track, state.sector);
#endif
	if (d->emutype == REAL) {
	  trs_disk_unimpl(cmd, "non-IBM write on real floppy");
	}
	non_ibm = 1;
      } else {
#if DISKDEBUG2
	if (state.sector >= 0x7c) {
	  fprintf(stderr, 
		  "IBM write: drv %02x, sid %d, trk %02x, sec %02x\n",
		  state.curdrive, state.curside, state.track, state.sector);
	}
#endif
      }
    }
    if (d->emutype == REAL) {
      state.status = TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      state.bytecount = size_code_to_size(d->real_size_code);
      break;
    }
    if (d->writeprot) {
      state.status = TRSDISK_WRITEPRT;
      break;
    }
    id_index = search(state.sector);
    if (id_index == -1) {
      state.status |= TRSDISK_BUSY;
      trs_schedule_event(trs_disk_done, 0, 512);
    } else {
      int dam = 0;
      if (state.controller == TRSDISK_P1771) {
	switch (cmd & (TRSDISK_CBIT|TRSDISK_DBIT)) {
	case 0:
	  dam = JV3_DAMSDFB;
	  break;
	case 1:
	  dam = JV3_DAMSDFA;
	  break;
	case 2:
	  dam = JV3_DAMSDF9;
	  break;
	case 3:
	  if (trs_disk_truedam) {
	    dam = JV3_DAMSDF8;
	  } else {
	    dam = JV3_DAMSDFA;
	  }
	  break;
	}
      } else if (state.density == 0) {
	/* 179x single */
	switch (cmd & TRSDISK_DBIT) {
	case 0:
	  dam = JV3_DAMSDFB;
	  break;
	case 1:
	  if (trs_disk_truedam) {
	    dam = JV3_DAMSDF8;
	  } else {
	    dam = JV3_DAMSDFA;
	  }
	  break;
	}
      } else {
	/* 179x double */
	switch (cmd & TRSDISK_DBIT) {
	case 0:
	  dam = JV3_DAMDDFB;
	  break;
	case 1:
	  dam = JV3_DAMDDF8;
	  break;
	}
      }

      if (d->emutype == JV1) {
	if (dam == JV3_DAMSDF9) {
	  trs_disk_unimpl(state.currcommand, "JV1 DAM cannot be F9");
	} else if ((dam == JV3_DAMSDFB) == (d->phytrack == 17)) {
	  trs_disk_unimpl(state.currcommand, "JV1 directory must be track 17");
	  break;
	}
	state.bytecount = JV1_SECSIZE;
      } else {
	SectorId *sid = &d->id[id_index];
	unsigned char newflags = sid->flags;
	newflags &= ~(JV3_ERROR|JV3_DAM); /* clear CRC error and DAM */
	newflags |= dam;
	if (newflags != sid->flags) {
	  int c;
	  fseek(d->file, idoffset(d, id_index) 
		         + ((char *) &sid->flags) - ((char *) sid), 0);
	  c = putc(newflags, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  c = fflush(d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  sid->flags = newflags;
	}

	/* Kludge for VTOS 3.0 */
	if (sid->flags & JV3_NONIBM) {
	  int i, j, c;
	  /* Smash following sectors. This is especially a kludge because
	     it uses the sector numbers, not the known physical sector
	     order. */
	  for (i = state.sector+1; i <= 0x7f; i++) {
	    j = search(i);
	    if (j != -1) {
#if DISKDEBUG2
	      fprintf(stderr, "smashing sector %02x\n", i);
#endif
	      free_sector(d, idoffset(d, j));
	      c = fflush(d->file);
	      if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	    }	      
	    /* Smash only one for non-IBM write */
	    if (non_ibm) break;
	  }
	}
	/* end kludge */

      }
      state.status |= TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      if (non_ibm) {
	state.bytecount = 16;
      } else {
	state.bytecount = id_index_to_size(d, id_index);
      }
      fseek(d->file, offset(d, id_index), 0);
    }
    break;
  case TRSDISK_WRITEM:
    state.last_readadr = -1;
    if (d->writeprot) {
      state.status = TRSDISK_WRITEPRT;
      break;
    }
    trs_disk_unimpl(cmd, "write multiple");
    break;
  case TRSDISK_READADR:
    if (d->emutype == REAL) {
      real_readadr();
      break;
    } else {
      int totbyt, i, ts, denok;
      float a, b, bytlen;
      id_index = search_adr();
      if (id_index == -1) {
	state.status = TRSDISK_BUSY;
	trs_schedule_event(trs_disk_done, TRSDISK_NOTFOUND,
			   200000*z80_state.clockMHz);
	break;
      }
      /* Compute how long it should have taken for this sector to come
	 by and delay by an appropriate number of t-states.  This
	 makes the "A" command in HyperZap work (on emulated floppies
	 only).  It is not terribly useful, since other important HyperZap
	 functions (like mixed-density formatting) do not work, while
	 SuperUtility and Trakcess both work fine without the delay feature.
	 Note: it would probably be better to assume the sectors are
	 positioned using nominal gap sizes (say, the ones that HyperZap
	 uses when generating tracks using the D/G subcommand) instead
	 of the even spacing nonsense below.
      */
      if (d->emutype == JV1) {
	/* Which sector header is next?  Use a rough assumption
	   that the sectors are all the same angular length (bytlen).
	*/
	a = angle();
	bytlen = (1.0 - GAP1ANGLE - GAP4ANGLE)/((float)JV1_SECPERTRK);
	i = (int)( (a - GAP1ANGLE) / bytlen + 1.0 );
	if (i >= JV1_SECPERTRK) {
	  /* Wrap around to start of track */
	  i = 0;
	}
	b = ((float)i) * bytlen + GAP1ANGLE;
	if (b < a) b += 1.0;
	i += id_index;
      } else {
	/* Count data bytes on track.  Also check if there
	   are any sectors of the correct density. */
	i = id_index;
	totbyt = 0;
	denok = 0;
	for (;;) {
	  SectorId *sid = &d->id[d->sorted_id[i]];
	  int dden = (sid->flags & JV3_DENSITY) != 0;
	  if (sid->track != d->phytrack ||
	      (sid->flags & JV3_SIDE ? 1 : 0) != state.curside) break;
	  totbyt += (dden ? 1 : 2) * id_index_to_size(d, d->sorted_id[i]);
	  if (dden == state.density) denok = 1;
	  i++;
	}
	if (!denok) {
	  /* No sectors of the correct density */
	  state.status = TRSDISK_BUSY;
	  trs_schedule_event(trs_disk_done, TRSDISK_NOTFOUND,
			     200000*z80_state.clockMHz);
	  break;
	}
	/* Which sector header is next?  Use a rough assumption that
           sectors are evenly spaced, taking up room proportional to
           their data length (and twice as much for single density).
	   Here bytlen = angular size per byte.
	*/
	a = angle();
	b = GAP1ANGLE;
	bytlen = (1.0 - GAP1ANGLE - GAP4ANGLE)/((float)totbyt);
	i = id_index;
	for (;;) {
	  SectorId *sid = &d->id[d->sorted_id[i]];
	  if (sid->track != d->phytrack ||
	      (sid->flags & JV3_SIDE ? 1 : 0) != state.curside) {
	    /* Wrap around to start of track */
	    i = id_index;
	    b = 1 + GAP1ANGLE;
	    break;
	  }
	  if (b > a && (((sid->flags & JV3_DENSITY) != 0) == state.density)) {
	    break;
	  }
	  b += ((sid->flags & JV3_DENSITY) ? 1 : 2) *
  	    id_index_to_size(d, d->sorted_id[i]) * bytlen;
	  i++;
	}
      }
      /* Convert angular delay to t-states */
      ts = (d->inches == 5 ? 200000 : 166667) * (b - a) * z80_state.clockMHz;

      state.status = TRSDISK_BUSY;
      state.last_readadr = i;
      state.bytecount = 6;
      trs_schedule_event(trs_disk_firstdrq, -1, ts);
#if DISKDEBUG5
      fprintf(stderr, "readadr phytrack %d angle %f i %d ts %d\n",
	      d->phytrack, a, i, ts);
#endif
    }
    break;
  case TRSDISK_READTRK:
    state.last_readadr = -1;
    if (d->emutype == REAL) {
      real_readtrk();
      break;
    }
    trs_disk_unimpl(cmd, "read track");
    break;
  case TRSDISK_WRITETRK:
    state.last_readadr = -1;
    /* Really a write track? */
    if (trs_model == 1 && (cmd == TRSDISK_P1771 || cmd == TRSDISK_P1791)) {
      /* No; emulate Percom Doubler */
      if (trs_disk_doubler & TRSDISK_PERCOM) {
	trs_disk_set_controller(cmd);
	/* The Doubler's 1791 is hardwired to double density */
	state.density = (state.controller == TRSDISK_P1791);
      }
    } else {
      /* Yes; a real write track */
      if (d->emutype != REAL && d->writeprot) {
	state.status = TRSDISK_WRITEPRT;
	break;
      }
      if (d->file == NULL) {
	/* How is this error indicated?  Did the controller just hang? */
	state.status |= (TRSDISK_BUSY|TRSDISK_WRITEFLT);
	trs_schedule_event(trs_disk_done, 0, 512);
	break;
      }
      if (d->emutype == JV3) {
	/* Erase track if already formatted */
	int i;
	for (i=0; i<=d->last_used_id; i++) {
	  if (d->id[i].track == d->phytrack &&
	      ((d->id[i].flags & JV3_SIDE) != 0) == state.curside) {
	    free_sector(d, i);
	  }
	}
      } else if (d->emutype == REAL) {
	d->real_size_code = -1; /* watch for first, then check others match */
	d->realfmt_nbytes = 0; /* size of PC formatting command buffer */
      }
      state.status = TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      state.format = FMT_GAP0;
      state.format_gapcnt = 0;
      if (disk[state.curdrive].inches == 5) {
	state.bytecount = TRKSIZE_DD;  /* decrement by 2's if SD */
      } else {
	state.bytecount = TRKSIZE_8DD; /* decrement by 2's if SD */
      }
    }
    break;
  case TRSDISK_FORCEINT:
    trs_do_event(); /* finish whatever is going on */
    state.status = 0;  /* and forget it */
    type1_status();
    if ((cmd & 0x07) != 0) {
      /* Conditional interrupt features not implemented. */
      trs_disk_unimpl(cmd, "force interrupt with condition");
    } else if ((cmd & 0x08) != 0) {
      /* Immediate interrupt */
      trs_disk_intrq_interrupt(1);
    } else {
      trs_disk_intrq_interrupt(0);
    }
    break;
  }
}

/* Interface to real floppy drive */
int
real_rate(DiskState *d)
{
  if (d->inches == 5) {
    if (d->real_rps == 5) {
      return 2;
    } else if (d->real_rps == 6) {
      return 1;
    }
  } else if (d->inches == 8) {
    return 0;
  }
  trs_disk_unimpl(state.currcommand, "real_rate internal error");
  return 1;
}

void
real_error(DiskState *d, unsigned int flags, char *msg, unsigned char status)
{
  time_t now = time(NULL);
  if (now > d->empty_timeout) {
    d->empty_timeout = time(NULL) + EMPTY_TIMEOUT;
    d->real_empty = 1;
  }
  state.status |= status;
  /*fprintf(stderr, "error on real_%s\n", msg);*/
}

void
real_ok(DiskState *d)
{
  d->empty_timeout = time(NULL) + EMPTY_TIMEOUT;
  d->real_empty = 0;
}

int
real_check_empty(DiskState *d)
{
#if __linux
  int reset_now = 0;
  struct floppy_raw_cmd raw_cmd;
  int res, i = 0;
  sigset_t set, oldset;

  if (time(NULL) <= d->empty_timeout) return d->real_empty;
  
  ioctl(fileno(d->file), FDRESET, &reset_now);

  /* Do a read id command.  Assume a disk is in the drive iff
     we get a nonnegative status back from the ioctl. */
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_INTR;
  raw_cmd.cmd[i++] = state.density ? 0x4a : 0x0a; /* read ID */
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd_count = i;
  raw_cmd.data = NULL;
  raw_cmd.length = 0;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);
  trs_paused = 1;
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    real_error(d, raw_cmd.flags, "check_empty", 0);
  } else {
    real_ok(d);
  }
#else
  trs_disk_unimpl(state.currcommand, "check for empty on real floppy");
#endif
  return d->real_empty;
}

void
real_verify()
{
  /* Verify that head is on the expected track */
  /*!! ignore for now*/
}

void
real_restore(curdrive)
{
#if __linux
  DiskState *d = &disk[curdrive];
  struct floppy_raw_cmd raw_cmd;
  int res, i = 0;
  sigset_t set, oldset;

  raw_cmd.flags = FD_RAW_INTR;
  raw_cmd.cmd[i++] = FD_RECALIBRATE;
  raw_cmd.cmd[i++] = 0;
  raw_cmd.cmd_count = i;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);
  trs_paused = 1;
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    real_error(d, raw_cmd.flags, "restore", TRSDISK_SEEKERR);
    return;
  }
#else
  trs_disk_unimpl(state.currcommand, "restore real floppy");
#endif
}

void
real_seek()
{
#if __linux
  DiskState *d = &disk[state.curdrive];
  struct floppy_raw_cmd raw_cmd;
  int res, i = 0;
  sigset_t set, oldset;

  /* Always use a recal if going to track 0.  This should help us
     recover from confusion about what track the disk is really on.
     I'm still not sure why the confusion sometimes arises. */
  if (d->phytrack == 0) {
    real_restore(state.curdrive);
    return;
  }

  state.last_readadr = -1;
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.length = 256;
  raw_cmd.data = NULL;
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_INTR;
  raw_cmd.cmd[i++] = FD_SEEK;
  raw_cmd.cmd[i++] = 0;
  raw_cmd.cmd[i++] = d->phytrack * d->real_step;
  raw_cmd.cmd_count = i;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);
  trs_paused = 1;
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    real_error(d, raw_cmd.flags, "seek", TRSDISK_SEEKERR);
    return;
  }
#else
  trs_disk_unimpl(state.currcommand, "seek real floppy");
#endif
}

void
real_read()
{
#if __linux
  DiskState *d = &disk[state.curdrive];
  struct floppy_raw_cmd raw_cmd;
  int res, i, retry;
  sigset_t set, oldset;

  /* Try once at each supported sector size */
  retry = 0;
  for (;;) {
    state.status = 0;
    memset(&raw_cmd, 0, sizeof(raw_cmd));
    raw_cmd.rate = real_rate(d);
    raw_cmd.flags = FD_RAW_READ | FD_RAW_INTR;
    i = 0;
    raw_cmd.cmd[i++] = state.density ? 0x46 : 0x06;
    raw_cmd.cmd[i++] = state.curside ? 4 : 0;
    raw_cmd.cmd[i++] = state.track;
    raw_cmd.cmd[i++] = state.curside;
    raw_cmd.cmd[i++] = state.sector;
    raw_cmd.cmd[i++] = d->real_size_code;
    raw_cmd.cmd[i++] = 255;
    raw_cmd.cmd[i++] = 0x0a;
    raw_cmd.cmd[i++] = 0xff; /* unused */
    raw_cmd.cmd_count = i;
    raw_cmd.data = (void*) d->buf;
    raw_cmd.length = 128 << d->real_size_code;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGIO);
    sigprocmask(SIG_BLOCK, &set, &oldset);
    trs_paused = 1;
    res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
    sigprocmask(SIG_SETMASK, &oldset, NULL);
    if (res < 0) {
      real_error(d, raw_cmd.flags, "read", TRSDISK_NOTFOUND);
    } else {
      real_ok(d); /* premature? */
      if (raw_cmd.reply[1] & 0x04) {
	/* Could have been due to wrong sector size, so we'll retry
	   internally in each other size before returning an error. */
#if DISKDEBUG4
	fprintf(stderr,
		"real_read not fnd: side %d tk %d sec %d size 0%d phytk %d\n",
		state.curside, state.track, state.sector, d->real_size_code,
		d->phytrack*d->real_step);
#endif
#if SIZERETRY
	d->real_size_code = (d->real_size_code + 1) % 4;
	if (++retry < 4) {
	  continue; /* retry */
	}
#endif
	state.status |= TRSDISK_NOTFOUND;
      }
      if (raw_cmd.reply[1] & 0x81) state.status |= TRSDISK_NOTFOUND;
      if (raw_cmd.reply[1] & 0x20) {
	state.status |= TRSDISK_CRCERR;
	if (!(raw_cmd.reply[2] & 0x20)) state.status |= TRSDISK_NOTFOUND;
      }
      if (raw_cmd.reply[1] & 0x10) state.status |= TRSDISK_LOSTDATA;
      if (raw_cmd.reply[2] & 0x40) {
	if (state.controller == TRSDISK_P1771) {
	  if (trs_disk_truedam) {
	    state.status |= TRSDISK_1771_F8;
	  } else {
	    state.status |= TRSDISK_1771_FA;
	  }
	} else {
	  state.status |= TRSDISK_1791_F8;
	}
      }
      if (raw_cmd.reply[2] & 0x20) state.status |= TRSDISK_CRCERR;
      if (raw_cmd.reply[2] & 0x13) state.status |= TRSDISK_NOTFOUND;
      if ((state.status & TRSDISK_NOTFOUND) == 0) {
	/* Start read */
	state.status |= TRSDISK_BUSY|TRSDISK_DRQ;
	trs_disk_drq_interrupt(1);
	state.bytecount = size_code_to_size(d->real_size_code);
	return;
      }
    }
    break; /* exit retry loop */
  }
  /* Sector not found; fail */
  state.status |= TRSDISK_BUSY;
  trs_schedule_event(trs_disk_done, 0, 512);
#else
  trs_disk_unimpl(state.currcommand, "read real floppy");
#endif
}

void
real_write()
{
#if __linux
  DiskState *d = &disk[state.curdrive];
  struct floppy_raw_cmd raw_cmd;
  int res, i = 0;
  sigset_t set, oldset;

  state.status = 0;
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR;
  if (trs_disk_truedam && !state.density) {
    switch (state.currcommand & 0x03) {
    case 0:
    case 3:
      break;
    case 1:
      error("writing FA DAM on real floppy");
      break;
    case 2:
      error("writing F9 DAM on real floppy");
      break;
    }
  }
  /* Use F8 DAM for F8, F9, or FA */
  raw_cmd.cmd[i++] = ((state.currcommand & 
		       (state.controller == TRSDISK_P1771 ? 0x03 : 0x01))
		      ? 0x09 : 0x05) | (state.density ? 0x40 : 0x00);
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd[i++] = state.track;
  raw_cmd.cmd[i++] = state.curside;
  raw_cmd.cmd[i++] = state.sector;
  raw_cmd.cmd[i++] = d->real_size_code;
  raw_cmd.cmd[i++] = 255;
  raw_cmd.cmd[i++] = 0x0a;
  raw_cmd.cmd[i++] = 0xff; /* 256 */
  raw_cmd.cmd_count = i;
  raw_cmd.data = (void*) d->buf;
  raw_cmd.length = 128 << d->real_size_code;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);
  trs_paused = 1;
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    real_error(d, raw_cmd.flags, "write", TRSDISK_NOTFOUND);
  } else {
    real_ok(d); /* premature? */
    if (raw_cmd.reply[1] & 0x04) {
      state.status |= TRSDISK_NOTFOUND;
      /* Could have been due to wrong sector size.  Presumably
         the Z-80 software will do some retries, so we'll cause
         it to try the next sector size next time. */
#if DISKDEBUG4
      fprintf(stderr,
	      "real_write not found: side %d tk %d sec %d size 0%d phytk %d\n",
	      state.curside, state.track, state.sector, d->real_size_code,
	      d->phytrack*d->real_step);
#endif
#if SIZERETRY
      d->real_size_code = (d->real_size_code + 1) % 4;
#endif
    }
    if (raw_cmd.reply[1] & 0x81) state.status |= TRSDISK_NOTFOUND;
    if (raw_cmd.reply[1] & 0x20) {
      state.status |= TRSDISK_CRCERR;
      if (!(raw_cmd.reply[2] & 0x20)) state.status |= TRSDISK_NOTFOUND;
    }
    if (raw_cmd.reply[1] & 0x10) state.status |= TRSDISK_LOSTDATA;
    if (raw_cmd.reply[1] & 0x02) {
      state.status |= TRSDISK_WRITEPRT;
      d->writeprot = 1;
    } else {
      d->writeprot = 0;
    }
    if (raw_cmd.reply[2] & 0x20) state.status |= TRSDISK_CRCERR;
    if (raw_cmd.reply[2] & 0x13) state.status |= TRSDISK_NOTFOUND;
  }
  state.bytecount = 0;
  trs_disk_drq_interrupt(0);
  state.status |= TRSDISK_BUSY;
  trs_schedule_event(trs_disk_done, 0, 512);
#else
  trs_disk_unimpl(state.currcommand, "write real floppy");
#endif
}

void
real_readadr()
{
#if __linux
  DiskState *d = &disk[state.curdrive];
  struct floppy_raw_cmd raw_cmd;
  int res, i = 0;
  sigset_t set, oldset;
#if REALRADLY
  static struct timeval last_tv = {0, 0};
  struct timeval tv;
#endif

  state.status = 0;
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_INTR;
  raw_cmd.cmd[i++] = state.density ? 0x4a : 0x0a;
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd_count = i;
  raw_cmd.data = NULL;
  raw_cmd.length = 0;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);
  trs_paused = 1;
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
#if REALRADLY
  gettimeofday(&tv, NULL);
#endif
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    real_error(d, raw_cmd.flags, "readadr", TRSDISK_NOTFOUND);
  } else {
    real_ok(d); /* premature? */
    if (raw_cmd.reply[1] & 0x85) state.status |= TRSDISK_NOTFOUND;
    if (raw_cmd.reply[1] & 0x20) state.status |= TRSDISK_CRCERR;
    if (raw_cmd.reply[1] & 0x10) state.status |= TRSDISK_LOSTDATA;
    if (raw_cmd.reply[2] & 0x40) {
      if (state.controller == TRSDISK_P1771) {
        state.status |= TRSDISK_1771_FA;
      } else {
        state.status |= TRSDISK_1791_F8;
      }
    }
    if (raw_cmd.reply[2] & 0x20) state.status |= TRSDISK_CRCERR;
    if (raw_cmd.reply[2] & 0x13) state.status |= TRSDISK_NOTFOUND;
    if ((state.status & TRSDISK_NOTFOUND) == 0) {
#if REALRADLY
      /* Emulate spacing between sectors by delaying response an
	 appropriate number of t-states, in an attempt to make
	 HyperZap's A command work.  Problems: (1) On slow PCs, this
	 can make us actually miss the next sector ID.  (2) We are not
	 sync'd with the physical index hole, so results are not
	 repeatable.  (3) Code is kludgy; only a witch's brew mixture
	 of real time measurements and nominal times based on sector
	 length works even partially.  (4) Other HyperZap features,
	 like mixed density formatting, still do not work.
      */
      int ts, savedelay;
      float us;

      /* Did two read_adrs in a row? */
      if (last_tv.tv_sec > 0 &&
	  state.last_readadr == (d->phytrack | (state.curdrive << 8))) {
	/* Yes; emulate the waiting time for this sector to come around */
	float realus = ((tv.tv_usec - last_tv.tv_usec) +
	      1000000.0 * (tv.tv_sec - last_tv.tv_sec)) *
	  d->real_rps / (d->inches == 5 ? 5 : 6);
	float nomus = ((float)size_code_to_size(d->real_size_code)) /
	  ((d->inches ? TRKSIZE_DD : TRKSIZE_8DD) /
	   (state.density ? 1 : 2)) *
	  (1000000.0 / (d->inches == 5 ? 5 : 6));
	/* Does the measurement look reasonable? */
	if (realus > 0 && realus < nomus * 1.5) {
	  /* Seems that way.  Emulate the time it really took. */
	  us = realus;
	} else {
	  /* Apparently not.  Emulate approx time for last sector's
	     length instead of the actual time.  This can happen if
	     the previous sector was physically the last on the track
	     and we had an extra large gap, or it may also be possible
	     that the measurement came out as 0 because of coarse clock
	     granularity.
	  */
	  us = nomus;
	}
#if DISKDEBUG5
	/* Warning: the time to do this printout makes us miss sectors */
	fprintf(stderr, "realus %f nomus %f us %f\n", realus, nomus, us);
#endif
      } else {
	/* No, emulate being right after the index hole */
	us = GAP1ANGLE * (1000000.0 / (d->inches == 5 ? 5 : 6));
      }
      ts = us * z80_state.clockMHz;
#if DISKDEBUG5
      /* Warning: the time to do this printout makes us miss sectors */
      fprintf(stderr, "real_readadr phytrack %d id %d %d %d %d ts %d\n",
	      d->phytrack, raw_cmd.reply[3], raw_cmd.reply[4],
	      raw_cmd.reply[5], raw_cmd.reply[6], ts);
#endif
      savedelay = z80_state.delay;
      z80_state.delay = 0;  /* disable delay to avoid falling behind disk */
      trs_paused = 1; /* disable next autodelay measurement */
      state.status |= TRSDISK_BUSY;
      trs_schedule_event(trs_disk_firstdrq, savedelay, ts);
      state.last_readadr = (d->phytrack | (state.curdrive << 8));
      last_tv = tv;
#else /*!REALRADLY*/
      /* No delay */
      state.status |= TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
#endif
      memcpy(d->buf, &raw_cmd.reply[3], 4);
      d->buf[4] = d->buf[5] = 0; /* CRC not emulated */
      state.bytecount = 6;
      d->real_size_code = d->buf[3]; /* update hint */
      return;
    }
  }
  state.last_readadr = -1;
  /* don't set error flags until the end */
  trs_schedule_event(trs_disk_done, state.status, 200000*z80_state.clockMHz);
  state.status = TRSDISK_BUSY;
#else
  trs_disk_unimpl(state.currcommand, "read address on real floppy");
#endif
}

void
real_readtrk()
{
  trs_disk_unimpl(state.currcommand, "read track on real floppy");
}

void
real_writetrk()
{
#if __linux
  DiskState *d = &disk[state.curdrive];
  struct floppy_raw_cmd raw_cmd;
  int res, i, gap3;
  sigset_t set, oldset;
  state.status = 0;

  /* Compute a usable gap3 */
  /* Constants based on IBM format as explained in "The floppy user guide"
     by Michael Haardt, Alain Knaff, and David C. Niemi */
  /* The formulas and constants are not factored out, in case some of
     those that are the same now need to change when I learn more. */
  if (state.density) {
    /* MFM recording */
    if (d->inches == 5) {
      /* 5" DD = 250 kHz MFM */
      gap3 = (TRKSIZE_DD - 161 - /*slop*/16)/(d->realfmt_nbytes / 4)
	- 62 - (128 << d->real_size_code) - /*slop*/2;
    } else {
      /* 8" DD = 5" HD = 500 kHz MFM */
      gap3 = (TRKSIZE_8DD - 161 - /*slop*/16)/(d->realfmt_nbytes / 4)
	- 62 - (128 << d->real_size_code) - /*slop*/2;
    }
  } else {
    /* FM recording */
    if (d->inches == 5) {
      /* 5" SD = 250 kHz FM (125 kbps) */
      gap3 = (TRKSIZE_SD - 99 - /*slop*/16)/(d->realfmt_nbytes / 4)
	- 33 - (128 << d->real_size_code) - /*slop*/2;
    } else {
      /* 8" SD = 5" HD operated in FM = 500 kHz FM (250 kbps) */
      gap3 = (TRKSIZE_8SD - 99 - /*slop*/16)/(d->realfmt_nbytes / 4)
	- 33 - (128 << d->real_size_code) - /*slop*/2;
    }
  }
  if (gap3 < 1) {
    error("gap3 too small");
    gap3 = 1;
  } else if (gap3 > 0xff) {
    gap3 = 0xff;
  }

  /* Do the actual write track */
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR;
  i = 0;
  raw_cmd.cmd[i++] = 0x0d | (state.density ? 0x40 : 0x00);
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd[i++] = d->real_size_code;
  raw_cmd.cmd[i++] = d->realfmt_nbytes / 4;
  raw_cmd.cmd[i++] = gap3;
  raw_cmd.cmd[i++] = d->realfmt_fill;
  raw_cmd.cmd_count = i;
  raw_cmd.data = (void*) d->buf;
  raw_cmd.length = d->realfmt_nbytes;
#if DISKDEBUG3
  fprintf(stderr,
	  "real_writetrk size 0%d secs %d gap3 %d fill 0x%02x hex data ",
	  d->real_size_code, d->realfmt_nbytes/4, gap3, d->realfmt_fill);
  for (i=0; i<d->realfmt_nbytes; i+=4) {
    fprintf(stderr, "%02x%02x%02x%02x ",
	    d->buf[i], d->buf[i+1], d->buf[i+2], d->buf[i+3]);
  }
  fprintf(stderr, "\n");
#endif
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);
  trs_paused = 1;
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    real_error(d, raw_cmd.flags, "writetrk", TRSDISK_WRITEFLT);
  } else {
    real_ok(d); /* premature? */
    if (raw_cmd.reply[1] & 0x85) state.status |= TRSDISK_NOTFOUND;
    if (raw_cmd.reply[1] & 0x20) state.status |= TRSDISK_CRCERR;
    if (raw_cmd.reply[1] & 0x10) state.status |= TRSDISK_LOSTDATA;
    if (raw_cmd.reply[1] & 0x02) {
      state.status |= TRSDISK_WRITEPRT;
      d->writeprot = 1;
    } else {
      d->writeprot = 0;
    }
    if (raw_cmd.reply[2] & 0x20) state.status |= TRSDISK_CRCERR;
    if (raw_cmd.reply[2] & 0x13) state.status |= TRSDISK_NOTFOUND;
  }
  state.bytecount = 0;
  trs_disk_drq_interrupt(0);
  state.status |= TRSDISK_BUSY;
  trs_schedule_event(trs_disk_done, 0, 512);
#else
  trs_disk_unimpl(state.currcommand, "write track on real floppy");
#endif
}
