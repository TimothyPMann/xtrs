/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Mon Aug 25 15:54:27 PDT 1997 by mann */

/*
 * trs_imp_exp.c
 *
 * Features to make transferring files into and out of the emulator
 *  easier.  
 */

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "trs_imp_exp.h"
#include "z80.h"

/* New emulator traps */

extern Uchar *memory;

void do_emt_open()
{
    int fd;
    fd = open(&memory[REG_HL], REG_BC, REG_DE);
    if (fd >= 0) {
	REG_A = 0;
	REG_F |= ZERO_MASK;
    } else {
	REG_A = errno;
	REG_F &= ~ZERO_MASK;
    }
    REG_DE = fd;
}

void do_emt_close()
{
    int res;
    res = close(REG_DE);
    if (res >= 0) {
	REG_A = 0;
	REG_F |= ZERO_MASK;
    } else {
	REG_A = errno;
	REG_F &= ~ZERO_MASK;
    }
}

void do_emt_read()
{
    int size;
    if (REG_HL + REG_BC > 0x10000) {
	REG_A = EFAULT;
	REG_F &= ~ZERO_MASK;
        REG_BC = 0xFFFF;
	return;
    }
    size = read(REG_DE, &memory[REG_HL], REG_BC);
    if (size >= 0) {
	REG_A = 0;
	REG_F |= ZERO_MASK;
    } else {
	REG_A = errno;
	REG_F &= ~ZERO_MASK;
    }
    REG_BC = size;
}


void do_emt_write()
{
    int size;
    if (REG_HL + REG_BC > 0x10000) {
	REG_A = EFAULT;
	REG_F &= ~ZERO_MASK;
        REG_BC = 0xFFFF;
	return;
    }
    size = write(REG_DE, &memory[REG_HL], REG_BC);
    if (size >= 0) {
	REG_A = 0;
	REG_F |= ZERO_MASK;
    } else {
	REG_A = errno;
	REG_F &= ~ZERO_MASK;
    }
    REG_BC = size;
}

void do_emt_lseek()
{
    int i;
    off_t offset;
    if (REG_HL + 8 > 0x10000) {
	REG_A = EFAULT;
	REG_F &= ~ZERO_MASK;
	return;
    }
    offset = 0;
    for (i=REG_HL; i<8; i++) {
	offset = offset + (memory[REG_HL + i] << i*8);
    }
    offset = lseek(REG_DE, offset, REG_BC);
    if (offset != (off_t) -1) {
	REG_A = 0;
	REG_F |= ZERO_MASK;
    } else {
	REG_A = errno;
	REG_F &= ~ZERO_MASK;
    }
    for (i=REG_HL; i<8; i++) {
	memory[REG_HL + i] = offset & 0xff;
	offset >>= 8;
    }
}

void do_emt_strerror()
{
    char *msg;
    int size;
    if (REG_HL + REG_BC > 0x10000) {
	REG_A = EFAULT;
	REG_F &= ~ZERO_MASK;
	return;
    }
    errno = 0;
    msg = strerror(REG_A);
    size = strlen(msg);
    if (errno != 0) {
	REG_A = errno;
	REG_F &= ~ZERO_MASK;
    } else if (REG_BC < size + 1) {
	REG_A = ENOMEM;
	REG_F |= ZERO_MASK;
	size = REG_BC - 1;
    }
    memcpy(&memory[REG_HL], msg, size);
    memory[REG_HL + size] = '\r';
}


/* Old kludgey way using i/o ports */

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
