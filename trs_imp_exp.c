/* Copyright (c) 1996, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/* Last modified on Fri Dec 15 15:23:50 PST 2000 by mann */

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
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "trs_imp_exp.h"
#include "z80.h"
#include "trs.h"
#include "trs_disk.h"

/* New emulator traps */

#define MAX_OPENDIR 32
DIR *dir[MAX_OPENDIR] = { NULL, };

typedef struct {
  int fd;
  int inuse;
} OpenDisk;
#define MAX_OPENDISK 32
OpenDisk od[MAX_OPENDISK];

void do_emt_system()
{
  int res;
  res = system(mem_pointer(REG_HL, 0));
  if (res == -1) {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  } else {
    REG_A = 0;
    REG_F |= ZERO_MASK;
  }
  REG_BC = res;
}

void do_emt_mouse()
{
  int x, y;
  unsigned int buttons, sens;
  switch (REG_B) {
  case 1:
    trs_get_mouse_pos(&x, &y, &buttons);
    REG_HL = x;
    REG_DE = y;
    REG_A = buttons;
    if (REG_A) {
      REG_F &= ~ZERO_MASK;
    } else {
      REG_F |= ZERO_MASK;
    }
    break;
  case 2:
    trs_set_mouse_pos(REG_HL, REG_DE);
    REG_A = 0;
    REG_F |= ZERO_MASK;
    break;
  case 3:
    trs_get_mouse_max(&x, &y, &sens);
    REG_HL = x;
    REG_DE = y;
    REG_A = sens;
    if (REG_A) {
      REG_F &= ~ZERO_MASK;
    } else {
      REG_F |= ZERO_MASK;
    }
    break;
  case 4:
    trs_set_mouse_max(REG_HL, REG_DE, REG_C);
    REG_A = 0;
    REG_F |= ZERO_MASK;
    break;
  case 5:
    REG_A = trs_get_mouse_type();
    if (REG_A) {
      REG_F &= ~ZERO_MASK;
    } else {
      REG_F |= ZERO_MASK;
    }
    break;
  default:
    error("undefined emt_mouse function code");
    break;
  }
}

void do_emt_getddir()
{
  if (REG_HL + REG_BC > 0x10000 ||
      REG_HL + strlen(trs_disk_dir) + 1 > REG_HL + REG_BC) {
    REG_A = EFAULT;
    REG_F &= ~ZERO_MASK;
    REG_BC = 0xFFFF;
    return;
  }
  strcpy(mem_pointer(REG_HL, 1), trs_disk_dir);
  REG_A = 0;
  REG_F |= ZERO_MASK;
  REG_BC = strlen(trs_disk_dir);
}

void do_emt_setddir()
{
  trs_disk_dir = strdup(mem_pointer(REG_HL, 0));
  if (trs_disk_dir[0] == '~' &&
      (trs_disk_dir[1] == '/' || trs_disk_dir[1] == '\0')) {
    char* home = getenv("HOME");
    if (home) {
      char *p = (char*)malloc(strlen(home) + strlen(trs_disk_dir) + 2);
      sprintf(p, "%s/%s", home, trs_disk_dir+1);
      free(trs_disk_dir);
      trs_disk_dir = p;
    }
  }
  REG_A = 0;
  REG_F |= ZERO_MASK;
}

void do_emt_open()
{
  int fd, oflag, eoflag;
  eoflag = REG_BC;
  switch (eoflag & EO_ACCMODE) {
  case EO_RDONLY:
  default:
    oflag = O_RDONLY;
    break;
  case EO_WRONLY:
    oflag = O_WRONLY;
    break;
  case EO_RDWR:
    oflag = O_RDWR;
    break;
  }
  if (eoflag & EO_CREAT)  oflag |= O_CREAT;
  if (eoflag & EO_EXCL)   oflag |= O_EXCL;
  if (eoflag & EO_TRUNC)  oflag |= O_TRUNC;
  if (eoflag & EO_APPEND) oflag |= O_APPEND;

  fd = open(mem_pointer(REG_HL, 0), oflag, REG_DE);
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
  size = read(REG_DE, mem_pointer(REG_HL, 1), REG_BC);
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
  size = write(REG_DE, mem_pointer(REG_HL, 0), REG_BC);
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
  for (i=0; i<8; i++) {
    offset = offset + (mem_read(REG_HL + i) << i*8);
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
    mem_write(REG_HL + i, offset & 0xff);
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
    REG_BC = 0xFFFF;
    return;
  }
  errno = 0;
  msg = strerror(REG_A);
  size = strlen(msg);
  if (errno != 0) {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  } else if (REG_BC < size + 2) {
    REG_A = ERANGE;
    REG_F &= ~ZERO_MASK;
    size = REG_BC - 1;
  } else {
    REG_A = 0;
    REG_F |= ZERO_MASK;
  }
  memcpy(mem_pointer(REG_HL, 1), msg, size);
  mem_write(REG_HL + size++, '\r');
  mem_write(REG_HL + size, '\0');
  if (errno == 0) {
    REG_BC = size;
  } else {
    REG_BC = 0xFFFF;
  }
}

