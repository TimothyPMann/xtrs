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
   Last modified on Tue Sep 30 17:58:39 PDT 1997 by mann
*/

#include "z80.h"
#include "trs.h"

#include <malloc.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#ifdef READLINE
#include <readline/readline.h>
#endif

/*SUPPRESS 112*/

#define MAXLINE		(256)
#define ADDRESS_SPACE	(0x10000)
#define MAX_TRAPS	(100)

#define BREAKPOINT_FLAG		(0x1)
#define TRACE_FLAG		(0x2)
#define DISASSEMBLE_ON_FLAG	(0x4)
#define DISASSEMBLE_OFF_FLAG	(0x8)
#define BREAK_ONCE_FLAG		(0x10)

static Uchar *traps;
static int num_traps;
static int print_instructions;
static int stop_signaled;
extern char help_message[];  /* defined at the end */

static struct
{
    int valid;
    int address;
    int flag;
} trap_table[MAX_TRAPS];


static char *trap_name(flag)
    int flag;
{
    switch(flag)
    {
      case BREAKPOINT_FLAG:
	return "breakpoint";
      case TRACE_FLAG:
	return "trace";
      case DISASSEMBLE_ON_FLAG:
	return "traceon";
      case DISASSEMBLE_OFF_FLAG:
	return "traceoff";
      case BREAK_ONCE_FLAG:
	return "temporary breakpoint";
      default:
	return "unknown trap";
    }
}

static void clear_all_traps()
{
    int i;
    for(i = 0; i < MAX_TRAPS; ++i)
    {
	if(trap_table[i].valid)
	{
	    traps[trap_table[i].address] &= ~(trap_table[i].flag);
	    trap_table[i].valid = 0;
	}
    }
    num_traps = 0;
}

static void print_traps()
{
    int i;

    if(num_traps)
    {
	for(i = 0; i < MAX_TRAPS; ++i)
	{
	    if(trap_table[i].valid)
	    {
		printf("[%d] %.4x (%s)\n", i, trap_table[i].address,
		       trap_name(trap_table[i].flag));
	    }
	}
    }
    else
    {
	printf("No traps are set.\n");
    }
}

static void set_trap(address, flag)
    int address, flag;
{
    int i;

    if(num_traps == MAX_TRAPS)
    {
	printf("Cannot set another trap.\n");
    }
    else
    {
	i = 0;
	while(trap_table[i].valid) ++i;
	
	trap_table[i].valid = 1;
	trap_table[i].address = address;
	trap_table[i].flag = flag;
	traps[address] |= flag;
	num_traps++;

	printf("Set %s [%d] at %.4x\n", trap_name(flag), i, address);
    }
}

static void clear_trap(i)
    int i;
{
    if((i < 0) || (i > MAX_TRAPS) || !trap_table[i].valid)
    {
	printf("[%d] is not a valid trap.\n", i);
    }
    else
    {
	traps[trap_table[i].address] &= ~(trap_table[i].flag);
	trap_table[i].valid = 0;
	num_traps--;
	printf("Cleared %s [%d] at %.4x\n",
	       trap_name(trap_table[i].flag), i, trap_table[i].address);
    }
}

static void clear_trap_address(address, flag)
    int address;
{
    int i;
    for(i = 0; i < MAX_TRAPS; ++i)
    {
	if(trap_table[i].valid && (trap_table[i].address == address)
	   && ((flag == 0) || (trap_table[i].flag == flag)))
	{
	    clear_trap(i);
	}
    }
}

void debug_print_registers()
{
    printf("\n       S Z   H  PV N C\n");
    printf("Flags: %d %d   %d   %d %d %d\n\n",
	   (SIGN_FLAG != 0),
	   (ZERO_FLAG != 0),
	   (HALF_CARRY_FLAG != 0),
	   (OVERFLOW_FLAG != 0),
	   (SUBTRACT_FLAG != 0),
	   (CARRY_FLAG != 0));

    printf("A F: %.2x %.2x    IX: %.4x        AF': %.4x\n",
	   REG_A, REG_F, REG_IX, REG_AF_PRIME);
    printf("B C: %.2x %.2x    IY: %.4x        BC': %.4x\n",
	   REG_B, REG_C, REG_IY, REG_BC_PRIME);
    printf("D E: %.2x %.2x    PC: %.4x        DE': %.4x\n",
	   REG_D, REG_E, REG_PC, REG_DE_PRIME);
    printf("H L: %.2x %.2x    SP: %.4x        HL': %.4x\n",
	   REG_H, REG_L, REG_SP, REG_HL_PRIME);
}


