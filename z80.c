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
   Last modified on Thu May  3 02:49:09 PDT 2001 by mann
*/

/*
 * z80.c:  The guts of the Z-80 emulator.
 *
 * The Z-80 emulator should be general and complete enough to be
 * easily adapted to emulate any Z-80 machine.  All of the documented
 * Z-80 flags and instructions are implemented.  The only documented
 * features we cheat a little on are interrupt handling (modes 0 and 2
 * are not supported) and the refresh register (reading it returns a
 * random number; writing it is ignored).
 *
 * All of the undocumented instructions, flags, and features listed in
 * http://www.msxnet.org/tech/Z80/z80undoc.txt are implemented too,
 * with some minor exceptions.  There seems to be a disagreement about
 * undocumented flag handling for "bit" instructions between
 * z80undoc.txt and the ZEXALL validator from Yaze.  Since ZEXALL
 * passes on both a real Z-80 and Yaze, but fails on my attempt to
 * implement "bit n,r" according to z80undoc.txt, I've imitated Yaze's
 * implementation.  On block in/out instructions, z80undoc.txt gives
 * some very complicated rules for undocumented flag handling that I
 * have not implemented.  
 */

#include "z80.h"
#include "trs.h"
#include "trs_imp_exp.h"
#include <stdlib.h>  /* for rand() */
#include <unistd.h>  /* for pause() */
#include <time.h>    /* for time() */

/*
 * Keep Saber quiet.
 */
/*SUPPRESS 53*/
/*SUPPRESS 51*/
/*SUPPRESS 112*/
/*SUPPRESS 115*/

/*
 * The state of our Z-80 registers is kept in this structure:
 */
struct z80_state_struct z80_state;

/*
 * Tables and routines for computing various flag values:
 */

static Uchar sign_carry_overflow_table[] =
{
    0,
    OVERFLOW_MASK | SIGN_MASK,
    CARRY_MASK,
    SIGN_MASK,
    CARRY_MASK,
    SIGN_MASK,
    CARRY_MASK | OVERFLOW_MASK,
    CARRY_MASK | SIGN_MASK,
};

static Uchar half_carry_table[] =
{
    0,
    0,
    HALF_CARRY_MASK,
    0,
    HALF_CARRY_MASK,
    0,
    HALF_CARRY_MASK,
    HALF_CARRY_MASK,
};

static Uchar subtract_sign_carry_overflow_table[] =
{
    0,
    CARRY_MASK | SIGN_MASK,
    CARRY_MASK,
    OVERFLOW_MASK | CARRY_MASK | SIGN_MASK,
    OVERFLOW_MASK,
    SIGN_MASK,
    0,
    CARRY_MASK | SIGN_MASK,
};

static Uchar subtract_half_carry_table[] =
{
    0,
    HALF_CARRY_MASK,
    HALF_CARRY_MASK,
    HALF_CARRY_MASK,
    0,
    0,
    0,
    HALF_CARRY_MASK,
};

static int parity(unsigned value)
{
    /* for parity flag, 1 = even parity, 0 = odd parity. */
    static char parity_table[256] =
    {
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
    };

    return(parity_table[value]);
}

static void do_add_flags(int a, int b, int result)
{
    /*
     * Compute the flag values for a + b = result operation
     */
    int index;
    int f;

    /*
     * Sign, carry, and overflow depend upon values of bit 7.
     * Half-carry depends upon values of bit 3.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     * Undocumented flags in bit 3, 5 of F come from the result.
     */


    index = ((a & 0x88) >> 1) | ((b & 0x88) >> 2) | ((result & 0x88) >> 3);
    f = half_carry_table[index & 7] |
      sign_carry_overflow_table[index >> 4] |
      (result & (UNDOC3_MASK|UNDOC5_MASK));

    if((result & 0xFF) == 0) f |= ZERO_MASK;

    REG_F = f;
}

static void do_sub_flags(int a, int b, int result)
{
    int index;
    int f;

    /*
     * Sign, carry, and overflow depend upon values of bit 7.
     * Half-carry depends upon values of bit 3.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     * Undocumented flags in bit 3, 5 of F come from the result.
     */

    index = ((a & 0x88) >> 1) | ((b & 0x88) >> 2) | ((result & 0x88) >> 3);
    f = SUBTRACT_MASK | subtract_half_carry_table[index & 7] |
      subtract_sign_carry_overflow_table[index >> 4] |
      (result & (UNDOC3_MASK|UNDOC5_MASK));

    if((result & 0xFF) == 0) f |= ZERO_MASK;

    REG_F = f;
}


static void do_adc_word_flags(int a, int b, int result)
{
    int index;
    int f;

    /*
     * Sign, carry, and overflow depend upon values of bit 15.
     * Half-carry depends upon values of bit 11.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     * Undocumented flags in bit 3, 5 of F come from the result high byte.
     */

    index = ((a & 0x8800) >> 9) | ((b & 0x8800) >> 10) |
      ((result & 0x8800) >> 11);

    f = half_carry_table[index & 7] |
      sign_carry_overflow_table[index >> 4] |
      ((result >> 8) & (UNDOC3_MASK|UNDOC5_MASK));

    if((result & 0xFFFF) == 0) f |= ZERO_MASK;

    REG_F = f;
}

static void do_add_word_flags(int a, int b, int result)
{
    int index;
    int f;

    /*
     * Carry depends upon values of bit 15.
     * Half-carry depends upon values of bit 11.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     * Undocumented flags in bit 3, 5 of F come from the result high byte.
     */

    index = ((a & 0x8800) >> 9) | ((b & 0x8800) >> 10) |
      ((result & 0x8800) >> 11);

    f = half_carry_table[index & 7] |
      (sign_carry_overflow_table[index >> 4] & CARRY_MASK) |
      (REG_F & (ZERO_MASK | PARITY_MASK | SIGN_MASK)) |
      ((result >> 8) & (UNDOC3_MASK | UNDOC5_MASK));

    REG_F = f;
}

static void do_sbc_word_flags(int a, int b, int result)
{
    int index;
    int f;

    /*
     * Sign, carry, and overflow depend upon values of bit 15.
     * Half-carry depends upon values of bit 11.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     * Undocumented flags in bit 3, 5 of F come from the result high byte.
     */

    index = ((a & 0x8800) >> 9) | ((b & 0x8800) >> 10) |
      ((result & 0x8800) >> 11);

    f = SUBTRACT_MASK | subtract_half_carry_table[index & 7] |
      subtract_sign_carry_overflow_table[index >> 4] |
      ((result >> 8) & (UNDOC3_MASK | UNDOC5_MASK));

    if((result & 0xFFFF) == 0) f |= ZERO_MASK;

    REG_F = f;
}

static void do_flags_dec_byte(int value)
{
    Uchar set;

    set = SUBTRACT_MASK;

    if(value == 0x7f)
      set |= OVERFLOW_MASK;
    if((value & 0xF) == 0xF)
      set |= HALF_CARRY_MASK;
    if(value == 0)
      set |= ZERO_MASK;
    if(value & 0x80)
      set |= SIGN_MASK;

    REG_F = (REG_F & CARRY_MASK) | set
      | (value & (UNDOC3_MASK | UNDOC5_MASK));
}

static void do_flags_inc_byte(int value)
{
    Uchar set;

    set = 0;

    if(value == 0x80)
      set |= OVERFLOW_MASK;
    if((value & 0xF) == 0)
      set |= HALF_CARRY_MASK;
    if(value == 0)
      set |= ZERO_MASK;
    if(value & 0x80)
      set |= SIGN_MASK;

    REG_F = (REG_F & CARRY_MASK) | set
      | (value & (UNDOC3_MASK | UNDOC5_MASK));
}

/*
 * Routines for executing or assisting various non-trivial arithmetic
 * instructions:
 */
static void do_and_byte(int value)
{
    int result;
    Uchar set;

    result = (REG_A &= value);

    set = HALF_CARRY_MASK;

    if(parity(result))
      set |= PARITY_MASK;
    if(result == 0)
      set |= ZERO_MASK;
    if(result & 0x80)
      set |= SIGN_MASK;

    REG_F = set | (result & (UNDOC3_MASK | UNDOC5_MASK));
}

static void do_or_byte(int value)
{
    int result;  /* the result of the or operation */
    Uchar set;

    result = (REG_A |= value);

    set = 0;

    if(parity(result))
      set |= PARITY_MASK;
    if(result == 0)
      set |= ZERO_MASK;
    if(result & 0x80)
      set |= SIGN_MASK;

    REG_F = set | (result & (UNDOC3_MASK | UNDOC5_MASK));
}

static void do_xor_byte(int value)
{
    int result;  /* the result of the xor operation */
    Uchar set;

    result = (REG_A ^= value);

    set = 0;

    if(parity(result))
      set |= PARITY_MASK;
    if(result == 0)
      set |= ZERO_MASK;
    if(result & 0x80)
      set |= SIGN_MASK;

    REG_F = set | (result & (UNDOC3_MASK | UNDOC5_MASK));
}

static void do_add_byte(int value)
{
    int a, result;

    result = (a = REG_A) + value;
    REG_A = result;
    do_add_flags(a, value, result);
}

static void do_adc_byte(int value)
{
    int a, result;

    if(CARRY_FLAG)
      result = (a = REG_A) + value + 1;
    else
      result = (a = REG_A) + value;
    REG_A = result;
    do_add_flags(a, value, result);
}

static void do_sub_byte(int value)
{
    int a, result;

    result = (a = REG_A) - value;
    REG_A = result;
    do_sub_flags(a, value, result);
}

static void do_negate()
{
    int a;

    a = REG_A;
    REG_A = - a;
    do_sub_flags(0, a, REG_A);
}

static void do_sbc_byte(int value)
{
    int a, result;

    if(CARRY_FLAG)
      result = (a = REG_A) - (value + 1);
    else
      result = (a = REG_A) - value;
    REG_A = result;
    do_sub_flags(a, value, result);
}

static void do_add_word(int value)
{
    int a, result;

    result = (a = REG_HL) + value;
    REG_HL = result;

    do_add_word_flags(a, value, result);
}

static void do_adc_word(int value)
{
    int a, result;

    if(CARRY_FLAG)
      result = (a = REG_HL) + value + 1;
    else
      result = (a = REG_HL) + value;

    REG_HL = result;

    do_adc_word_flags(a, value, result);
}

static void do_sbc_word(int value)
{
    int a, result;

    if(CARRY_FLAG)
      result = (a = REG_HL) - (value + 1);
    else
      result = (a = REG_HL) - value;

    REG_HL = result;

    do_sbc_word_flags(a, value, result);
}

static void do_add_word_index(Ushort *regp, int value)
{
    int a, result;

    result = (a = *regp) + value;
    *regp = result;

    do_add_word_flags(a, value, result);
}

/* compare this value with A's contents */
static void do_cp(int value)
{
    int a, result;
    int index;
    int f;

    result = (a = REG_A) - value;

    /*
     * Sign, carry, and overflow depend upon values of bit 7.
     * Half-carry depends upon values of bit 3.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     * Undocumented flags in bit 3, 5 of F come from the second operand.
     */

    index = ((a & 0x88) >> 1) | ((value & 0x88) >> 2) | ((result & 0x88) >> 3);
    f = SUBTRACT_MASK | subtract_half_carry_table[index & 7] |
      subtract_sign_carry_overflow_table[index >> 4] |
      (value & (UNDOC3_MASK|UNDOC5_MASK));

    if((result & 0xFF) == 0) f |= ZERO_MASK;

    REG_F = f;
}

static void do_cpd()
{
    int oldcarry = REG_F & CARRY_MASK;
    int a, value, result;
    value = mem_read(REG_HL);
    result = (a = REG_A) - value;
    REG_HL--;
    REG_BC--;

    do_sub_flags(a, value, result);
    REG_F = (REG_F & ~(CARRY_MASK | OVERFLOW_MASK | UNDOC5_MASK))
      | oldcarry | (REG_BC == 0 ? 0 : OVERFLOW_MASK)
      | (((result - ((REG_F & HALF_CARRY_MASK) >> 4)) & 2) << 4);
    if ((result & 15) == 8 && (REG_F & HALF_CARRY_MASK) != 0) {
      REG_F &= ~UNDOC3_MASK;
    }

    T_COUNT(16);
}

static void do_cpi()
{
    int oldcarry = REG_F & CARRY_MASK;
    int a, value, result;
    value = mem_read(REG_HL);
    result = (a = REG_A) - value;
    REG_HL++;
    REG_BC--;

    do_sub_flags(a, value, result);
    REG_F = (REG_F & ~(CARRY_MASK | OVERFLOW_MASK | UNDOC5_MASK))
      | oldcarry | (REG_BC == 0 ? 0 : OVERFLOW_MASK)
      | (((result - ((REG_F & HALF_CARRY_MASK) >> 4)) & 2) << 4);
    if ((result & 15) == 8 && (REG_F & HALF_CARRY_MASK) != 0) {
      REG_F &= ~UNDOC3_MASK;
    }

    T_COUNT(16);
}

static void do_cpdr()
{
    int oldcarry = REG_F & CARRY_MASK;
    int a = REG_A, value, result;
    do
    {
        result = a - (value = mem_read(REG_HL));
	REG_HL--;
	REG_BC--;

	T_COUNT(21);

    } while((REG_BC != 0) && (result != 0));

    do_sub_flags(a, value, result);
    REG_F = (REG_F & ~(CARRY_MASK | OVERFLOW_MASK | UNDOC5_MASK))
      | oldcarry | (REG_BC == 0 ? 0 : OVERFLOW_MASK)
      | (((result - ((REG_F & HALF_CARRY_MASK) >> 4)) & 2) << 4);
    if ((result & 15) == 8 && (REG_F & HALF_CARRY_MASK) != 0) {
      REG_F &= ~UNDOC3_MASK;
    }

    T_COUNT(-5);
}

static void do_cpir()
{
    int oldcarry = REG_F & CARRY_MASK;
    int a = REG_A, value, result;
    do
    {
        result = a - (value = mem_read(REG_HL));
	REG_HL++;
	REG_BC--;

	T_COUNT(21);

    } while((REG_BC != 0) && (result != 0));

    do_sub_flags(a, value, result);
    REG_F = (REG_F & ~(CARRY_MASK | OVERFLOW_MASK | UNDOC5_MASK))
      | oldcarry | (REG_BC == 0 ? 0 : OVERFLOW_MASK)
      | (((result - ((REG_F & HALF_CARRY_MASK) >> 4)) & 2) << 4);
    if ((result & 15) == 8 && (REG_F & HALF_CARRY_MASK) != 0) {
      REG_F &= ~UNDOC3_MASK;
    }

    T_COUNT(-5);
}

#if 1
/* The following passes ZEXALL and matches Yaze, but if you believe
   http://www.msxnet.org/tech/Z80/z80undoc.txt, it gets UNDOC3 and UNDOC5 wrong.
   It remains to be seen which (if either) is really right. */
static void do_test_bit(int op, int value, int bit)
{
    int result = value & (1 << bit);
    REG_F = (REG_F & CARRY_MASK) | HALF_CARRY_MASK | (result & SIGN_MASK)
      | (result ? 0 : (OVERFLOW_MASK | ZERO_MASK))
      | (((op & 7) == 6) ? 0 : (value & (UNDOC3_MASK | UNDOC5_MASK)));
}
#else
/* The following matches http://www.msxnet.org/tech/Z80/z80undoc.txt
   for "bit n,r", but does not attempt to emulate the full weirdness
   of "bit n,(ix/iy+d)" and "bit n,(hl)".  It fails ZEXALL even if
   code is added to make the latter two instructions behave as in
   the version that passes ZEXALL, leading me to think that z80undoc.txt
   may be mistaken about "bit n,r".  This should be checked in detail
   against a real Z-80, I suppose.  Ugh. */
static void do_test_bit(int op, int value, int bit)
{
    int result = value & (1 << bit);
    REG_F = (REG_F & CARRY_MASK) | HALF_CARRY_MASK
      | (result & (UNDOC3_MASK | UNDOC5_MASK | SIGN_MASK))
      | (result ? 0 : (OVERFLOW_MASK | ZERO_MASK));
}
#endif

static int rl_byte(int value)
{
    /*
     * Compute rotate-left-through-carry
     * operation, setting flags as appropriate.
     */

    Uchar set;
    int result;

    set = 0;

    if(CARRY_FLAG)
    {
	result = ((value << 1) & 0xFF) | 1;
    }
    else
    {
	result = (value << 1) & 0xFF;
    }

    if(result & 0x80)
      set |= SIGN_MASK;
    if(result == 0)
      set |= ZERO_MASK;
    if(parity(result))
      set |= PARITY_MASK;
    if(value & 0x80)
      set |= CARRY_MASK;

    REG_F = (result & (UNDOC3_MASK | UNDOC5_MASK)) | set;

    return result;
}

