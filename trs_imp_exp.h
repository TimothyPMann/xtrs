/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Mon Aug 25 15:41:19 PDT 1997 by mann */

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
 * ED30 open
 *         Before, HL => path, null terminated
 *                 BC =  oflag
 *                 DE =  mode
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 DE =  fd, 0xFFFF if error
 *
 * ED31 close
 *         Before, DE =  fd
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *
 * ED32 read
 *         Before, BC =  nbytes
 *                 DE =  fd
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 BC =  nbytes read, 0xFFFF if error
 *
 * ED33 write
 *         Before, BC =  nbytes
 *                 DE =  fd
 *                 HL => buffer
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 BC =  nbytes written, 0xFFFF if error
 *
 * ED34 lseek
 *         Before, BC =  whence
 *                 DE =  fd
 *                 HL => offset (8-byte little-endian integer)
 *         After,  AF =  0 if OK, error number if not (Z flag affected)
 *                 HL => location in file (8-byte little-endian integer)
 *
 * ED35 strerror
 *         Before, A  =  error number
 *                 HL => buffer for error string
 *                 BC =  buffer size
 *         After,  AF =  0 if OK, new error number if not (Z flag affected)
 *                 HL => same buffer, containing \r-terminated error string
 *
 * ED36-ED3F reserved
 *
 * Old import feature
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

extern void do_emt_open();
extern void do_emt_close();
extern void do_emt_read();
extern void do_emt_write();
extern void do_emt_lseek();
extern void do_emt_strerror();

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

