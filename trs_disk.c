/* Copyright (c) 1996-97, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Mon Oct 12 23:28:08 PDT 1998 by mann */

/*
 * Emulate Model I or III/4 disk controller
 */

/*#define DISKDEBUG 1*/
/*#define DISKDEBUG2 1*/
#define TSTATEREV 1

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
int trs_disk_doublestep = 0;
int trs_disk_doubler = TRSDISK_BOTH;
char *trs_disk_dir = DISKDIR;
unsigned short trs_disk_changecount = 0;
float trs_disk_holewidth = 0.01;

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
  unsigned curdrive;
  unsigned curside;
  unsigned density;		/* sden=0, dden=1 */
  unsigned char controller;	/* TRSDISK_P1771 or TRSDISK_P1791 */
  int last_readadr;             /* id index found by last readadr */
  int last_readadr_density;
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
#define JV3_LOCKED      0x40  /* doc says: high order DAM bit; really MBZ */
#define JV3_DIRECTORY   0x20  /* dden: 1=deleted DAM (F8); sden: 1=F8 or FA */
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
#define MAXTRACKS     96

#define REAL_SECSIZE 256  /* !!temporary limitation */
#define JV1_SECSIZE 256

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
#define REAL 0

typedef struct {
  int writeprot;		  /* emulated write protect tab */
  int phytrack;			  /* where are we really? */
  int emutype;
  int inches;                     /* 5 or 8, as seen by TRS-80 */
  int real_rps;                   /* used only for emutype REAL */
  FILE* file;
  int free_id[4];		  /* first free id, if any, of each size */
  int last_used_id;		  /* last used index */
  int nblocks;                    /* number of blocks of ids, 1 or 2 */
  int sorted_valid;               /* sorted_id array valid */
  SectorId id[JV3_SECSMAX + 1];   /* extra one is a loop sentinel */
  int offset[JV3_SECSMAX + 1];    /* offset into file for each id */
  short sorted_id[JV3_SECSMAX + 1];
  short track_start[MAXTRACKS][JV3_SIDES];
  unsigned char buf[REAL_SECSIZE];
} DiskState;

DiskState disk[NDRIVES];

/* Emulate interleave in JV1 mode */
unsigned char jv1_interleave[10] = {0, 5, 1, 6, 2, 7, 3, 8, 4, 9};

/* Forward */
void real_verify();
void real_restore();
void real_seek();
void real_read();
void real_write();
void real_readadr();
void real_readtrk();
void real_writetrk();

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
  state.last_readadr_density = 0;
  for (i=0; i<NDRIVES; i++) {
    disk[i].phytrack = 0;
    disk[i].emutype = JV3;
  }
  disk[0].inches = 5;
  disk[1].inches = 5;
  disk[2].inches = 5;
  disk[3].inches = 5;
  disk[4].inches = 8;
  disk[5].inches = 8;
  disk[6].inches = 8;
  disk[7].inches = 8;
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
trs_disk_done(int dummy)
{
    state.status &= ~TRSDISK_BUSY;
    trs_disk_intrq_interrupt(1);
}


static void
trs_disk_unimpl(unsigned char cmd, char* more)
{
  state.status = TRSDISK_WRITEFLT|TRSDISK_NOTFOUND;
  state.bytecount = state.format_bytecount = 0;
  state.format = FMT_DONE;
  trs_disk_drq_interrupt(0);
  trs_schedule_event(trs_disk_done, 0, 0);
  fprintf(stderr, "trs_disk_command(0x%02x) not implemented - %s\n",
	  cmd, more);
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
        c = fwrite((void*)&d->id[JV3_SECSPERBLK], 3, JV3_SECSPERBLK, d->file);
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
  return ++d->last_used_id;
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
    while (d->id[d->last_used_id].track == JV3_FREE) {
      d->last_used_id--;
    }
    fflush(d->file);
    rewind(d->file);
    ftruncate(fileno(d->file),
	      offset(d, d->last_used_id) +
	      id_index_to_size(d, d->last_used_id));
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
    d->emutype = REAL;
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


/* Search for the first sector on the current physical track and
   return its index within the sorted index array (JV3), or index
   within the sector array (JV1).  Return -1 if there is no such sector. */
static int
search_adr()
{
  DiskState *d = &disk[state.curdrive];
  if (d->emutype == JV1) { 
    if (d->phytrack < 0 || d->phytrack >= MAXTRACKS ||
	state.curside > 0 || d->file == NULL || state.density == 1) {
      state.status |= TRSDISK_NOTFOUND;
      return -1;
    }
    return JV1_SECPERTRK * d->phytrack;
  } else {
    int i;
    SectorId *sid;
    if (d->phytrack < 0 || d->phytrack >= MAXTRACKS ||
	state.curside >= JV3_SIDES || d->file == NULL) {
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
	if (((sid->flags & JV3_DENSITY) ? 1 : 0) == state.density) {
	  return i;
	}
	i++;
      }
    }
    state.status |= TRSDISK_NOTFOUND;
    return -1;
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
    if (state.density == 1) {
      state.status |= TRSDISK_NOTFOUND;
    } else if (state.track != d->phytrack) {
      state.status |= TRSDISK_SEEKERR;
    }
  } else {
    search(-1);	/* TRSDISK_SEEKERR == TRSDISK_NOTFOUND */
  }
}


