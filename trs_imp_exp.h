/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Tue Dec 17 13:06:18 PST 1996 by mann */

/*
 * trs_imp_exp.h
 *
 * Features to make transferring files into and out of the emulator
 * easier.  
 *
 * Import: 
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
 * Export:
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
 * Reset:
 *    Write IMPEXP_CMD_RESET to the command port.  This closes the file if
 *    it is currently open and makes the return status of fclose 
 *    available from the status port.  If no file was open,
 *    the status will be IMPEXP_EOF.
 */

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
