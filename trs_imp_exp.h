/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Mon Jan 12 19:48:21 PST 1998 by mann */

/*
 * trs_imp_exp.h
 *
 * Features to make transferring files into and out of the emulator
 * easier.  There are two sets of features implemented.  The old, slow
 * ones use some special reserved ports.  The interface is something
 * that could conceivably be implemented in real hardware.  The new,
 * fast, flexible ones use a set of emulator traps (instructions that
 * don't exist in a real Z-80).  The old ones should probably go away
 * at some point.
 *
 * ED30 emt_open
 *         Before, HL => path, null terminated
 *                 BC =  oflag (use EO_ values defined below)
 *                 DE =  mode
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 DE =  fd, 0xFFFF if error
 *
 * ED31 emt_close
 *         Before, DE =  fd
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 * ED32 emt_read
 *         Before, BC =  nbytes
 *                 DE =  fd
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 BC =  nbytes read, 0xFFFF if error
 *
 * ED33 emt_write
 *         Before, BC =  nbytes
 *                 DE =  fd
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 BC =  nbytes written, 0xFFFF if error
 *
 * ED34 emt_lseek
 *         Before, BC =  whence
 *                 DE =  fd
 *                 HL => offset (8-byte little-endian integer)
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 HL => location in file, 0xFFFFFFFF if error
 *
 * ED35 emt_strerror
 *         Before, A  =  error number
 *                 HL => buffer for error string
 *                 BC =  buffer size
 *         After,  AF =  0 if OK, new error number if not (Z flag affected)
 *                 HL => same buffer, containing \r\0-terminated error string
 *                 BC =  strlen(buffer), 0xFFFF if error
 *
 * ED36 emt_time
 *         Before, A  =  0 for UTC (GMT), 1 for local time
 *         After,  BCDE= time in seconds since Jan 1, 1970 00:00:00
 *
 * ED37 emt_opendir
 *         Before, HL => path, null terminated
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 DE =  dirfd, 0xFFFF if error
 *
 * ED38 emt_closedir
 *         Before, DE =  dirfd
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 * ED39 emt_readdir
 *         Before, BC =  nbytes
 *                 DE =  dirfd
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 HL => same buffer, containing NUL-terminated filename
 *                 BC =  strlen(buffer), 0xFFFF if error
 *
 * ED3A emt_chdir
 *   Warning: Changing the working directory will change where the disk*-*
 *   files are found on the next disk-change command, unless -diskdir has
 *   been specified as an absolute pathname.
 *         Before, HL => path, null terminated
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 * ED3B emt_getcwd
 *         Before, BC =  nbytes
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 HL => same buffer, containing NUL-terminated pathname
 *                 BC =  strlen(buffer), 0xFFFF if error
 *
 * ED3C emt_misc
 *         Before, A  = function code, one of the following:
 *                      0 = disk change
 *                      1 = exit emulator
 *                      2 = enter debugger (if active)
 *                      3 = press reset button
 *                      4 = query disk change count
 *         After,  HL = result, depending on function code
 *                      0,4: disk change count (F7 or emt_misc)
 *                      1-3: no result; HL unchanged
 *
 * ED3D emt_ftruncate
 *         Before, DE =  fd
 *                 HL => offset (8-byte little-endian integer)
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 * ED3E emt_opendisk
 *   Similar to emt_open, except that (1) the path, if not absolute, is
 *   interpreted relative to -diskdir, (2) emt_closedisk must be
 *   used in place of emt_close.
 *         Before, HL => path, null terminated
 *                 BC =  oflag (use EO_ values defined below)
 *                 DE =  mode
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 DE =  fd, 0xFFFF if error
 *
 * ED3F emt_closedisk
 *   Similar to emt_close, but pairs with emt_opendisk.
 *         Before, DE =  fd, or -1 to close all fds opened with emt_opendisk
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 */

/* Minimal subset of standard O_ flags.  We have to define our own for
   portability; the numeric values differ amongst Unix
   implementations. */

#define EO_ACCMODE   03
#define EO_RDONLY    00
#define EO_WRONLY    01
#define EO_RDWR      02
#define EO_CREAT   0100
#define EO_EXCL    0200
#define EO_TRUNC  01000
#define EO_APPEND 02000

extern void do_emt_open();
extern void do_emt_close();
extern void do_emt_read();
extern void do_emt_write();
extern void do_emt_lseek();
extern void do_emt_strerror();
extern void do_emt_time();
extern void do_emt_opendir();
extern void do_emt_closedir();
extern void do_emt_readdir();
extern void do_emt_chdir();
extern void do_emt_getcwd();
extern void do_emt_misc();
extern void do_emt_ftruncate();
extern void do_emt_opendisk();
extern void do_emt_closedisk();

/* Old import feature
 *  1) Write IMPEXP_CMD_IMPORT to the command port, write a Unix
 *     filename to the data port, and read the status port to
 *     check for errors on the fopen() call.
 *  2) Read the file data from the data port.  If end of file or error
 *     occurs, 0x00 bytes will be returned.  Distinguish this from a
 *     real 0x00 byte by checking the status port.  Reading the last
 *     byte closes the file and makes the return status of fclose 
 *     available from the status port.  Normal completion will be
 *     indicated by IMPEXP_EOF.
 *
 * Old export feature
 * 1) Write IMPEXP_CMD_EXPORT to the command port, write a Unix
 *    filename to the data port, and read the status port to
 *    check for errors on the fopen() call.
 * 2) Write the file's contents to the data port.  If a write
 *    error occurs, the status port will reflect the error.
 * 3) Write IMPEXP_CMD_EOF to the command port.  This 
 *    closes the file and makes the return status of fclose 
 *    available from the status port.  Normal completion will be
 *    indicated by IMPEXP_EOF.
 *
 * Reset old import/export feature
 *    Write IMPEXP_CMD_RESET to the command port.  This closes the file if
 *    it is currently open and makes the return status of fclose 
 *    available from the status port.  If no file was open,
 *    the status will be IMPEXP_EOF.  */

#define IMPEXP_CMD     0xd0
#define IMPEXP_STATUS  0xd0
#define IMPEXP_DATA    0xd1

/* Commands */
#define IMPEXP_CMD_RESET   0
#define IMPEXP_CMD_IMPORT  1
#define IMPEXP_CMD_EXPORT  2
#define IMPEXP_CMD_EOF     3

/* Status - any other value is a Unix errno code */
#define IMPEXP_EOF           0x00
#define IMPEXP_UNKNOWN_ERROR 0xfe
#define IMPEXP_MORE_DATA     0xff

#define IMPEXP_MAX_CMD_LEN 2048

extern void trs_impexp_cmd_write(unsigned char data);
extern unsigned char trs_impexp_status_read(void);
extern void trs_impexp_data_write(unsigned char data);
extern unsigned char trs_impexp_data_read(void);

