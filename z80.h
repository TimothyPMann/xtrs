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
   Last modified on Thu Sep 24 16:37:40 PDT 1998 by mann
*/

#ifndef _Z80_H
#define _Z80_H

#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

typedef unsigned short Ushort;
typedef unsigned char Uchar;

struct twobyte
{
#ifdef big_endian
    Uchar high, low;
#else
    Uchar low, high;
#endif
};

/* for implementing registers which can be seen as bytes or words: */
typedef union
{
    struct twobyte byte;
    Ushort word;
} wordregister;

struct z80_state_struct
{
    wordregister af;
    wordregister bc;
    wordregister de;
    wordregister hl;
    wordregister ix;
    wordregister iy;
    wordregister sp;
    wordregister pc;

    wordregister af_prime;
    wordregister bc_prime;
    wordregister de_prime;
    wordregister hl_prime;

    Uchar i;	/* interrupt-page address register */
    /* Uchar r; */  /* no memory-refresh register, just fetch random values */

    Uchar iff1, iff2;
    Uchar interrupt_mode;

    /* To signal a maskable interrupt, set irq TRUE.  The CPU does not
     * turn off irq; the external device must turn it off when
     * appropriately tickled by some IO port or memory address that it
     * decodes.  INT is level triggered, so Z-80 code must tickle the
     * device before reenabling interrupts.
     *
     * There is no support as yet for fetching an interrupt vector or
     * RST instruction from the interrupting device, as this gets
     * rather complex when there can be more than one device with an
     * interrupt pending.  So you'd better use interrupt_mode 1 only
     * (which is what the TRS-80 does).
     */
    int irq;

    /* To signal a nonmaskable interrupt, set nmi to TRUE.  The CPU
     * does not turn off nmi; the external device must turn it off
     * when tickled (or after a timeout).  NMI is edge triggered, so
     * it has to be turned off and back on again before it can cause
     * another interrupt.  nmi_seen remembers that an edge has been seen,
     * so turn off both nmi and nmi_seen when the interrupt is acknowledged.
     */
    int nmi, nmi_seen;

    /* Speed control.  0 = full speed */
    int delay;

    /* Simple event scheduler.  If >= 0, counts down before each
     * instruction is executed.  When count goes from 0 to -1, 
     * trs_do_event() is called.
     */
    int sched;

    /* Cyclic T-state counter */
#ifdef __GNUC__
    unsigned long long t_count;
#else
    unsigned long t_count;
#endif
    /* Clock in MHz = T-states per microsecond */
    float clockMHz;
};

#define Z80_ADDRESS_LIMIT	(1 << 16)

/*
 * Register accessors:
 */

#define REG_A	(z80_state.af.byte.high)
#define REG_F	(z80_state.af.byte.low)
#define REG_B	(z80_state.bc.byte.high)
#define REG_C	(z80_state.bc.byte.low)
#define REG_D	(z80_state.de.byte.high)
#define REG_E	(z80_state.de.byte.low)
#define REG_H	(z80_state.hl.byte.high)
#define REG_L	(z80_state.hl.byte.low)
#define REG_IX_HIGH	(z80_state.ix.byte.high)
#define REG_IX_LOW	(z80_state.ix.byte.low)
#define REG_IY_HIGH	(z80_state.iy.byte.high)
#define REG_IY_LOW	(z80_state.iy.byte.low)

#define REG_SP	(z80_state.sp.word)
#define REG_PC	(z80_state.pc.word)

#define REG_AF	(z80_state.af.word)
#define REG_BC	(z80_state.bc.word)
#define REG_DE	(z80_state.de.word)
#define REG_HL	(z80_state.hl.word)

#define REG_AF_PRIME	(z80_state.af_prime.word)
#define REG_BC_PRIME	(z80_state.bc_prime.word)
#define REG_DE_PRIME	(z80_state.de_prime.word)
#define REG_HL_PRIME	(z80_state.hl_prime.word)

