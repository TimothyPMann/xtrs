/* Copyright (c) 2000, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Sun May 14 21:30:57 PDT 2000 by mann */

/*
 * Definitions for the Radio Shack TRS-80 Model I/III/4/4P
 * hard disk controller.  This is a Western Digital WD1000/WD1010
 * mapped at ports 0xc8-0xcf, plus control registers at 0xc0-0xc1.
 * 
 * Definitions from LS-DOS 6.3.1 driver TRSHD6/DCT source code, with
 * some details filled in from Linux hdreg.h.  The latter is really
 * for IDE drives, which use a distant descendant of the WD1010
 * interface, so I may have accidentally filled in some things that
 * didn't exist or differed in a real WD1010.  Anyone have a WD1010
 * data sheet?
 */

extern void trs_hard_init(int reset_button);
extern int trs_hard_in(int port);
extern void trs_hard_out(int port, int value);
extern char* trs_disk_dir;

/* Sector size is always 256 for TRSDOS/LDOS/etc. */
/* Other sizes currently not emulated */
#define TRS_HARD_SECSIZE 256
#define TRS_HARD_SECSIZE_CODE 0x0f /* size code for 256 byte sectors?!! */

/* RSHARD assumes 32 sectors/track */
/* Other sizes currently not emulated */
#define TRS_HARD_SEC_PER_TRK 32

/*
 * Tandy-specific registers
 */

/* Write protect switch register (read only): 
 *  abcd----
 *  a = drive 0 write protected
 *  b = drive 1 write protected
 *  c = drive 2 write protected
 *  d = drive 3 write protected
 */
#define TRS_HARD_WP 0xc0
#define TRS_HARD_WPBIT(d) (0x80 >> (d))

/* Control register (read(?)/write): 
 *  ---sdw--
 *  s = software reset
 *  d = device enable
 *  w = wait enable
 */
#define TRS_HARD_CONTROL 0xc1
#define TRS_HARD_SOFTWARE_RESET 0x10
#define TRS_HARD_DEVICE_ENABLE  0x08
#define TRS_HARD_WAIT_ENABLE    0x04

/*
 * WD1010 registers
 */

/* Data register (read/write) */
#define TRS_HARD_DATA 0xc8

/* Error register (read only):
 *  bdin-atm
 *  b = block marked bad
 *  e = uncorrectable error in data 
 *  i = uncorrectable error in id
 *  n = id not found
 *  - = unused
 *  a = command aborted
 *  t = track 0 not found
 *  m = bad address mark
 */
#define TRS_HARD_ERROR (TRS_HARD_DATA+1)
#define	TRS_HARD_BBDERR   0x80
#define TRS_HARD_DATAERR  0x40
#define TRS_HARD_IDERR    0x20
#define TRS_HARD_NFERR    0x10
#define TRS_HARD_MCRERR   0x08
#define TRS_HARD_ABRTERR  0x04
#define TRS_HARD_TRK0ERR  0x02
#define TRS_HARD_MARKERR  0x01

/* Write precompensation register (write only) */
#define TRS_HARD_PRECOMP (TRS_HARD_DATA+1)

/* Sector count register (read/write) */
#define TRS_HARD_SECCNT (TRS_HARD_DATA+2)

/* Sector number register (read/write) */
#define TRS_HARD_SECNUM (TRS_HARD_DATA+3)

/* Cylinder low byte register (read/write) */
#define TRS_HARD_CYLLO (TRS_HARD_DATA+4)

/* Cylinder high byte register (read/write) */
#define TRS_HARD_CYLHI (TRS_HARD_DATA+5)

/* Size/drive/head register (read??/write):
 *  sssddhhh
 *  sss = LS-DOS uses 000; Linux IDE uses 101
 *  dd = drive
 *  hhh = head
 */
#define TRS_HARD_SDH (TRS_HARD_DATA+6)
#define TRS_HARD_SIZEMASK   0xe0
#define TRS_HARD_SIZESHIFT  5
#define TRS_HARD_DRIVEMASK  0x18
#define TRS_HARD_DRIVESHIFT 3
#define TRS_HARD_MAXDRIVES  4
#define TRS_HARD_HEADMASK   0x07
#define TRS_HARD_HEADSHIFT  0
#define TRS_HARD_MAXHEADS   8

/* Status register (read only): 
 *  brwsdcie
 *  b = busy
 *  r = ready (?)
 *  w = write error (?)
 *  s = seek (?)
 *  d = data request
 *  c = corrected ecc (??)
 *  i = CIP (=software reset required)
 *  e = error
 */
#define TRS_HARD_STATUS (TRS_HARD_DATA+7)
#define TRS_HARD_BUSY    0x80
#define TRS_HARD_READY   0x40
#define TRS_HARD_WRERR   0x20
#define TRS_HARD_SEEKING 0x10
#define TRS_HARD_DRQ	 0x08
#define TRS_HARD_ECC	 0x04
#define TRS_HARD_CIP	 0x02
#define TRS_HARD_ERR	 0x01

/* Command register (write only) */
#define TRS_HARD_COMMAND (TRS_HARD_DATA+7)

/*
 * WD1010 commands
 */

#define TRS_HARD_CMDMASK 0xf0

/* Restore: 
 *  0001rrrr
 *  rrrr = step rate
 */
#define TRS_HARD_RESTORE 0x10

/* Read sector:
 *  0010d000
 *  d = 0 for programmed I/O, 1 for DMA
 */
#define TRS_HARD_READ 0x20
#define TRS_HARD_DMA  0x08 /* not emulated! */

/* Write sector:
 *  00110000 (no DMA allowed??)
 */
#define TRS_HARD_WRITE 0x30

/* Verify sector: (?)
 *  01000000
 */
#define TRS_HARD_VERIFY 0x40

/* Format track:
 *  01010000
 */
#define TRS_HARD_FORMAT 0x50

/* Init (??):
 *  01100000
 */
#define TRS_HARD_INIT 0x60

/* Seek to specified sector/head/cylinder:
 *  0111rrrr
 *  rrrr = step rate
 */
#define TRS_HARD_SEEK 0x70
