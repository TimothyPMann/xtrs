/*
 * Copyright (C) 1992 Clarendon Hill Software.
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software, provided this copyright notice is retained. 
 *
 * This software is provided "as is" without any expressed or implied
 * warranty.  If this software brings on any sort of damage -- physical,
 * monetary, emotional, or brain -- too bad.  You've got no one to blame
 * but yourself. 
 *
 * The software may be modified for your own purposes, but modified versions
 * must retain this notice.
 */

#include "z80.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

extern char *program_name;

void debug(const char *fmt, ...)
{
  va_list args;
  char xfmt[2048];

  strcpy(xfmt, "debug: ");
  strcat(xfmt, fmt);
  /*strcat(xfmt, "\n");*/
  va_start(args, fmt);
  vfprintf(stderr, xfmt, args);
  va_end(args);
}

void error(const char *fmt, ...)
{
  va_list args;
  char xfmt[2048];

  strcpy(xfmt, program_name);
  strcat(xfmt, " error: ");
  strcat(xfmt, fmt);
  strcat(xfmt, "\n");
  va_start(args, fmt);
  vfprintf(stderr, xfmt, args);
  va_end(args);
}

void fatal(const char *fmt, ...)
{
  va_list args;
  char xfmt[2048];

  strcpy(xfmt, program_name);
  strcat(xfmt, " fatal error: ");
  strcat(xfmt, fmt);
  strcat(xfmt, "\n");
  va_start(args, fmt);
  vfprintf(stderr, xfmt, args);
  va_end(args);
  exit(1);
}