static void
type1_status()
{
  DiskState *d = &disk[state.curdrive];

  if (cmd_type(state.currcommand) != 1) return;
  if (d->file == NULL) {
    state.status |= TRSDISK_INDEX;
  } else {
    /* Simulate index hole going by at 300, 360, or 3000 RPM */
    /* !! if (d->emutype == REAL) { check if disk in drive } */
    /* Set revus to number of microseconds per revolution */
    int revus = d->inches == 5 ? 200000 /* 300 RPM */ : 166666 /* 360 RPM */;
#if TSTATEREV
    /* Lock revolution rate to emulated time measured in T-states */
    /* Minor bug: there will be a glitch when t_count wraps around on
       a 32-bit machine */
    if ((z80_state.t_count % (int)(revus * z80_state.clockMHz))
	 < (int)(revus * z80_state.clockMHz * trs_disk_holewidth)) {
      state.status |= TRSDISK_INDEX;
    } else {
      state.status &= ~TRSDISK_INDEX;
    }
#else
    /* Old way: lock revolution rate to real time */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    /* Ignore the seconds field; this is OK if there are a round number
       of revolutions per second */
    if ((tv.tv_usec % revus) < (int)(revus * trs_disk_holewidth)) {
      state.status |= TRSDISK_INDEX;
    } else {
      state.status &= ~TRSDISK_INDEX;
    }
#endif
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
  fprintf(stderr, "select_write(0x%02x)\n", data);
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
    fprintf(stderr, "trs_disk: bogus drive select 0x%02x\n", data);
    break;
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
	c = d->buf[REAL_SECSIZE - state.bytecount];
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
	trs_schedule_event(trs_disk_done, 0, 8);
      }
    }
    break;
  case TRSDISK_READADR:
    if (state.bytecount <= 0) break;
    if (state.last_readadr >= 0) {
      if (d->emutype == REAL) {
	switch (state.bytecount) {
	case 6:
	  state.sector = d->phytrack; /*sic*/
	  /* fall through */
	case 5:
	case 4:
	case 3:
	  state.data = d->buf[6 - state.bytecount];
	  break;
	case 2:
	case 1:
	  state.data = 0;     /* CRC not emulated */
	  break;
	}
      } else if (d->emutype == JV1) {
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
      trs_schedule_event(trs_disk_done, 0, 8);
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
	d->buf[REAL_SECSIZE - state.bytecount] = data;
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
	trs_schedule_event(trs_disk_done, 0, 8);
	c = fflush(d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      }
    }
    break;
  case TRSDISK_WRITETRK:
    if (state.bytecount-- <= 0) state.format = FMT_GAP4;
    switch (state.format) {
    case FMT_GAP0:
      if (d->emutype == REAL) {
	d->nblocks = 0; /* (mis)use field to count sector header bytes */
      }
      if (data == 0xfc) {
	state.format = FMT_GAP1;
      } else if (data == 0xfe) {
	/* There wasn't a gap 0; we were really in gap 1 */
	goto got_idam;
      }
      break;
    case FMT_GAP1:
    case FMT_GAP3:
      if (data == 0xfe) {
      got_idam:
	/* We've received an ID address mark */
	state.format = FMT_TRACKID;
      }
      break;
    case FMT_TRACKID:
      if (d->emutype == REAL) {
	if (d->nblocks >= sizeof(d->buf)) {
	  /* Data structure full */
	  state.status |= TRSDISK_WRITEFLT;
	  state.bytecount = 0;
	  state.format_bytecount = 0;
	  state.format = FMT_DONE;
	} else {
	  d->buf[d->nblocks++] = data;
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
	d->buf[d->nblocks++] = data;
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
	d->buf[d->nblocks++] = data;
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
      if (d->emutype == JV3) {
	int id_index;
	if (data > 0x03) {
	  trs_disk_unimpl(state.currcommand, "invalid sector size");
	}
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

      } else {
	if (data != 0x01) {
	  trs_disk_unimpl(state.currcommand, "sector size != 256");
	}
	if (d->emutype == REAL) {
	  d->buf[d->nblocks++] = data;
	}
      }
      state.format = FMT_GAP2;
      break;
    case FMT_GAP2:
      if ((data & 0xfc) == 0xf8) {
	/* Found a DAM */
	if ((data & 3) != 3) {
          if (d->emutype == REAL) {
	    /* !!Could implement this by issuing a Write Deleted later */
	    trs_disk_unimpl(state.currcommand,
			    "format DAM != FB on real floppy");
	  } else {
	    /* Treat any non-FB DAM as a directory DAM */
	    if (d->emutype == JV1) {
	      if (d->phytrack != 17) 
		trs_disk_unimpl(state.currcommand,
				"JV1 directory track must be 17");
	    } else {
	      d->id[state.format_sec].flags |= JV3_DIRECTORY;
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
	  state.format_bytecount = REAL_SECSIZE;
	}
	state.format = FMT_DATA;
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
    case FMT_GAP4:
      state.format = FMT_DONE;
      state.status &= ~TRSDISK_DRQ;
      if (d->emutype == REAL) {
	real_writetrk();
      } else {
	c = fflush(d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      }
      trs_disk_drq_interrupt(0);
      trs_schedule_event(trs_disk_done, 0, 8);
      break;
    case FMT_DONE:
      break;
    case FMT_IAM:
    case FMT_IDAM:
    case FMT_IDCRC:
    case FMT_DAM:
    default:
      fprintf(stderr, "error in format state machine\n");
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
#ifdef DISKDEBUG
  if (state.status != last_status) {
    fprintf(stderr, "status_read() => 0x%02x\n", state.status);
    last_status = state.status;
  }
#endif
  /* Do this directly -- don't call trs_schedule_event, which would
   *  cancel a pending interrupt that should occur later and prevent
   *  it from ever happening.
   */
  trs_disk_intrq_interrupt(0);

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
    d->phytrack = 0;
    state.track = 0;
    state.status = TRSDISK_TRKZERO|TRSDISK_BUSY;
    if (d->emutype == REAL) real_restore();
    /* Should this set lastdirection? */
    if (cmd & TRSDISK_VBIT) verify();
    trs_schedule_event(trs_disk_done, 0, 128);
    break;
  case TRSDISK_SEEK:
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
    trs_schedule_event(trs_disk_done, 0, 64);
    break;
  case TRSDISK_STEP:
  case TRSDISK_STEPU:
  step:
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
    trs_schedule_event(trs_disk_done, 0, 16);
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
      trs_schedule_event(trs_disk_done, 0, 128);
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
	if (d->id[id_index].flags & JV3_DIRECTORY) {
	  if (state.controller == TRSDISK_P1771) {
	    state.status |= TRSDISK_1771_FA;
	  } else {
	    state.status |= TRSDISK_1791_F8;
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
    trs_disk_unimpl(cmd, "read multiple");
    break;
  case TRSDISK_WRITE:
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
      state.bytecount = REAL_SECSIZE;
      break;
    }
    if (d->writeprot) {
      state.status = TRSDISK_WRITEPRT;
      break;
    }
    id_index = search(state.sector);
    if (id_index == -1) {
      state.status |= TRSDISK_BUSY;
      trs_schedule_event(trs_disk_done, 0, 128);
    } else {
      int dirdam = (state.controller == TRSDISK_P1771 ?
		    (cmd & (TRSDISK_CBIT|TRSDISK_DBIT)) != 0 :
		    (cmd & TRSDISK_DBIT) != 0);
      if (d->emutype == JV1) {
	if (dirdam != (d->phytrack == 17)) {
	  trs_disk_unimpl(state.currcommand, "JV1 directory must be track 17");
	  break;
	}
	state.bytecount = JV1_SECSIZE;
      } else {
	SectorId *sid = &d->id[id_index];
	unsigned char newflags = sid->flags;
	newflags &= ~JV3_ERROR; /* clear CRC error */
	if (dirdam) {
	  newflags |= JV3_DIRECTORY;
	} else {
	  newflags &= ~JV3_DIRECTORY;
	}
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
    }
    state.status = 0;
    /* Note: not synchronized with emulated index hole */
    if (d->emutype == JV1) {
      if (state.last_readadr == -1 ||
	  (state.last_readadr / JV1_SECPERTRK) != d->phytrack ||
	  (state.last_readadr % JV1_SECPERTRK) == (JV1_SECPERTRK - 1) ||
	  state.last_readadr_density != state.density) {
	state.last_readadr = search_adr();
      } else {
	state.last_readadr++;
	if ((state.last_readadr % JV1_SECPERTRK) == 0) {
	  state.last_readadr -= JV1_SECPERTRK;
	}
      }
    } else {
      if (state.last_readadr == -1 ||
	  state.last_readadr_density != state.density) {
	/* Start new track */
	state.last_readadr = search_adr();
      } else {
	SectorId *sid = &d->id[d->sorted_id[state.last_readadr]];
	SectorId *sid2 = &d->id[d->sorted_id[state.last_readadr + 1]];
	if (sid->track != d->phytrack ||
  	    ((sid->flags & JV3_SIDE) != 0) != state.curside ||
	    sid2->track != d->phytrack ||
  	    ((sid2->flags & JV3_SIDE) != 0) != state.curside) {
	  /* Start new track */
	  state.last_readadr = search_adr();
        } else {
	  /* Give next sector on this track */
	  state.last_readadr = state.last_readadr + 1;
        }
      }
    }
    if (state.last_readadr == -1) {
      state.status |= TRSDISK_BUSY;
      trs_schedule_event(trs_disk_done, 0, 256);
    } else {
      state.last_readadr_density = state.density;
      state.status |= TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      state.bytecount = 6;
    }
    break;
  case TRSDISK_READTRK:
    if (d->emutype == REAL) {
      real_readtrk();
      break;
    }
    trs_disk_unimpl(cmd, "read track");
    break;
  case TRSDISK_WRITETRK:
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
      if (d->emutype == JV3) {
	/* Erase track if already formatted */
	int i;
	for (i=0; i<=d->last_used_id; i++) {
	  if (d->id[i].track == d->phytrack &&
	      ((d->id[i].flags & JV3_SIDE) != 0) == state.curside) {
	    free_sector(d, i);
	  }
	}
      }
      state.status = TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      state.format = FMT_GAP0;
      if (disk[state.curdrive].inches == 5) {
	if (state.density) {
	  state.bytecount = TRKSIZE_DD;
	} else {
	  state.bytecount = TRKSIZE_SD;
	}
      } else {
	if (state.density) {
	  state.bytecount = TRKSIZE_8DD;
	} else {
	  state.bytecount = TRKSIZE_8SD;
	}
      }
    }
    break;
  case TRSDISK_FORCEINT:
    if ((cmd & 0x07) != 0) {
      /* Interrupt features not implemented. */
      trs_disk_unimpl(cmd, "force interrupt with condition");
    } else {
      if (state.status & TRSDISK_BUSY) {
	state.status &= ~TRSDISK_BUSY;
      } else {
	state.currcommand = TRSDISK_SEEK; /* force type I status */
	if (d->phytrack == 0) {
	  state.status = TRSDISK_TRKZERO;
	} else {
	  state.status = 0;
	}
      }
      if ((cmd & 0x08) != 0) {
	trs_disk_intrq_interrupt(1);
      }
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
real_verify()
{
  /* Verify that head is on the expected track */
  /*!! ignore for now*/
}

void
real_restore()
{
#if __linux
  DiskState *d = &disk[state.curdrive];
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
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    /*int reset_now = 0;*/
    perror("real_restore");
    /*ioctl(fileno(d->file), FDRESET, &reset_now);*/
    state.status |= TRSDISK_SEEKERR;
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

  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.length = 256;
  raw_cmd.data = NULL;
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_INTR;
  raw_cmd.cmd[i++] = FD_SEEK;
  raw_cmd.cmd[i++] = 0;
  raw_cmd.cmd[i++] = d->phytrack * (trs_disk_doublestep ? 2 : 1);
  raw_cmd.cmd_count = i;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    /*int reset_now = 0;*/
    perror("real_seek");
    /*ioctl(fileno(d->file), FDRESET, &reset_now);*/
    state.status |= TRSDISK_SEEKERR;
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
  int res, i = 0;
  sigset_t set, oldset;

  state.status = 0;
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_READ | FD_RAW_INTR;
  raw_cmd.cmd[i++] = state.density ? 0x46 : 0x06;
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd[i++] = state.track;
  raw_cmd.cmd[i++] = state.curside;
  raw_cmd.cmd[i++] = state.sector;
  raw_cmd.cmd[i++] = 1; /* 256 */
  raw_cmd.cmd[i++] = 255;
  raw_cmd.cmd[i++] = 0x0a;
  raw_cmd.cmd[i++] = 0xff; /* 256 */
  raw_cmd.cmd_count = i;
  raw_cmd.data = (void*) d->buf;
  raw_cmd.length = 256;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    /*int reset_now = 0;*/
    perror("real_read");
    /*ioctl(fileno(d->file), FDRESET, &reset_now);*/
    state.status |= TRSDISK_NOTFOUND;
  } else {
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
    if ((raw_cmd.reply[0] & 0xc0) == 0) {
      state.status |= TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      state.bytecount = REAL_SECSIZE;
      return;
    }
  }
  state.status |= TRSDISK_BUSY;
  trs_schedule_event(trs_disk_done, 0, 128);
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
  raw_cmd.cmd[i++] = ((state.currcommand & 
		       (state.controller == TRSDISK_P1771 ? 0x03 : 0x01))
		      ? 0x09 : 0x05) | (state.density ? 0x40 : 0x00);
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd[i++] = state.track;
  raw_cmd.cmd[i++] = state.curside;
  raw_cmd.cmd[i++] = state.sector;
  raw_cmd.cmd[i++] = 1; /* 256 */
  raw_cmd.cmd[i++] = 255;
  raw_cmd.cmd[i++] = 0x0a;
  raw_cmd.cmd[i++] = 0xff; /* 256 */
  raw_cmd.cmd_count = i;
  raw_cmd.data = (void*) d->buf;
  raw_cmd.length = 256;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    /*int reset_now = 0;*/
    perror("real_write");
    /*ioctl(fileno(d->file), FDRESET, &reset_now);*/
    state.status |= TRSDISK_WRITEFLT;
  } else {
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
  trs_schedule_event(trs_disk_done, 0, 128);
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
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    /*int reset_now = 0;*/
    perror("real_readadr");
    /*ioctl(fileno(d->file), FDRESET, &reset_now);*/
    state.status |= TRSDISK_NOTFOUND;
  } else {
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
    if ((raw_cmd.reply[0] & 0xc0) == 0) {
      state.status |= TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      memcpy(d->buf, &raw_cmd.reply[3], 4);
      state.bytecount = 4;
      return;
    }
  }
  state.status |= TRSDISK_BUSY;
  trs_schedule_event(trs_disk_done, 0, 128);
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
  int res, i = 0;
  sigset_t set, oldset;

  state.status = 0;
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR;
  raw_cmd.cmd[i++] = 0x0d | (state.density ? 0x40 : 0x00);
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd[i++] = 1; /* 256 */
  raw_cmd.cmd[i++] = d->nblocks / 4;
  raw_cmd.cmd[i++] = 0x0c;
  raw_cmd.cmd[i++] = state.density ? 0x6d : 0xe5;
  raw_cmd.cmd_count = i;
  raw_cmd.data = (void*) d->buf;
  raw_cmd.length = d->nblocks;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGIO);
  sigprocmask(SIG_BLOCK, &set, &oldset);
  res = ioctl(fileno(d->file), FDRAWCMD, &raw_cmd);
  sigprocmask(SIG_SETMASK, &oldset, NULL);
  if (res < 0) {
    /*int reset_now = 0;*/
    perror("real_writetrk");
    /*ioctl(fileno(d->file), FDRESET, &reset_now);*/
    state.status |= TRSDISK_WRITEFLT;
  } else {
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
  trs_schedule_event(trs_disk_done, 0, 128);
#else
  trs_disk_unimpl(state.currcommand, "write track on real floppy");
#endif
}