void do_emt_time()
{
  time_t now = time(0);
  if (REG_A == 1) {
#if __alpha
    struct tm *loctm = localtime(&now);
    now += loctm->tm_gmtoff;
#else
    struct tm loctm = *(localtime(&now));
    struct tm gmtm = *(gmtime(&now));
    int daydiff = loctm.tm_mday - gmtm.tm_mday;
    now += (loctm.tm_sec - gmtm.tm_sec)
      + (loctm.tm_min - gmtm.tm_min) * 60
      + (loctm.tm_hour - gmtm.tm_hour) * 3600;
    switch (daydiff) {
    case 0:
    case 1:
    case -1:
      now += 24*3600 * daydiff;
      break;
    case 30:
    case 29:
    case 28:
    case 27:
      now -= 24*3600;
      break;
    case -30:
    case -29:
    case -28:
    case -27:
      now += 24*3600;
      break;
    default:
      error("trouble computing local time in emt_time");
    }
#endif
  } else if (REG_A != 0) {
    error("unsupported function code to emt_time");
  }
  REG_BC = (now >> 16) & 0xffff;
  REG_DE = now & 0xffff;
}

void do_emt_opendir()
{
  int i;
  for (i = 0; i < MAX_OPENDIR; i++) {
    if (dir[i] == NULL) break;
  }
  if (i == MAX_OPENDIR) {
    REG_DE = 0xffff;
    REG_A = EMFILE;
    return;
  }
  dir[i] = opendir(mem_pointer(REG_HL, 0));
  if (dir[i] == NULL) {
    REG_DE = 0xffff;
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  } else {
    REG_DE = i;
    REG_A = 0;
    REG_F |= ZERO_MASK;
  }
}

void do_emt_closedir()
{
  int i = REG_DE;
  int ok;
  if (i < 0 || i >= MAX_OPENDIR || dir[i] == NULL) {
    REG_A = EBADF;
    REG_F &= ~ZERO_MASK;
    return;
  }	
  ok = closedir(dir[i]);
  dir[i] = NULL;
  if (ok >= 0) {
    REG_A = 0;
    REG_F |= ZERO_MASK;
  } else {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  }
}

void do_emt_readdir()
{
  int size, i = REG_DE;
  struct dirent *result;

  if (i < 0 || i >= MAX_OPENDIR || dir[i] == NULL) {
    REG_A = EBADF;
    REG_F &= ~ZERO_MASK;
    REG_BC = 0xFFFF;
    return;
  }	
  if (REG_HL + REG_BC > 0x10000) {
    REG_A = EFAULT;
    REG_F &= ~ZERO_MASK;
    REG_BC = 0xFFFF;
    return;
  }
  result = readdir(dir[i]);
  if (result == NULL) {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
    REG_BC = 0xFFFF;
    return;
  }
  size = strlen(result->d_name);
  if (size + 1 > REG_BC) {
    REG_A = ERANGE;
    REG_F &= ~ZERO_MASK;
    REG_BC = 0xFFFF;
    return;
  }
  strcpy(mem_pointer(REG_HL, 1), result->d_name);
  REG_A = 0;
  REG_F |= ZERO_MASK;
  REG_BC = size;
}

void do_emt_chdir()
{
  int ok = chdir(mem_pointer(REG_HL, 0));
  if (ok < 0) {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  } else {
    REG_A = 0;
    REG_F |= ZERO_MASK;
  }
}

void do_emt_getcwd()
{
  char *result;
  if (REG_HL + REG_BC > 0x10000) {
    REG_A = EFAULT;
    REG_F &= ~ZERO_MASK;
    REG_BC = 0xFFFF;
    return;
  }
  result = getcwd(mem_pointer(REG_HL, 1), REG_BC);
  if (result == NULL) {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
    REG_BC = 0xFFFF;
    return;
  }
  REG_A = 0;
  REG_F |= ZERO_MASK;
  REG_BC = strlen(result);
}

