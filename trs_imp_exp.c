/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Tue Dec 17 13:06:18 PST 1996 by mann */

/*
 * trs_imp_exp.c
 *
 * Kludge features to make transferring files into and out of the emulator
 *  easier.  
 */

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include "trs_imp_exp.h"

typedef struct {
  FILE* f;
  unsigned char cmd;
  unsigned char status;
  char filename[IMPEXP_MAX_CMD_LEN+1];
  int len;
} Channel;

Channel ch;

static void
reset()
{
    if (ch.f) {
	int v;
	v = fclose(ch.f);
	ch.f = NULL;
	if (v == 0) {
	    ch.status = IMPEXP_EOF;
	} else {
	    ch.status = errno;
	    if (ch.status == 0) ch.status = IMPEXP_UNKNOWN_ERROR;
	}
    } else {
	ch.status = IMPEXP_EOF;
    }
    ch.cmd = IMPEXP_CMD_RESET;
    ch.len = 0;
}

/* Common routine for writing file names */
static void
filename_write(char* dir, unsigned char data)
{
    if (ch.len < IMPEXP_MAX_CMD_LEN) {
	ch.filename[ch.len++] = data;
    }
    if (data == 0) {
	ch.f = fopen(ch.filename, dir);
	if (ch.f == NULL) {
	    ch.status = errno;
	    if (ch.status == 0) {
		/* Bogus popen, doesn't set errno */
		ch.status = IMPEXP_UNKNOWN_ERROR;
	    }
	    ch.cmd = IMPEXP_CMD_RESET;
	} else {
	    ch.status = IMPEXP_EOF;
	}
    }
}

void
trs_impexp_cmd_write(unsigned char data)
{
    switch (data) {
      case IMPEXP_CMD_RESET:
      case IMPEXP_CMD_EOF:
	reset();
	break;
      case IMPEXP_CMD_IMPORT:
      case IMPEXP_CMD_EXPORT:
	if (ch.cmd != IMPEXP_CMD_RESET) reset();
	ch.cmd = data;
	break;
    }
}

unsigned char
trs_impexp_status_read()
{
    return ch.status;
}

void
trs_impexp_data_write(unsigned char data)
{
    int c;
    switch (ch.cmd) {
      case IMPEXP_CMD_RESET:
      case IMPEXP_CMD_EOF:
	/* ignore */
	break;
      case IMPEXP_CMD_IMPORT:
	if (ch.f != NULL) {
	    /* error; ignore */
	} else {
	    filename_write("r", data);
	}
	break;
      case IMPEXP_CMD_EXPORT:
	if (ch.f != NULL) {
	    c = putc(data, ch.f);
	    if (c == EOF) {
		reset();
	    }
	} else {
	    filename_write("w", data);
	}
	break;
    }	
}

unsigned char
trs_impexp_data_read()
{
    int c;
    switch (ch.cmd) {
      case IMPEXP_CMD_RESET:
      case IMPEXP_CMD_EOF:
	break;
      case IMPEXP_CMD_IMPORT:
	if (ch.f != NULL) {
	    int c = getc(ch.f);
	    if (c == EOF) {
		reset();
	    } else {
		ch.status = IMPEXP_MORE_DATA;
		return c;
	    }
	}
	break;
      case IMPEXP_CMD_EXPORT:
	break;
    }	
    /* Return end of file or error */
    if (ch.status == 0) ch.status = IMPEXP_EOF;
    return IMPEXP_EOF;
}
