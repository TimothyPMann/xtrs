/* Copyright (c) 1996-98, Timothy Mann */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/*
 * Last modified on Fri Apr 24 03:52:04 PDT 1998 by mann    
 */

/* TRS-80 DOS /cmd file loader.
 *
 * See the LDOS Quarterly, April 1, 1982 (Vol 1, No 4), for documentation
 *  of the TRS-80 DOS /cmd file format. 
 */

#include <stdio.h>
#include <string.h>
#include "load_cmd.h"

#define STATE_NORMAL 0
#define STATE_

int
load_cmd(FILE* f, unsigned char memory[1<<16],
	 unsigned char* loadmap, int verbosity, FILE* outf,
	 int isam, char* pds, int* xferaddr, int stopxfer)
{
  int lastaddr = -1, lastcount = -1;
  int c, a1, a2, a3, v, count, dummy, i;
  int pflags1, pflags2;
  char pentry[9];
  int ientry, iaddr, iseek, ilen;
  int status = LOAD_CMD_OK;
  unsigned short addr; /* wrap at 2**16 */

  if (loadmap) {
    memset(loadmap, 0, 1<<16);
  }
  if (xferaddr) {
    *xferaddr = -1;
  } else {
    xferaddr = &dummy;
  }
  
  for (;;) {
    c = getc(f); /* get block type code */
    if (c != 1 /* not loading bytes into memory */) {
      if (verbosity >= VERBOSITY_MAP) {
	if (lastaddr != -1) {
	  fprintf(outf, "loaded 0x%04x - 0x%04x\n",
		  lastaddr, lastaddr + lastcount - 1);
	}
	lastaddr = -1;
      }
    }
    if (c == EOF) return status;
    count = getc(f);
    if (count == EOF) {
      if (c == 3) return status;
      return LOAD_CMD_EOF;
    }
    if (count == 0) count = 256;

    switch (c) {
    case 0: /* skip (used by Model I TRSDOS BOOT/SYS easter egg) */
      if (verbosity >= VERBOSITY_MAP) {
	fprintf(outf, "skip block, %d bytes\n", count);
      }
      while (count-- > 0) {
	v = getc(f);
	if (v == EOF) return LOAD_CMD_EOF;
      }
      break;

    case 1: /* load bytes into memory */
      a1 = getc(f);
      if (a1 == EOF) return LOAD_CMD_EOF;
      a2 = getc(f);
      if (a2 == EOF) return LOAD_CMD_EOF;
      addr = a1 + a2 * 256;
      count -= 2;
      if (count <= 0) count += 256;
      if (verbosity >= VERBOSITY_MAP) {
	if (verbosity >= VERBOSITY_DETAILED ||
	    addr != lastaddr + lastcount) {
	  if (lastaddr != -1) {
	    fprintf(outf, "loaded 0x%04x - 0x%04x\n",
		    lastaddr, lastaddr + lastcount - 1);
	  }
	  lastaddr = addr;
	  lastcount = count;
	} else {
	  lastcount += count;
	}
      }
      while (count-- > 0) {
	v = getc(f);
	if (v == EOF) return LOAD_CMD_EOF;
	/* Don't load PDS front end if specific member requested */
	if (!(pds && isam == -1)) {
	  memory[addr] = v;
	  if (loadmap) loadmap[addr]++;
	}
	addr++;
      }
      break;

    case 2: /* transfer address and end of file */
      if (count != 2) return c;
      a1 = getc(f);
      if (a1 == EOF) return LOAD_CMD_EOF;
      a2 = getc(f);
      if (a2 == EOF) return LOAD_CMD_EOF;
      *xferaddr = a1 + a2 * 256;
      if (verbosity >= VERBOSITY_MAP) {
	fprintf(outf, "transfer address = 0x%04x\n", *xferaddr);
      }
      if (stopxfer) return status;
      break;

    case 3: /* end of file with no transfer address */
      while (count-- > 0) {
	v = getc(f);
	if (v == EOF) return LOAD_CMD_EOF;
      }
      return status;

    case 4: /* end of ISAM/PDS member */
      if (verbosity >= VERBOSITY_MAP) {
	fprintf(outf, "end of ISAM/PDS member\n");
      }
      while (count-- > 0) {
	v = getc(f);
	if (v == EOF) return LOAD_CMD_EOF;
      }
      if (isam != -1) return status;
      break;
	    
    case 5: /* module header */
      if (verbosity >= VERBOSITY_TEXT) {
	fprintf(outf, "module header = \"");
	while (count-- > 0) {
	  v = getc(f);
	  if (v == EOF) return LOAD_CMD_EOF;
	  putc(v, outf);
	}
	fprintf(outf, "\"\n");
      } else {
	while (count-- > 0) {
	  v = getc(f);
	  if (v == EOF) return LOAD_CMD_EOF;
	}
      }
      break;

    case 6: /* PDS header */
      if (verbosity >= VERBOSITY_TEXT) {
	fprintf(outf, "PDS header = \"");
	while (count-- > 0) {
	  v = getc(f);
	  if (v == EOF) return LOAD_CMD_EOF;
	  putc(v, outf);
	}
	fprintf(outf, "\"\n");
      } else {
	while (count-- > 0) {
	  v = getc(f);
	  if (v == EOF) return LOAD_CMD_EOF;
	}
      }
      break;

    case 7: /* Patch name header */
      if (verbosity >= VERBOSITY_TEXT) {
	fprintf(outf, "patch name = \"");
	while (count-- > 0) {
	  v = getc(f);
	  if (v == EOF) return LOAD_CMD_EOF;
	  putc(v, outf);
	}
	fprintf(outf, "\"\n");
      } else {
	while (count-- > 0) {
	  v = getc(f);
	  if (v == EOF) return LOAD_CMD_EOF;
	}
      }
      break;

    case 8: /* ISAM directory entry */
      if (status == LOAD_CMD_OK) status = LOAD_CMD_ISAM;
      if (count == 6 || count == 9) {
	v = getc(f);
	if (v == EOF) return LOAD_CMD_EOF;
	ientry = v;
	a1 = getc(f);
	if (a1 == EOF) return LOAD_CMD_EOF;
	a2 = getc(f);
	if (a2 == EOF) return LOAD_CMD_EOF;
	iaddr = a1 + a2 * 256;
	a1 = getc(f);
	if (a1 == EOF) return LOAD_CMD_EOF;
	a2 = getc(f);
	if (a2 == EOF) return LOAD_CMD_EOF;
	a3 = getc(f);
	if (a3 == EOF) return LOAD_CMD_EOF;
	iseek = (a2 << 16) + (a1 << 8) + a3;
      } else {
	return c;
      }
      if (count == 9) {
	/* Is this right? */
	a1 = getc(f);
	if (a1 == EOF) return LOAD_CMD_EOF;
	a2 = getc(f);
	if (a2 == EOF) return LOAD_CMD_EOF;
	a3 = getc(f);
	if (a3 == EOF) return LOAD_CMD_EOF;
	ilen = (a2 << 16) + (a1 << 8) + a3; /*???*/
      } else {
	ilen = -1;
      }
      if (verbosity >= VERBOSITY_MAP) {
	fprintf(outf, "ISAM entry 0x%02x, transfer 0x%04x, seek ptr 0x%06x",
		ientry, iaddr, iseek);
	if (ilen != -1) {
	  fprintf(outf, ", length 0x%06x\n", ilen);
	} else {
	  fprintf(outf, "\n");
	}
      }
      if (ientry == isam) {
	fseek(f, iseek, 0);
	*xferaddr = iaddr;
      }
      break;
	    
    case 0x0a: /* end of ISAM directory */
      if (verbosity >= VERBOSITY_MAP) {
	fprintf(outf, "end of ISAM directory\n");
      }
      while (count-- > 0) {
	v = getc(f);
	if (v == EOF) return LOAD_CMD_EOF;
      }
      if (isam != -1) return LOAD_CMD_NOT_FOUND;
      break;

    case 0x0c: /* PDS directory entry */
      status = LOAD_CMD_PDS;
      if (count != 11) {
	return c;
      }
      for (i=0; i<8; i++) {
	v = getc(f);
	if (v == EOF) return LOAD_CMD_EOF;
	pentry[i] = v;
      }
      pentry[8] = '\000';
      v = getc(f);
      if (v == EOF) return LOAD_CMD_EOF;
      ientry = v;
      v = getc(f);
      if (v == EOF) return LOAD_CMD_EOF;
      pflags1 = v;
      v = getc(f);
      if (v == EOF) return LOAD_CMD_EOF;
      pflags2 = v;
      if (pds && strcmp(pds, pentry) == 0) {
	isam = ientry;
      }
      if (verbosity >= VERBOSITY_MAP) {
	fprintf(outf, "PDS entry \"%s\", ISAM 0x%02x, flags 0x%02x 0x%02x\n",
		pentry, ientry, pflags1, pflags2);
      }
      break;

    case 0x0e: /* end of PDS directory */
      if (verbosity >= VERBOSITY_MAP) {
	fprintf(outf, "end of PDS directory\n");
      }
      while (count-- > 0) {
	v = getc(f);
	if (v == EOF) return LOAD_CMD_EOF;
      }
      if (pds != NULL && isam == -1) return LOAD_CMD_NOT_FOUND;
      break;

    case 0x10: /* yanked load block */
      a1 = getc(f);
      if (a1 == EOF) return LOAD_CMD_EOF;
      a2 = getc(f);
      if (a2 == EOF) return LOAD_CMD_EOF;
      addr = a1 + a2 * 256;
      count -= 2;
      if (count <= 0) count += 256;
      if (verbosity >= VERBOSITY_MAP) {
	fprintf(outf, "yanked load block 0x%04x - 0x%04x\n",
		addr, addr + count - 1);
      }
      while (count-- > 0) {
	v = getc(f);
	if (v == EOF) return LOAD_CMD_EOF;
      }
      break;

    case 0x1f: /* copyright block */
      if (verbosity >= VERBOSITY_TEXT) {
	fprintf(outf, "====copyright block====\n");
	while (count-- > 0) {
	  v = getc(f);
	  if (v == EOF) return LOAD_CMD_EOF;
	  putc(v, outf);
	}
	fprintf(outf, "\n=======================\n");
      } else {
	while (count-- > 0) {
	  v = getc(f);
	  if (v == EOF) return LOAD_CMD_EOF;
	}
      }
      break;

    default:
      return c;
    }
  }
  return status;
}