static int rr_byte(int value)
{
    /*
     * Compute rotate-right-through-carry
     * operation, setting flags as appropriate.
     */

    Uchar set;
    int result;

    set = 0;

    if(CARRY_FLAG)
    {
	result = (value >> 1) | 0x80;
    }
    else
    {
	result = (value >> 1);
    }

    if(result & 0x80)
      set |= SIGN_MASK;
    if(result == 0)
      set |= ZERO_MASK;
    if(parity(result))
      set |= PARITY_MASK;
    if(value & 0x1)
      set |= CARRY_MASK;

    REG_F = (result & (UNDOC3_MASK | UNDOC5_MASK)) | set;

    return result;
}

static int rlc_byte(int value)
{
    /*
     * Compute the result of an RLC operation and set the flags appropriately.
     * This does not do the right thing for the RLCA instruction.
     */

    Uchar set;
    int result;

    set = 0;

    if(value & 0x80)
    {
	result = ((value << 1) & 0xFF) | 1;
	set |= CARRY_MASK;
    }
    else
    {
	result = (value << 1) & 0xFF;
    }

    if(result & 0x80)
      set |= SIGN_MASK;
    if(result == 0)
      set |= ZERO_MASK;
    if(parity(result))
      set |= PARITY_MASK;

    REG_F = (result & (UNDOC3_MASK | UNDOC5_MASK)) | set;

    return result;
}

static int rrc_byte(int value)
{
    Uchar set;
    int result;

    set = 0;

    if(value & 0x1)
    {
	result = (value >> 1) | 0x80;
	set |= CARRY_MASK;
    }
    else
    {
	result = (value >> 1);
    }

    if(result & 0x80)
      set |= SIGN_MASK;
    if(result == 0)
      set |= ZERO_MASK;
    if(parity(result))
      set |= PARITY_MASK;

    REG_F = (result & (UNDOC3_MASK | UNDOC5_MASK)) | set;

    return result;
}

/*
 * Perform the RLA, RLCA, RRA, RRCA instructions.  These set the flags
 * differently than the other rotate instrucitons.
 */
static void do_rla()
{
    Uchar set;

    set = 0;

    if(REG_A & 0x80)
      set |= CARRY_MASK;

    if(CARRY_FLAG)
    {
	REG_A = ((REG_A << 1) & 0xFF) | 1;
    }
    else
    {
	REG_A = (REG_A << 1) & 0xFF;
    }

    REG_F = (REG_F & (OVERFLOW_MASK | ZERO_MASK | SIGN_MASK))
      | set | (REG_A & (UNDOC3_MASK | UNDOC5_MASK ));
}

static void do_rra()
{
    Uchar set;

    set = 0;

    if(REG_A & 0x1)
      set |= CARRY_MASK;

    if(CARRY_FLAG)
    {
	REG_A = (REG_A >> 1) | 0x80;
    }
    else
    {
	REG_A = REG_A >> 1;
    }
    REG_F = (REG_F & (OVERFLOW_MASK | ZERO_MASK | SIGN_MASK))
      | set | (REG_A & (UNDOC3_MASK | UNDOC5_MASK ));
}

static void do_rlca()
{
    Uchar set;

    set = 0;

    if(REG_A & 0x80)
    {
	REG_A = ((REG_A << 1) & 0xFF) | 1;
	set |= CARRY_MASK;
    }
    else
    {
	REG_A = (REG_A << 1) & 0xFF;
    }
    REG_F = (REG_F & (OVERFLOW_MASK | ZERO_MASK | SIGN_MASK))
      | set | (REG_A & (UNDOC3_MASK | UNDOC5_MASK ));
}

static void do_rrca()
{
    Uchar set;

    set = 0;

    if(REG_A & 0x1)
    {
	REG_A = (REG_A >> 1) | 0x80;
	set |= CARRY_MASK;
    }
    else
    {
	REG_A = REG_A >> 1;
    }
    REG_F = (REG_F & (OVERFLOW_MASK | ZERO_MASK | SIGN_MASK))
      | set | (REG_A & (UNDOC3_MASK | UNDOC5_MASK ));
}

static int sla_byte(int value)
{
    Uchar set;
    int result;

    set = 0;

    result = (value << 1) & 0xFF;

    if(result & 0x80)
      set |= SIGN_MASK;
    if(result == 0)
      set |= ZERO_MASK;
    if(parity(result))
      set |= PARITY_MASK;
    if(value & 0x80)
      set |= CARRY_MASK;

    REG_F = (result & (UNDOC3_MASK | UNDOC5_MASK)) | set;

    return result;
}

static int sra_byte(int value)
{
    Uchar set;
    int result;

    set = 0;

    if(value & 0x80)
    {
	result = (value >> 1) | 0x80;
	set |= SIGN_MASK;
    }
    else
    {
	result = value >> 1;
    }

    if(result == 0)
      set |= ZERO_MASK;
    if(parity(result))
      set |= PARITY_MASK;
    if(value & 0x1)
      set |= CARRY_MASK;

    REG_F = (result & (UNDOC3_MASK | UNDOC5_MASK)) | set;

    return result;
}

/* undocumented opcode slia: shift left and increment */
static int slia_byte(int value)
{
    Uchar set;
    int result;

    set = 0;

    result = ((value << 1) & 0xFF) | 1;

    if(result & 0x80)
      set |= SIGN_MASK;
    if(result == 0)
      set |= ZERO_MASK;
    if(parity(result))
      set |= PARITY_MASK;
    if(value & 0x80)
      set |= CARRY_MASK;

    REG_F = (result & (UNDOC3_MASK | UNDOC5_MASK)) | set;

    return result;
}

static int srl_byte(int value)
{
    Uchar set;
    int result;

    set = 0;

    result = value >> 1;

    if(result & 0x80)
      set |= SIGN_MASK;
    if(result == 0)
      set |= ZERO_MASK;
    if(parity(result))
      set |= PARITY_MASK;
    if(value & 0x1)
      set |= CARRY_MASK;

    REG_F = (result & (UNDOC3_MASK | UNDOC5_MASK)) | set;

    return result;
}

static void do_ldd()
{
    int moved, undoc;
    mem_write(REG_DE, moved = mem_read(REG_HL));
    REG_DE--;
    REG_HL--;
    REG_BC--;

    if(REG_BC == 0)
      CLEAR_OVERFLOW();
    else
      SET_OVERFLOW();
    undoc = REG_A + moved;
    REG_F = (REG_F & ~(UNDOC3_MASK|UNDOC5_MASK|HALF_CARRY_MASK|SUBTRACT_MASK))
      | (undoc & UNDOC3_MASK) | ((undoc & 2) ? UNDOC5_MASK : 0);
    T_COUNT(16);
}

static void do_ldi()
{
    int moved, undoc;
    mem_write(REG_DE, moved = mem_read(REG_HL));
    REG_DE++;
    REG_HL++;
    REG_BC--;

    if(REG_BC == 0)
      CLEAR_OVERFLOW();
    else
      SET_OVERFLOW();
    undoc = REG_A + moved;
    REG_F = (REG_F & ~(UNDOC3_MASK|UNDOC5_MASK|HALF_CARRY_MASK|SUBTRACT_MASK))
      | (undoc & UNDOC3_MASK) | ((undoc & 2) ? UNDOC5_MASK : 0);
    T_COUNT(16);
}

static void do_ldir()
{
    /* repeating block load with increment */
    int moved, undoc;

    moved = mem_block_transfer(REG_DE, REG_HL, 1, REG_BC);
    T_COUNT(((REG_BC-1) & 0xffff) * 21 + 16);

    /* set registers to final values */
    REG_DE += REG_BC;
    REG_HL += REG_BC;
    REG_BC = 0;

    /* set up flags */
    undoc = REG_A + moved;
    REG_F = (REG_F & (CARRY_MASK | ZERO_MASK | SIGN_MASK)) 
      | (undoc & UNDOC3_MASK) | ((undoc & 2) ? UNDOC5_MASK : 0);
}

static void do_lddr()
{
    /* repeating block load with decrement */
    int moved, undoc;

    moved = mem_block_transfer(REG_DE, REG_HL, -1, REG_BC);
    T_COUNT(((REG_BC-1) & 0xffff) * 21 + 16);

    /* set registers to final values */
    REG_DE -= REG_BC;
    REG_HL -= REG_BC;
    REG_BC = 0;

    /* set up flags */
    undoc = REG_A + moved;
    REG_F = (REG_F & (CARRY_MASK | ZERO_MASK | SIGN_MASK)) 
      | (undoc & UNDOC3_MASK) | ((undoc & 2) ? UNDOC5_MASK : 0);
}

static void do_ld_a_i()
{
    Uchar set;

    set = 0;

    REG_A = REG_I;

    if(REG_A & 0x80)
      set |= SIGN_MASK;
    if(REG_A == 0)
      set |= ZERO_MASK;

    if(z80_state.iff2)
      set |= OVERFLOW_MASK;

    REG_F = (REG_F & CARRY_MASK) | (REG_A & (UNDOC3_MASK | UNDOC5_MASK)) | set;
}

static void do_ld_a_r()
{
    Uchar set;

    set = 0;

    /* Fetch a random value. */
    REG_A = (rand() >> 8) & 0xFF;

    if(REG_A & 0x80)
      set |= SIGN_MASK;
    if(REG_A == 0)
      set |= ZERO_MASK;

    if(z80_state.iff2)
      set |= OVERFLOW_MASK;

    REG_F = (REG_F & CARRY_MASK) | (REG_A & (UNDOC3_MASK | UNDOC5_MASK)) | set;
}

/* Completely new implementation adapted from yaze.
   The old one was very wrong. */
static void do_daa()
{
  int a = REG_A, f = REG_F;
  int alow = a & 0xf;  
  int carry = f & CARRY_MASK;
  int hcarry = f & HALF_CARRY_MASK;
  if (f & SUBTRACT_MASK) {
    int hd = carry || a > 0x99;
    if (hcarry || alow > 9) {
       if (alow > 5) hcarry = 0;
       a = (a - 6) & 0xff;
     }
     if (hd) a -= 0x160;
  } else {
    if (hcarry || alow > 9) {
      hcarry = alow > 9 ? HALF_CARRY_MASK : 0;
      a += 6;
    }
    if (carry || ((a & 0x1f0) > 0x90)) {
      a += 0x60;
    }
  }
  if (a & 0x100) carry = CARRY_MASK;

  REG_A = a = a & 0xff;
  REG_F = ((a & 0x80) ? SIGN_MASK : 0)
    | (a & (UNDOC3_MASK|UNDOC5_MASK))
    | (a ? 0 : ZERO_MASK)
    | (f & SUBTRACT_MASK)
    | hcarry | (parity(a) ? PARITY_MASK : 0) | carry;
}

static void do_rld()
{
    /*
     * Rotate-left-decimal.
     */
    int old_value, new_value;
    Uchar set;

    set = 0;

    old_value = mem_read(REG_HL);

    /* left-shift old value, add lower bits of a */
    new_value = ((old_value << 4) | (REG_A & 0x0f)) & 0xff;

    /* rotate high bits of old value into low bits of a */
    REG_A = (REG_A & 0xf0) | (old_value >> 4);

    if(REG_A & 0x80)
      set |= SIGN_MASK;
    if(REG_A == 0)
      set |= ZERO_MASK;
    if(parity(REG_A))
      set |= PARITY_MASK;

    REG_F = (REG_F & CARRY_MASK) | set | (REG_A & (UNDOC3_MASK | UNDOC5_MASK));
    mem_write(REG_HL,new_value);
}

static void do_rrd()
{
    /*
     * Rotate-right-decimal.
     */
    int old_value, new_value;
    Uchar set;

    set = 0;

    old_value = mem_read(REG_HL);

    /* right-shift old value, add lower bits of a */
    new_value = (old_value >> 4) | ((REG_A & 0x0f) << 4);

    /* rotate low bits of old value into low bits of a */
    REG_A = (REG_A & 0xf0) | (old_value & 0x0f);

    if(REG_A & 0x80)
      set |= SIGN_MASK;
    if(REG_A == 0)
      set |= ZERO_MASK;
    if(parity(REG_A))
      set |= PARITY_MASK;

    REG_F = (REG_F & CARRY_MASK) | set | (REG_A & (UNDOC3_MASK | UNDOC5_MASK));
    mem_write(REG_HL,new_value);
}


/*
 * Input/output instruction support:
 */

static void do_ind()
{
    mem_write(REG_HL, z80_in(REG_C));
    REG_HL--;
    REG_B--;

    if(REG_B == 0)
      SET_ZERO();
    else
      CLEAR_ZERO();

    SET_SUBTRACT();
    T_COUNT(15);
}

static void do_indr()
{
    do
    {
	mem_write(REG_HL, z80_in(REG_C));
	REG_HL--;
	REG_B--;
	T_COUNT(20);
    } while(REG_B != 0);
    T_COUNT(-5);

    SET_ZERO();
    SET_SUBTRACT();
}

static void do_ini()
{
    mem_write(REG_HL, z80_in(REG_C));
    REG_HL++;
    REG_B--;

    if(REG_B == 0)
      SET_ZERO();
    else
      CLEAR_ZERO();

    SET_SUBTRACT();
    T_COUNT(15);
}

static void do_inir()
{
    do
    {
	mem_write(REG_HL, z80_in(REG_C));
	REG_HL++;
	REG_B--;
	T_COUNT(20);
    } while(REG_B != 0);
    T_COUNT(-5);

    SET_ZERO();
    SET_SUBTRACT();
}

static int in_with_flags(int port)
{
    /*
     * Do the appropriate flag calculations for the in instructions
     * which compute the flags.  Return the input value.
     */

    int value;
    Uchar clear, set;

    clear = (Uchar) ~(SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK |
		      PARITY_MASK | SUBTRACT_MASK);
    set = 0;

    value = z80_in(port);

    if(value & 0x80)
      set |= SIGN_MASK;
    if(value == 0)
      set |= ZERO_MASK;
    if(parity(value))
      set |= PARITY_MASK;

    /* What should the half-carry do?  Is this a mistake? */

    REG_F = (REG_F & clear) | set;

    return value;
}

static void do_outd()
{
    z80_out(REG_C, mem_read(REG_HL));
    REG_HL--;
    REG_B--;

    if(REG_B == 0)
      SET_ZERO();
    else
      CLEAR_ZERO();

    SET_SUBTRACT();
    T_COUNT(15);
}

static void do_outdr()
{
    do
    {
	z80_out(REG_C, mem_read(REG_HL));
	REG_HL--;
	REG_B--;
	T_COUNT(20);
    } while(REG_B != 0);
    T_COUNT(-5);

    SET_ZERO();
    SET_SUBTRACT();
}

static void do_outi()
{
    z80_out(REG_C, mem_read(REG_HL));
    REG_HL++;
    REG_B--;

    if(REG_B == 0)
      SET_ZERO();
    else
      CLEAR_ZERO();

    SET_SUBTRACT();
    T_COUNT(15);
}

static void do_outir()
{
    do
    {
	z80_out(REG_C, mem_read(REG_HL));
	REG_HL++;
	REG_B--;
	T_COUNT(20);
    } while(REG_B != 0);
    T_COUNT(-5);

    SET_ZERO();
    SET_SUBTRACT();
}


/*
 * Interrupt handling routines:
 */

static void do_di()
{
    z80_state.iff1 = z80_state.iff2 = 0;
}

static void do_ei()
{
    z80_state.iff1 = z80_state.iff2 = 1;
}

static void do_im0()
{
    z80_state.interrupt_mode = 0;
}

static void do_im1()
{
    z80_state.interrupt_mode = 1;
}

static void do_im2()
{
    z80_state.interrupt_mode = 2;
}

static void do_int()
{
    /* handle a maskable interrupt */
    REG_SP -= 2;
    mem_write_word(REG_SP, REG_PC);
    z80_state.iff1 = 0;
    switch (z80_state.interrupt_mode) {
    case 0:
      /* REG_PC = get_irq_vector() & 0x38; */
      error("interrupt in im0 not supported");
      break;
    case 1:
      REG_PC = 0x38;
      break;
    case 2:
      /* REG_PC = REG_I << 8 + get_irq_vector(); */
      error("interrupt in im2 not supported");
      break;
    }
}

static void do_nmi()
{
    /* handle a non-maskable interrupt */
    REG_SP -= 2;
    mem_write_word(REG_SP, REG_PC);
    z80_state.iff1 = 0;
    REG_PC = 0x66;
}

/*
 * Extended instructions which have 0xCB as the first byte:
 */
