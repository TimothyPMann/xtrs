/* xtrsemt.h -- Misosys C interface to xtrs emulator traps */
/* Copyright (c) 1997, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Thu Apr  9 12:35:26 PDT 1998 by mann */

#ifndef _TIME_T
#include <time.h>
#endif

extern int emt_system(/* char *cmd */);
extern int emt_open(/* char *fname, int oflag, int mode */);
extern int emt_close(/* int fd */);
extern int emt_read(/* int fd, char *buffer, int bytes */);
extern int emt_write(/* int fd, char *buffer, int bytes */);
extern long emt_lseek(/* int fd, long offset, int whence */);
extern int emt_strerror(/* int err, char *buffer, int size */);
extern time_t emt_time(/* int local */);
extern int emt_dopen(/* char *fname */);
extern int emt_dclose(/* int dirfd */);
extern int emt_dread(/* int dirfd, char *buffer, int bytes */);
extern int emt_chdir(/* char *fname */);
extern int emt_getcwd(/* char *buffer, int bytes */);
extern int emt_misc(/* int func */);
extern void emt_misc2(/* int func, int *hl, int *bc, int *de */);
extern int emt_ftruncate(/* int fd, long length */);
extern int emt_opendisk(/* char *fname, int oflag, int mode */);
extern int emt_closedisk(/* int fd */);

/* oflag values for emt_open and emt_opendisk */
#define EO_ACCMODE   03
#define EO_RDONLY    00
#define EO_WRONLY    01
#define EO_RDWR      02
#define EO_CREAT   0100
#define EO_EXCL    0200
#define EO_TRUNC  01000
#define EO_APPEND 02000

/* local values for emt_time */
#define EMT_TIME_GMT 0
#define EMT_TIME_LOCAL 1

/* func values for emt_misc */
#define EMT_MISC_DISK_CHANGE       0
#define EMT_MISC_EXIT              1
#define EMT_MISC_DEBUG             2
#define EMT_MISC_RESET_BUTTON      3
#define EMT_MISC_QUERY_DISK_CHANGE 4
#define EMT_MISC_QUERY_MODEL       5
#define EMT_MISC_QUERY_DISK_SIZE   6
#define EMT_MISC_SET_DISK_SIZE     7
#define EMT_MISC_QUERY_DBL_STEP    8
#define EMT_MISC_SET_DBL_STEP      9
#define EMT_MISC_QUERY_MICROLABS  10
#define EMT_MISC_SET_MICROLABS    11
#define EMT_MISC_QUERY_DELAY      12
#define EMT_MISC_SET_DELAY        13
#define EMT_MISC_QUERY_KEYSTRETCH 14
#define EMT_MISC_SET_KEYSTRETCH   15
#define EMT_MISC_QUERY_DOUBLER    16
#define EMT_MISC_SET_DOUBLER      17
#define EMT_MISC_QUERY_VOLUME     18
#define EMT_MISC_SET_VOLUME       19
