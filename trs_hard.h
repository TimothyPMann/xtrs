/* Copyright (c) 2000, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Wed May 17 23:33:12 PDT 2000 by mann */

/*
 * Definitions for the Radio Shack TRS-80 Model I/III/4/4P
 * hard disk controller.  This is a Western Digital WD1000/WD1010
 * mapped at ports 0xc8-0xcf, plus control registers at 0xc0-0xc1.
 * 
 * Definitions inferred from various drivers and sketchy documents
 * found in odd corners.  Anyone have a real WD10xx data sheet?
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
 *  abcd--pi
 *  a = drive 0 write protected
 *  b = drive 1 write protected
 *  c = drive 2 write protected
 *  d = drive 3 write protected
 *  p = at least one drive write protected
 *  i = interrupt request
 */
#define TRS_HARD_WP 0xc0
#define TRS_HARD_WPBIT(d) (0x80 >> (d))
#define TRS_HARD_WPSOME 0x02
#define TRS_HARD_INTRQ  0x01 /* not emulated */

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

/* ID register (Model II only!)
 */
#define TRS_HARD_ID0 0xc2
#define TRS_HARD_ID1 0xc3

/* CTC channels (Model II only!)
 */
#define TRS_HARD_CTC0 0xc4
#define TRS_HARD_CTC1 0xc5
#define TRS_HARD_CTC2 0xc6
#define TRS_HARD_CTC3 0xc7

/*
 * WD1010 registers
 */

/* Data register (read/write) */
#define TRS_HARD_DATA 0xc8

/* Error register (read only):
 *  bdin-atm
 *  b = block marked bad
 *  e = uncorrectable error in data 
 *  i = uncorrectable error in id (unused?)
 *  n = id not found
 *  - = unused
 *  a = command aborted
 *  t = track 0 not found
 *  m = bad address mark
 */
#define TRS_HARD_ERROR (TRS_HARD_DATA+1)
#define	TRS_HARD_BBDERR   0x80
#define TRS_HARD_DATAERR  0x40
#define TRS_HARD_IDERR    0x20 /* unused? */
#define TRS_HARD_NFERR    0x10
#define TRS_HARD_MCRERR   0x08 /* unused */
#define TRS_HARD_ABRTERR  0x04
#define TRS_HARD_TRK0ERR  0x02
#define TRS_HARD_MARKERR  0x01

/* Write precompensation register (write only) */
/* Value *4 is the starting cylinder for write precomp */
#define TRS_HARD_PRECOMP (TRS_HARD_DATA+1)

/* Sector count register (read/write) */
/* Used only for multiple sector accesses; otherwise ignored. */
/* Autodecrements when used. */
#define TRS_HARD_SECCNT (TRS_HARD_DATA+2)

/* Sector number register (read/write) */
#define TRS_HARD_SECNUM (TRS_HARD_DATA+3)

/* Cylinder low byte register (read/write) */
#define TRS_HARD_CYLLO (TRS_HARD_DATA+4)

/* Cylinder high byte register (read/write) */
#define TRS_HARD_CYLHI (TRS_HARD_DATA+5)

/* Size/drive/head register (read/write):
 *  essddhhh
 *  e = 0 if CRCs used; 1 if ECC used
 *  ss = sector size; 00=256, 01=512, 10=1024, 11=128
 *  dd = drive
 *  hhh = head
 */
#define TRS_HARD_SDH (TRS_HARD_DATA+6)
#define TRS_HARD_ECCMASK    0x80
#define TRS_HARD_ECCSHIFT   7
#define TRS_HARD_SIZEMASK   0x60
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
 *  r = drive ready
 *  w = write error
 *  s = seek complete
 *  d = data request
 *  c = corrected ecc (reserved)
 *  i = command in progress (=software reset required)
 *  e = error
 */
#define TRS_HARD_STATUS (TRS_HARD_DATA+7)
#define TRS_HARD_BUSY     0x80
#define TRS_HARD_READY    0x40
#define TRS_HARD_WRERR    0x20
#define TRS_HARD_SEEKDONE 0x10
#define TRS_HARD_DRQ	  0x08
#define TRS_HARD_ECC	  0x04
#define TRS_HARD_CIP	  0x02
#define TRS_HARD_ERR	  0x01

/* Command register (write only) */
#define TRS_HARD_COMMAND (TRS_HARD_DATA+7)

/*
 * WD1010 commands
 */

#define TRS_HARD_CMDMASK 0xf0

/* Restore: 
 *  0001rrrr
 *  rrrr = step rate; 0000=35us, else rrrr*0.5ms
 */
#define TRS_HARD_RESTORE 0x10

/* Read sector:
 *  0010dm00
 *  d = 0 for interrupt on DRQ, 1 for interrupt at end (DMA style)
 *      TRS-80 always uses programmed I/O, INTRQ not connected, I believe.
 *  m = multiple sector flag (not emulated!)
 */
#define TRS_HARD_READ  0x20
#define TRS_HARD_DMA   0x08
#define TRS_HARD_MULTI 0x04

/* Write sector:
 *  00110m00
 *  m = multiple sector flag (not emulated!) 
 */
#define TRS_HARD_WRITE 0x30

/* Verify sector (or "Scan ID"):
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
 *  rrrr = step rate; 0000=35us, else rrrr*0.5ms
 */
#define TRS_HARD_SEEK 0x70
