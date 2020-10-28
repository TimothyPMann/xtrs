/* xtrsemt.h -- Misosys C interface to xtrs emulator traps */

/* 
 * Copyright (c) 1997-2008, Timothy P. Mann
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

#ifndef _TIME_T
#include <time.h>
#endif

/* Some names are changed to keep them unique in the first seven characters */
extern int emt_system(/* char *cmd */);
extern char* /*emt_getddir*/ emt_gtddir(/* char *buffer, int bytes */);
extern int /*emt_setddir*/ emt_stddir(/* char *fname */);
extern int emt_open(/* char *fname, int oflag, int mode */);
extern int emt_close(/* int fd */);
extern int emt_read(/* int fd, char *buffer, int bytes */);
extern int emt_write(/* int fd, char *buffer, int bytes */);
extern long emt_lseek(/* int fd, long offset, int whence */);
extern int emt_strerror(/* int err, char *buffer, int size */);
extern time_t emt_time(/* int local */);
extern int /*emt_opendir*/ emt_dropen(/* char *fname */);
extern int /*emt_closedir*/ emt_drclose(/* int dirfd */);
extern int /*emt_readdir*/ emt_drread(/*int dirfd, char *buffer, int bytes*/);
extern int emt_chdir(/* char *fname */);
extern char* emt_getcwd(/* char *buffer, int bytes */);
extern int emt_misc(/* int func */);
extern void emt_4misc(/* int func, int *hl, int *bc, int *de */);
extern int emt_ftruncate(/* int fd, long length */);
extern int /*emt_opendisk*/ emt_dkopen(/* char *fname, int oflag, int mode */);
extern int /*emt_closedisk*/ emt_dkclose(/* int fd */);

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
#define EMT_MISC_QUERY_TRUEDAM    20
#define EMT_MISC_SET_TRUEDAM      21