static void do_CB_instruction()
{
    Uchar instruction;
    
    instruction = mem_read(REG_PC++);
    
    switch(instruction)
    {
      case 0x47:	/* bit 0, a */
	do_test_bit(instruction, REG_A, 0);  T_COUNT(8);
	break;
      case 0x40:	/* bit 0, b */
	do_test_bit(instruction, REG_B, 0);  T_COUNT(8);
	break;
      case 0x41:	/* bit 0, c */
	do_test_bit(instruction, REG_C, 0);  T_COUNT(8);
	break;
      case 0x42:	/* bit 0, d */
	do_test_bit(instruction, REG_D, 0);  T_COUNT(8);
	break;
      case 0x43:	/* bit 0, e */
	do_test_bit(instruction, REG_E, 0);  T_COUNT(8);
	break;
      case 0x44:	/* bit 0, h */
	do_test_bit(instruction, REG_H, 0);  T_COUNT(8);
	break;
      case 0x45:	/* bit 0, l */
	do_test_bit(instruction, REG_L, 0);  T_COUNT(8);
	break;
      case 0x4F:	/* bit 1, a */
	do_test_bit(instruction, REG_A, 1);  T_COUNT(8);
	break;
      case 0x48:	/* bit 1, b */
	do_test_bit(instruction, REG_B, 1);  T_COUNT(8);
	break;
      case 0x49:	/* bit 1, c */
	do_test_bit(instruction, REG_C, 1);  T_COUNT(8);
	break;
      case 0x4A:	/* bit 1, d */
	do_test_bit(instruction, REG_D, 1);  T_COUNT(8);
	break;
      case 0x4B:	/* bit 1, e */
	do_test_bit(instruction, REG_E, 1);  T_COUNT(8);
	break;
      case 0x4C:	/* bit 1, h */
	do_test_bit(instruction, REG_H, 1);  T_COUNT(8);
	break;
      case 0x4D:	/* bit 1, l */
	do_test_bit(instruction, REG_L, 1);  T_COUNT(8);
	break;
      case 0x57:	/* bit 2, a */
	do_test_bit(instruction, REG_A, 2);  T_COUNT(8);
	break;
      case 0x50:	/* bit 2, b */
	do_test_bit(instruction, REG_B, 2);  T_COUNT(8);
	break;
      case 0x51:	/* bit 2, c */
	do_test_bit(instruction, REG_C, 2);  T_COUNT(8);
	break;
      case 0x52:	/* bit 2, d */
	do_test_bit(instruction, REG_D, 2);  T_COUNT(8);
	break;
      case 0x53:	/* bit 2, e */
	do_test_bit(instruction, REG_E, 2);  T_COUNT(8);
	break;
      case 0x54:	/* bit 2, h */
	do_test_bit(instruction, REG_H, 2);  T_COUNT(8);
	break;
      case 0x55:	/* bit 2, l */
	do_test_bit(instruction, REG_L, 2);  T_COUNT(8);
	break;
      case 0x5F:	/* bit 3, a */
	do_test_bit(instruction, REG_A, 3);  T_COUNT(8);
	break;
      case 0x58:	/* bit 3, b */
	do_test_bit(instruction, REG_B, 3);  T_COUNT(8);
	break;
      case 0x59:	/* bit 3, c */
	do_test_bit(instruction, REG_C, 3);  T_COUNT(8);
	break;
      case 0x5A:	/* bit 3, d */
	do_test_bit(instruction, REG_D, 3);  T_COUNT(8);
	break;
      case 0x5B:	/* bit 3, e */
	do_test_bit(instruction, REG_E, 3);  T_COUNT(8);
	break;
      case 0x5C:	/* bit 3, h */
	do_test_bit(instruction, REG_H, 3);  T_COUNT(8);
	break;
      case 0x5D:	/* bit 3, l */
	do_test_bit(instruction, REG_L, 3);  T_COUNT(8);
	break;
      case 0x67:	/* bit 4, a */
	do_test_bit(instruction, REG_A, 4);  T_COUNT(8);
	break;
      case 0x60:	/* bit 4, b */
	do_test_bit(instruction, REG_B, 4);  T_COUNT(8);
	break;
      case 0x61:	/* bit 4, c */
	do_test_bit(instruction, REG_C, 4);  T_COUNT(8);
	break;
      case 0x62:	/* bit 4, d */
	do_test_bit(instruction, REG_D, 4);  T_COUNT(8);
	break;
      case 0x63:	/* bit 4, e */
	do_test_bit(instruction, REG_E, 4);  T_COUNT(8);
	break;
      case 0x64:	/* bit 4, h */
	do_test_bit(instruction, REG_H, 4);  T_COUNT(8);
	break;
      case 0x65:	/* bit 4, l */
	do_test_bit(instruction, REG_L, 4);  T_COUNT(8);
	break;
      case 0x6F:	/* bit 5, a */
	do_test_bit(instruction, REG_A, 5);  T_COUNT(8);
	break;
      case 0x68:	/* bit 5, b */
	do_test_bit(instruction, REG_B, 5);  T_COUNT(8);
	break;
      case 0x69:	/* bit 5, c */
	do_test_bit(instruction, REG_C, 5);  T_COUNT(8);
	break;
      case 0x6A:	/* bit 5, d */
	do_test_bit(instruction, REG_D, 5);  T_COUNT(8);
	break;
      case 0x6B:	/* bit 5, e */
	do_test_bit(instruction, REG_E, 5);  T_COUNT(8);
	break;
      case 0x6C:	/* bit 5, h */
	do_test_bit(instruction, REG_H, 5);  T_COUNT(8);
	break;
      case 0x6D:	/* bit 5, l */
	do_test_bit(instruction, REG_L, 5);  T_COUNT(8);
	break;
      case 0x77:	/* bit 6, a */
	do_test_bit(instruction, REG_A, 6);  T_COUNT(8);
	break;
      case 0x70:	/* bit 6, b */
	do_test_bit(instruction, REG_B, 6);  T_COUNT(8);
	break;
      case 0x71:	/* bit 6, c */
	do_test_bit(instruction, REG_C, 6);  T_COUNT(8);
	break;
      case 0x72:	/* bit 6, d */
	do_test_bit(instruction, REG_D, 6);  T_COUNT(8);
	break;
      case 0x73:	/* bit 6, e */
	do_test_bit(instruction, REG_E, 6);  T_COUNT(8);
	break;
      case 0x74:	/* bit 6, h */
	do_test_bit(instruction, REG_H, 6);  T_COUNT(8);
	break;
      case 0x75:	/* bit 6, l */
	do_test_bit(instruction, REG_L, 6);  T_COUNT(8);
	break;
      case 0x7F:	/* bit 7, a */
	do_test_bit(instruction, REG_A, 7);  T_COUNT(8);
	break;
      case 0x78:	/* bit 7, b */
	do_test_bit(instruction, REG_B, 7);  T_COUNT(8);
	break;
      case 0x79:	/* bit 7, c */
	do_test_bit(instruction, REG_C, 7);  T_COUNT(8);
	break;
      case 0x7A:	/* bit 7, d */
	do_test_bit(instruction, REG_D, 7);  T_COUNT(8);
	break;
      case 0x7B:	/* bit 7, e */
	do_test_bit(instruction, REG_E, 7);  T_COUNT(8);
	break;
      case 0x7C:	/* bit 7, h */
	do_test_bit(instruction, REG_H, 7);  T_COUNT(8);
	break;
      case 0x7D:	/* bit 7, l */
	do_test_bit(instruction, REG_L, 7);  T_COUNT(8);
	break;
	
      case 0x46:	/* bit 0, (hl) */
	do_test_bit(instruction, mem_read(REG_HL), 0);  T_COUNT(12);
	break;
      case 0x4E:	/* bit 1, (hl) */
	do_test_bit(instruction, mem_read(REG_HL), 1);  T_COUNT(12);
	break;
      case 0x56:	/* bit 2, (hl) */
	do_test_bit(instruction, mem_read(REG_HL), 2);  T_COUNT(12);
	break;
      case 0x5E:	/* bit 3, (hl) */
	do_test_bit(instruction, mem_read(REG_HL), 3);  T_COUNT(12);
	break;
      case 0x66:	/* bit 4, (hl) */
	do_test_bit(instruction, mem_read(REG_HL), 4);  T_COUNT(12);
	break;
      case 0x6E:	/* bit 5, (hl) */
	do_test_bit(instruction, mem_read(REG_HL), 5);  T_COUNT(12);
	break;
      case 0x76:	/* bit 6, (hl) */
	do_test_bit(instruction, mem_read(REG_HL), 6);  T_COUNT(12);
	break;
      case 0x7E:	/* bit 7, (hl) */
	do_test_bit(instruction, mem_read(REG_HL), 7);  T_COUNT(12);
	break;

      case 0x87:	/* res 0, a */
	REG_A &= ~(1 << 0);  T_COUNT(8);
	break;
      case 0x80:	/* res 0, b */
	REG_B &= ~(1 << 0);  T_COUNT(8);
	break;
      case 0x81:	/* res 0, c */
	REG_C &= ~(1 << 0);  T_COUNT(8);
	break;
      case 0x82:	/* res 0, d */
	REG_D &= ~(1 << 0);  T_COUNT(8);
	break;
      case 0x83:	/* res 0, e */
	REG_E &= ~(1 << 0);  T_COUNT(8);
	break;
      case 0x84:	/* res 0, h */
	REG_H &= ~(1 << 0);  T_COUNT(8);
	break;
      case 0x85:	/* res 0, l */
	REG_L &= ~(1 << 0);  T_COUNT(8);
	break;
      case 0x8F:	/* res 1, a */
	REG_A &= ~(1 << 1);  T_COUNT(8);
	break;
      case 0x88:	/* res 1, b */
	REG_B &= ~(1 << 1);  T_COUNT(8);
	break;
      case 0x89:	/* res 1, c */
	REG_C &= ~(1 << 1);  T_COUNT(8);
	break;
      case 0x8A:	/* res 1, d */
	REG_D &= ~(1 << 1);  T_COUNT(8);
	break;
      case 0x8B:	/* res 1, e */
	REG_E &= ~(1 << 1);  T_COUNT(8);
	break;
      case 0x8C:	/* res 1, h */
	REG_H &= ~(1 << 1);  T_COUNT(8);
	break;
      case 0x8D:	/* res 1, l */
	REG_L &= ~(1 << 1);  T_COUNT(8);
	break;
      case 0x97:	/* res 2, a */
	REG_A &= ~(1 << 2);  T_COUNT(8);
	break;
      case 0x90:	/* res 2, b */
	REG_B &= ~(1 << 2);  T_COUNT(8);
	break;
      case 0x91:	/* res 2, c */
	REG_C &= ~(1 << 2);  T_COUNT(8);
	break;
      case 0x92:	/* res 2, d */
	REG_D &= ~(1 << 2);  T_COUNT(8);
	break;
      case 0x93:	/* res 2, e */
	REG_E &= ~(1 << 2);  T_COUNT(8);
	break;
      case 0x94:	/* res 2, h */
	REG_H &= ~(1 << 2);  T_COUNT(8);
	break;
      case 0x95:	/* res 2, l */
	REG_L &= ~(1 << 2);  T_COUNT(8);
	break;
      case 0x9F:	/* res 3, a */
	REG_A &= ~(1 << 3);  T_COUNT(8);
	break;
      case 0x98:	/* res 3, b */
	REG_B &= ~(1 << 3);  T_COUNT(8);
	break;
      case 0x99:	/* res 3, c */
	REG_C &= ~(1 << 3);  T_COUNT(8);
	break;
      case 0x9A:	/* res 3, d */
	REG_D &= ~(1 << 3);  T_COUNT(8);
	break;
      case 0x9B:	/* res 3, e */
	REG_E &= ~(1 << 3);  T_COUNT(8);
	break;
      case 0x9C:	/* res 3, h */
	REG_H &= ~(1 << 3);  T_COUNT(8);
	break;
      case 0x9D:	/* res 3, l */
	REG_L &= ~(1 << 3);  T_COUNT(8);
	break;
      case 0xA7:	/* res 4, a */
	REG_A &= ~(1 << 4);  T_COUNT(8);
	break;
      case 0xA0:	/* res 4, b */
	REG_B &= ~(1 << 4);  T_COUNT(8);
	break;
      case 0xA1:	/* res 4, c */
	REG_C &= ~(1 << 4);  T_COUNT(8);
	break;
      case 0xA2:	/* res 4, d */
	REG_D &= ~(1 << 4);  T_COUNT(8);
	break;
      case 0xA3:	/* res 4, e */
	REG_E &= ~(1 << 4);  T_COUNT(8);
	break;
      case 0xA4:	/* res 4, h */
	REG_H &= ~(1 << 4);  T_COUNT(8);
	break;
      case 0xA5:	/* res 4, l */
	REG_L &= ~(1 << 4);  T_COUNT(8);
	break;
      case 0xAF:	/* res 5, a */
	REG_A &= ~(1 << 5);  T_COUNT(8);
	break;
      case 0xA8:	/* res 5, b */
	REG_B &= ~(1 << 5);  T_COUNT(8);
	break;
      case 0xA9:	/* res 5, c */
	REG_C &= ~(1 << 5);  T_COUNT(8);
	break;
      case 0xAA:	/* res 5, d */
	REG_D &= ~(1 << 5);  T_COUNT(8);
	break;
      case 0xAB:	/* res 5, e */
	REG_E &= ~(1 << 5);  T_COUNT(8);
	break;
      case 0xAC:	/* res 5, h */
	REG_H &= ~(1 << 5);  T_COUNT(8);
	break;
      case 0xAD:	/* res 5, l */
	REG_L &= ~(1 << 5);  T_COUNT(8);
	break;
      case 0xB7:	/* res 6, a */
	REG_A &= ~(1 << 6);  T_COUNT(8);
	break;
      case 0xB0:	/* res 6, b */
	REG_B &= ~(1 << 6);  T_COUNT(8);
	break;
      case 0xB1:	/* res 6, c */
	REG_C &= ~(1 << 6);  T_COUNT(8);
	break;
      case 0xB2:	/* res 6, d */
	REG_D &= ~(1 << 6);  T_COUNT(8);
	break;
      case 0xB3:	/* res 6, e */
	REG_E &= ~(1 << 6);  T_COUNT(8);
	break;
      case 0xB4:	/* res 6, h */
	REG_H &= ~(1 << 6);  T_COUNT(8);
	break;
      case 0xB5:	/* res 6, l */
	REG_L &= ~(1 << 6);  T_COUNT(8);
	break;
      case 0xBF:	/* res 7, a */
	REG_A &= ~(1 << 7);  T_COUNT(8);
	break;
      case 0xB8:	/* res 7, b */
	REG_B &= ~(1 << 7);  T_COUNT(8);
	break;
      case 0xB9:	/* res 7, c */
	REG_C &= ~(1 << 7);  T_COUNT(8);
	break;
      case 0xBA:	/* res 7, d */
	REG_D &= ~(1 << 7);  T_COUNT(8);
	break;
      case 0xBB:	/* res 7, e */
	REG_E &= ~(1 << 7);  T_COUNT(8);
	break;
      case 0xBC:	/* res 7, h */
	REG_H &= ~(1 << 7);  T_COUNT(8);
	break;
      case 0xBD:	/* res 7, l */
	REG_L &= ~(1 << 7);  T_COUNT(8);
	break;

      case 0x86:	/* res 0, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) & ~(1 << 0));  T_COUNT(15);
	break;
      case 0x8E:	/* res 1, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) & ~(1 << 1));  T_COUNT(15);
	break;
      case 0x96:	/* res 2, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) & ~(1 << 2));  T_COUNT(15);
	break;
      case 0x9E:	/* res 3, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) & ~(1 << 3));  T_COUNT(15);
	break;
      case 0xA6:	/* res 4, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) & ~(1 << 4));  T_COUNT(15);
	break;
      case 0xAE:	/* res 5, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) & ~(1 << 5));  T_COUNT(15);
	break;
      case 0xB6:	/* res 6, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) & ~(1 << 6));  T_COUNT(15);
	break;
      case 0xBE:	/* res 7, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) & ~(1 << 7));  T_COUNT(15);
	break;

      case 0x17:	/* rl a */
	REG_A = rl_byte(REG_A);  T_COUNT(8);
	break;
      case 0x10:	/* rl b */
	REG_B = rl_byte(REG_B);  T_COUNT(8);
	break;
      case 0x11:	/* rl c */
	REG_C = rl_byte(REG_C);  T_COUNT(8);
	break;
      case 0x12:	/* rl d */
	REG_D = rl_byte(REG_D);  T_COUNT(8);
	break;
      case 0x13:	/* rl e */
	REG_E = rl_byte(REG_E);  T_COUNT(8);
	break;
      case 0x14:	/* rl h */
	REG_H = rl_byte(REG_H);  T_COUNT(8);
	break;
      case 0x15:	/* rl l */
	REG_L = rl_byte(REG_L);  T_COUNT(8);
	break;
      case 0x16:	/* rl (hl) */
	mem_write(REG_HL, rl_byte(mem_read(REG_HL)));  T_COUNT(15);
	break;

      case 0x07:	/* rlc a */
	REG_A = rlc_byte(REG_A);  T_COUNT(8);
	break;
      case 0x00:	/* rlc b */
	REG_B = rlc_byte(REG_B);  T_COUNT(8);
	break;
      case 0x01:	/* rlc c */
	REG_C = rlc_byte(REG_C);  T_COUNT(8);
	break;
      case 0x02:	/* rlc d */
	REG_D = rlc_byte(REG_D);  T_COUNT(8);
	break;
      case 0x03:	/* rlc e */
	REG_E = rlc_byte(REG_E);  T_COUNT(8);
	break;
      case 0x04:	/* rlc h */
	REG_H = rlc_byte(REG_H);  T_COUNT(8);
	break;
      case 0x05:	/* rlc l */
	REG_L = rlc_byte(REG_L);  T_COUNT(8);
	break;
      case 0x06:	/* rlc (hl) */
	mem_write(REG_HL, rlc_byte(mem_read(REG_HL)));  T_COUNT(15);
	break;

      case 0x1F:	/* rr a */
	REG_A = rr_byte(REG_A);  T_COUNT(8);
	break;
      case 0x18:	/* rr b */
	REG_B = rr_byte(REG_B);  T_COUNT(8);
	break;
      case 0x19:	/* rr c */
	REG_C = rr_byte(REG_C);  T_COUNT(8);
	break;
      case 0x1A:	/* rr d */
	REG_D = rr_byte(REG_D);  T_COUNT(8);
	break;
      case 0x1B:	/* rr e */
	REG_E = rr_byte(REG_E);  T_COUNT(8);
	break;
      case 0x1C:	/* rr h */
	REG_H = rr_byte(REG_H);  T_COUNT(8);
	break;
      case 0x1D:	/* rr l */
	REG_L = rr_byte(REG_L);  T_COUNT(8);
	break;
      case 0x1E:	/* rr (hl) */
	mem_write(REG_HL, rr_byte(mem_read(REG_HL)));  T_COUNT(15);
	break;

      case 0x0F:	/* rrc a */
	REG_A = rrc_byte(REG_A);  T_COUNT(8);
	break;
      case 0x08:	/* rrc b */
	REG_B = rrc_byte(REG_B);  T_COUNT(8);
	break;
      case 0x09:	/* rrc c */
	REG_C = rrc_byte(REG_C);  T_COUNT(8);
	break;
      case 0x0A:	/* rrc d */
	REG_D = rrc_byte(REG_D);  T_COUNT(8);
	break;
      case 0x0B:	/* rrc e */
	REG_E = rrc_byte(REG_E);  T_COUNT(8);
	break;
      case 0x0C:	/* rrc h */
	REG_H = rrc_byte(REG_H);  T_COUNT(8);
	break;
      case 0x0D:	/* rrc l */
	REG_L = rrc_byte(REG_L);  T_COUNT(8);
	break;
      case 0x0E:	/* rrc (hl) */
	mem_write(REG_HL, rrc_byte(mem_read(REG_HL)));  T_COUNT(15);
	break;

      case 0xC7:	/* set 0, a */
	REG_A |= (1 << 0);  T_COUNT(8);
	break;
      case 0xC0:	/* set 0, b */
	REG_B |= (1 << 0);  T_COUNT(8);
	break;
      case 0xC1:	/* set 0, c */
	REG_C |= (1 << 0);  T_COUNT(8);
	break;
      case 0xC2:	/* set 0, d */
	REG_D |= (1 << 0);  T_COUNT(8);
	break;
      case 0xC3:	/* set 0, e */
	REG_E |= (1 << 0);  T_COUNT(8);
	break;
      case 0xC4:	/* set 0, h */
	REG_H |= (1 << 0);  T_COUNT(8);
	break;
      case 0xC5:	/* set 0, l */
	REG_L |= (1 << 0);  T_COUNT(8);
	break;
      case 0xCF:	/* set 1, a */
	REG_A |= (1 << 1);  T_COUNT(8);
	break;
      case 0xC8:	/* set 1, b */
	REG_B |= (1 << 1);  T_COUNT(8);
	break;
      case 0xC9:	/* set 1, c */
	REG_C |= (1 << 1);  T_COUNT(8);
	break;
      case 0xCA:	/* set 1, d */
	REG_D |= (1 << 1);  T_COUNT(8);
	break;
      case 0xCB:	/* set 1, e */
	REG_E |= (1 << 1);  T_COUNT(8);
	break;
      case 0xCC:	/* set 1, h */
	REG_H |= (1 << 1);  T_COUNT(8);
	break;
      case 0xCD:	/* set 1, l */
	REG_L |= (1 << 1);  T_COUNT(8);
	break;
      case 0xD7:	/* set 2, a */
	REG_A |= (1 << 2);  T_COUNT(8);
	break;
      case 0xD0:	/* set 2, b */
	REG_B |= (1 << 2);  T_COUNT(8);
	break;
      case 0xD1:	/* set 2, c */
	REG_C |= (1 << 2);  T_COUNT(8);
	break;
      case 0xD2:	/* set 2, d */
	REG_D |= (1 << 2);  T_COUNT(8);
	break;
      case 0xD3:	/* set 2, e */
	REG_E |= (1 << 2);  T_COUNT(8);
	break;
      case 0xD4:	/* set 2, h */
	REG_H |= (1 << 2);  T_COUNT(8);
	break;
      case 0xD5:	/* set 2, l */
	REG_L |= (1 << 2);  T_COUNT(8);
	break;
      case 0xDF:	/* set 3, a */
	REG_A |= (1 << 3);  T_COUNT(8);
	break;
      case 0xD8:	/* set 3, b */
	REG_B |= (1 << 3);  T_COUNT(8);
	break;
      case 0xD9:	/* set 3, c */
	REG_C |= (1 << 3);  T_COUNT(8);
	break;
      case 0xDA:	/* set 3, d */
	REG_D |= (1 << 3);  T_COUNT(8);
	break;
      case 0xDB:	/* set 3, e */
	REG_E |= (1 << 3);  T_COUNT(8);
	break;
      case 0xDC:	/* set 3, h */
	REG_H |= (1 << 3);  T_COUNT(8);
	break;
      case 0xDD:	/* set 3, l */
	REG_L |= (1 << 3);  T_COUNT(8);
	break;
      case 0xE7:	/* set 4, a */
	REG_A |= (1 << 4);  T_COUNT(8);
	break;
      case 0xE0:	/* set 4, b */
	REG_B |= (1 << 4);  T_COUNT(8);
	break;
      case 0xE1:	/* set 4, c */
	REG_C |= (1 << 4);  T_COUNT(8);
	break;
      case 0xE2:	/* set 4, d */
	REG_D |= (1 << 4);  T_COUNT(8);
	break;
      case 0xE3:	/* set 4, e */
	REG_E |= (1 << 4);  T_COUNT(8);
	break;
      case 0xE4:	/* set 4, h */
	REG_H |= (1 << 4);  T_COUNT(8);
	break;
      case 0xE5:	/* set 4, l */
	REG_L |= (1 << 4);  T_COUNT(8);
	break;
      case 0xEF:	/* set 5, a */
	REG_A |= (1 << 5);  T_COUNT(8);
	break;
      case 0xE8:	/* set 5, b */
	REG_B |= (1 << 5);  T_COUNT(8);
	break;
      case 0xE9:	/* set 5, c */
	REG_C |= (1 << 5);  T_COUNT(8);
	break;
      case 0xEA:	/* set 5, d */
	REG_D |= (1 << 5);  T_COUNT(8);
	break;
      case 0xEB:	/* set 5, e */
	REG_E |= (1 << 5);  T_COUNT(8);
	break;
      case 0xEC:	/* set 5, h */
	REG_H |= (1 << 5);  T_COUNT(8);
	break;
      case 0xED:	/* set 5, l */
	REG_L |= (1 << 5);  T_COUNT(8);
	break;
      case 0xF7:	/* set 6, a */
	REG_A |= (1 << 6);  T_COUNT(8);
	break;
      case 0xF0:	/* set 6, b */
	REG_B |= (1 << 6);  T_COUNT(8);
	break;
      case 0xF1:	/* set 6, c */
	REG_C |= (1 << 6);  T_COUNT(8);
	break;
      case 0xF2:	/* set 6, d */
	REG_D |= (1 << 6);  T_COUNT(8);
	break;
      case 0xF3:	/* set 6, e */
	REG_E |= (1 << 6);  T_COUNT(8);
	break;
      case 0xF4:	/* set 6, h */
	REG_H |= (1 << 6);  T_COUNT(8);
	break;
      case 0xF5:	/* set 6, l */
	REG_L |= (1 << 6);  T_COUNT(8);
	break;
      case 0xFF:	/* set 7, a */
	REG_A |= (1 << 7);  T_COUNT(8);
	break;
      case 0xF8:	/* set 7, b */
	REG_B |= (1 << 7);  T_COUNT(8);
	break;
      case 0xF9:	/* set 7, c */
	REG_C |= (1 << 7);  T_COUNT(8);
	break;
      case 0xFA:	/* set 7, d */
	REG_D |= (1 << 7);  T_COUNT(8);
	break;
      case 0xFB:	/* set 7, e */
	REG_E |= (1 << 7);  T_COUNT(8);
	break;
      case 0xFC:	/* set 7, h */
	REG_H |= (1 << 7);  T_COUNT(8);
	break;
      case 0xFD:	/* set 7, l */
	REG_L |= (1 << 7);  T_COUNT(8);
	break;

      case 0xC6:	/* set 0, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) | (1 << 0));  T_COUNT(15);
	break;
      case 0xCE:	/* set 1, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) | (1 << 1));  T_COUNT(15);
	break;
      case 0xD6:	/* set 2, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) | (1 << 2));  T_COUNT(15);
	break;
      case 0xDE:	/* set 3, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) | (1 << 3));  T_COUNT(15);
	break;
      case 0xE6:	/* set 4, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) | (1 << 4));  T_COUNT(15);
	break;
      case 0xEE:	/* set 5, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) | (1 << 5));  T_COUNT(15);
	break;
      case 0xF6:	/* set 6, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) | (1 << 6));  T_COUNT(15);
	break;
      case 0xFE:	/* set 7, (hl) */
	mem_write(REG_HL, mem_read(REG_HL) | (1 << 7));  T_COUNT(15);
	break;

      case 0x27:	/* sla a */
	REG_A = sla_byte(REG_A);  T_COUNT(8);
	break;
      case 0x20:	/* sla b */
	REG_B = sla_byte(REG_B);  T_COUNT(8);
	break;
      case 0x21:	/* sla c */
	REG_C = sla_byte(REG_C);  T_COUNT(8);
	break;
      case 0x22:	/* sla d */
	REG_D = sla_byte(REG_D);  T_COUNT(8);
	break;
      case 0x23:	/* sla e */
	REG_E = sla_byte(REG_E);  T_COUNT(8);
	break;
      case 0x24:	/* sla h */
	REG_H = sla_byte(REG_H);  T_COUNT(8);
	break;
      case 0x25:	/* sla l */
	REG_L = sla_byte(REG_L);  T_COUNT(8);
	break;
      case 0x26:	/* sla (hl) */
	mem_write(REG_HL, sla_byte(mem_read(REG_HL)));  T_COUNT(15);
	break;

      case 0x2F:	/* sra a */
	REG_A = sra_byte(REG_A);  T_COUNT(8);
	break;
      case 0x28:	/* sra b */
	REG_B = sra_byte(REG_B);  T_COUNT(8);
	break;
      case 0x29:	/* sra c */
	REG_C = sra_byte(REG_C);  T_COUNT(8);
	break;
      case 0x2A:	/* sra d */
	REG_D = sra_byte(REG_D);  T_COUNT(8);
	break;
      case 0x2B:	/* sra e */
	REG_E = sra_byte(REG_E);  T_COUNT(8);
	break;
      case 0x2C:	/* sra h */
	REG_H = sra_byte(REG_H);  T_COUNT(8);
	break;
      case 0x2D:	/* sra l */
	REG_L = sra_byte(REG_L);  T_COUNT(8);
	break;
      case 0x2E:	/* sra (hl) */
	mem_write(REG_HL, sra_byte(mem_read(REG_HL)));  T_COUNT(15);
	break;

      case 0x37:	/* slia a [undocumented] */
	REG_A = slia_byte(REG_A);  T_COUNT(8);
	break;
      case 0x30:	/* slia b [undocumented] */
	REG_B = slia_byte(REG_B);  T_COUNT(8);
	break;
      case 0x31:	/* slia c [undocumented] */
	REG_C = slia_byte(REG_C);  T_COUNT(8);
	break;
      case 0x32:	/* slia d [undocumented] */
	REG_D = slia_byte(REG_D);  T_COUNT(8);
	break;
      case 0x33:	/* slia e [undocumented] */
	REG_E = slia_byte(REG_E);  T_COUNT(8);
	break;
      case 0x34:	/* slia h [undocumented] */
	REG_H = slia_byte(REG_H);  T_COUNT(8);
	break;
      case 0x35:	/* slia l [undocumented] */
	REG_L = slia_byte(REG_L);  T_COUNT(8);
	break;
      case 0x36:	/* slia (hl) [undocumented] */
	mem_write(REG_HL, slia_byte(mem_read(REG_HL)));  T_COUNT(15);
	break;

      case 0x3F:	/* srl a */
	REG_A = srl_byte(REG_A);  T_COUNT(8);
	break;
      case 0x38:	/* srl b */
	REG_B = srl_byte(REG_B);  T_COUNT(8);
	break;
      case 0x39:	/* srl c */
	REG_C = srl_byte(REG_C);  T_COUNT(8);
	break;
      case 0x3A:	/* srl d */
	REG_D = srl_byte(REG_D);  T_COUNT(8);
	break;
      case 0x3B:	/* srl e */
	REG_E = srl_byte(REG_E);  T_COUNT(8);
	break;
      case 0x3C:	/* srl h */
	REG_H = srl_byte(REG_H);  T_COUNT(8);
	break;
      case 0x3D:	/* srl l */
	REG_L = srl_byte(REG_L);  T_COUNT(8);
	break;
      case 0x3E:	/* srl (hl) */
	mem_write(REG_HL, srl_byte(mem_read(REG_HL)));  T_COUNT(15);
	break;

      default:
	disassemble(REG_PC - 2);
	error("unsupported instruction");
    }
}