void do_emt_misc()
{
  switch (REG_A) {
  case 0:
    trs_disk_change_all();
    REG_HL = trs_disk_changecount;
    break;
  case 1:
    trs_exit();
    break;
  case 2:
    trs_debug();
    break;
  case 3:
    trs_reset(0);
    break;
  case 4:
    REG_HL = trs_disk_changecount;
    break;
  case 5:
    REG_HL = trs_model;
    break;
  case 6:
    REG_HL = trs_disk_getsize(REG_BC);
    break;
  case 7:
    trs_disk_setsize(REG_BC, REG_HL);
    break;
  case 8:
    REG_HL = trs_disk_getstep(REG_BC);
    break;
  case 9:
    trs_disk_setstep(REG_BC, REG_HL);
    break;
  case 10:
    REG_HL = grafyx_get_microlabs();
    break;
  case 11:
    grafyx_set_microlabs(REG_HL);
    break;
  case 12:
    REG_HL = z80_state.delay;
    REG_BC = trs_autodelay;
    break;
  case 13:
    z80_state.delay = REG_HL;
    trs_autodelay = REG_BC;
    break;
  case 14:
    REG_HL = stretch_amount;
    break;
  case 15:
    stretch_amount = REG_HL;
    break;
  case 16:
    REG_HL = trs_disk_doubler;
    break;
  case 17:
    trs_disk_doubler = REG_HL;
    break;
  case 18:
    REG_HL = sb_get_volume();
    break;
  case 19:
    sb_set_volume(REG_HL);
    break;
  case 20:
    REG_HL = trs_disk_truedam;
    break;
  case 21:
    trs_disk_truedam = REG_HL;
    break;
  default:
    error("unsupported function code to emt_misc");
    break;
  }
}

void do_emt_ftruncate()
{
  int i, result;
  off_t offset;
  if (REG_HL + 8 > 0x10000) {
    REG_A = EFAULT;
    REG_F &= ~ZERO_MASK;
    return;
  }
  offset = 0;
  for (i=0; i<8; i++) {
    offset = offset + (mem_read(REG_HL + i) << i*8);
  }
  result = ftruncate(REG_DE, offset);
  if (result == 0) {
    REG_A = 0;
    REG_F |= ZERO_MASK;
  } else {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  }
}

void do_emt_opendisk()
{
  char *name = (char *)mem_pointer(REG_HL, 0);
  char *qname;
  int i;
  int oflag, eoflag;

  eoflag = REG_BC;
  switch (eoflag & EO_ACCMODE) {
  case EO_RDONLY:
  default:
    oflag = O_RDONLY;
    break;
  case EO_WRONLY:
    oflag = O_WRONLY;
    break;
  case EO_RDWR:
    oflag = O_RDWR;
    break;
  }
  if (eoflag & EO_CREAT)  oflag |= O_CREAT;
  if (eoflag & EO_EXCL)   oflag |= O_EXCL;
  if (eoflag & EO_TRUNC)  oflag |= O_TRUNC;
  if (eoflag & EO_APPEND) oflag |= O_APPEND;

  if (*name == '/' || *trs_disk_dir == '\0') {
    qname = strdup(name);
  } else {
    qname = (char *)malloc(strlen(trs_disk_dir) + 1 + strlen(name) + 1);
    strcpy(qname, trs_disk_dir);
    strcat(qname, "/");
    strcat(qname, name);
  }
  for (i = 0; i < MAX_OPENDISK; i++) {
    if (!od[i].inuse) break;
  }
  if (i == MAX_OPENDISK) {
    REG_DE = 0xffff;
    REG_A = EMFILE;
    REG_F &= ~ZERO_MASK;
    free(qname);
    return;
  }
  od[i].fd = open(qname, oflag, REG_DE);
  free(qname);
  if (od[i].fd >= 0) {
    od[i].inuse = 1;
    REG_A = 0;
    REG_F |= ZERO_MASK;
  } else {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  }
  REG_DE = od[i].fd;
}

void do_emt_closedisk()
{
  int i;
  int res;
  if (REG_DE == 0xffff) {
    for (i = 0; i < MAX_OPENDISK; i++) {
      if (od[i].inuse) {
	close(od[i].fd);
	od[i].inuse = 0;
      }
    }
    REG_A = 0;
    REG_F |= ZERO_MASK;
    return;
  }

  for (i = 0; i < MAX_OPENDISK; i++) {
    if (od[i].inuse && od[i].fd == REG_DE) break;
  }
  if (i == MAX_OPENDISK) {
    REG_A = EBADF;
    REG_F &= ~ZERO_MASK;
    return;
  }
  od[i].inuse = 0;
  res = close(od[i].fd);
  if (res >= 0) {
    REG_A = 0;
    REG_F |= ZERO_MASK;
  } else {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  }
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