static void signal_handler()
{
    stop_signaled = 1;
}

void trs_debug()
{
    stop_signaled = 1;
}

void debug_init()
{
    int i;

    traps = (Uchar *) malloc(ADDRESS_SPACE * sizeof(Uchar));
    bzero(traps, ADDRESS_SPACE * sizeof(Uchar));

    for(i = 0; i < MAX_TRAPS; ++i) trap_table[i].valid = 0;
}

static void print_memory(address, num_bytes)
    Ushort address;
    int num_bytes;
{
    int bytes_to_print, i;
    int byte;

    while(num_bytes > 0)
    {
	bytes_to_print = 16;
	if(bytes_to_print > num_bytes) bytes_to_print = num_bytes;

	printf("%.4x:\t", address);
	for(i = 0; i < bytes_to_print; ++i)
	{
	    printf("%.2x ", mem_read((address + i) & 0xffff));
	}
	for(i = bytes_to_print; i < 16; ++i)
	{
	    printf("   ");
	}
	printf("    ");
	for(i = 0; i < bytes_to_print; ++i)
	{
	    byte = mem_read((address + i) & 0xffff);
	    if(isprint(byte))
	    {
		printf("%c", byte);
	    }
	    else
	    {
		printf(".");
	    }
	}
	printf("\n");
	num_bytes -= bytes_to_print;
	address += bytes_to_print;
    }
}

static void debug_run()
{
    void (*old_signal_handler)();
    Uchar t;

    /* catch control-c signal */
    old_signal_handler = signal(SIGINT, signal_handler);

    stop_signaled = 0;

    t = traps[REG_PC];
    while(!stop_signaled)
    {
	if(t)
	{
	    if(t & TRACE_FLAG)
	    {
		printf("Trace: ");
		disassemble(REG_PC);
	    }
	    if(t & DISASSEMBLE_ON_FLAG)
	    {
		print_instructions = 1;
	    }
	    if(t & DISASSEMBLE_OFF_FLAG)
	    {
		print_instructions = 0;
	    }
	}
	
	if(print_instructions) disassemble(REG_PC);
	
	if(z80_run(0))
	{
	    /* halt instruction */
	    printf("Halt instruction executed.\n");
	    stop_signaled = 1;
	}

	t = traps[REG_PC];
	if(t & BREAKPOINT_FLAG)
	{
	    stop_signaled = 1;
	}
	if(t & BREAK_ONCE_FLAG)
	{
	    stop_signaled = 1;
	    clear_trap_address(REG_PC, BREAK_ONCE_FLAG);
        }
    }
    signal(SIGINT, old_signal_handler);
    printf("Stopped at %.4x\n", REG_PC);
}

