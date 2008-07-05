/* Copyright (c) 2008, Timothy Mann */
/* $Id$ */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/*
 * Emulate Exatron stringy floppy.
 * XXX Not really started yet
 */

#include "z80.h"
#include "trs.h"

#define STRINGYDEBUG_IN 1
#define STRINGYDEBUG_OUT 1

int
stringy_read(int unit)
{
#if STRINGYDEBUG_IN
  debug("stringy_read(%d)\n", unit);
#endif  
  return 0xff;
}

void
stringy_write(int unit, int value)
{
#if STRINGYDEBUG_OUT
  debug("stringy_write(%d, %d)\n", unit, value);
#endif  
}
