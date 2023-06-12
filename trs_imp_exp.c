/* 
 * Copyright (c) 1996-2020, Timothy P. Mann
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

/*
 * trs_imp_exp.c
 *
 * Features to make transferring files into and out of the emulator
 *  easier.  
 */

#define _XOPEN_SOURCE 500 /* ftruncate(), strdup() */

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
#include "trs_hard.h"

/*
   If the following option is set, potentially dangerous emulator traps
   will be blocked, including file writes to the host filesystem and shell
   command execution.
 */
int trs_emtsafe = TRUE;

/* New emulator traps */

#define MAX_OPENDIR 32
DIR *dir[MAX_OPENDIR] = { NULL, };

typedef struct {
  int fd;
  int inuse;
} OpenDisk;
#define MAX_OPENDISK 32
OpenDisk od[MAX_OPENDISK];

void do_emt_system(void)
{
  int res;
  if (trs_emtsafe) {
    error("emt_system: potentially dangerous emulator trap blocked");
    REG_A = EACCES;
    REG_F &= ~ZERO_MASK;
    return;
  }
  res = system((char *)mem_pointer(REG_HL, 0));
  if (res == -1) {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  } else {
    REG_A = 0;
    REG_F |= ZERO_MASK;
  }
  REG_BC = res;
}

void do_emt_mouse(void)
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

void do_emt_getddir(void)
{
  if (REG_HL + REG_BC > 0x10000 ||
      REG_HL + strlen(trs_disk_dir) + 1 > REG_HL + REG_BC) {
    REG_A = EFAULT;
    REG_F &= ~ZERO_MASK;
    REG_BC = 0xFFFF;
    return;
  }
  strcpy((char *)mem_pointer(REG_HL, 1), trs_disk_dir);
  REG_A = 0;
  REG_F |= ZERO_MASK;
  REG_BC = strlen(trs_disk_dir);
}

void do_emt_setddir(void)
{
  if (trs_emtsafe) {
    error("emt_setddir: potentially dangerous emulator trap blocked");
    REG_A = EACCES;
    REG_F &= ~ZERO_MASK;
    return;
  }
  trs_disk_dir = strdup((char *)mem_pointer(REG_HL, 0));
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

void do_emt_open(void)
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

  if (trs_emtsafe && oflag != O_RDONLY) {
    error("emt_open: potentially dangerous emulator trap blocked");
    REG_A = EACCES;
    REG_F &= ~ZERO_MASK;
    return;
  }
  fd = open((char *)mem_pointer(REG_HL, 0), oflag, REG_DE);
  if (fd >= 0) {
    REG_A = 0;
    REG_F |= ZERO_MASK;
  } else {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  }
  REG_DE = fd;
}

void do_emt_close(void)
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

void do_emt_read(void)
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


void do_emt_write(void)
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

void do_emt_lseek(void)
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

void do_emt_strerror(void)
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

void do_emt_time(void)
{
  time_t now = time(0) + trs_timeoffset;
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

void do_emt_opendir(void)
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
  dir[i] = opendir((char *)mem_pointer(REG_HL, 0));
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

void do_emt_closedir(void)
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

void do_emt_readdir(void)
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
  strcpy((char *)mem_pointer(REG_HL, 1), result->d_name);
  REG_A = 0;
  REG_F |= ZERO_MASK;
  REG_BC = size;
}

void do_emt_chdir(void)
{
  int ok;
  if (trs_emtsafe) {
    error("emt_chdir: potentially dangerous emulator trap blocked");
    REG_A = EACCES;
    REG_F &= ~ZERO_MASK;
    return;
  }
  ok = chdir((char *)mem_pointer(REG_HL, 0));
  if (ok < 0) {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  } else {
    REG_A = 0;
    REG_F |= ZERO_MASK;
  }
}

void do_emt_getcwd(void)
{
  char *result;
  if (REG_HL + REG_BC > 0x10000) {
    REG_A = EFAULT;
    REG_F &= ~ZERO_MASK;
    REG_BC = 0xFFFF;
    return;
  }
  result = getcwd((char *)mem_pointer(REG_HL, 1), REG_BC);
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

void do_emt_misc(void)
{
  switch (REG_A) {
  case 0:
    trs_change_all();
    REG_HL = trs_changecount;
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
    REG_HL = trs_changecount;
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
  //case 18: // removed; do not reuse
  //case 19: // removed; do not reuse
  case 20:
    REG_HL = trs_disk_truedam;
    break;
  case 21:
    trs_disk_truedam = REG_HL;
    break;
  case 22:
    REG_HL = trs_keydelay;
    break;
  case 23:
    trs_keydelay = REG_HL;
    break;
  case 24:
    REG_HL = trs_lowercase;
    break;
  case 25:
    trs_lowercase = REG_HL;
    break;
  default:
    error("unsupported function code to emt_misc");
    break;
  }
}

void do_emt_ftruncate(void)
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

/*
 * In xtrs 4.9d and earlier, you could open any filename through this
 * API in any mode, with an API similar to emt_open -- filename
 * (relative to trs_disk_dir) in HL, flags in BC, mode in DE.  Now
 * instead we take the unit number in the three low-order bits of A
 * and open the same filename that trs_hard.c would open.  This keeps
 * compatibility with xtrshard, the only existing client of the API,
 * because xtrshard conveniently happens to leave the unit number
 * there.  The new API works better with gxtrs where the user
 * selects the hard drive name from the xtrs File menu.  It is also
 * safer; it does not need to be restricted by emt_safe.
 *
 * This API could be improved further (better integration with
 * trs_hard.c, handle write-protect better, reduce the amount of Z80
 * code in xtrshard, etc.), but I prefer to keep compatibility with
 * the old xtrshard.
 */
void do_emt_opendisk(void)
{
  int drive = REG_A % TRS_HARD_MAXDRIVES;
  int i;
  int readonly = 0;
  
  for (i = 0; i < MAX_OPENDISK; i++) {
    if (!od[i].inuse) break;
  }
  if (i == MAX_OPENDISK) {
    REG_DE = 0xffff;
    REG_A = EMFILE;
    REG_F &= ~ZERO_MASK;
    return;
  }
  od[i].fd = open(trs_hard_get_name(drive), O_RDWR);
  if (od[i].fd < 0) {
    od[i].fd = open(trs_hard_get_name(drive), O_RDONLY);
    readonly = 1;
  }

  if (od[i].fd >= 0) {
    od[i].inuse = 1;
    REG_A = 0;
    REG_F |= ZERO_MASK;
  } else {
    REG_A = errno;
    REG_F &= ~ZERO_MASK;
  }
  REG_DE = od[i].fd;
  REG_BC = readonly;
}

void do_emt_closedisk(void)
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
