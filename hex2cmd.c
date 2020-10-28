/* Convert Intel Hex format to TRS-80 CMD format */

/* 
 * Copyright (c) 1996-2008, Timothy P. Mann
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "cmd.h"
#include "z80.h"

char *program_name;

/* Called by load_hex */
void
hex_data(int address, int value)
{
    cmd_data(address, value);
}

void
hex_transfer_address(int address)
{
    cmd_transfer_address(address);
}

int
main(int argc, char *argv[])
{
    FILE *f;
    program_name = argv[0];
    cmd_init(stdout);
    if (argc == 1) {
	f = stdin;
    } else if (argc == 2) {
	f = fopen(argv[1], "r");
	if (f == NULL) {
	    perror(argv[1]);
	    exit(1);
	}
    } else {
	fprintf(stderr, "Usage: %s [<] file.hex > file.cmd\n",
		program_name);
	exit(2);
    }
    load_hex(f);
    cmd_end_of_file();
    return 0;
}