/*
 * Extended instructions which have 0xDD or 0xFD as the first byte:
 */
static void do_indexed_instruction(Ushort *ixp)
{
    Uchar instruction;
    
    instruction = mem_read(REG_PC++);
    
    switch(instruction)
    {
	/* same for FD, except uses IY */

      case 0x8E:	/* adc a, (ix + offset) */
	do_adc_byte(mem_read((*ixp + (signed char) mem_read(REG_PC++))
			     & 0xffff));
	T_COUNT(19);
	break;

      case 0x86:	/* add a, (ix + offset) */
	do_add_byte(mem_read((*ixp + (signed char) mem_read(REG_PC++))
			     & 0xffff));
	T_COUNT(19);
	break;

      case 0x09:	/* add ix, bc */
	do_add_word_index(ixp, REG_BC);  T_COUNT(15);
	break;
      case 0x19:	/* add ix, de */
	do_add_word_index(ixp, REG_DE);  T_COUNT(15);
	break;
      case 0x29:	/* add ix, ix */
	do_add_word_index(ixp, *ixp);  T_COUNT(15);
	break;
      case 0x39:	/* add ix, sp */
	do_add_word_index(ixp, REG_SP);  T_COUNT(15);
	break;

      case 0xA6:	/* and (ix + offset) */
	do_and_byte(mem_read((*ixp + (signed char) mem_read(REG_PC++))
			     & 0xffff));
	T_COUNT(19);
	break;

      case 0xBE:	/* cp (ix + offset) */
	do_cp(mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff));
	T_COUNT(19);
	break;

      case 0x35:	/* dec (ix + offset) */
        {
	  Ushort address;
	  Uchar value;
	  address = *ixp + (signed char) mem_read(REG_PC++);
	  value = mem_read(address) - 1;
	  mem_write(address, value);
	  do_flags_dec_byte(value);
        }
	T_COUNT(23);
	break;

      case 0x2B:	/* dec ix */
	(*ixp)--;
	T_COUNT(10);
	break;

      case 0xE3:	/* ex (sp), ix */
        {
	  Ushort temp;
	  temp = mem_read_word(REG_SP);
	  mem_write_word(REG_SP, *ixp);
	  *ixp = temp;
        }
	T_COUNT(23);
	break;

      case 0x34:	/* inc (ix + offset) */
        {
	  Ushort address;
	  Uchar value;
	  address = *ixp + (signed char) mem_read(REG_PC++);
	  value = mem_read(address) + 1;
	  mem_write(address, value);
	  do_flags_inc_byte(value);
        }
	T_COUNT(23);
	break;

      case 0x23:	/* inc ix */
	(*ixp)++;
	T_COUNT(10);
	break;

      case 0xE9:	/* jp (ix) */
	REG_PC = *ixp;
	T_COUNT(8);
	break;

      case 0x7E:	/* ld a, (ix + offset) */
	REG_A = mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff);
	T_COUNT(19);
	break;
      case 0x46:	/* ld b, (ix + offset) */
	REG_B = mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff);
	T_COUNT(19);
	break;
      case 0x4E:	/* ld c, (ix + offset) */
	REG_C = mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff);
	T_COUNT(19);
	break;
      case 0x56:	/* ld d, (ix + offset) */
	REG_D = mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff);
	T_COUNT(19);
	break;
      case 0x5E:	/* ld e, (ix + offset) */
	REG_E = mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff);
	T_COUNT(19);
	break;
      case 0x66:	/* ld h, (ix + offset) */
	REG_H = mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff);
	T_COUNT(19);
	break;
      case 0x6E:	/* ld l, (ix + offset) */
	REG_L = mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff);
	T_COUNT(19);
	break;

      case 0x36:	/* ld (ix + offset), value */
	mem_write(*ixp + (signed char) mem_read(REG_PC), mem_read((REG_PC+1)&0xffff));
	REG_PC += 2;
	T_COUNT(19);
	break;

      case 0x77:	/* ld (ix + offset), a */
	mem_write(*ixp + (signed char) mem_read(REG_PC++), REG_A);
	T_COUNT(19);
	break;
      case 0x70:	/* ld (ix + offset), b */
	mem_write(*ixp + (signed char) mem_read(REG_PC++), REG_B);
	T_COUNT(19);
	break;
      case 0x71:	/* ld (ix + offset), c */
	mem_write(*ixp + (signed char) mem_read(REG_PC++), REG_C);
	T_COUNT(19);
	break;
      case 0x72:	/* ld (ix + offset), d */
	mem_write(*ixp + (signed char) mem_read(REG_PC++), REG_D);
	T_COUNT(19);
	break;
      case 0x73:	/* ld (ix + offset), e */
	mem_write(*ixp + (signed char) mem_read(REG_PC++), REG_E);
	T_COUNT(19);
	break;
      case 0x74:	/* ld (ix + offset), h */
	mem_write(*ixp + (signed char) mem_read(REG_PC++), REG_H);
	T_COUNT(19);
	break;
      case 0x75:	/* ld (ix + offset), l */
	mem_write(*ixp + (signed char) mem_read(REG_PC++), REG_L);
	T_COUNT(19);
	break;

      case 0x22:	/* ld (address), ix */
	mem_write_word(mem_read_word(REG_PC), *ixp);
	REG_PC += 2;
	T_COUNT(20);
	break;

      case 0xF9:	/* ld sp, ix */
	REG_SP = *ixp;
	T_COUNT(10);
	break;

      case 0x21:	/* ld ix, value */
	*ixp = mem_read_word(REG_PC);
        REG_PC += 2;
	T_COUNT(14);
	break;

      case 0x2A:	/* ld ix, (address) */
	*ixp = mem_read_word(mem_read_word(REG_PC));
	REG_PC += 2;
	T_COUNT(20);
	break;

      case 0xB6:	/* or (ix + offset) */
	do_or_byte(mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff));
	T_COUNT(19);
	break;

      case 0xE1:	/* pop ix */
	*ixp = mem_read_word(REG_SP);
	REG_SP += 2;
	T_COUNT(14);
	break;

      case 0xE5:	/* push ix */
	REG_SP -= 2;
	mem_write_word(REG_SP, *ixp);
	T_COUNT(15);
	break;

      case 0x9E:	/* sbc a, (ix + offset) */
	do_sbc_byte(mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff));
	T_COUNT(19);
	break;

      case 0x96:	/* sub a, (ix + offset) */
	do_sub_byte(mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff));
	T_COUNT(19);
	break;

      case 0xAE:	/* xor (ix + offset) */
	do_xor_byte(mem_read((*ixp + (signed char) mem_read(REG_PC++)) & 0xffff));
	T_COUNT(19);
	break;

      case 0xCB:
        {
	  signed char offset, result = 0;
	  Uchar sub_instruction;

	  offset = (signed char) mem_read(REG_PC++);
	  sub_instruction = mem_read(REG_PC++);

	  /* Instructions with (sub_instruction & 7) != 6 are undocumented;
	     their extra effect is handled after this switch */
	  switch(sub_instruction&0xf8)
	  {
	    case 0x00:	/* rlc (ix + offset) */
	      result = rlc_byte(mem_read((*ixp + offset) & 0xffff));
	      mem_write(*ixp + offset, result);
	      T_COUNT(23);
	      break;

	    case 0x08:	/* rrc (ix + offset) */
	      result = rrc_byte(mem_read((*ixp + offset) & 0xffff));
	      mem_write(*ixp + offset, result);
	      T_COUNT(23);
	      break;

	    case 0x10:	/* rl (ix + offset) */
	      result = rl_byte(mem_read((*ixp + offset) & 0xffff));
	      mem_write(*ixp + offset, result);
	      T_COUNT(23);
	      break;

	    case 0x18:	/* rr (ix + offset) */
	      result = rr_byte(mem_read((*ixp + offset) & 0xffff));
	      mem_write(*ixp + offset, result);
	      T_COUNT(23);
	      break;

	    case 0x20:	/* sla (ix + offset) */
	      result = sla_byte(mem_read((*ixp + offset) & 0xffff));
	      mem_write(*ixp + offset, result);
	      T_COUNT(23);
	      break;

	    case 0x28:	/* sra (ix + offset) */
	      result = sra_byte(mem_read((*ixp + offset) & 0xffff));
	      mem_write(*ixp + offset, result);
	      T_COUNT(23);
	      break;

	    case 0x30:	/* slia (ix + offset) [undocumented] */
	      result = slia_byte(mem_read((*ixp + offset) & 0xffff));
	      mem_write(*ixp + offset, result);
	      T_COUNT(23);
	      break;

	    case 0x38:	/* srl (ix + offset) */
	      result = srl_byte(mem_read((*ixp + offset) & 0xffff));
	      mem_write(*ixp + offset, result);
	      T_COUNT(23);
	      break;

	    case 0x40:  /* bit 0, (ix + offset) */
	    case 0x48:  /* bit 1, (ix + offset) */
	    case 0x50:  /* bit 2, (ix + offset) */
	    case 0x58:  /* bit 3, (ix + offset) */
	    case 0x60:  /* bit 4, (ix + offset) */
	    case 0x68:  /* bit 5, (ix + offset) */
	    case 0x70:  /* bit 6, (ix + offset) */
	    case 0x78:  /* bit 7, (ix + offset) */
	      do_test_bit(sub_instruction, mem_read((*ixp + offset) & 0xffff),
			  (sub_instruction >> 3) & 7);
	      T_COUNT(20);
	      break;

	    case 0x80:	/* res 0, (ix + offset) */
	    case 0x88:	/* res 1, (ix + offset) */
	    case 0x90:	/* res 2, (ix + offset) */
	    case 0x98:	/* res 3, (ix + offset) */
	    case 0xA0:	/* res 4, (ix + offset) */
	    case 0xA8:	/* res 5, (ix + offset) */
	    case 0xB0:	/* res 6, (ix + offset) */
	    case 0xB8:	/* res 7, (ix + offset) */
	      result = mem_read((*ixp + offset) & 0xffff) &
		~(1 << ((sub_instruction >> 3) & 7));
	      mem_write(*ixp + offset, result);
	      T_COUNT(23);
	      break;

	    case 0xC0:	/* set 0, (ix + offset) */
	    case 0xC8:	/* set 1, (ix + offset) */
	    case 0xD0:	/* set 2, (ix + offset) */
	    case 0xD8:	/* set 3, (ix + offset) */
	    case 0xE0:	/* set 4, (ix + offset) */
	    case 0xE8:	/* set 5, (ix + offset) */
	    case 0xF0:	/* set 6, (ix + offset) */
	    case 0xF8:	/* set 7, (ix + offset) */
	      result = mem_read((*ixp + offset) & 0xffff) |
		(1 << ((sub_instruction >> 3) & 7));
	      mem_write(*ixp + offset, result);
	      T_COUNT(23);
	      break;
	  }

	  if (sub_instruction < 0x40 || sub_instruction > 0x7f)
          {
	    switch (sub_instruction & 7) 
	    {
	      /* Undocumented cases */
	      case 0:  REG_B = result; break;
	      case 1:  REG_C = result; break;
	      case 2:  REG_D = result; break;
	      case 3:  REG_E = result; break;
	      case 4:  REG_H = result; break;
	      case 5:  REG_L = result; break;
	      case 7:  REG_A = result; break;
	    }
	  }
        }
	break;

      /* begin undocumented instructions -- timings are a (good) guess */
      case 0x8C:	/* adc a, ixh */
	do_adc_byte(HIGH(ixp));  T_COUNT(8);
	break;
      case 0x8D:	/* adc a, ixl */
	do_adc_byte(LOW(ixp));  T_COUNT(8);
	break;
      case 0x84:	/* add a, ixh */
	do_add_byte(HIGH(ixp));  T_COUNT(8);
	break;
      case 0x85:	/* add a, ixl */
	do_add_byte(LOW(ixp));  T_COUNT(8);
	break;
      case 0xA4:	/* and ixh */
	do_and_byte(HIGH(ixp));  T_COUNT(8);
	break;
      case 0xA5:	/* and ixl */
	do_and_byte(LOW(ixp));  T_COUNT(8);
	break;
      case 0xBC:	/* cp ixh */
	do_cp(HIGH(ixp));  T_COUNT(8);
	break;
      case 0xBD:	/* cp ixl */
	do_cp(LOW(ixp));  T_COUNT(8);
	break;
      case 0x25:	/* dec ixh */
	do_flags_dec_byte(--HIGH(ixp));  T_COUNT(8);
	break;
      case 0x2D:	/* dec ixl */
	do_flags_dec_byte(--LOW(ixp));  T_COUNT(8);
	break;
      case 0x24:	/* inc ixh */
	HIGH(ixp)++;
	do_flags_inc_byte(HIGH(ixp));  T_COUNT(8);
	break;
      case 0x2C:	/* inc ixl */
	LOW(ixp)++;
	do_flags_inc_byte(LOW(ixp));  T_COUNT(8);
	break;
      case 0x7C:	/* ld a, ixh */
	REG_A = HIGH(ixp);  T_COUNT(8);
	break;
      case 0x7D:	/* ld a, ixl */
	REG_A = LOW(ixp);  T_COUNT(8);
	break;
      case 0x44:	/* ld b, ixh */
	REG_B = HIGH(ixp);  T_COUNT(8);
	break;
      case 0x45:	/* ld b, ixl */
	REG_B = LOW(ixp);  T_COUNT(8);
	break;
      case 0x4C:	/* ld c, ixh */
	REG_C = HIGH(ixp);  T_COUNT(8);
	break;
      case 0x4D:	/* ld c, ixl */
	REG_C = LOW(ixp);  T_COUNT(8);
	break;
      case 0x54:	/* ld d, ixh */
	REG_D = HIGH(ixp);  T_COUNT(8);
	break;
      case 0x55:	/* ld d, ixl */
	REG_D = LOW(ixp);  T_COUNT(8);
	break;
      case 0x5C:	/* ld e, ixh */
	REG_E = HIGH(ixp);  T_COUNT(8);
	break;
      case 0x5D:	/* ld e, ixl */
	REG_E = LOW(ixp);  T_COUNT(8);
	break;
      case 0x67:	/* ld ixh, a */
	HIGH(ixp) = REG_A;  T_COUNT(8);
	break;
      case 0x60:	/* ld ixh, b */
	HIGH(ixp) = REG_B;  T_COUNT(8);
	break;
      case 0x61:	/* ld ixh, c */
	HIGH(ixp) = REG_C;  T_COUNT(8);
	break;
      case 0x62:	/* ld ixh, d */
	HIGH(ixp) = REG_D;  T_COUNT(8);
	break;
      case 0x63:	/* ld ixh, e */
	HIGH(ixp) = REG_E;  T_COUNT(8);
	break;
      case 0x64:	/* ld ixh, ixh */
	HIGH(ixp) = HIGH(ixp);  T_COUNT(8);
	break;
      case 0x65:	/* ld ixh, ixl */
	HIGH(ixp) = LOW(ixp);  T_COUNT(8);
	break;
      case 0x6F:	/* ld ixl, a */
	LOW(ixp) = REG_A;  T_COUNT(8);
	break;
      case 0x68:	/* ld ixl, b */
	LOW(ixp) = REG_B;  T_COUNT(8);
	break;
      case 0x69:	/* ld ixl, c */
	LOW(ixp) = REG_C;  T_COUNT(8);
	break;
      case 0x6A:	/* ld ixl, d */
	LOW(ixp) = REG_D;  T_COUNT(8);
	break;
      case 0x6B:	/* ld ixl, e */
	LOW(ixp) = REG_E;  T_COUNT(8);
	break;
      case 0x6C:	/* ld ixl, ixh */
	LOW(ixp) = HIGH(ixp);  T_COUNT(8);
	break;
      case 0x6D:	/* ld ixl, ixl */
	LOW(ixp) = LOW(ixp);  T_COUNT(8);
	break;
      case 0x26:	/* ld ixh, value */
	HIGH(ixp) = mem_read(REG_PC++);  T_COUNT(11);
	break;
      case 0x2E:	/* ld ixl, value */
	LOW(ixp) = mem_read(REG_PC++);  T_COUNT(11);
	break;
      case 0xB4:	/* or ixh */
	do_or_byte(HIGH(ixp));  T_COUNT(8);
	break;
      case 0xB5:	/* or ixl */
	do_or_byte(LOW(ixp));  T_COUNT(8);
	break;
      case 0x9C:	/* sbc a, ixh */
	do_sbc_byte(HIGH(ixp));  T_COUNT(8);
	break;
      case 0x9D:	/* sbc a, ixl */
	do_sbc_byte(LOW(ixp));  T_COUNT(8);
	break;
      case 0x94:	/* sub a, ixh */
	do_sub_byte(HIGH(ixp));  T_COUNT(8);
	break;
      case 0x95:	/* sub a, ixl */
	do_sub_byte(LOW(ixp));  T_COUNT(8);
	break;
      case 0xAC:	/* xor ixh */
	do_xor_byte(HIGH(ixp));  T_COUNT(8);
	break;
      case 0xAD:	/* xor ixl */
	do_xor_byte(LOW(ixp));  T_COUNT(8);
	break;
      /* end undocumented instructions */

      default:
	/* Ignore DD or FD prefix and retry as normal instruction;
	   this is a correct emulation. [undocumented, timing guessed] */
	REG_PC--;
	T_COUNT(4);
	break;
    }
}


