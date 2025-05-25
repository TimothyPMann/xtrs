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
 * Portions copyright (c) 1996-2020, Timothy P. Mann
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

#define _POSIX_C_SOURCE 200112L /* signal.h: sigemptyset(), ... */

#include "z80.h"
#include "trs.h"

#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <strings.h> /* strcasecmp() */

#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>
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
#define WATCHPOINT_FLAG		(0x20)

static Uchar *traps;
static int num_traps;
static int print_instructions;
static int stop_signaled;
static unsigned int num_watchpoints = 0;

static char help_message[] =

"(zbx) commands:\n\
\n\
Running:\n\
    run\n\
        Hard reset the Z80 and devices and commence execution.\n\
    cont\n\
        Continue execution.\n\
    step\n\
    stepint\n\
        Execute one instruction, or if the instruction is repeating (such as\n\
        LDIR), execute only one iteration.  With \"step\", an interrupt is\n\
        not allowed to occur after the instruction; with \"stepint\", an\n\
        interrupt is allowed.\n\
    next\n\
    nextint\n\
        Execute one instruction.  If the instruction is a CALL, continue\n\
        until the return.  If the instruction is repeating (such as LDIR),\n\
        continue until it finishes.  Interrupts are always allowed during\n\
        execution, but only \"nextint\" allows an interrupt afterwards.\n\
    reset\n\
        Hard reset the Z80 and devices.\n\
    softreset\n\
        Press the system reset button.  On Model I/III, softreset resets the\n\
        devices and posts a nonmaskable interrupt to the CPU; on Model 4/4P,\n\
        softreset is the same as hard reset.\n\
Printing:\n\
    dump\n\
        Print the values of the Z80 registers.\n\
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
    diskdump\n\
        Print the state of the floppy disk controller emulation.\n\
Traps:\n\
    status\n\
        Show all traps (breakpoints, tracepoints, watchpoints).\n\
    clear\n\
        Delete the trap at the current address.\n\
    delete <n>\n\
    delete *\n\
        Delete trap n, or all traps.\n\
    stop at <address>\n\
    break <address>\n\
        Set a breakpoint at the specified hex address.\n\
    trace <address>\n\
        Set a trap to trace execution at the specified hex address.\n\
    traceon at <address>\n\
        Set a trap to enable tracing at the specified hex address.\n\
    traceoff at <address>\n\
        Set a trap to disable tracing at the specified hex address.\n\
    watch <address>\n\
        Set a trap to watch specified hex address for changes.\n\
Miscellaneous:\n\
    assign $<reg> = <value>\n\
    assign <addr> = <value>\n\
    set $<reg> = <value>\n\
    set <addr> = <value>\n\
        Change the value of a register, register pair, or memory byte.\n\
    in <port>\n\
        Input from the given I/O port.\n\
    out <port> = <value>\n\
        Output to the given I/O port.\n\
    timeroff\n\
    timeron\n\
        Disable/enable the emulated TRS-80 real time clock interrupt.\n\
    diskdebug <hexval>\n\
        Set floppy disk controller debug flags to hexval.\n\
        1=FDC register I/O, 2=FDC commands, 4=VTOS 3.0 JV3 kludges, 8=Gaps,\n\
        10=Phys sector sizes, 20=Readadr timing, 40=DMK, 80=ioctl errors.\n\
    iodebug <hexval>\n\
        Set I/O port debug flags to hexval: 1=port input, 2=port output.\n\
    zbxinfo\n\
        Display information about this debugger.\n\
    help\n\
    ?\n\
        Print this message.\n\
    quit\n\
        Exit from xtrs.\n";

static struct
{
    int   valid;
    int   address;
    int   flag;
    Uchar byte; /* used only by watchpoints */
} trap_table[MAX_TRAPS];

static char *trap_name(int flag)
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
      case WATCHPOINT_FLAG:
	return "watchpoint";
      default:
	return "unknown trap";
    }
}

