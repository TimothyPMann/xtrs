/* Routines to write a TRS-80 DOS "/cmd" file */
/* Copyright (c) 1996, Timothy Mann */
/* $Id$ */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

extern void cmd_init(FILE *outf);
extern void cmd_data(int address, int value);
extern void cmd_transfer_address(int address);
extern void cmd_end_of_file(void);