/*
 * Extended instructions which have 0xED as the first byte:
 */
static int do_ED_instruction()
{
    Uchar instruction;
    int debug = 0;
    
    instruction = mem_read(REG_PC++);
    
    switch(instruction)
    {
      case 0x4A:	/* adc hl, bc */
	do_adc_word(REG_BC);  T_COUNT(15);
	break;
      case 0x5A:	/* adc hl, de */
	do_adc_word(REG_DE);  T_COUNT(15);
	break;
      case 0x6A:	/* adc hl, hl */
	do_adc_word(REG_HL);  T_COUNT(15);
	break;
      case 0x7A:	/* adc hl, sp */
	do_adc_word(REG_SP);  T_COUNT(15);
	break;

      case 0xA9:	/* cpd */
	do_cpd();
	break;
      case 0xB9:	/* cpdr */
	do_cpdr();
	break;

      case 0xA1:	/* cpi */
	do_cpi();
	break;
      case 0xB1:	/* cpir */
	do_cpir();
	break;

      case 0x46:	/* im 0 */
      case 0x66:	/* im 0 [undocumented]*/
	do_im0();  T_COUNT(8);
	break;
      case 0x56:	/* im 1 */
      case 0x76:	/* im 1 [undocumented] */
	do_im1();  T_COUNT(8);
	break;
      case 0x5E:	/* im 2 */
      case 0x7E:	/* im 2 [undocumented] */
	do_im2();  T_COUNT(8);
	break;

      case 0x78:	/* in a, (c) */
	REG_A = in_with_flags(REG_C);  T_COUNT(11);
	break;
      case 0x40:	/* in b, (c) */
	REG_B = in_with_flags(REG_C);  T_COUNT(11);
	break;
      case 0x48:	/* in c, (c) */
	REG_C = in_with_flags(REG_C);  T_COUNT(11);
	break;
      case 0x50:	/* in d, (c) */
	REG_D = in_with_flags(REG_C);  T_COUNT(11);
	break;
      case 0x58:	/* in e, (c) */
	REG_E = in_with_flags(REG_C);  T_COUNT(11);
	break;
      case 0x60:	/* in h, (c) */
	REG_H = in_with_flags(REG_C);  T_COUNT(11);
	break;
      case 0x68:	/* in l, (c) */
	REG_L = in_with_flags(REG_C);  T_COUNT(11);
	break;
      case 0x70:	/* in (c) [undocumented] */
	(void) in_with_flags(REG_C);  T_COUNT(11);
	break;

      case 0xAA:	/* ind */
	do_ind();
	break;
      case 0xBA:	/* indr */
	do_indr();
	break;
      case 0xA2:	/* ini */
	do_ini();
	break;
      case 0xB2:	/* inir */
	do_inir();
	break;

      case 0x57:	/* ld a, i */
	do_ld_a_i();  T_COUNT(9);
	break;
      case 0x47:	/* ld i, a */
	REG_I = REG_A;  T_COUNT(9);
	break;

      case 0x5F:	/* ld a, r */
	do_ld_a_r();  T_COUNT(9);
	break;
      case 0x4F:	/* ld r, a */
	/* unimplemented; ignore */
	T_COUNT(9);
	break;

      case 0x4B:	/* ld bc, (address) */
	REG_BC = mem_read_word(mem_read_word(REG_PC));
	REG_PC += 2;
	T_COUNT(20);
	break;
      case 0x5B:	/* ld de, (address) */
	REG_DE = mem_read_word(mem_read_word(REG_PC));
	REG_PC += 2;
	T_COUNT(20);
	break;
      case 0x6B:	/* ld hl, (address) */
	/* this instruction is redundant with the 2A instruction */
	REG_HL = mem_read_word(mem_read_word(REG_PC));
	REG_PC += 2;
	T_COUNT(20);
	break;
      case 0x7B:	/* ld sp, (address) */
	REG_SP = mem_read_word(mem_read_word(REG_PC));
	REG_PC += 2;
	T_COUNT(20);
	break;

      case 0x43:	/* ld (address), bc */
	mem_write_word(mem_read_word(REG_PC), REG_BC);
	REG_PC += 2;
	T_COUNT(20);
	break;
      case 0x53:	/* ld (address), de */
	mem_write_word(mem_read_word(REG_PC), REG_DE);
	REG_PC += 2;
	T_COUNT(20);
	break;
      case 0x63:	/* ld (address), hl */
	/* this instruction is redundant with the 22 instruction */
	mem_write_word(mem_read_word(REG_PC), REG_HL);
	REG_PC += 2;
	T_COUNT(20);
	break;
      case 0x73:	/* ld (address), sp */
	mem_write_word(mem_read_word(REG_PC), REG_SP);
	REG_PC += 2;
	T_COUNT(20);
	break;

      case 0xA8:	/* ldd */
	do_ldd();
	break;
      case 0xB8:	/* lddr */
	do_lddr();
	break;
      case 0xA0:	/* ldi */
	do_ldi();
	break;
      case 0xB0:	/* ldir */
	do_ldir();
	break;

      case 0x44:	/* neg */
      case 0x4C:	/* neg [undocumented] */
      case 0x54:	/* neg [undocumented] */
      case 0x5C:	/* neg [undocumented] */
      case 0x64:	/* neg [undocumented] */
      case 0x6C:	/* neg [undocumented] */
      case 0x74:	/* neg [undocumented] */
      case 0x7C:	/* neg [undocumented] */
	do_negate();
	T_COUNT(8);
	break;

      case 0x79:	/* out (c), a */
	z80_out(REG_C, REG_A);
	T_COUNT(12);
	break;
      case 0x41:	/* out (c), b */
	z80_out(REG_C, REG_B);
	T_COUNT(12);
	break;
      case 0x49:	/* out (c), c */
	z80_out(REG_C, REG_C);
	T_COUNT(12);
	break;
      case 0x51:	/* out (c), d */
	z80_out(REG_C, REG_D);
	T_COUNT(12);
	break;
      case 0x59:	/* out (c), e */
	z80_out(REG_C, REG_E);
	T_COUNT(12);
	break;
      case 0x61:	/* out (c), h */
	z80_out(REG_C, REG_H);
	T_COUNT(12);
	break;
      case 0x69:	/* out (c), l */
	z80_out(REG_C, REG_L);
	T_COUNT(12);
	break;
      case 0x71:	/* out (c), 0 [undocumented] */
	z80_out(REG_C, 0);
	T_COUNT(12);
	break;

      case 0xAB:	/* outd */
	do_outd();
	break;
      case 0xBB:	/* outdr */
	do_outdr();
	break;
      case 0xA3:	/* outi */
	do_outi();
	break;
      case 0xB3:	/* outir */
	do_outir();
	break;

      case 0x4D:	/* reti */
	/* no support for alerting peripherals, just like ret */
	REG_PC = mem_read_word(REG_SP);
	REG_SP += 2;
	T_COUNT(14);
	break;

      case 0x45:	/* retn */
	REG_PC = mem_read_word(REG_SP);
	REG_SP += 2;
	z80_state.iff1 = z80_state.iff2;  /* restore the iff state */
	T_COUNT(14);
	break;

      case 0x55:	/* ret [undocumented] */
      case 0x5D:	/* ret [undocumented] */
      case 0x65:	/* ret [undocumented] */
      case 0x6D:	/* ret [undocumented] */
      case 0x75:	/* ret [undocumented] */
      case 0x7D:	/* ret [undocumented] */
	REG_PC = mem_read_word(REG_SP);
	REG_SP += 2;
	T_COUNT(14);
	break;

      case 0x6F:	/* rld */
	do_rld();
	T_COUNT(18);
	break;

      case 0x67:	/* rrd */
	do_rrd();
	T_COUNT(18);
	break;

      case 0x42:	/* sbc hl, bc */
	do_sbc_word(REG_BC);
	T_COUNT(15);
	break;
      case 0x52:	/* sbc hl, de */
	do_sbc_word(REG_DE);
	T_COUNT(15);
	break;
      case 0x62:	/* sbc hl, hl */
	do_sbc_word(REG_HL);
	T_COUNT(15);
	break;
      case 0x72:	/* sbc hl, sp */
	do_sbc_word(REG_SP);
	T_COUNT(15);
	break;

      /* Emulator traps -- not real Z-80 instructions */
      case 0x28:        /* emt_system */
	do_emt_system();
	break;
      case 0x29:        /* emt_mouse */
	do_emt_mouse();
	break;
      case 0x2a:        /* emt_getddir */
	do_emt_getddir();
	break;
      case 0x2b:        /* emt_setddir */
	do_emt_setddir();
	break;
      case 0x2f:        /* emt_debug */
	if (trs_continuous > 0) trs_continuous = 0;
	debug = 1;
	break;
      case 0x30:        /* emt_open */
	do_emt_open();
	break;
      case 0x31:	/* emt_close */
	do_emt_close();
	break;
      case 0x32:	/* emt_read */
	do_emt_read();
	break;
      case 0x33:	/* emt_write */
	do_emt_write();
	break;
      case 0x34:	/* emt_lseek */
	do_emt_lseek();
	break;
      case 0x35:	/* emt_strerror */
	do_emt_strerror();
	break;
      case 0x36:	/* emt_time */
	do_emt_time();
	break;
      case 0x37:        /* emt_opendir */
	do_emt_opendir();
	break;
      case 0x38:	/* emt_closedir */
	do_emt_closedir();
	break;
      case 0x39:	/* emt_readdir */
	do_emt_readdir();
	break;
      case 0x3a:	/* emt_chdir */
	do_emt_chdir();
	break;
      case 0x3b:	/* emt_getcwd */
	do_emt_getcwd();
	break;
      case 0x3c:	/* emt_misc */
	do_emt_misc();
	break;
      case 0x3d:	/* emt_ftruncate */
	do_emt_ftruncate();
	break;
      case 0x3e:        /* emt_opendisk */
	do_emt_opendisk();
	break;
      case 0x3f:	/* emt_closedisk */
	do_emt_closedisk();
	break;

      default:
	disassemble(REG_PC - 2);
	error("unsupported instruction");
    }

    return debug;
}