static void show_zbxinfo(void)
{
    printf("zbx: Z80 debugger by David Gingold, Alex Wolman, and Timothy"
           " Mann\n");
    printf("\n");
    printf("Traps set: %d (maximum %d)\n", num_traps, MAX_TRAPS);
    printf("Size of address space: 0x%x\n", ADDRESS_SPACE);
    printf("Maximum length of command line: %d\n", MAXLINE);
#ifdef READLINE
    printf("GNU Readline library support enabled.\n");
#else
    printf("GNU Readline library support disabled.\n");
#endif
}

static void clear_all_traps(void)
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
    num_watchpoints = 0;
}

static void print_traps(void)
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

static void set_trap(int address, int flag)
{
    int i;

    if(num_traps == MAX_TRAPS)
    {
	printf("Cannot set more than %d traps.\n", MAX_TRAPS);
    }
    else
    {
	i = 0;
	while(trap_table[i].valid) ++i;
	
	trap_table[i].valid = 1;
	trap_table[i].address = address;
	trap_table[i].flag = flag;
	if (trap_table[i].flag == WATCHPOINT_FLAG) {
	    /* Initialize the byte field to current memory contents. */
	    trap_table[i].byte = mem_read(address);
	    /* Increment number of set watchpoints. */
	    num_watchpoints++;
	}
	traps[address] |= flag;
	num_traps++;

	printf("Set %s [%d] at %.4x\n", trap_name(flag), i, address);
    }
}

static void clear_trap(int i)
{
    if((i < 0) || (i >= MAX_TRAPS) || !trap_table[i].valid)
    {
	printf("[%d] is not a valid trap.\n", i);
    }
    else
    {
	traps[trap_table[i].address] &= ~(trap_table[i].flag);
	trap_table[i].valid = 0;
	if (trap_table[i].flag == WATCHPOINT_FLAG) {
	    /* Decrement number of set watchpoints. */
	    num_watchpoints--;
	}
	num_traps--;
	printf("Cleared %s [%d] at %.4x\n",
	       trap_name(trap_table[i].flag), i, trap_table[i].address);
    }
}

static void clear_trap_address(int address, int flag)
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

void debug_print_registers(void)
{
    printf("\n       S Z - H - PV N C   IFF1 IFF2 IM\n");
    printf("Flags: %d %d %d %d %d  %d %d %d     %d    %d   %d\n\n",
	   (SIGN_FLAG != 0),
	   (ZERO_FLAG != 0),
	   (REG_F & UNDOC5_MASK) != 0,
	   (HALF_CARRY_FLAG != 0),
	   (REG_F & UNDOC3_MASK) != 0,
	   (OVERFLOW_FLAG != 0),
	   (SUBTRACT_FLAG != 0),
	   (CARRY_FLAG != 0),
	   z80_state.iff1, z80_state.iff2, z80_state.interrupt_mode);

    printf("A F: %.2x %.2x    IX: %.4x    AF': %.4x\n",
	   REG_A, REG_F, REG_IX, REG_AF_PRIME);
    printf("B C: %.2x %.2x    IY: %.4x    BC': %.4x\n",
	   REG_B, REG_C, REG_IY, REG_BC_PRIME);
    printf("D E: %.2x %.2x    PC: %.4x    DE': %.4x\n",
	   REG_D, REG_E, REG_PC, REG_DE_PRIME);
    printf("H L: %.2x %.2x    SP: %.4x    HL': %.4x\n",
	   REG_H, REG_L, REG_SP, REG_HL_PRIME);
    printf("I R: %.2x %.2x\n", REG_I, REG_R7 | (REG_R & 0x7f));

    printf("\nT-state counter: %" TSTATE_T_LEN "    ", z80_state.t_count);
    printf("Delay setting: %d + %d (%s)\n", z80_state.delay,
	   z80_state.keydelay, trs_autodelay ? "auto" : "fixed");
}