void debug_shell()
{
    char input[MAXLINE];
    char command[MAXLINE];
    int done = 0;

#ifdef READLINE
    char *line;
    char history_file[MAXLINE];
    char *home = (char *)getenv ("HOME");
    if (!home) home = ".";
    sprintf (history_file, "%s/.zbx-history", home);
    read_history(history_file);
#endif READLINE

    z80_reset();

    while(!done)
    {
	printf("\n");
	disassemble(REG_PC);

#ifdef READLINE
	/*
	 * Use the way cool gnu readline() utility.  Get completion, history,
	 * way way cool.   **Doesn't work now that we use SIGALRM --tpm**
	 */
        {

	    line = readline("(zbx) ");
	    if(line)
	    {
		if(strlen(line) > 0)
		{
		    add_history(line);
		}
		strncpy(input, line, MAXLINE - 1);
		free(line);
	    }
	    else
	    {
		done = 1;
	    }
	}
#else
	printf("(zbx) ");  fflush(stdout);

	if (fgets(input, MAXLINE, stdin) == NULL) 
	  done = 1;
#endif

	if(sscanf(input, "%s", command))
	{
	    if(!strcmp(command, "help") || !strcmp(command, "?"))
	    {
		printf(help_message);
	    }
	    else if(!strcmp(command, "clear"))
	    {
		clear_trap_address(REG_PC, 0);
	    }
	    else if(!strcmp(command, "cont"))
	    {
		debug_run();
	    }
	    else if(!strcmp(command, "dump"))
	    {
		debug_print_registers();
	    }
	    else if(!strcmp(command, "delete"))
	    {
		int i;

		if(!strcmp(input, "delete *"))
		{
		    clear_all_traps();
		}
		else if(sscanf(input, "delete %d", &i) != 1)
		{
		    printf("A trap must be specified.\n");
		}
		else
		{
		    clear_trap(i);
		}
	    }
	    else if(!strcmp(command, "list"))
	    {
		int x, y;
		Ushort start, end = 0;
		int lines = 0;

		if(sscanf(input, "list %x , %x", &x, &y) == 2)
		{
		    start = x;
		    end = y;
		}
		else if(sscanf(input, "list %x", &x) == 1)
		{
		    start = x;
		    lines = 10;
		}
		else
		{
		    start = REG_PC;
		    lines = 10;
		}

		if(lines)
		{
		    while(lines--)
		    {
			start = disassemble(start);
		    }
		}
		else
		{
		    while(start <= end)
		    {
			start = disassemble(start);
		    }
		}
	    }
	    else if(!strcmp(command, "next") || !strcmp(command, "nextint"))
	    {
		int is_call;
		switch(mem_read(REG_PC)) {
		  case 0xCD:	/* call address */
		    is_call = 1;
		    break;
		  case 0xC4:	/* call nz, address */
		    is_call = !ZERO_FLAG;
		    break;
		  case 0xCC:	/* call z, address */
		    is_call = ZERO_FLAG;
		    break;
		  case 0xD4:	/* call nc, address */
		    is_call = !CARRY_FLAG;
		    break;
		  case 0xDC:	/* call c, address */
		    is_call = CARRY_FLAG;
		    break;
		  case 0xE4:	/* call po, address */
		    is_call = !PARITY_FLAG;
		    break;
		  case 0xEC:	/* call pe, address */
		    is_call = PARITY_FLAG;
		    break;
		  case 0xF4:	/* call p, address */
		    is_call = !SIGN_FLAG;
		    break;
		  case 0xFC:	/* call m, address */
		    is_call = SIGN_FLAG;
		    break;
		  default:
		    is_call = 0;
		    break;
		}
		if (is_call) {
		    set_trap((REG_PC + 3) % ADDRESS_SPACE, BREAK_ONCE_FLAG);
		    debug_run();
		} else {
		    z80_run((!strcmp(command, "nextint")) ? 0 : -1);
		}
	    }
	    else if(!strcmp(command, "quit"))
	    {
		done = 1;
	    }
	    else if(!strcmp(command, "reset"))
	    {
		printf("Resetting.");
		z80_reset();
	    }
	    else if(!strcmp(command, "run"))
	    {
		printf("Running.\n");
		z80_reset();
		debug_run();
	    }
	    else if(!strcmp(command, "status"))
	    {
		print_traps();
	    }
	    else if(!strcmp(command, "set") || !strcmp(command, "assign"))
	    {
		char regname[MAXLINE];
		int addr, value;

		if(sscanf(input, "%*s $%[a-zA-Z] = %x", regname, &value) == 2)
		{
		    if(!strcasecmp(regname, "a")) {
			REG_A = value;
		    } else if(!strcasecmp(regname, "f")) {
			REG_F = value;
		    } else if(!strcasecmp(regname, "b")) {
			REG_B = value;
		    } else if(!strcasecmp(regname, "c")) {
			REG_C = value;
		    } else if(!strcasecmp(regname, "d")) {
			REG_D = value;
		    } else if(!strcasecmp(regname, "e")) {
			REG_E = value;
		    } else if(!strcasecmp(regname, "h")) {
			REG_H = value;
		    } else if(!strcasecmp(regname, "l")) {
			REG_L = value;
		    } else if(!strcasecmp(regname, "sp")) {
			REG_SP = value;
		    } else if(!strcasecmp(regname, "pc")) {
			REG_PC = value;
		    } else if(!strcasecmp(regname, "af")) {
			REG_AF = value;
		    } else if(!strcasecmp(regname, "bc")) {
			REG_BC = value;
		    } else if(!strcasecmp(regname, "de")) {
			REG_DE = value;
		    } else if(!strcasecmp(regname, "hl")) {
			REG_HL = value;
		    } else if(!strcasecmp(regname, "af'")) {
			REG_AF_PRIME = value;
		    } else if(!strcasecmp(regname, "bc'")) {
			REG_BC_PRIME = value;
		    } else if(!strcasecmp(regname, "de'")) {
			REG_DE_PRIME = value;
		    } else if(!strcasecmp(regname, "hl'")) {
			REG_HL_PRIME = value;
		    } else if(!strcasecmp(regname, "ix")) {
			REG_IX = value;
		    } else if(!strcasecmp(regname, "iy")) {
			REG_IY = value;
		    } else if(!strcasecmp(regname, "i")) {
			REG_I = value;
		    } else {
			printf("Unrecognized register name %s.\n", regname);
		    }
		}
		else if(sscanf(input, "%*s %x = %x", &addr, &value) == 2)
		{
		    mem_write(addr, value);
		}
		else 
		{
		    printf("Syntax error.  (Type \"help\" for commands.)\n");
		}
	    }
	    else if(!strcmp(command, "step"))
	    {
		z80_run(-1);
	    }
	    else if(!strcmp(command, "stepint"))
	    {
		z80_run(0);
	    }
	    else if(!strcmp(command, "stop"))
	    {
		int address;

		if(sscanf(input, "stop at %x", &address) != 1)
		{
		    address = REG_PC;
		}
		address %= ADDRESS_SPACE;
		set_trap(address, BREAKPOINT_FLAG);
	    }
	    else if(!strcmp(command, "trace"))
	    {
		int address;

		if(sscanf(input, "trace %x", &address) != 1)
		{
		    address = REG_PC;
		}
		address %= ADDRESS_SPACE;
		set_trap(address, TRACE_FLAG);
	    }
	    else if(!strcmp(command, "untrace"))
	    {
		printf("Untrace not implemented.\n");
	    }
	    else if(!strcmp(command, "traceon"))
	    {
		int address;

		if(sscanf(input, "traceon at %x", &address) == 1)
		{
		    set_trap(address, DISASSEMBLE_ON_FLAG);
		}
		else
		{
		    print_instructions = 1;
		    printf("Tracing enabled.\n");
		}
	    }
	    else if(!strcmp(command, "traceoff"))
	    {
		int address;

		if(sscanf(input, "traceoff at %x", &address) == 1)
		{
		    set_trap(address, DISASSEMBLE_OFF_FLAG);
		}
		else
		{
		    print_instructions = 0;
		    printf("Tracing disabled.\n");
		}
	    }
	    else if(!strcmp(command, "timeroff"))
	    {
	        /* Turn off emulated real time clock interrupt */
	        trs_timer_off();
            }
	    else if(!strcmp(command, "timeron"))
	    {
	        /* Turn off emulated real time clock interrupt */
	        trs_timer_on();
            }
	    else
	    {
		int start_address, end_address, num_bytes;

		if(sscanf(input, "%x , %x / ", &start_address, &end_address) == 2)
		{
		    print_memory(start_address, end_address - start_address);
		}
		else if(sscanf(input, "%x / %x ", &start_address, &num_bytes) == 2)
		{
		    print_memory(start_address, num_bytes);
		}
		else if(sscanf(input, "%x = ", &start_address) == 1)
		{
		    print_memory(start_address, 1);
		}
		else
		{
		    printf("Syntax error.  (Type \"help\" for commands.)\n");
		}
	    }
	}
    }
#ifdef READLINE
    write_history(history_file);
#endif
}

