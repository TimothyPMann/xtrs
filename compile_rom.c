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
   Modified by Timothy Mann, 1996
   Last modified on Sat Apr 25 00:57:31 PDT 1998 by mann
*/

#include "z80.h"
#include "load_cmd.h"

char *program_name;
static int highest_address = 0;
static Uchar memory[Z80_ADDRESS_LIMIT];
static Uchar loadmap[Z80_ADDRESS_LIMIT];

/* Called by load_hex */
void hex_data(int address, int value)
{
  address &= 0xffff;

  memory[address] = value;
  if(highest_address < address)
    highest_address = address;
}

void hex_transfer_address(int address)
{
  /* Ignore */
}

static void load_rom(char *filename)
{
  FILE *program;
  int c, a;
    
  if((program = fopen(filename, "r")) == NULL)
    {
      char message[100];
      sprintf(message, "could not read %s", filename);
      fatal(message);
    }
  c = getc(program);
  if (c == ':') {
    /* Assume Intel hex format */
    rewind(program);
    load_hex(program);
    fclose(program);
    return;
  } else if (c == 1 || c == 5) {
    /* Assume MODELA/III file */
    int res;
    rewind(program);
    res = load_cmd(program, memory, loadmap, 0, NULL, -1, NULL, NULL, 1);
    if (res == LOAD_CMD_OK) {
      highest_address = Z80_ADDRESS_LIMIT;
      while (highest_address > 0) {
	if (loadmap[--highest_address] != 0) {
	  break;
	}
      }
      fclose(program);
      return;
    } else {
      /* Guess it wasn't one */
      rewind(program);
      c = getc(program);
    } 
  }
  a = 0;
  while (c != EOF) {
    hex_data(a++, c);
    c = getc(program);
  }
}

static void write_output(char *which)
{
  int address = 0;
  int i;
    
  highest_address++;

  printf("int trs_rom%s_size = %d;\n", which, highest_address);
  printf("unsigned char trs_rom%s[%d] = \n{\n", which, highest_address);
    
  while(address < highest_address) 
    {
      printf("    ");
      for(i = 0; i < 8; ++i)
	{
	  printf("0x%.2x,", memory[address++]);

	  if(address == highest_address)
	    break;
	}
      printf("\n");
    }
  printf("};\n");
}

static void write_norom_output(char *which)
{
  printf("int trs_rom%s_size = -1;\n", which);
  printf("unsigned char trs_rom%s[1];\n", which);
}

int main(int argc, char *argv[])
{
  program_name = argv[0];
  if(argc == 2)
    {
      fprintf(stderr,
        "%s: no specified ROM file, ROM %s will not be built into program.\n",
	program_name, argv[1]);
      write_norom_output(argv[1]);
    }
  else if(argc != 3)
    {
      fprintf(stderr, "Usage: %s model hexfile", program_name);
    }
  else
    {
      load_rom(argv[2]);
      write_output(argv[1]);
    }
  return 0;
}