#define REG_IX	(z80_state.ix.word)
#define REG_IY	(z80_state.iy.word)

#define REG_I	(z80_state.i)

#define HIGH(p) (((struct twobyte *)(p))->high)
#define LOW(p) (((struct twobyte *)(p))->low)

#define T_COUNT(n) z80_state.t_count += (n)
/*#define T_COUNT(n)*/

/*
 * Flag accessors:
 *
 * Flags are:
 *
 *	7   6   5   4   3   2   1   0
 *	S   Z   -   H   -  P/V  N   C
 *	
 *	C	Carry
 *	N	Subtract
 *	P/V	Parity/Overflow
 *	H	Half-carry
 *	Z	Zero
 *	S	Sign
 */

#define CARRY_MASK		(0x1)
#define SUBTRACT_MASK		(0x2)
#define PARITY_MASK		(0x4)
#define OVERFLOW_MASK		(0x4)
#define UNDOC3_MASK             (0x8)
#define HALF_CARRY_MASK		(0x10)
#define UNDOC5_MASK             (0x20)
#define ZERO_MASK		(0x40)
#define	SIGN_MASK		(0x80)
#define ALL_FLAGS_MASK		(CARRY_MASK | SUBTRACT_MASK | OVERFLOW_MASK | \
				 HALF_CARRY_MASK | ZERO_MASK | SIGN_MASK)

#define SET_SIGN()		(REG_F |= SIGN_MASK)
#define CLEAR_SIGN()		(REG_F &= (~SIGN_MASK))
#define SET_ZERO()		(REG_F |= ZERO_MASK)
#define CLEAR_ZERO()		(REG_F &= (~ZERO_MASK))
#define SET_HALF_CARRY()       	(REG_F |= HALF_CARRY_MASK)
#define CLEAR_HALF_CARRY()	(REG_F &= (~HALF_CARRY_MASK))
#define SET_OVERFLOW()		(REG_F |= OVERFLOW_MASK)
#define CLEAR_OVERFLOW()	(REG_F &= (~OVERFLOW_MASK))
#define SET_PARITY()		(REG_F |= PARITY_MASK)
#define CLEAR_PARITY()		(REG_F &= (~PARITY_MASK))
#define SET_SUBTRACT()		(REG_F |= SUBTRACT_MASK)
#define CLEAR_SUBTRACT()	(REG_F &= (~SUBTRACT_MASK))
#define SET_CARRY()		(REG_F |= CARRY_MASK)
#define CLEAR_CARRY()		(REG_F &= (~CARRY_MASK))

#define SIGN_FLAG		(REG_F & SIGN_MASK)
#define ZERO_FLAG		(REG_F & ZERO_MASK)
#define HALF_CARRY_FLAG		(REG_F & HALF_CARRY_MASK)
#define OVERFLOW_FLAG		(REG_F & OVERFLOW_MASK)
#define PARITY_FLAG		(REG_F & PARITY_MASK)
#define SUBTRACT_FLAG		(REG_F & SUBTRACT_MASK)
#define CARRY_FLAG		(REG_F & CARRY_MASK)

extern struct z80_state_struct z80_state;

extern void z80_reset(void);
extern int z80_run(int continuous);
extern void mem_init(void);
extern int mem_read(int address);  /* 0 <= address <= 0xffff REQUIRED */
extern void mem_write(int address, int value);
extern void mem_write_rom(int address, int value);
extern int mem_read_word(int address);
extern void mem_write_word(int address, int value);
Uchar *mem_pointer(int address, int writing);
extern int mem_block_transfer(Ushort dest, Ushort source, int direction,
			      Ushort count);
extern int load_hex(); /* returns highest address loaded + 1 */
extern void error(char *string);
extern void fatal(char *string);
extern void z80_out(int port, int value);
extern int z80_in(int port);
extern int disassemble(unsigned short pc);
extern void debug_init(void);
extern void debug_shell(void);

#endif