static char help_message[] =

"(zbx) commands:\n\
\n\
Running:\n\
    run\n\
        Reset the Z-80 and commence execution at address 0000.\n\
    cont\n\
        Continue execution.\n\
    step\n\
        Execute one instruction, disallowing interrupts.\n\
    stepint\n\
        Execute one instruction, allowing an interrupt afterwards.\n\
    next\n\
    nextint\n\
        Execute one instruction.  If the instruction is a CALL, continue\n\
        until the return.  Interrupts are always allowed inside the call,\n\
        but only the nextint form allows an interrupt afterwards.\n\
    reset\n\
        Reset the Z-80.\n\
    timeroff\n\
    timeron\n\
        Disable/enable the emulated TRS-80 real time clock interrupt.\n\
Printing:\n\
    dump\n\
        Print the values of the Z-80 registers.\n\
    list\n\
    list <addr>\n\
    list <start addr> , <end addr>\n\
        Disassemble 10 instructions at the current pc, 10 instructions at\n\
        the specified hex address, or the instructions in the range of hex\n\
        addresses.\n\
    <start addr> , <end addr> /\n\
    <start addr> / <num bytes>\n\
    <addr> =\n\
        Print the memory values in the specified range.  All values are hex.\n\
    traceon\n\
        Enable tracing of all instructions.\n\
    traceoff\n\
        Disable tracing.\n\
Traps:\n\
    status\n\
        Show all traps (breakpoints, trace points).\n\
    clear\n\
        Delete the trap at the current address.\n\
    delete <n>\n\
    delete *\n\
        Delete trap n, or all traps.\n\
    stop at <address>\n\
        Set a breakpoint at the specified hex address.\n\
    trace <address>\n\
        Set a trap to trace execution at the specified hex address.\n\
    traceon at <address>\n\
        Set a trap to enable tracing at the specified hex address.\n\
    traceoff at <address>\n\
        Set a trap to disable tracing at the specified hex address.\n\
Miscellaneous:\n\
    assign $<reg> = <value>\n\
    assign <addr> = <value>\n\
        Change the value of a register, register pair, or memory byte.\n\
    help\n\
    ?\n\
        Print this message.\n\
    quit\n\
        Exit the zbx.\n";