static void signal_handler(int sig)
{
    stop_signaled = 1;
    if (trs_continuous > 0) trs_continuous = 0;
    trs_skip_next_kbwait();
}

void trs_debug(void)
{
    stop_signaled = 1;
    if (trs_continuous > 0) trs_continuous = 0;
    trs_skip_next_kbwait();
}

void debug_init(void)
{
    int i;

    traps = (Uchar *) malloc(ADDRESS_SPACE * sizeof(Uchar));
    memset(traps, 0, ADDRESS_SPACE * sizeof(Uchar));

    for(i = 0; i < MAX_TRAPS; ++i) trap_table[i].valid = 0;

    printf("Type \"help\" for a list of commands.\n");
}

static void print_memory(Ushort address, int num_bytes)
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
	    printf("%.2x ", mem_read(address + i));
	}
	for(i = bytes_to_print; i < 16; ++i)
	{
	    printf("   ");
	}
	printf("    ");
	for(i = 0; i < bytes_to_print; ++i)
	{
	    byte = mem_read(address + i);
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

static void debug_run(void)
{
    void (*old_signal_handler)();
    Uchar t;
    Uchar byte;
    int continuous;
    int i;
    int watch_triggered = 0;

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
	
	continuous = (!print_instructions && num_traps == 0);
	if (z80_run(continuous)) {
	  printf("emt_debug instruction executed.\n");
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

	/*
	 * Iterate over the trap list looking for watchpoints only if we
	 * know there are any to be found.
	 */
	if (num_watchpoints)
	{
	    for (i = 0; i < MAX_TRAPS; ++i)
	    {
		if (trap_table[i].valid &&
		    trap_table[i].flag == WATCHPOINT_FLAG)
		{
		    byte = mem_read(trap_table[i].address);
		    if (byte != trap_table[i].byte)
		    {
			/*
			 * If a watched memory location has changed, report
			 * it, update the watch entry in the trap table to
			 * reflect the new value, and set the
			 * watch_triggered flag so that we stop after all
			 * watchpoints have been processed.
			 */
			printf("Memory location 0x%.4x changed value from "
			       "0x%.2x to 0x%.2x.\n", trap_table[i].address,
			       trap_table[i].byte, byte);
			trap_table[i].byte = byte;
			watch_triggered = 1;
		    }
		}
	    }
	    if (watch_triggered)
	    {
		stop_signaled = 1;
	    }
	}

    }
    signal(SIGINT, old_signal_handler);
    printf("Stopped at %.4x\n", REG_PC);
}

void debug_shell(void)
{
    char input[MAXLINE];
    char command[MAXLINE];
    int done = 0;
    sigset_t set, oldset;

#ifdef READLINE
    char *line;
    char history_file[MAXLINE];
    char *home = (char *)getenv ("HOME");
    if (!home) home = ".";
    sprintf (history_file, "%s/.zbx-history", home);
    read_history(history_file);
#endif

    while(!done)
    {
	printf("\n");
	disassemble(REG_PC);

	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	sigprocmask(SIG_BLOCK, &set, &oldset);

#ifdef READLINE
	/*
	 * Use the way cool gnu readline() utility.  Get completion,
	 * history, way way cool.
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
		break;
	    }
	}
#else
	printf("(zbx) ");  fflush(stdout);

	if (fgets(input, MAXLINE, stdin) == NULL) break;
#endif

	sigprocmask(SIG_SETMASK, &oldset, NULL);

	if(sscanf(input, "%s", command))
	{
	    if(!strcmp(command, "help") || !strcmp(command, "?"))
	    {
	        fputs(help_message, stdout);
	    }
	    else if (!strcmp(command, "zbxinfo"))
	    {
		show_zbxinfo();
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
                
		if(!strncmp(input, "delete *", 8))
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
		Ushort start, old_start;
		int bytes = 0;
		int lines = 0;

		if(sscanf(input, "list %x , %x", &x, &y) == 2)
		{
		    start = x;
		    bytes = (y - x) & 0xffff;
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
		    while (bytes >= 0) {
			start = disassemble(old_start = start);
			bytes -= (start - old_start) & 0xffff;
		    }
		}
	    }
	    else if(!strcmp(command, "in"))
	    {
		int port;

		if(sscanf(input, "in %x", &port) == 1)
			printf("in %x = %x\n", port, z80_in(port));
		else
			printf("A port must be specified.\n");
	    }
	    else if(!strcmp(command, "out"))
	    {
		int port, value;

		if(sscanf(input, "out %x = %x", &port, &value) == 2)
			z80_out(port, value);
		else
			printf("A port and a value must be specified.\n");
	    }

	    else if(!strcmp(command, "next") || !strcmp(command, "nextint"))
	    {
		int is_call = 0, is_rst = 0, is_rep = 0;
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
		  case 0xC7:
		  case 0xCF:
		  case 0xD7:
		  case 0xDF:
		  case 0xE7:
		  case 0xEF:
		  case 0xF7:
		  case 0xFF:
		    is_rst = 1;
		    break;
		  case 0xED:
		    switch(mem_read(REG_PC+1)) {
		      case 0xB0: /* ldir */
		      case 0xB8: /* lddr */
		      case 0xB1: /* cpir */
		      case 0xB9: /* cpdr */
		      case 0xB2: /* inir */
		      case 0xBA: /* indr */
		      case 0xB3: /* otir */
		      case 0xBB: /* otdr */
			is_rep = 1;
			break;
		      default:
			break;
		    }
		    break;
		  default:
		    break;
		}
		if (is_call) {
		    set_trap((REG_PC + 3) % ADDRESS_SPACE, BREAK_ONCE_FLAG);
		    debug_run();
		} else if (is_rst) {
		    set_trap((REG_PC + 1) % ADDRESS_SPACE, BREAK_ONCE_FLAG);
		    debug_run();
		} else if (is_rep) {
		    set_trap((REG_PC + 2) % ADDRESS_SPACE, BREAK_ONCE_FLAG);
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
		printf("Performing hard reset.");
		trs_reset(1);
	    }
	    else if(!strcmp(command, "softreset"))
	    {
		printf("Pressing reset button.");
		trs_reset(0);
	    }
	    else if(!strcmp(command, "run"))
	    {
		printf("Performing hard reset and running.\n");
		trs_reset(1);
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

		if(sscanf(input, "%*s $%[a-zA-Z'] = %x", regname, &value) == 2)
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
		    } else if(!strcasecmp(regname, "r")) {
			REG_R = value;
			REG_R7 = value & 0x80;
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
	    else if(!strcmp(command, "stop") || !strcmp(command, "break"))
	    {
		int address;

		if(sscanf(input, "stop at %x", &address) != 1 &&
		   sscanf(input, "break %x", &address) != 1)
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
		printf("Untrace not implemented; use delete.\n");
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
	    else if(!strcmp(command, "watch"))
	    {
		int address;

		if(sscanf(input, "watch %x", &address) == 1)
		{
		    address %= ADDRESS_SPACE;
		    set_trap(address, WATCHPOINT_FLAG);
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
	    else if(!strcmp(command, "diskdump"))
	    {
		trs_disk_debug();
	    }
	    else if(!strcmp(command, "diskdebug"))
	    {
		trs_disk_debug_flags = 0;
		sscanf(input, "diskdebug %x", &trs_disk_debug_flags);
	    }
	    else if(!strcmp(command, "iodebug"))
	    {
		trs_io_debug_flags = 0;
		sscanf(input, "iodebug %x", &trs_io_debug_flags);
	    }
	    else
	    {
		int start_address, end_address, num_bytes;

		if(sscanf(input, "%x , %x / ", &start_address, &end_address) == 2)
		{
		    print_memory(start_address, end_address - start_address + 1);
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
