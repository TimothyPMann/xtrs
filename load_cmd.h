/* Copyright (c) 1996-98, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/*
 * Last modified on Thu Apr 23 00:36:11 PDT 1998 by mann    
 */

/* TRS-80 DOS /cmd file loader */

#define LOAD_CMD_OK 0
#define LOAD_CMD_EOF -1
#define LOAD_CMD_NOT_FOUND -2
#define LOAD_CMD_ISAM -3
#define LOAD_CMD_PDS -4

#define VERBOSITY_QUIET 0
#define VERBOSITY_TEXT 1
#define VERBOSITY_MAP 2
#define VERBOSITY_DETAILED 3

#define ISAM_NONE -1

/* Load the /cmd file f into the given memory, optionally selecting
 * out an ISAM or PDS member.  Return LOAD_CMD_OK for success if f was a
 * normal /cmd file, LOAD_CMD_ISAM for success if it was an ISAM file,
 * LOAD_CMD_PDS for success if this was a PDS file, LOAD_CMD_EOF for 
 * failure due to premature end of file, LOAD_CMD_NOT_FOUND for ISAM
 * or PDF member not found, or a positive number B for an unknown or
 * badly formatted load block of typecode B (load file format error). 
 *
 * Optional flags:
 *
 * If loadmap is not NULL, it must point to an array of 2**16
 * bytes.  Each byte in the return value will have a count (mod 256)
 * of the number of times that memory location was loaded.  Usually each
 * count will be 0 or 1, of course. 
 * 
 * If verbosity is VERBOSITY_QUIET, print nothing.  If verbosity is
 * VERBOSITY_TEXT, print module headers, PDS headers, patch names, and
 * copyright notices.  If verbosity is VERBOSITY_MAP, also print load
 * map information as we go along, but coalesce adjacent blocks that
 * load contiguously into memory.  If verbosity is VERBOSITY_DETAILED,
 * don't coalesce. 
 *
 * If isam is not -1, search for the given isam member number and load
 * it instead of loading the whole file.
 *
 * If pds is not NULL, search for the given isam member number and
 * load it instead of loading the whole file.  isam and pds cannot
 * both be used.
 *
 * If xferaddr is not NULL, return the transfer address there, or if
 * there is no transfer address, return -1.
 *
 * If stopxfer is 1, stop loading when a transfer address is seen; if
 * 0, continue loading.  stopxfer = 0 is needed if you want to parse
 * a PDS file with a front end loader.  stopxfer = 1 is useful to deal
 * with ordinary /cmd files that have extra garbage at the end, as
 * sometimes happens.  stopxfer = 0 should be considered the default.
 */


int
load_cmd(FILE* f, unsigned char memory[65536],
	 unsigned char* loadmap, int verbosity, FILE* outf,
	 int isam, char* pds, int* xferaddr, int stopxfer);