#ifdef NEVER

/*
 * Test code:
 */

carry_in(a, b, r)
{
    return (a ^ b ^ r);
}

carry_out(a, b, r)
{
    return (a & b) | ((a | b) & ~r);
}

overflow(a, b, r)
{
    return carry_in(a, b, r) ^ carry_out(a, b, r);
}

test_add(a, b)
    int a, b;
{
    Uchar result;
    Uchar flags;

    result = a + b;

    flags = 0;

    if(result & 0x80) flags |= SIGN_MASK;

    if(result == 0) flags |= ZERO_MASK;

    if(carry_out(a, b, result) & 0x8) flags |= HALF_CARRY_MASK;

    if(overflow(a, b, result) & 0x80) flags |= OVERFLOW_MASK;

    if(carry_out(a, b, result) & 0x80) flags |= CARRY_MASK;

    do_add_flags(a, b, result);
    if(REG_F != flags)
    {
	printf("error: %d + %d = %d, expected %.2x, got %.2x\n",
	       a, b, result, flags, REG_F);
    }
}

test_sub(a, b)
    int a, b;
{
    Uchar result;
    Uchar flags;

    result = a - b;
    
    flags = 0;

    if(result & 0x80) flags |= SIGN_MASK;

    if(result == 0) flags |= ZERO_MASK;

    if(carry_out(a, - b, result) & 0x8) flags |= HALF_CARRY_MASK;

    if(overflow(a, - b, result) & 0x80) flags |= OVERFLOW_MASK;

    flags |= SUBTRACT_MASK;

    if(carry_out(a, - b, result) & 0x80) flags |= CARRY_MASK;

    do_sub_flags(a, b, result);

    if(REG_F != flags)
    {
	printf("error: %d - %d = %d, expected %.2x, got %.2x\n",
	       a, b, result, flags, REG_F);
    }
}

test_all()
{
    int a, b;

    for(a = 0; a < 256; ++a)
    {
	for(b = 0; b < 256; ++b)
	{
	    test_add(a, b);
	    test_sub(a, b);
	}
    }
}

#endif