volatile int x_poll_count = 0;
volatile int x_flush_needed = 0;
#define X_POLL_INTERVAL 10000

/* Hack, hack, see if we can speed this up. */
/*extern Uchar *memory;*/
/*#define MEM_READ(a) ((a < 0x3000) ? memory[a] : mem_read(a));*/
/* #define MEM_READ(a) (((((a) - 0x3000) & 0xffff) >= 0xc00) ? memory[a] : mem_read(a)) */

int trs_continuous;

int z80_run(int continuous)
     /* 1= continuous, 0= singlestep,
	-1= singlestep and disallow interrupts */
{
    Uchar instruction;
    Ushort address; /* generic temps */
    int ret = 0;
    int i;
    trs_continuous = continuous;

    /* loop to do a z80 instruction */
    do {
#if HAVE_SIGIO
        /* If we are using SIGIO, the SIGIO handler tells us when to
	   poll for X events by setting x_poll_count to 0, and when to
	   flush output to the X server by setting x_flush_needed to 1. */
	if (x_poll_count <= 0) {
	    x_poll_count = X_POLL_INTERVAL;
	    trs_get_event(0);
	} else if (x_flush_needed) {
	    x_flush_needed = 0;
	    trs_x_flush();
	}
#else
        /* If we aren't using SIGIO, we need to poll for X events 
	   periodically, and that also flushes output to the X server. */
	if (x_poll_count <= 0) {
	    x_poll_count = X_POLL_INTERVAL;
	    trs_get_event(0);
	} else {
	    x_poll_count--;
	}
#endif
        /* Speed control */
        if ((i = z80_state.delay)) {
	    while (--i) /*nothing*/;
	}

	instruction = mem_read(REG_PC++);
	
	switch(instruction)
	{
	  case 0xCB:	/* CB.. extended instruction */
	    do_CB_instruction();
	    break;
	  case 0xDD:	/* DD.. extended instruction */
	    do_indexed_instruction(&REG_IX);
	    break;
	  case 0xED:	/* ED.. extended instruction */
	    ret = do_ED_instruction();
	    break;
	  case 0xFD:	/* FD.. extended instruction */
	    do_indexed_instruction(&REG_IY);
	    break;
	    
	  case 0x8F:	/* adc a, a */
	    do_adc_byte(REG_A);	 T_COUNT(4);
	    break;
	  case 0x88:	/* adc a, b */
	    do_adc_byte(REG_B);	 T_COUNT(4);
	    break;
	  case 0x89:	/* adc a, c */
	    do_adc_byte(REG_C);	 T_COUNT(4);
	    break;
	  case 0x8A:	/* adc a, d */
	    do_adc_byte(REG_D);	 T_COUNT(4);
	    break;
	  case 0x8B:	/* adc a, e */
	    do_adc_byte(REG_E);	 T_COUNT(4);
	    break;
	  case 0x8C:	/* adc a, h */
	    do_adc_byte(REG_H);	 T_COUNT(4);
	    break;
	  case 0x8D:	/* adc a, l */
	    do_adc_byte(REG_L);	 T_COUNT(4);
	    break;
	  case 0xCE:	/* adc a, value */
	    do_adc_byte(mem_read(REG_PC++));  T_COUNT(7);
	    break;
	  case 0x8E:	/* adc a, (hl) */
	    do_adc_byte(mem_read(REG_HL));  T_COUNT(7);
	    break;
	    
	  case 0x87:	/* add a, a */
	    do_add_byte(REG_A);	 T_COUNT(4);
	    break;
	  case 0x80:	/* add a, b */
	    do_add_byte(REG_B);	 T_COUNT(4);
	    break;
	  case 0x81:	/* add a, c */
	    do_add_byte(REG_C);	 T_COUNT(4);
	    break;
	  case 0x82:	/* add a, d */
	    do_add_byte(REG_D);	 T_COUNT(4);
	    break;
	  case 0x83:	/* add a, e */
	    do_add_byte(REG_E);	 T_COUNT(4);
	    break;
	  case 0x84:	/* add a, h */
	    do_add_byte(REG_H);	 T_COUNT(4);
	    break;
	  case 0x85:	/* add a, l */
	    do_add_byte(REG_L);	 T_COUNT(4);
	    break;
	  case 0xC6:	/* add a, value */
	    do_add_byte(mem_read(REG_PC++));  T_COUNT(7);
	    break;
	  case 0x86:	/* add a, (hl) */
	    do_add_byte(mem_read(REG_HL));  T_COUNT(7);
	    break;
	    
	  case 0x09:	/* add hl, bc */
	    do_add_word(REG_BC);  T_COUNT(11);
	    break;
	  case 0x19:	/* add hl, de */
	    do_add_word(REG_DE);  T_COUNT(11);
	    break;
	  case 0x29:	/* add hl, hl */
	    do_add_word(REG_HL);  T_COUNT(11);
	    break;
	  case 0x39:	/* add hl, sp */
	    do_add_word(REG_SP);  T_COUNT(11);
	    break;
	    
	  case 0xA7:	/* and a */
	    do_and_byte(REG_A);	 T_COUNT(4);
	    break;
	  case 0xA0:	/* and b */
	    do_and_byte(REG_B);	 T_COUNT(4);
	    break;
	  case 0xA1:	/* and c */
	    do_and_byte(REG_C);	 T_COUNT(4);
	    break;
	  case 0xA2:	/* and d */
	    do_and_byte(REG_D);	 T_COUNT(4);
	    break;
	  case 0xA3:	/* and e */
	    do_and_byte(REG_E);	 T_COUNT(4);
	    break;
	  case 0xA4:	/* and h */
	    do_and_byte(REG_H);	 T_COUNT(4);
	    break;
	  case 0xA5:	/* and l */
	    do_and_byte(REG_L);  T_COUNT(4);
	    break;
	  case 0xE6:	/* and value */
	    do_and_byte(mem_read(REG_PC++));  T_COUNT(7);
	    break;
	  case 0xA6:	/* and (hl) */
	    do_and_byte(mem_read(REG_HL));  T_COUNT(7);
	    break;
	    
	  case 0xCD:	/* call address */
	    address = mem_read_word(REG_PC);
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_PC + 2);
	    REG_PC = address;
	    T_COUNT(17);
	    break;
	    
	  case 0xC4:	/* call nz, address */
	    if(!ZERO_FLAG)
	    {
		address = mem_read_word(REG_PC);
		REG_SP -= 2;
		mem_write_word(REG_SP, REG_PC + 2);
		REG_PC = address;
		T_COUNT(17);
	    }
	    else
	    {
		REG_PC += 2;
		T_COUNT(10);
	    }
	    break;
	  case 0xCC:	/* call z, address */
	    if(ZERO_FLAG)
	    {
		address = mem_read_word(REG_PC);
		REG_SP -= 2;
		mem_write_word(REG_SP, REG_PC + 2);
		REG_PC = address;
		T_COUNT(17);
	    }
	    else
	    {
		REG_PC += 2;
		T_COUNT(10);
	    }
	    break;
	  case 0xD4:	/* call nc, address */
	    if(!CARRY_FLAG)
	    {
		address = mem_read_word(REG_PC);
		REG_SP -= 2;
		mem_write_word(REG_SP, REG_PC + 2);
		REG_PC = address;
		T_COUNT(17);
	    }
	    else
	    {
		REG_PC += 2;
		T_COUNT(10);
	    }
	    break;
	  case 0xDC:	/* call c, address */
	    if(CARRY_FLAG)
	    {
		address = mem_read_word(REG_PC);
		REG_SP -= 2;
		mem_write_word(REG_SP, REG_PC + 2);
		REG_PC = address;
		T_COUNT(17);
	    }
	    else
	    {
		REG_PC += 2;
		T_COUNT(10);
	    }
	    break;
	  case 0xE4:	/* call po, address */
	    if(!PARITY_FLAG)
	    {
		address = mem_read_word(REG_PC);
		REG_SP -= 2;
		mem_write_word(REG_SP, REG_PC + 2);
		REG_PC = address;
		T_COUNT(17);
	    }
	    else
	    {
		REG_PC += 2;
		T_COUNT(10);
	    }
	    break;
	  case 0xEC:	/* call pe, address */
	    if(PARITY_FLAG)
	    {
		address = mem_read_word(REG_PC);
		REG_SP -= 2;
		mem_write_word(REG_SP, REG_PC + 2);
		REG_PC = address;
		T_COUNT(17);
	    }
	    else
	    {
		REG_PC += 2;
		T_COUNT(10);
	    }
	    break;
	  case 0xF4:	/* call p, address */
	    if(!SIGN_FLAG)
	    {
		address = mem_read_word(REG_PC);
		REG_SP -= 2;
		mem_write_word(REG_SP, REG_PC + 2);
		REG_PC = address;
		T_COUNT(17);
	    }
	    else
	    {
		REG_PC += 2;
		T_COUNT(10);
	    }
	    break;
	  case 0xFC:	/* call m, address */
	    if(SIGN_FLAG)
	    {
		address = mem_read_word(REG_PC);
		REG_SP -= 2;
		mem_write_word(REG_SP, REG_PC + 2);
		REG_PC = address;
		T_COUNT(17);
	    }
	    else
	    {
		REG_PC += 2;
		T_COUNT(10);
	    }
	    break;
	    
	    
	  case 0x3F:	/* ccf */
	    REG_F = (REG_F & (ZERO_MASK|PARITY_MASK|SIGN_MASK))
	      | (~REG_F & CARRY_MASK)
	      | ((REG_F & CARRY_MASK) ? HALF_CARRY_MASK : 0)
	      | (REG_A & (UNDOC3_MASK|UNDOC5_MASK));
	    T_COUNT(4);
	    break;
	    
	  case 0xBF:	/* cp a */
	    do_cp(REG_A);  T_COUNT(4);
	    break;
	  case 0xB8:	/* cp b */
	    do_cp(REG_B);  T_COUNT(4);
	    break;
	  case 0xB9:	/* cp c */
	    do_cp(REG_C);  T_COUNT(4);
	    break;
	  case 0xBA:	/* cp d */
	    do_cp(REG_D);  T_COUNT(4);
	    break;
	  case 0xBB:	/* cp e */
	    do_cp(REG_E);  T_COUNT(4);
	    break;
	  case 0xBC:	/* cp h */
	    do_cp(REG_H);  T_COUNT(4);
	    break;
	  case 0xBD:	/* cp l */
	    do_cp(REG_L);  T_COUNT(4);
	    break;
	  case 0xFE:	/* cp value */
	    do_cp(mem_read(REG_PC++));  T_COUNT(7);
	    break;
	  case 0xBE:	/* cp (hl) */
	    do_cp(mem_read(REG_HL));  T_COUNT(7);
	    break;
	    
	  case 0x2F:	/* cpl */
	    REG_A = ~REG_A;
	    REG_F = (REG_F & (CARRY_MASK|PARITY_MASK|ZERO_MASK|SIGN_MASK))
	      | (HALF_CARRY_MASK|SUBTRACT_MASK)
	      | (REG_A & (UNDOC3_MASK|UNDOC5_MASK));
	    T_COUNT(4);
	    break;

	  case 0x27:	/* daa */
	    do_daa();
	    T_COUNT(4);
	    break;

	  case 0x3D:	/* dec a */
	    do_flags_dec_byte(--REG_A);  T_COUNT(4);
	    break;
	  case 0x05:	/* dec b */
	    do_flags_dec_byte(--REG_B);  T_COUNT(4);
	    break;
	  case 0x0D:	/* dec c */
	    do_flags_dec_byte(--REG_C);  T_COUNT(4);
	    break;
	  case 0x15:	/* dec d */
	    do_flags_dec_byte(--REG_D);  T_COUNT(4);
	    break;
	  case 0x1D:	/* dec e */
	    do_flags_dec_byte(--REG_E);  T_COUNT(4);
	    break;
	  case 0x25:	/* dec h */
	    do_flags_dec_byte(--REG_H);  T_COUNT(4);
	    break;
	  case 0x2D:	/* dec l */
	    do_flags_dec_byte(--REG_L);  T_COUNT(4);
	    break;
	    
	  case 0x35:	/* dec (hl) */
	    {
	      Uchar value = mem_read(REG_HL) - 1;
	      mem_write(REG_HL, value);
	      do_flags_dec_byte(value);
	    }
	    T_COUNT(11);
	    break;
	    
	  case 0x0B:	/* dec bc */
	    REG_BC--;
	    T_COUNT(6);
	    break;
	  case 0x1B:	/* dec de */
	    REG_DE--;
	    T_COUNT(6);
	    break;
	  case 0x2B:	/* dec hl */
	    REG_HL--;
	    T_COUNT(6);
	    break;
	  case 0x3B:	/* dec sp */
	    REG_SP--;
	    T_COUNT(6);
	    break;
	    
	  case 0xF3:	/* di */
	    do_di();
	    T_COUNT(4);
	    break;
	    
	  case 0x10:	/* djnz offset */
	    /* Zaks says no flag changes. */
	    if(--REG_B != 0)
	    {
		signed char byte_value;
		byte_value = (signed char) mem_read(REG_PC++);
		REG_PC += byte_value;
		T_COUNT(13);
	    }
	    else
	    {
		REG_PC++;
		T_COUNT(8);
	    }
	    break;
	    
	  case 0xFB:	/* ei */
	    do_ei();
	    T_COUNT(4);
	    break;
	    
	  case 0x08:	/* ex af, af' */
	  {
	      Ushort temp;
	      temp = REG_AF;
	      REG_AF = REG_AF_PRIME;
	      REG_AF_PRIME = temp;
	  }
	    T_COUNT(4);
	    break;
	    
	  case 0xEB:	/* ex de, hl */
	  {
	      Ushort temp;
	      temp = REG_DE;
	      REG_DE = REG_HL;
	      REG_HL = temp;
	  }
	    T_COUNT(4);
	    break;
	    
	  case 0xE3:	/* ex (sp), hl */
	  {
	      Ushort temp;
	      temp = mem_read_word(REG_SP);
	      mem_write_word(REG_SP, REG_HL);
	      REG_HL = temp;
	  }
	    T_COUNT(19);
	    break;
	    
	  case 0xD9:	/* exx */
	  {
	      Ushort tmp;
	      tmp = REG_BC_PRIME;
	      REG_BC_PRIME = REG_BC;
	      REG_BC = tmp;
	      tmp = REG_DE_PRIME;
	      REG_DE_PRIME = REG_DE;
	      REG_DE = tmp;
	      tmp = REG_HL_PRIME;
	      REG_HL_PRIME = REG_HL;
	      REG_HL = tmp;
	  }
	    T_COUNT(4);
	    break;
	    
	  case 0x76:	/* halt */
	    if (trs_model == 1) {
		/* Z-80 HALT output is tied to reset button circuit */
		trs_reset(0);
	    } else {
		/* Really halt (i.e., wait for interrupt) */
	        /* Slight kludge: we back up the PC and keep going
		   around the main loop reexecuting the halt.  A real
		   Z-80 does not back up and re-fetch the halt
		   instruction repeatedly; it just executes NOPs
		   internally.  When an interrupt or NMI is delivered,
		   (see below) we undo this decrement to get out of
		   the halt state. */
	        REG_PC--;
		if (continuous > 0 &&
		    !(z80_state.nmi && !z80_state.nmi_seen) &&
		    !(z80_state.irq && z80_state.iff1) &&
		    !trs_event_scheduled()) {
		    trs_paused = 1;
		    pause();
		}
	    }
	    T_COUNT(4);
	    break;

	  case 0xDB:	/* in a, (port) */
	    REG_A = z80_in(mem_read(REG_PC++));
	    T_COUNT(10);
	    break;
	    
	  case 0x3C:	/* inc a */
	    REG_A++;
	    do_flags_inc_byte(REG_A);  T_COUNT(4);
	    break;
	  case 0x04:	/* inc b */
	    REG_B++;
	    do_flags_inc_byte(REG_B);  T_COUNT(4);
	    break;
	  case 0x0C:	/* inc c */
	    REG_C++;
	    do_flags_inc_byte(REG_C);  T_COUNT(4);
	    break;
	  case 0x14:	/* inc d */
	    REG_D++;
	    do_flags_inc_byte(REG_D);  T_COUNT(4);
	    break;
	  case 0x1C:	/* inc e */
	    REG_E++;
	    do_flags_inc_byte(REG_E);  T_COUNT(4);
	    break;
	  case 0x24:	/* inc h */
	    REG_H++;
	    do_flags_inc_byte(REG_H);  T_COUNT(4);
	    break;
	  case 0x2C:	/* inc l */
	    REG_L++;
	    do_flags_inc_byte(REG_L);  T_COUNT(4);
	    break;
	    
	  case 0x34:	/* inc (hl) */
	  {
	      Uchar value = mem_read(REG_HL) + 1;
	      mem_write(REG_HL, value);
	      do_flags_inc_byte(value);
	  }
	    T_COUNT(11);
	    break;
	    
	  case 0x03:	/* inc bc */
	    REG_BC++;
	    T_COUNT(6);
	    break;
	  case 0x13:	/* inc de */
	    REG_DE++;
	    T_COUNT(6);
	    break;
	  case 0x23:	/* inc hl */
	    REG_HL++;
	    T_COUNT(6);
	    break;
	  case 0x33:	/* inc sp */
	    REG_SP++;
	    T_COUNT(6);
	    break;
	    
	  case 0xC3:	/* jp address */
	    REG_PC = mem_read_word(REG_PC);
	    T_COUNT(10);
	    break;
	    
	  case 0xE9:	/* jp (hl) */
	    REG_PC = REG_HL;
	    T_COUNT(4);
	    break;
	    
	  case 0xC2:	/* jp nz, address */
	    if(!ZERO_FLAG)
	    {
		REG_PC = mem_read_word(REG_PC);
	    }
	    else
	    {
		REG_PC += 2;
	    }
	    T_COUNT(10);
	    break;
	  case 0xCA:	/* jp z, address */
	    if(ZERO_FLAG)
	    {
		REG_PC = mem_read_word(REG_PC);
	    }
	    else
	    {
		REG_PC += 2;
	    }
	    T_COUNT(10);
	    break;
	  case 0xD2:	/* jp nc, address */
	    if(!CARRY_FLAG)
	    {
		REG_PC = mem_read_word(REG_PC);
	    }
	    else
	    {
		REG_PC += 2;
	    }
	    T_COUNT(10);
	    break;
	  case 0xDA:	/* jp c, address */
	    if(CARRY_FLAG)
	    {
		REG_PC = mem_read_word(REG_PC);
	    }
	    else
	    {
		REG_PC += 2;
	    }
	    T_COUNT(10);
	    break;
	  case 0xE2:	/* jp po, address */
	    if(!PARITY_FLAG)
	    {
		REG_PC = mem_read_word(REG_PC);
	    }
	    else
	    {
		REG_PC += 2;
	    }
	    T_COUNT(10);
	    break;
	  case 0xEA:	/* jp pe, address */
	    if(PARITY_FLAG)
	    {
		REG_PC = mem_read_word(REG_PC);
	    }
	    else
	    {
		REG_PC += 2;
	    }
	    T_COUNT(10);
	    break;
	  case 0xF2:	/* jp p, address */
	    if(!SIGN_FLAG)
	    {
		REG_PC = mem_read_word(REG_PC);
	    }
	    else
	    {
		REG_PC += 2;
	    }
	    T_COUNT(10);
	    break;
	  case 0xFA:	/* jp m, address */
	    if(SIGN_FLAG)
	    {
		REG_PC = mem_read_word(REG_PC);
	    }
	    else
	    {
		REG_PC += 2;
	    }
	    T_COUNT(10);
	    break;
	    
	  case 0x18:	/* jr offset */
	  {
	      signed char byte_value;
	      byte_value = (signed char) mem_read(REG_PC++);
	      REG_PC += byte_value;
	  }
	    T_COUNT(12);
	    break;
	    
	  case 0x20:	/* jr nz, offset */
	    if(!ZERO_FLAG)
	    {
		signed char byte_value;
		byte_value = (signed char) mem_read(REG_PC++);
		REG_PC += byte_value;
		T_COUNT(12);
	    }
	    else
	    {
		REG_PC++;
		T_COUNT(7);
	    }
	    break;
	  case 0x28:	/* jr z, offset */
	    if(ZERO_FLAG)
	    {
		signed char byte_value;
		byte_value = (signed char) mem_read(REG_PC++);
		REG_PC += byte_value;
		T_COUNT(12);
	    }
	    else
	    {
		REG_PC++;
		T_COUNT(7);
	    }
	    break;
	  case 0x30:	/* jr nc, offset */
	    if(!CARRY_FLAG)
	    {
		signed char byte_value;
		byte_value = (signed char) mem_read(REG_PC++);
		REG_PC += byte_value;
		T_COUNT(12);
	    }
	    else
	    {
		REG_PC++;
		T_COUNT(7);
	    }
	    break;
	  case 0x38:	/* jr c, offset */
	    if(CARRY_FLAG)
	    {
		signed char byte_value;
		byte_value = (signed char) mem_read(REG_PC++);
		REG_PC += byte_value;
		T_COUNT(12);
	    }
	    else
	    {
		REG_PC++;
		T_COUNT(7);
	    }
	    break;
	    
	  case 0x7F:	/* ld a, a */
	    REG_A = REG_A;  T_COUNT(4);
	    break;
	  case 0x78:	/* ld a, b */
	    REG_A = REG_B;  T_COUNT(4);
	    break;
	  case 0x79:	/* ld a, c */
	    REG_A = REG_C;  T_COUNT(4);
	    break;
	  case 0x7A:	/* ld a, d */
	    REG_A = REG_D;  T_COUNT(4);
	    break;
	  case 0x7B:	/* ld a, e */
	    REG_A = REG_E;  T_COUNT(4);
	    break;
	  case 0x7C:	/* ld a, h */
	    REG_A = REG_H;  T_COUNT(4);
	    break;
	  case 0x7D:	/* ld a, l */
	    REG_A = REG_L;  T_COUNT(4);
	    break;
	  case 0x47:	/* ld b, a */
	    REG_B = REG_A;  T_COUNT(4);
	    break;
	  case 0x40:	/* ld b, b */
	    REG_B = REG_B;  T_COUNT(4);
	    break;
	  case 0x41:	/* ld b, c */
	    REG_B = REG_C;  T_COUNT(4);
	    break;
	  case 0x42:	/* ld b, d */
	    REG_B = REG_D;  T_COUNT(4);
	    break;
	  case 0x43:	/* ld b, e */
	    REG_B = REG_E;  T_COUNT(4);
	    break;
	  case 0x44:	/* ld b, h */
	    REG_B = REG_H;  T_COUNT(4);
	    break;
	  case 0x45:	/* ld b, l */
	    REG_B = REG_L;  T_COUNT(4);
	    break;
	  case 0x4F:	/* ld c, a */
	    REG_C = REG_A;  T_COUNT(4);
	    break;
	  case 0x48:	/* ld c, b */
	    REG_C = REG_B;  T_COUNT(4);
	    break;
	  case 0x49:	/* ld c, c */
	    REG_C = REG_C;  T_COUNT(4);
	    break;
	  case 0x4A:	/* ld c, d */
	    REG_C = REG_D;  T_COUNT(4);
	    break;
	  case 0x4B:	/* ld c, e */
	    REG_C = REG_E;  T_COUNT(4);
	    break;
	  case 0x4C:	/* ld c, h */
	    REG_C = REG_H;  T_COUNT(4);
	    break;
	  case 0x4D:	/* ld c, l */
	    REG_C = REG_L;  T_COUNT(4);
	    break;
	  case 0x57:	/* ld d, a */
	    REG_D = REG_A;  T_COUNT(4);
	    break;
	  case 0x50:	/* ld d, b */
	    REG_D = REG_B;  T_COUNT(4);
	    break;
	  case 0x51:	/* ld d, c */
	    REG_D = REG_C;  T_COUNT(4);
	    break;
	  case 0x52:	/* ld d, d */
	    REG_D = REG_D;  T_COUNT(4);
	    break;
	  case 0x53:	/* ld d, e */
	    REG_D = REG_E;  T_COUNT(4);
	    break;
	  case 0x54:	/* ld d, h */
	    REG_D = REG_H;  T_COUNT(4);
	    break;
	  case 0x55:	/* ld d, l */
	    REG_D = REG_L;  T_COUNT(4);
	    break;
	  case 0x5F:	/* ld e, a */
	    REG_E = REG_A;  T_COUNT(4);
	    break;
	  case 0x58:	/* ld e, b */
	    REG_E = REG_B;  T_COUNT(4);
	    break;
	  case 0x59:	/* ld e, c */
	    REG_E = REG_C;  T_COUNT(4);
	    break;
	  case 0x5A:	/* ld e, d */
	    REG_E = REG_D;  T_COUNT(4);
	    break;
	  case 0x5B:	/* ld e, e */
	    REG_E = REG_E;  T_COUNT(4);
	    break;
	  case 0x5C:	/* ld e, h */
	    REG_E = REG_H;  T_COUNT(4);
	    break;
	  case 0x5D:	/* ld e, l */
	    REG_E = REG_L;  T_COUNT(4);
	    break;
	  case 0x67:	/* ld h, a */
	    REG_H = REG_A;  T_COUNT(4);
	    break;
	  case 0x60:	/* ld h, b */
	    REG_H = REG_B;  T_COUNT(4);
	    break;
	  case 0x61:	/* ld h, c */
	    REG_H = REG_C;  T_COUNT(4);
	    break;
	  case 0x62:	/* ld h, d */
	    REG_H = REG_D;  T_COUNT(4);
	    break;
	  case 0x63:	/* ld h, e */
	    REG_H = REG_E;  T_COUNT(4);
	    break;
	  case 0x64:	/* ld h, h */
	    REG_H = REG_H;  T_COUNT(4);
	    break;
	  case 0x65:	/* ld h, l */
	    REG_H = REG_L;  T_COUNT(4);
	    break;
	  case 0x6F:	/* ld l, a */
	    REG_L = REG_A;  T_COUNT(4);
	    break;
	  case 0x68:	/* ld l, b */
	    REG_L = REG_B;  T_COUNT(4);
	    break;
	  case 0x69:	/* ld l, c */
	    REG_L = REG_C;  T_COUNT(4);
	    break;
	  case 0x6A:	/* ld l, d */
	    REG_L = REG_D;  T_COUNT(4);
	    break;
	  case 0x6B:	/* ld l, e */
	    REG_L = REG_E;  T_COUNT(4);
	    break;
	  case 0x6C:	/* ld l, h */
	    REG_L = REG_H;  T_COUNT(4);
	    break;
	  case 0x6D:	/* ld l, l */
	    REG_L = REG_L;  T_COUNT(4);
	    break;
	    
	  case 0x02:	/* ld (bc), a */
	    mem_write(REG_BC, REG_A);  T_COUNT(7);
	    break;
	  case 0x12:	/* ld (de), a */
	    mem_write(REG_DE, REG_A);  T_COUNT(7);
	    break;
	  case 0x77:	/* ld (hl), a */
	    mem_write(REG_HL, REG_A);  T_COUNT(7);
	    break;
	  case 0x70:	/* ld (hl), b */
	    mem_write(REG_HL, REG_B);  T_COUNT(7);
	    break;
	  case 0x71:	/* ld (hl), c */
	    mem_write(REG_HL, REG_C);  T_COUNT(7);
	    break;
	  case 0x72:	/* ld (hl), d */
	    mem_write(REG_HL, REG_D);  T_COUNT(7);
	    break;
	  case 0x73:	/* ld (hl), e */
	    mem_write(REG_HL, REG_E);  T_COUNT(7);
	    break;
	  case 0x74:	/* ld (hl), h */
	    mem_write(REG_HL, REG_H);  T_COUNT(7);
	    break;
	  case 0x75:	/* ld (hl), l */
	    mem_write(REG_HL, REG_L);  T_COUNT(7);
	    break;
	    
	  case 0x7E:	/* ld a, (hl) */
	    REG_A = mem_read(REG_HL);  T_COUNT(7);
	    break;
	  case 0x46:	/* ld b, (hl) */
	    REG_B = mem_read(REG_HL);  T_COUNT(7);
	    break;
	  case 0x4E:	/* ld c, (hl) */
	    REG_C = mem_read(REG_HL);  T_COUNT(7);
	    break;
	  case 0x56:	/* ld d, (hl) */
	    REG_D = mem_read(REG_HL);  T_COUNT(7);
	    break;
	  case 0x5E:	/* ld e, (hl) */
	    REG_E = mem_read(REG_HL);  T_COUNT(7);
	    break;
	  case 0x66:	/* ld h, (hl) */
	    REG_H = mem_read(REG_HL);  T_COUNT(7);
	    break;
	  case 0x6E:	/* ld l, (hl) */
	    REG_L = mem_read(REG_HL);  T_COUNT(7);
	    break;
	    
	  case 0x3E:	/* ld a, value */
	    REG_A = mem_read(REG_PC++);  T_COUNT(7);
	    break;
	  case 0x06:	/* ld b, value */
	    REG_B = mem_read(REG_PC++);  T_COUNT(7);
	    break;
	  case 0x0E:	/* ld c, value */
	    REG_C = mem_read(REG_PC++);  T_COUNT(7);
	    break;
	  case 0x16:	/* ld d, value */
	    REG_D = mem_read(REG_PC++);  T_COUNT(7);
	    break;
	  case 0x1E:	/* ld e, value */
	    REG_E = mem_read(REG_PC++);  T_COUNT(7);
	    break;
	  case 0x26:	/* ld h, value */
	    REG_H = mem_read(REG_PC++);  T_COUNT(7);
	    break;
	  case 0x2E:	/* ld l, value */
	    REG_L = mem_read(REG_PC++);  T_COUNT(7);
	    break;
	    
	  case 0x01:	/* ld bc, value */
	    REG_BC = mem_read_word(REG_PC);
	    REG_PC += 2;
	    T_COUNT(10);
	    break;
	  case 0x11:	/* ld de, value */
	    REG_DE = mem_read_word(REG_PC);
	    REG_PC += 2;
	    T_COUNT(10);
	    break;
	  case 0x21:	/* ld hl, value */
	    REG_HL = mem_read_word(REG_PC);
	    REG_PC += 2;
	    T_COUNT(10);
	    break;
	  case 0x31:	/* ld sp, value */
	    REG_SP = mem_read_word(REG_PC);
	    REG_PC += 2;
	    T_COUNT(10);
	    break;
	    
	    
	  case 0x3A:	/* ld a, (address) */
	    /* this one is missing from Zaks */
	    REG_A = mem_read(mem_read_word(REG_PC));
	    REG_PC += 2;
	    T_COUNT(13);
	    break;
	    
	  case 0x0A:	/* ld a, (bc) */
	    REG_A = mem_read(REG_BC);
	    T_COUNT(7);
	    break;
	  case 0x1A:	/* ld a, (de) */
	    REG_A = mem_read(REG_DE);
	    T_COUNT(7);
	    break;
	    
	  case 0x32:	/* ld (address), a */
	    mem_write(mem_read_word(REG_PC), REG_A);
	    REG_PC += 2;
	    T_COUNT(13);
	    break;
	    
	  case 0x22:	/* ld (address), hl */
	    mem_write_word(mem_read_word(REG_PC), REG_HL);
	    REG_PC += 2;
	    T_COUNT(16);
	    break;
	    
	  case 0x36:	/* ld (hl), value */
	    mem_write(REG_HL, mem_read(REG_PC++));
	    T_COUNT(10);
	    break;
	    
	  case 0x2A:	/* ld hl, (address) */
	    REG_HL = mem_read_word(mem_read_word(REG_PC));
	    REG_PC += 2;
	    T_COUNT(16);
	    break;
	    
	  case 0xF9:	/* ld sp, hl */
	    REG_SP = REG_HL;
	    T_COUNT(6);
	    break;
	    
	  case 0x00:	/* nop */
	    T_COUNT(4);
	    break;
	    
	  case 0xF6:	/* or value */
	    do_or_byte(mem_read(REG_PC++));
	    T_COUNT(7);
	    break;
	    
	  case 0xB7:	/* or a */
	    do_or_byte(REG_A);  T_COUNT(4);
	    break;
	  case 0xB0:	/* or b */
	    do_or_byte(REG_B);  T_COUNT(4);
	    break;
	  case 0xB1:	/* or c */
	    do_or_byte(REG_C);  T_COUNT(4);
	    break;
	  case 0xB2:	/* or d */
	    do_or_byte(REG_D);  T_COUNT(4);
	    break;
	  case 0xB3:	/* or e */
	    do_or_byte(REG_E);  T_COUNT(4);
	    break;
	  case 0xB4:	/* or h */
	    do_or_byte(REG_H);  T_COUNT(4);
	    break;
	  case 0xB5:	/* or l */
	    do_or_byte(REG_L);  T_COUNT(4);
	    break;
	    
	  case 0xB6:	/* or (hl) */
	    do_or_byte(mem_read(REG_HL));  T_COUNT(7);
	    break;
	    
	  case 0xD3:	/* out (port), a */
	    z80_out(mem_read(REG_PC++), REG_A);
	    T_COUNT(11);
	    break;
	    
	  case 0xC1:	/* pop bc */
	    REG_BC = mem_read_word(REG_SP);
	    REG_SP += 2;
	    T_COUNT(10);
	    break;
	  case 0xD1:	/* pop de */
	    REG_DE = mem_read_word(REG_SP);
	    REG_SP += 2;
	    T_COUNT(10);
	    break;
	  case 0xE1:	/* pop hl */
	    REG_HL = mem_read_word(REG_SP);
	    REG_SP += 2;
	    T_COUNT(10);
	    break;
	  case 0xF1:	/* pop af */
	    REG_AF = mem_read_word(REG_SP);
	    REG_SP += 2;
	    T_COUNT(10);
	    break;
	    
	  case 0xC5:	/* push bc */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_BC);
	    T_COUNT(11);
	    break;
	  case 0xD5:	/* push de */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_DE);
	    T_COUNT(11);
	    break;
	  case 0xE5:	/* push hl */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_HL);
	    T_COUNT(11);
	    break;
	  case 0xF5:	/* push af */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_AF);
	    T_COUNT(11);
	    break;
	    
	  case 0xC9:	/* ret */
	    REG_PC = mem_read_word(REG_SP);
	    REG_SP += 2;
	    T_COUNT(10);
	    break;
	    
	  case 0xC0:	/* ret nz */
	    if(!ZERO_FLAG)
	    {
		REG_PC = mem_read_word(REG_SP);
		REG_SP += 2;
		T_COUNT(11);
            } else {
	        T_COUNT(5);
	    }
	    break;
	  case 0xC8:	/* ret z */
	    if(ZERO_FLAG)
	    {
		REG_PC = mem_read_word(REG_SP);
		REG_SP += 2;
		T_COUNT(11);
            } else {
	        T_COUNT(5);
	    }
	    break;
	  case 0xD0:	/* ret nc */
	    if(!CARRY_FLAG)
	    {
		REG_PC = mem_read_word(REG_SP);
		REG_SP += 2;
		T_COUNT(11);
            } else {
	        T_COUNT(5);
	    }
	    break;
	  case 0xD8:	/* ret c */
	    if(CARRY_FLAG)
	    {
		REG_PC = mem_read_word(REG_SP);
		REG_SP += 2;
		T_COUNT(11);
            } else {
	        T_COUNT(5);
	    }
	    break;
	  case 0xE0:	/* ret po */
	    if(!PARITY_FLAG)
	    {
		REG_PC = mem_read_word(REG_SP);
		REG_SP += 2;
		T_COUNT(11);
            } else {
	        T_COUNT(5);
	    }
	    break;
	  case 0xE8:	/* ret pe */
	    if(PARITY_FLAG)
	    {
		REG_PC = mem_read_word(REG_SP);
		REG_SP += 2;
		T_COUNT(11);
            } else {
	        T_COUNT(5);
	    }
	    break;
	  case 0xF0:	/* ret p */
	    if(!SIGN_FLAG)
	    {
		REG_PC = mem_read_word(REG_SP);
		REG_SP += 2;
		T_COUNT(11);
            } else {
	        T_COUNT(5);
	    }
	    break;
	  case 0xF8:	/* ret m */
	    if(SIGN_FLAG)
	    {
		REG_PC = mem_read_word(REG_SP);
		REG_SP += 2;
		T_COUNT(11);
            } else {
	        T_COUNT(5);
	    }
	    break;
	    
	  case 0x17:	/* rla */
	    do_rla();
	    T_COUNT(4);
	    break;
	    
	  case 0x07:	/* rlca */
	    do_rlca();
	    T_COUNT(4);
	    break;
	    
	  case 0x1F:	/* rra */
	    do_rra();
	    T_COUNT(4);
	    break;
	    
	  case 0x0F:	/* rrca */
	    do_rrca();
	    T_COUNT(4);
	    break;
	    
	  case 0xC7:	/* rst 00h */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_PC);
	    REG_PC = 0x00;
	    T_COUNT(11);
	    break;
	  case 0xCF:	/* rst 08h */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_PC);
	    REG_PC = 0x08;
	    T_COUNT(11);
	    break;
	  case 0xD7:	/* rst 10h */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_PC);
	    REG_PC = 0x10;
	    T_COUNT(11);
	    break;
	  case 0xDF:	/* rst 18h */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_PC);
	    REG_PC = 0x18;
	    T_COUNT(11);
	    break;
	  case 0xE7:	/* rst 20h */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_PC);
	    REG_PC = 0x20;
	    T_COUNT(11);
	    break;
	  case 0xEF:	/* rst 28h */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_PC);
	    REG_PC = 0x28;
	    T_COUNT(11);
	    break;
	  case 0xF7:	/* rst 30h */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_PC);
	    REG_PC = 0x30;
	    T_COUNT(11);
	    break;
	  case 0xFF:	/* rst 38h */
	    REG_SP -= 2;
	    mem_write_word(REG_SP, REG_PC);
	    REG_PC = 0x38;
	    T_COUNT(11);
	    break;
	    
	  case 0x37:	/* scf */
	    REG_F = (REG_F & (ZERO_FLAG|PARITY_FLAG|SIGN_FLAG))
	      | CARRY_MASK
	      | (REG_A & (UNDOC3_MASK|UNDOC5_MASK));
	    T_COUNT(4);
	    break;
	    
	  case 0x9F:	/* sbc a, a */
	    do_sbc_byte(REG_A);  T_COUNT(4);
	    break;
	  case 0x98:	/* sbc a, b */
	    do_sbc_byte(REG_B);  T_COUNT(4);
	    break;
	  case 0x99:	/* sbc a, c */
	    do_sbc_byte(REG_C);  T_COUNT(4);
	    break;
	  case 0x9A:	/* sbc a, d */
	    do_sbc_byte(REG_D);  T_COUNT(4);
	    break;
	  case 0x9B:	/* sbc a, e */
	    do_sbc_byte(REG_E);  T_COUNT(4);
	    break;
	  case 0x9C:	/* sbc a, h */
	    do_sbc_byte(REG_H);  T_COUNT(4);
	    break;
	  case 0x9D:	/* sbc a, l */
	    do_sbc_byte(REG_L);  T_COUNT(4);
	    break;
	  case 0xDE:	/* sbc a, value */
	    do_sbc_byte(mem_read(REG_PC++));  T_COUNT(7);
	    break;
	  case 0x9E:	/* sbc a, (hl) */
	    do_sbc_byte(mem_read(REG_HL));  T_COUNT(7);
	    break;
	    
	  case 0x97:	/* sub a, a */
	    do_sub_byte(REG_A);  T_COUNT(4);
	    break;
	  case 0x90:	/* sub a, b */
	    do_sub_byte(REG_B);  T_COUNT(4);
	    break;
	  case 0x91:	/* sub a, c */
	    do_sub_byte(REG_C);  T_COUNT(4);
	    break;
	  case 0x92:	/* sub a, d */
	    do_sub_byte(REG_D);  T_COUNT(4);
	    break;
	  case 0x93:	/* sub a, e */
	    do_sub_byte(REG_E);  T_COUNT(4);
	    break;
	  case 0x94:	/* sub a, h */
	    do_sub_byte(REG_H);  T_COUNT(4);
	    break;
	  case 0x95:	/* sub a, l */
	    do_sub_byte(REG_L);  T_COUNT(4);
	    break;
	  case 0xD6:	/* sub a, value */
	    do_sub_byte(mem_read(REG_PC++));  T_COUNT(7);
	    break;
	  case 0x96:	/* sub a, (hl) */
	    do_sub_byte(mem_read(REG_HL));  T_COUNT(7);
	    break;
	    
	  case 0xEE:	/* xor value */
	    do_xor_byte(mem_read(REG_PC++));  T_COUNT(7);
	    break;
	    
	  case 0xAF:	/* xor a */
	    do_xor_byte(REG_A);  T_COUNT(4);
	    break;
	  case 0xA8:	/* xor b */
	    do_xor_byte(REG_B);  T_COUNT(4);
	    break;
	  case 0xA9:	/* xor c */
	    do_xor_byte(REG_C);  T_COUNT(4);
	    break;
	  case 0xAA:	/* xor d */
	    do_xor_byte(REG_D);  T_COUNT(4);
	    break;
	  case 0xAB:	/* xor e */
	    do_xor_byte(REG_E);  T_COUNT(4);
	    break;
	  case 0xAC:	/* xor h */
	    do_xor_byte(REG_H);  T_COUNT(4);
	    break;
	  case 0xAD:	/* xor l */
	    do_xor_byte(REG_L);  T_COUNT(4);
	    break;
	  case 0xAE:	/* xor (hl) */
	    do_xor_byte(mem_read(REG_HL));  T_COUNT(7);
	    break;
	    
	  default:
	    disassemble(REG_PC - 1);
	    error("unsupported instruction");
	}

	/* Event scheduler */
	if (z80_state.sched &&
	    (z80_state.sched - z80_state.t_count > TSTATE_T_MID)) {
	  /* Subtraction wrapped; time for event to happen */
	  trs_do_event();	    
	}

	/* Check for an interrupt */
	if (trs_continuous >= 0)
        {
	    /* Handle NMI first */
	    if (z80_state.nmi && !z80_state.nmi_seen)
	    {
	        if (instruction == 0x76) {
		    /* Taking a NMI gets us out of a halt */
		    REG_PC++;
		}
	        do_nmi();
	        z80_state.nmi_seen = TRUE;
                if (trs_model == 1) {
		  /* Simulate releasing the pushbutton here; ugh. */
		  trs_reset_button_interrupt(0);
		}
	    }
	    /* Allow IRQ if enabled and instruction was not EI */
	    else if (z80_state.irq && z80_state.iff1 == 1
		     && instruction != 0xFB)
            {
	        if (instruction == 0x76) {
		    /* Taking an interrupt gets us out of a halt */
		    REG_PC++;
		}
	        do_int();
	    }
	}
    } while (trs_continuous > 0);
    return ret;
}


void z80_reset()
{
    REG_PC = 0;
    z80_state.i = 0;
    z80_state.iff1 = 0;
    z80_state.iff2 = 0;
    z80_state.interrupt_mode = 0;
    z80_state.irq = z80_state.nmi = FALSE;
    z80_state.sched = 0;

    /* z80_state.r = 0; */
    srand(time(NULL));  /* Seed the RNG, for reading the refresh register */
}

