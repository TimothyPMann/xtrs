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

/* 
 * Portions copyright (c) 1996-2008, Timothy P. Mann
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

void warning(const char *fmt, ...)
{
  va_list args;
  char xfmt[2048];

  strcpy(xfmt, program_name);
  strcat(xfmt, " warning: ");
  strcat(xfmt, fmt);
  strcat(xfmt, "\n");
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
