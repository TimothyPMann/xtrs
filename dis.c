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
 * dis.c -- a hacked version of "zdis" which can print out instructions
 * as they are executed.
 */

#include "z80.h"

static char undefined[] = "undefined";

struct opcode {
	char	*name;
	int	args;
};

static struct opcode major[256] = {
	"nop",			0,		/* 00 */
	"ld	bc,%02x%02xh",	2,		/* 01 */
	"ld	(bc),a",	0,		/* 02 */
	"inc	bc",		0,		/* 03 */
	"inc	b",		0,		/* 04 */
	"dec	b",		0,		/* 05 */
	"ld	b,%02xh",	1,		/* 06 */
	"rlc	a",		0,		/* 07 */

	"ex	af,af'",	0,		/* 08 */
	"add	hl,bc",		0,		/* 09 */
	"ld	a,(bc)",	0,		/* 0a */
	"dec	bc",		0,		/* 0b */
	"inc	c",		0,		/* 0c */
	"dec	c",		0,		/* 0d */
	"ld	c,%02xh",	1,		/* 0e */
	"rrc	a",		0,		/* 0f */

	"djnz	%04xh",		-1,		/* 10 */
	"ld	de,%02x%02xh",	2,		/* 11 */
	"ld	(de),a",	0,		/* 12 */
	"inc	de",		0,		/* 13 */
	"inc	d",		0,		/* 14 */
	"dec	d",		0,		/* 15 */
	"ld	d,%02xh",	1,		/* 16 */
	"rla",			0,		/* 17 */

	"jr	%04xh",		-1,		/* 18 */
	"add	hl,de",		0,		/* 19 */
	"ld	a,(de)",	0,		/* 1a */
	"dec	de",		0,		/* 1b */
	"inc	e",		0,		/* 1c */
	"dec	e",		0,		/* 1d */
	"ld	e,%02xh",	1,		/* 1e */
	"rra",			0,		/* 1f */

	"jr	nz,%04xh",	-1,		/* 20 */
	"ld	hl,%02x%02xh",	2,		/* 21 */
	"ld	(%02x%02xh),hl",2,		/* 22 */
	"inc	hl",		0,		/* 23 */
	"inc	h",		0,		/* 24 */
	"dec	h",		0,		/* 25 */
	"ld	h,%02xh",	1,		/* 26 */
	"daa",			0,		/* 27 */

	"jr	z,%04xh",	-1,		/* 28 */
	"add	hl,hl",		0,		/* 29 */
	"ld	hl,(%02x%02xh)",2,		/* 2a */
	"dec	hl",		0,		/* 2b */
	"inc	l",		0,		/* 2c */
	"dec	l",		0,		/* 2d */
	"ld	l,%02xh",	1,		/* 2e */
	"cpl",			0,		/* 2f */

	"jr	nc,%04xh",	-1,		/* 30 */
	"ld	sp,%02x%02xh",	2,		/* 31 */
	"ld	(%02x%02xh),a",	2,		/* 32 */
	"inc	sp",		0,		/* 33 */
	"inc	(hl)",		0,		/* 34 */
	"dec	(hl)",		0,		/* 35 */
	"ld	(hl),%02xh",	1,		/* 36 */
	"scf",			0,		/* 37 */

	"jr	c,%04xh",	-1,		/* 38 */
	"add	hl,sp",		0,		/* 39 */
	"ld	a,(%02x%02xh)",	2,		/* 3a */
	"dec	sp",		0,		/* 3b */
	"inc	a",		0,		/* 3c */
	"dec	a",		0,		/* 3d */
	"ld	a,%02xh",	1,		/* 3e */
	"ccf",			0,		/* 3f */

	"ld	b,b",		0,		/* 40 */
	"ld	b,c",		0,		/* 41 */
	"ld	b,d",		0,		/* 42 */
	"ld	b,e",		0,		/* 43 */
	"ld	b,h",		0,		/* 44 */
	"ld	b,l",		0,		/* 45 */
	"ld	b,(hl)",	0,		/* 46 */
	"ld	b,a",		0,		/* 47 */

	"ld	c,b",		0,		/* 48 */
	"ld	c,c",		0,		/* 49 */
	"ld	c,d",		0,		/* 4a */
	"ld	c,e",		0,		/* 4b */
	"ld	c,h",		0,		/* 4c */
	"ld	c,l",		0,		/* 4d */
	"ld	c,(hl)",	0,		/* 4e */
	"ld	c,a",		0,		/* 4f */

	"ld	d,b",		0,		/* 50 */
	"ld	d,c",		0,		/* 51 */
	"ld	d,d",		0,		/* 52 */
	"ld	d,e",		0,		/* 53 */
	"ld	d,h",		0,		/* 54 */
	"ld	d,l",		0,		/* 55 */
	"ld	d,(hl)",	0,		/* 56 */
	"ld	d,a",		0,		/* 57 */

	"ld	e,b",		0,		/* 58 */
	"ld	e,c",		0,		/* 59 */
	"ld	e,d",		0,		/* 5a */
	"ld	e,e",		0,		/* 5b */
	"ld	e,h",		0,		/* 5c */
	"ld	e,l",		0,		/* 5d */
	"ld	e,(hl)",	0,		/* 5e */
	"ld	e,a",		0,		/* 5f */

	"ld	h,b",		0,		/* 60 */
	"ld	h,c",		0,		/* 61 */
	"ld	h,d",		0,		/* 62 */
	"ld	h,e",		0,		/* 63 */
	"ld	h,h",		0,		/* 64 */
	"ld	h,l",		0,		/* 65 */
	"ld	h,(hl)",	0,		/* 66 */
	"ld	h,a",		0,		/* 67 */

	"ld	l,b",		0,		/* 68 */
	"ld	l,c",		0,		/* 69 */
	"ld	l,d",		0,		/* 6a */
	"ld	l,e",		0,		/* 6b */
	"ld	l,h",		0,		/* 6c */
	"ld	l,l",		0,		/* 6d */
	"ld	l,(hl)",	0,		/* 6e */
	"ld	l,a",		0,		/* 6f */

	"ld	(hl),b",	0,		/* 70 */
	"ld	(hl),c",	0,		/* 71 */
	"ld	(hl),d",	0,		/* 72 */
	"ld	(hl),e",	0,		/* 73 */
	"ld	(hl),h",	0,		/* 74 */
	"ld	(hl),l",	0,		/* 75 */
	"halt",			0,		/* 76 */
	"ld	(hl),a",	0,		/* 77 */

	"ld	a,b",		0,		/* 78 */
	"ld	a,c",		0,		/* 79 */
	"ld	a,d",		0,		/* 7a */
	"ld	a,e",		0,		/* 7b */
	"ld	a,h",		0,		/* 7c */
	"ld	a,l",		0,		/* 7d */
	"ld	a,(hl)",	0,		/* 7e */
	"ld	a,a",		0,		/* 7f */

	"add	a,b",		0,		/* 80 */
	"add	a,c",		0,		/* 81 */
	"add	a,d",		0,		/* 82 */
	"add	a,e",		0,		/* 83 */
	"add	a,h",		0,		/* 84 */
	"add	a,l",		0,		/* 85 */
	"add	a,(hl)",	0,		/* 86 */
	"add	a,a",		0,		/* 87 */

	"adc	a,b",		0,		/* 88 */
	"adc	a,c",		0,		/* 89 */
	"adc	a,d",		0,		/* 8a */
	"adc	a,e",		0,		/* 8b */
	"adc	a,h",		0,		/* 8c */
	"adc	a,l",		0,		/* 8d */
	"adc	a,(hl)",	0,		/* 8e */
	"adc	a,a",		0,		/* 8f */

	"sub	b",		0,		/* 90 */
	"sub	c",		0,		/* 91 */
	"sub	d",		0,		/* 92 */
	"sub	e",		0,		/* 93 */
	"sub	h",		0,		/* 94 */
	"sub	l",		0,		/* 95 */
	"sub	(hl)",		0,		/* 96 */
	"sub	a",		0,		/* 97 */

	"sbc	a,b",		0,		/* 98 */
	"sbc	a,c",		0,		/* 99 */
	"sbc	a,d",		0,		/* 9a */
	"sbc	a,e",		0,		/* 9b */
	"sbc	a,h",		0,		/* 9c */
	"sbc	a,l",		0,		/* 9d */
	"sbc	a,(hl)",	0,		/* 9e */
	"sbc	a,a",		0,		/* 9f */

	"and	b",		0,		/* a0 */
	"and	c",		0,		/* a1 */
	"and	d",		0,		/* a2 */
	"and	e",		0,		/* a3 */
	"and	h",		0,		/* a4 */
	"and	l",		0,		/* a5 */
	"and	(hl)",		0,		/* a6 */
	"and	a",		0,		/* a7 */

	"xor	b",		0,		/* a8 */
	"xor	c",		0,		/* a9 */
	"xor	d",		0,		/* aa */
	"xor	e",		0,		/* ab */
	"xor	h",		0,		/* ac */
	"xor	l",		0,		/* ad */
	"xor	(hl)",		0,		/* ae */
	"xor	a",		0,		/* af */

	"or	b",		0,		/* b0 */
	"or	c",		0,		/* b1 */
	"or	d",		0,		/* b2 */
	"or	e",		0,		/* b3 */
	"or	h",		0,		/* b4 */
	"or	l",		0,		/* b5 */
	"or	(hl)",		0,		/* b6 */
	"or	a",		0,		/* b7 */

	"cp	b",		0,		/* b8 */
	"cp	c",		0,		/* b9 */
	"cp	d",		0,		/* ba */
	"cp	e",		0,		/* bb */
	"cp	h",		0,		/* bc */
	"cp	l",		0,		/* bd */
	"cp	(hl)",		0,		/* be */
	"cp	a",		0,		/* bf */

	"ret	nz",		0,		/* c0 */
	"pop	bc",		0,		/* c1 */
	"jp	nz,%02x%02xh",	2,		/* c2 */
	"jp	%02x%02xh",	2,		/* c3 */
	"call	nz,%02x%02xh",	2,		/* c4 */
	"push	bc",		0,		/* c5 */
	"add	a,%02xh",	1,		/* c6 */
	"rst	0",		0,		/* c7 */

	"ret	z",		0,		/* c8 */
	"ret",			0,		/* c9 */
	"jp	z,%02x%02xh",	2,		/* ca */
	0,			0,		/* cb */
	"call	z,%02x%02xh",	2,		/* cc */
	"call	%02x%02xh",	2,		/* cd */
	"adc	a,%02xh",	1,		/* ce */
	"rst	8",		0,		/* cf */
	
	"ret	nc",		0,		/* d0 */
	"pop	de",		0,		/* d1 */
	"jp	nc,%02x%02xh",	2,		/* d2 */
	"out	(%02xh),a",	1,		/* d3 */
	"call	nc,%02x%02xh",	2,		/* d4 */
	"push	de",		0,		/* d5 */
	"sub	%02xh",		1,		/* d6 */
	"rst	10h",		0,		/* d7 */
	
	"ret	c",		0,		/* d8 */
	"exx",			0,		/* d9 */
	"jp	c,%02x%02xh",	2,		/* da */
	"in	a,(%02xh)",	1,		/* db */
	"call	c,%02x%02xh",	2,		/* dc */
	0,			1,		/* dd */
	"sbc	a,%02xh",	1,		/* de */
	"rst	18h",		0,		/* df */
	
	"ret	po",		0,		/* e0 */
	"pop	hl",		0,		/* e1 */
	"jp	po,%02x%02xh",	2,		/* e2 */
	"ex	(sp),hl",	0,		/* e3 */
	"call	po,%02x%02xh",	2,		/* e4 */
	"push	hl",		0,		/* e5 */
	"and	%02xh",		1,		/* e6 */
	"rst	20h",		0,		/* e7 */
	"ret	pe",		0,		/* e8 */
	
	"jp	(hl)",		0,		/* e9 */
	"jp	pe,%02x%02xh",	2,		/* ea */
	"ex	de,hl",		0,		/* eb */
	"call	pe,%02x%02xh",	2,		/* ec */
	0,			2,		/* ed */
	"xor	%02xh",		1,		/* ee */
	"rst	28h",		0,		/* ef */
	
	"ret	p",		0,		/* f0 */
	"pop	af",		0,		/* f1 */
	"jp	p,%02x%02xh",	2,		/* f2 */
	"di",			0,		/* f3 */
	"call	p,%02x%02xh",	2,		/* f4 */
	"push	af",		0,		/* f5 */
	"or	%02xh",		1,		/* f6 */
	"rst	30h",		0,		/* f7 */
	
	"ret	m",		0,		/* f8 */
	"ld	sp,hl",		0,		/* f9 */
	"jp	m,%02x%02xh",	2,		/* fa */
	"ei",			0,		/* fb */
	"call	m,%02x%02xh",	2,		/* fc */
	0,			3,		/* fd */
	"cp	%02xh",		1,		/* fe */
	"rst	38h",		0,		/* ff */
};

static struct opcode minor[6][256] = {
							/* cb */
	"rlc	b",		0,		/* cb00 */
	"rlc	c",		0,		/* cb01 */
	"rlc	d",		0,		/* cb02 */
	"rlc	e",		0,		/* cb03 */
	"rlc	h",		0,		/* cb04 */
	"rlc	l",		0,		/* cb05 */
	"rlc	(hl)",		0,		/* cb06 */
	"rlc	a",		0,		/* cb07 */
	
	"rrc	b",		0,		/* cb08 */
	"rrc	c",		0,		/* cb09 */
	"rrc	d",		0,		/* cb0a */
	"rrc	e",		0,		/* cb0b */
	"rrc	h",		0,		/* cb0c */
	"rrc	l",		0,		/* cb0d */
	"rrc	(hl)",		0,		/* cb0e */
	"rrc	a",		0,		/* cb0f */
	
	"rl	b",		0,		/* cb10 */
	"rl	c",		0,		/* cb11 */
	"rl	d",		0,		/* cb12 */
	"rl	e",		0,		/* cb13 */
	"rl	h",		0,		/* cb14 */
	"rl	l",		0,		/* cb15 */
	"rl	(hl)",		0,		/* cb16 */
	"rl	a",		0,		/* cb17 */
	
	"rr	b",		0,		/* cb18 */
	"rr	c",		0,		/* cb19 */
	"rr	d",		0,		/* cb1a */
	"rr	e",		0,		/* cb1b */
	"rr	h",		0,		/* cb1c */
	"rr	l",		0,		/* cb1d */
	"rr	(hl)",		0,		/* cb1e */
	"rr	a",		0,		/* cb1f */
	
	"sla	b",		0,		/* cb20 */
	"sla	c",		0,		/* cb21 */
	"sla	d",		0,		/* cb22 */
	"sla	e",		0,		/* cb23 */
	"sla	h",		0,		/* cb24 */
	"sla	l",		0,		/* cb25 */
	"sla	(hl)",		0,		/* cb26 */
	"sla	a",		0,		/* cb27 */
	
	"sra	b",		0,		/* cb28 */
	"sra	c",		0,		/* cb29 */
	"sra	d",		0,		/* cb2a */
	"sra	e",		0,		/* cb2b */
	"sra	h",		0,		/* cb2c */
	"sra	l",		0,		/* cb2d */
	"sra	(hl)",		0,		/* cb2e */
	"sra	a",		0,		/* cb2f */
	
	undefined,		0,		/* cb30 */
	undefined,		0,		/* cb31 */
	undefined,		0,		/* cb32 */
	undefined,		0,		/* cb33 */
	undefined,		0,		/* cb34 */
	undefined,		0,		/* cb35 */
	undefined,		0,		/* cb36 */
	undefined,		0,		/* cb37 */
	
	"srl	b",		0,		/* cb38 */
	"srl	c",		0,		/* cb39 */
	"srl	d",		0,		/* cb3a */
	"srl	e",		0,		/* cb3b */
	"srl	h",		0,		/* cb3c */
	"srl	l",		0,		/* cb3d */
	"srl	(hl)",		0,		/* cb3e */
	"srl	a",		0,		/* cb3f */
	
	"bit	0,b",		0,		/* cb40 */
	"bit	0,c",		0,		/* cb41 */
	"bit	0,d",		0,		/* cb42 */
	"bit	0,e",		0,		/* cb43 */
	"bit	0,h",		0,		/* cb44 */
	"bit	0,l",		0,		/* cb45 */
	"bit	0,(hl)",	0,		/* cb46 */
	"bit	0,a",		0,		/* cb47 */
	
	"bit	1,b",		0,		/* cb48 */
	"bit	1,c",		0,		/* cb49 */
	"bit	1,d",		0,		/* cb4a */
	"bit	1,e",		0,		/* cb4b */
	"bit	1,h",		0,		/* cb4c */
	"bit	1,l",		0,		/* cb4d */
	"bit	1,(hl)",	0,		/* cb4e */
	"bit	1,a",		0,		/* cb4f */
	
	"bit	2,b",		0,		/* cb50 */
	"bit	2,c",		0,		/* cb51 */
	"bit	2,d",		0,		/* cb52 */
	"bit	2,e",		0,		/* cb53 */
	"bit	2,h",		0,		/* cb54 */
	"bit	2,l",		0,		/* cb55 */
	"bit	2,(hl)",	0,		/* cb56 */
	"bit	2,a",		0,		/* cb57 */
	
	"bit	3,b",		0,		/* cb58 */
	"bit	3,c",		0,		/* cb59 */
	"bit	3,d",		0,		/* cb5a */
	"bit	3,e",		0,		/* cb5b */
	"bit	3,h",		0,		/* cb5c */
	"bit	3,l",		0,		/* cb5d */
	"bit	3,(hl)",	0,		/* cb5e */
	"bit	3,a",		0,		/* cb5f */
	
	"bit	4,b",		0,		/* cb60 */
	"bit	4,c",		0,		/* cb61 */
	"bit	4,d",		0,		/* cb62 */
	"bit	4,e",		0,		/* cb63 */
	"bit	4,h",		0,		/* cb64 */
	"bit	4,l",		0,		/* cb65 */
	"bit	4,(hl)",	0,		/* cb66 */
	"bit	4,a",		0,		/* cb67 */
	
	"bit	5,b",		0,		/* cb68 */
	"bit	5,c",		0,		/* cb69 */
	"bit	5,d",		0,		/* cb6a */
	"bit	5,e",		0,		/* cb6b */
	"bit	5,h",		0,		/* cb6c */
	"bit	5,l",		0,		/* cb6d */
	"bit	5,(hl)",	0,		/* cb6e */
	"bit	5,a",		0,		/* cb6f */
	
	"bit	6,b",		0,		/* cb70 */
	"bit	6,c",		0,		/* cb71 */
	"bit	6,d",		0,		/* cb72 */
	"bit	6,e",		0,		/* cb73 */
	"bit	6,h",		0,		/* cb74 */
	"bit	6,l",		0,		/* cb75 */
	"bit	6,(hl)",	0,		/* cb76 */
	"bit	6,a",		0,		/* cb77 */
	
	"bit	7,b",		0,		/* cb78 */
	"bit	7,c",		0,		/* cb79 */
	"bit	7,d",		0,		/* cb7a */
	"bit	7,e",		0,		/* cb7b */
	"bit	7,h",		0,		/* cb7c */
	"bit	7,l",		0,		/* cb7d */
	"bit	7,(hl)",	0,		/* cb7e */
	"bit	7,a",		0,		/* cb7f */
	
	"res	0,b",		0,		/* cb80 */
	"res	0,c",		0,		/* cb81 */
	"res	0,d",		0,		/* cb82 */
	"res	0,e",		0,		/* cb83 */
	"res	0,h",		0,		/* cb84 */
	"res	0,l",		0,		/* cb85 */
	"res	0,(hl)",	0,		/* cb86 */
	"res	0,a",		0,		/* cb87 */
	
	"res	1,b",		0,		/* cb88 */
	"res	1,c",		0,		/* cb89 */
	"res	1,d",		0,		/* cb8a */
	"res	1,e",		0,		/* cb8b */
	"res	1,h",		0,		/* cb8c */
	"res	1,l",		0,		/* cb8d */
	"res	1,(hl)",	0,		/* cb8e */
	"res	1,a",		0,		/* cb8f */
	
	"res	2,b",		0,		/* cb90 */
	"res	2,c",		0,		/* cb91 */
	"res	2,d",		0,		/* cb92 */
	"res	2,e",		0,		/* cb93 */
	"res	2,h",		0,		/* cb94 */
	"res	2,l",		0,		/* cb95 */
	"res	2,(hl)",	0,		/* cb96 */
	"res	2,a",		0,		/* cb97 */
	
	"res	3,b",		0,		/* cb98 */
	"res	3,c",		0,		/* cb99 */
	"res	3,d",		0,		/* cb9a */
	"res	3,e",		0,		/* cb9b */
	"res	3,h",		0,		/* cb9c */
	"res	3,l",		0,		/* cb9d */
	"res	3,(hl)",	0,		/* cb9e */
	"res	3,a",		0,		/* cb9f */
	
	"res	4,b",		0,		/* cba0 */
	"res	4,c",		0,		/* cba1 */
	"res	4,d",		0,		/* cba2 */
	"res	4,e",		0,		/* cba3 */
	"res	4,h",		0,		/* cba4 */
	"res	4,l",		0,		/* cba5 */
	"res	4,(hl)",	0,		/* cba6 */
	"res	4,a",		0,		/* cba7 */
	
	"res	5,b",		0,		/* cba8 */
	"res	5,c",		0,		/* cba9 */
	"res	5,d",		0,		/* cbaa */
	"res	5,e",		0,		/* cbab */
	"res	5,h",		0,		/* cbac */
	"res	5,l",		0,		/* cbad */
	"res	5,(hl)",	0,		/* cbae */
	"res	5,a",		0,		/* cbaf */
	
	"res	6,b",		0,		/* cbb0 */
	"res	6,c",		0,		/* cbb1 */
	"res	6,d",		0,		/* cbb2 */
	"res	6,e",		0,		/* cbb3 */
	"res	6,h",		0,		/* cbb4 */
	"res	6,l",		0,		/* cbb5 */
	"res	6,(hl)",	0,		/* cbb6 */
	"res	6,a",		0,		/* cbb7 */
	
	"res	7,b",		0,		/* cbb8 */
	"res	7,c",		0,		/* cbb9 */
	"res	7,d",		0,		/* cbba */
	"res	7,e",		0,		/* cbbb */
	"res	7,h",		0,		/* cbbc */
	"res	7,l",		0,		/* cbbd */
	"res	7,(hl)",	0,		/* cbbe */
	"res	7,a",		0,		/* cbbf */
	
	"set	0,b",		0,		/* cbc0 */
	"set	0,c",		0,		/* cbc1 */
	"set	0,d",		0,		/* cbc2 */
	"set	0,e",		0,		/* cbc3 */
	"set	0,h",		0,		/* cbc4 */
	"set	0,l",		0,		/* cbc5 */
	"set	0,(hl)",	0,		/* cbc6 */
	"set	0,a",		0,		/* cbc7 */
	
	"set	1,b",		0,		/* cbc8 */
	"set	1,c",		0,		/* cbc9 */
	"set	1,d",		0,		/* cbca */
	"set	1,e",		0,		/* cbcb */
	"set	1,h",		0,		/* cbcc */
	"set	1,l",		0,		/* cbcd */
	"set	1,(hl)",	0,		/* cbce */
	"set	1,a",		0,		/* cbcf */
	
	"set	2,b",		0,		/* cbd0 */
	"set	2,c",		0,		/* cbd1 */
	"set	2,d",		0,		/* cbd2 */
	"set	2,e",		0,		/* cbd3 */
	"set	2,h",		0,		/* cbd4 */
	"set	2,l",		0,		/* cbd5 */
	"set	2,(hl)",	0,		/* cbd6 */
	"set	2,a",		0,		/* cbd7 */
	
	"set	3,b",		0,		/* cbd8 */
	"set	3,c",		0,		/* cbd9 */
	"set	3,d",		0,		/* cbda */
	"set	3,e",		0,		/* cbdb */
	"set	3,h",		0,		/* cbdc */
	"set	3,l",		0,		/* cbdd */
	"set	3,(hl)",	0,		/* cbde */
	"set	3,a",		0,		/* cbdf */
	
	"set	4,b",		0,		/* cbe0 */
	"set	4,c",		0,		/* cbe1 */
	"set	4,d",		0,		/* cbe2 */
	"set	4,e",		0,		/* cbe3 */
	"set	4,h",		0,		/* cbe4 */
	"set	4,l",		0,		/* cbe5 */
	"set	4,(hl)",	0,		/* cbe6 */
	"set	4,a",		0,		/* cbe7 */
	
	"set	5,b",		0,		/* cbe8 */
	"set	5,c",		0,		/* cbe9 */
	"set	5,d",		0,		/* cbea */
	"set	5,e",		0,		/* cbeb */
	"set	5,h",		0,		/* cbec */
	"set	5,l",		0,		/* cbed */
	"set	5,(hl)",	0,		/* cbee */
	"set	5,a",		0,		/* cbef */
	
	"set	6,b",		0,		/* cbf0 */
	"set	6,c",		0,		/* cbf1 */
	"set	6,d",		0,		/* cbf2 */
	"set	6,e",		0,		/* cbf3 */
	"set	6,h",		0,		/* cbf4 */
	"set	6,l",		0,		/* cbf5 */
	"set	6,(hl)",	0,		/* cbf6 */
	"set	6,a",		0,		/* cbf7 */
	
	"set	7,b",		0,		/* cbf8 */
	"set	7,c",		0,		/* cbf9 */
	"set	7,d",		0,		/* cbfa */
	"set	7,e",		0,		/* cbfb */
	"set	7,h",		0,		/* cbfc */
	"set	7,l",		0,		/* cbfd */
	"set	7,(hl)",	0,		/* cbfe */
	"set	7,a",		0,		/* cbff */
							/* dd */
	undefined,		0,		/* dd00 */
	undefined,		0,		/* dd01 */
	undefined,		0,		/* dd02 */
	undefined,		0,		/* dd03 */
	undefined,		0,		/* dd04 */
	undefined,		0,		/* dd05 */
	undefined,		0,		/* dd06 */
	undefined,		0,		/* dd07 */

	undefined,		0,		/* dd08 */
	"add	ix,bc",		0,		/* dd09 */
	undefined,		0,		/* dd0a */
	undefined,		0,		/* dd0b */
	undefined,		0,		/* dd0c */
	undefined,		0,		/* dd0d */
	undefined,		0,		/* dd0e */
	undefined,		0,		/* dd0f */

	undefined,		0,		/* dd10 */
	undefined,		0,		/* dd11 */
	undefined,		0,		/* dd12 */
	undefined,		0,		/* dd13 */
	undefined,		0,		/* dd14 */
	undefined,		0,		/* dd15 */
	undefined,		0,		/* dd16 */
	undefined,		0,		/* dd17 */

	undefined,		0,		/* dd18 */
	"add	ix,de",		0,		/* dd19 */
	undefined,		0,		/* dd1a */
	undefined,		0,		/* dd1b */
	undefined,		0,		/* dd1c */
	undefined,		0,		/* dd1d */
	undefined,		0,		/* dd1e */
	undefined,		0,		/* dd1f */

	undefined,		0,		/* dd20 */
	"ld	ix,%02x%02xh",	2,		/* dd21 */
	"ld	(%02x%02xh),ix",2,		/* dd22 */
	"inc	ix",		0,		/* dd23 */
	undefined,		0,		/* dd24 */
	undefined,		0,		/* dd25 */
	undefined,		0,		/* dd26 */
	undefined,		0,		/* dd27 */

	undefined,		0,		/* dd28 */
	"add	ix,ix",		0,		/* dd29 */
	"ld	ix,(%02x%02xh)",2,		/* dd2a */
	"dec	ix",		0,		/* dd2b */
	undefined,		0,		/* dd2c */
	undefined,		0,		/* dd2d */
	undefined,		0,		/* dd2e */
	undefined,		0,		/* dd2f */

	undefined,		0,		/* dd30 */
	undefined,		0,		/* dd31 */
	undefined,		0,		/* dd32 */
	undefined,		0,		/* dd33 */
	"inc	(ix+%02xh)",	1,		/* dd34 */
	"dec	(ix+%02xh)",	1,		/* dd35 */
	"ld	(ix+%02xh),%02xh",2,		/* dd36 */
	undefined,		0,		/* dd37 */

	undefined,		0,		/* dd38 */
	"add	ix,sp",		0,		/* dd39 */
	undefined,		0,		/* dd3a */
	undefined,		0,		/* dd3b */
	undefined,		0,		/* dd3c */
	undefined,		0,		/* dd3d */
	undefined,		0,		/* dd3e */
	undefined,		0,		/* dd3f */

	undefined,		0,		/* dd40 */
	undefined,		0,		/* dd41 */
	undefined,		0,		/* dd42 */
	undefined,		0,		/* dd43 */
	undefined,		0,		/* dd44 */
	undefined,		0,		/* dd45 */
	"ld	b,(ix+%02xh)",	1,		/* dd46 */
	undefined,		0,		/* dd47 */

	undefined,		0,		/* dd48 */
	undefined,		0,		/* dd49 */
	undefined,		0,		/* dd4a */
	undefined,		0,		/* dd4b */
	undefined,		0,		/* dd4c */
	undefined,		0,		/* dd4d */
	"ld	c,(ix+%02xh)",	1,		/* dd4e */
	undefined,		0,		/* dd4f */
	
	undefined,		0,		/* dd50 */
	undefined,		0,		/* dd51 */
	undefined,		0,		/* dd52 */
	undefined,		0,		/* dd53 */
	undefined,		0,		/* dd54 */
	undefined,		0,		/* dd55 */
	"ld	d,(ix+%02xh)",	1,		/* dd56 */
	undefined,		0,		/* dd57 */

	undefined,		0,		/* dd58 */
	undefined,		0,		/* dd59 */
	undefined,		0,		/* dd5a */
	undefined,		0,		/* dd5b */
	undefined,		0,		/* dd5c */
	undefined,		0,		/* dd5d */
	"ld	e,(ix+%02xh)",	1,		/* dd5e */
	undefined,		0,		/* dd5f */
	
	undefined,		0,		/* dd60 */
	undefined,		0,		/* dd61 */
	undefined,		0,		/* dd62 */
	undefined,		0,		/* dd63 */
	undefined,		0,		/* dd64 */
	undefined,		0,		/* dd65 */
	"ld	h,(ix+%02xh)",	1,		/* dd66 */
	undefined,		0,		/* dd67 */

	undefined,		0,		/* dd68 */
	undefined,		0,		/* dd69 */
	undefined,		0,		/* dd6a */
	undefined,		0,		/* dd6b */
	undefined,		0,		/* dd6c */
	undefined,		0,		/* dd6d */
	"ld	l,(ix+%02xh)",	1,		/* dd6e */
	undefined,		0,		/* dd6f */
	
	"ld	(ix+%02xh),b",	1,		/* dd70 */
	"ld	(ix+%02xh),c",	1,		/* dd71 */
	"ld	(ix+%02xh),d",	1,		/* dd72 */
	"ld	(ix+%02xh),e",	1,		/* dd73 */
	"ld	(ix+%02xh),h",	1,		/* dd74 */
	"ld	(ix+%02xh),l",	1,		/* dd75 */
	undefined,		0,		/* dd76 */
	"ld	(ix+%02xh),a",	1,		/* dd77 */

	undefined,		0,		/* dd78 */
	undefined,		0,		/* dd79 */
	undefined,		0,		/* dd7a */
	undefined,		0,		/* dd7b */
	undefined,		0,		/* dd7c */
	undefined,		0,		/* dd7d */
	"ld	a,(ix+%02xh)",	1,		/* dd7e */
	undefined,		0,		/* dd7f */

	undefined,		0,		/* dd80 */
	undefined,		0,		/* dd81 */
	undefined,		0,		/* dd82 */
	undefined,		0,		/* dd83 */
	undefined,		0,		/* dd84 */
	undefined,		0,		/* dd85 */
	"add	a,(ix+%02xh)",	1,		/* dd86 */
	undefined,		0,		/* dd87 */

	undefined,		0,		/* dd88 */
	undefined,		0,		/* dd89 */
	undefined,		0,		/* dd8a */
	undefined,		0,		/* dd8b */
	undefined,		0,		/* dd8c */
	undefined,		0,		/* dd8d */
	"adc	a,(ix+%02xh)",	1,		/* dd8e */
	undefined,		0,		/* dd8f */
	
	undefined,		0,		/* dd90 */
	undefined,		0,		/* dd91 */
	undefined,		0,		/* dd92 */
	undefined,		0,		/* dd93 */
	undefined,		0,		/* dd94 */
	undefined,		0,		/* dd95 */
	"sub	(ix+%02xh)",	1,		/* dd96 */
	undefined,		0,		/* dd97 */

	undefined,		0,		/* dd98 */
	undefined,		0,		/* dd99 */
	undefined,		0,		/* dd9a */
	undefined,		0,		/* dd9b */
	undefined,		0,		/* dd9c */
	undefined,		0,		/* dd9d */
	"sbc	a,(ix+%02xh)",	1,		/* dd9e */
	undefined,		0,		/* dd9f */
	
	undefined,		0,		/* dda0 */
	undefined,		0,		/* dda1 */
	undefined,		0,		/* dda2 */
	undefined,		0,		/* dda3 */
	undefined,		0,		/* dda4 */
	undefined,		0,		/* dda5 */
	"and	(ix+%02xh)",	1,		/* dda6 */
	undefined,		0,		/* dda7 */

	undefined,		0,		/* dda8 */
	undefined,		0,		/* dda9 */
	undefined,		0,		/* ddaa */
	undefined,		0,		/* ddab */
	undefined,		0,		/* ddac */
	undefined,		0,		/* ddad */
	"xor	(ix+%02xh)",	1,		/* ddae */
	undefined,		0,		/* ddaf */
	
	undefined,		0,		/* ddb0 */
	undefined,		0,		/* ddb1 */
	undefined,		0,		/* ddb2 */
	undefined,		0,		/* ddb3 */
	undefined,		0,		/* ddb4 */
	undefined,		0,		/* ddb5 */
	"or	(ix+%02xh)",	1,		/* ddb6 */
	undefined,		0,		/* ddb7 */

	undefined,		0,		/* ddb8 */
	undefined,		0,		/* ddb9 */
	undefined,		0,		/* ddba */
	undefined,		0,		/* ddbb */
	undefined,		0,		/* ddbc */
	undefined,		0,		/* ddbd */
	"cp	(ix+%02xh)",	1,		/* ddbe */
	undefined,		0,		/* ddbf */
	
	undefined,		0,		/* ddc0 */
	undefined,		0,		/* ddc1 */
	undefined,		0,		/* ddc2 */
	undefined,		0,		/* ddc3 */
	undefined,		0,		/* ddc4 */
	undefined,		0,		/* ddc5 */
	undefined,		0,		/* ddc6 */
	undefined,		0,		/* ddc7 */

	undefined,		0,		/* ddc8 */
	undefined,		0,		/* ddc9 */
	undefined,		0,		/* ddca */
	0,	                4,		/* ddcb */
	undefined,		0,		/* ddcc */
	undefined,		0,		/* ddcd */
	undefined,		0,		/* ddce */
	undefined,		0,		/* ddcf */
	
	undefined,		0,		/* ddd0 */
	undefined,		0,		/* ddd1 */
	undefined,		0,		/* ddd2 */
	undefined,		0,		/* ddd3 */
	undefined,		0,		/* ddd4 */
	undefined,		0,		/* ddd5 */
	undefined,		0,		/* ddd6 */
	undefined,		0,		/* ddd7 */

	undefined,		0,		/* ddd8 */
	undefined,		0,		/* ddd9 */
	undefined,		0,		/* ddda */
	undefined,		0,		/* dddb */
	undefined,		0,		/* dddc */
	undefined,		0,		/* dddd */
	undefined,		0,		/* ddde */
	undefined,		0,		/* dddf */
	
	undefined,		0,		/* dde0 */
	"pop	ix",		0,		/* dde1 */
	undefined,		0,		/* dde2 */
	"ex	(sp),ix",	0,		/* dde3 */
	undefined,		0,		/* dde4 */
	"push	ix",		0,		/* dde5 */
	undefined,		0,		/* dde6 */
	undefined,		0,		/* dde7 */

	undefined,		0,		/* dde8 */
	"jp	(ix)",		0,		/* dde9 */
	undefined,		0,		/* ddea */
	undefined,		0,		/* ddeb */
	undefined,		0,		/* ddec */
	undefined,		0,		/* dded */
	undefined,		0,		/* ddee */
	undefined,		0,		/* ddef */
	
	undefined,		0,		/* ddf0 */
	undefined,		0,		/* ddf1 */
	undefined,		0,		/* ddf2 */
	undefined,		0,		/* ddf3 */
	undefined,		0,		/* ddf4 */
	undefined,		0,		/* ddf5 */
	undefined,		0,		/* ddf6 */
	undefined,		0,		/* ddf7 */

	undefined,		0,		/* ddf8 */
	"ld	sp,ix",		0,		/* ddf9 */
	undefined,		0,		/* ddfa */
	undefined,		0,		/* ddfb */
	undefined,		0,		/* ddfc */
	undefined,		0,		/* ddfd */
	undefined,		0,		/* ddfe */
	undefined,		0,		/* ddff */
							/* ed */
	undefined,		0,		/* ed00 */
	undefined,		0,		/* ed01 */
	undefined,		0,		/* ed02 */
	undefined,		0,		/* ed03 */
	undefined,		0,		/* ed04 */
	undefined,		0,		/* ed05 */
	undefined,		0,		/* ed06 */
	undefined,		0,		/* ed07 */

	undefined,		0,		/* ed08 */
	undefined,		0,		/* ed09 */
	undefined,		0,		/* ed0a */
	undefined,		0,		/* ed0b */
	undefined,		0,		/* ed0c */
	undefined,		0,		/* ed0d */
	undefined,		0,		/* ed0e */
	undefined,		0,		/* ed0f */

	undefined,		0,		/* ed10 */
	undefined,		0,		/* ed11 */
	undefined,		0,		/* ed12 */
	undefined,		0,		/* ed13 */
	undefined,		0,		/* ed14 */
	undefined,		0,		/* ed15 */
	undefined,		0,		/* ed16 */
	undefined,		0,		/* ed17 */

	undefined,		0,		/* ed18 */
	undefined,		0,		/* ed19 */
	undefined,		0,		/* ed1a */
	undefined,		0,		/* ed1b */
	undefined,		0,		/* ed1c */
	undefined,		0,		/* ed1d */
	undefined,		0,		/* ed1e */
	undefined,		0,		/* ed1f */

	undefined,		0,		/* ed20 */
	undefined,		0,		/* ed21 */
	undefined,		0,		/* ed22 */
	undefined,		0,		/* ed23 */
	undefined,		0,		/* ed24 */
	undefined,		0,		/* ed25 */
	undefined,		0,		/* ed26 */
	undefined,		0,		/* ed27 */

	undefined,		0,		/* ed28 */
	undefined,		0,		/* ed29 */
	undefined,		0,		/* ed2a */
	undefined,		0,		/* ed2b */
	undefined,		0,		/* ed2c */
	undefined,		0,		/* ed2d */
	undefined,		0,		/* ed2e */
	undefined,		0,		/* ed2f */

	undefined,		0,		/* ed30 */
	undefined,		0,		/* ed31 */
	undefined,		0,		/* ed32 */
	undefined,		0,		/* ed33 */
	undefined,		0,		/* ed34 */
	undefined,		0,		/* ed35 */
	undefined,		0,		/* ed36 */
	undefined,		0,		/* ed37 */

	undefined,		0,		/* ed38 */
	undefined,		0,		/* ed39 */
	undefined,		0,		/* ed3a */
	undefined,		0,		/* ed3b */
	undefined,		0,		/* ed3c */
	undefined,		0,		/* ed3d */
	undefined,		0,		/* ed3e */
	undefined,		0,		/* ed3f */

	"in	b,(c)",		0,		/* ed40 */
	"out	(c),b",		0,		/* ed41 */
	"sbc	hl,bc",		0,		/* ed42 */
	"ld	(%02x%02xh),bc",2,		/* ed43 */
	"neg",			0,		/* ed44 */
	"retn",			0,		/* ed45 */
	"im	0",		0,		/* ed46 */
	"ld	i,a",		0,		/* ed47 */
	
	"in	c,(c)",		0,		/* ed48 */
	"out	(c),c",		0,		/* ed49 */
	"adc	hl,bc",		0,		/* ed4a */
	"ld	bc,(%02x%02xh)",2,		/* ed4b */
	undefined,		0,		/* ed4c */
	"reti",			0,		/* ed4d */
	undefined,		0,		/* ed4e */
	undefined,		0,		/* ed4f */

	"in	d,(c)",		0,		/* ed50 */
	"out	(c),d",		0,		/* ed51 */
	"sbc	hl,de",		0,		/* ed52 */
	"ld	(%02x%02xh),de",2,		/* ed53 */
	undefined,		0,		/* ed54 */
	undefined,		0,		/* ed55 */
	"im	1",		0,		/* ed56 */
	"ld	a,i",		0,		/* ed57 */

	"in	e,(c)",		0,		/* ed58 */
	"out	(c),e",		0,		/* ed59 */
	"adc	hl,de",		0,		/* ed5a */
	"ld	de,(%02x%02xh)",2,		/* ed5b */
	undefined,		0,		/* ed5c */
	undefined,		0,		/* ed5d */
	"im	2",		0,		/* ed5e */
	undefined,		0,		/* ed5f */

	"in	h,(c)",		0,		/* ed60 */
	"out	(c),h",		0,		/* ed61 */
	"sbc	hl,hl",		0,		/* ed62 */
	undefined,		0,		/* ed63 */
	undefined,		0,		/* ed64 */
	undefined,		0,		/* ed65 */
	undefined,		0,		/* ed66 */
	"rrd",			0,		/* ed67 */

	"in	l,(c)",		0,		/* ed68 */
	"out	(c),l",		0,		/* ed69 */
	"adc	hl,hl",		0,		/* ed6a */
	undefined,		0,		/* ed6b */
	undefined,		0,		/* ed6c */
	undefined,		0,		/* ed6d */
	undefined,		0,		/* ed6e */
	"rld",			0,		/* ed6f */
	
	undefined,		0,		/* ed70 */
	undefined,		0,		/* ed71 */
	"sbc	hl,sp",		0,		/* ed72 */
	"ld	(%02x%02xh),sp",2,		/* ed73 */
	undefined,		0,		/* ed74 */
	undefined,		0,		/* ed75 */
	undefined,		0,		/* ed76 */
	undefined,		0,		/* ed77 */

	"in	a,(c)",		0,		/* ed78 */
	"out	(c),a",		0,		/* ed79 */
	"adc	hl,sp",		0,		/* ed7a */
	"ld	sp,(%02x%02xh)",2,		/* ed7b */
	undefined,		0,		/* ed7c */
	undefined,		0,		/* ed7d */
	undefined,		0,		/* ed7e */
	undefined,		0,		/* ed7f */

	undefined,		0,		/* ed80 */
	undefined,		0,		/* ed81 */
	undefined,		0,		/* ed82 */
	undefined,		0,		/* ed83 */
	undefined,		0,		/* ed84 */
	undefined,		0,		/* ed85 */
	undefined,		0,		/* ed86 */
	undefined,		0,		/* ed87 */

	undefined,		0,		/* ed88 */
	undefined,		0,		/* ed89 */
	undefined,		0,		/* ed8a */
	undefined,		0,		/* ed8b */
	undefined,		0,		/* ed8c */
	undefined,		0,		/* ed8d */
	undefined,		0,		/* ed8e */
	undefined,		0,		/* ed8f */

	undefined,		0,		/* ed90 */
	undefined,		0,		/* ed91 */
	undefined,		0,		/* ed92 */
	undefined,		0,		/* ed93 */
	undefined,		0,		/* ed94 */
	undefined,		0,		/* ed95 */
	undefined,		0,		/* ed96 */
	undefined,		0,		/* ed97 */

	undefined,		0,		/* ed98 */
	undefined,		0,		/* ed99 */
	undefined,		0,		/* ed9a */
	undefined,		0,		/* ed9b */
	undefined,		0,		/* ed9c */
	undefined,		0,		/* ed9d */
	undefined,		0,		/* ed9e */
	undefined,		0,		/* ed9f */

	"ldi",			0,		/* eda0 */
	"cpi",			0,		/* eda1 */
	"ini",			0,		/* eda2 */
	"outi",			0,		/* eda3 */
	undefined,		0,		/* eda4 */
	undefined,		0,		/* eda5 */
	undefined,		0,		/* eda6 */
	undefined,		0,		/* eda7 */

	"ldd",			0,		/* eda8 */
	"cpd",			0,		/* eda9 */
	"ind",			0,		/* edaa */
	"outd",			0,		/* edab */
	undefined,		0,		/* edac */
	undefined,		0,		/* edad */
	undefined,		0,		/* edae */
	undefined,		0,		/* edaf */

	"ldir",			0,		/* edb0 */
	"cpir",			0,		/* edb1 */
	"inir",			0,		/* edb2 */
	"otir",			0,		/* edb3 */
	undefined,		0,		/* edb4 */
	undefined,		0,		/* edb5 */
	undefined,		0,		/* edb6 */
	undefined,		0,		/* edb7 */

	"lddr",			0,		/* edb8 */
	"cpdr",			0,		/* edb9 */
	"indr",			0,		/* edba */
	"otdr",			0,		/* edbb */
	undefined,		0,		/* edbc */
	undefined,		0,		/* edbd */
	undefined,		0,		/* edbe */
	undefined,		0,		/* edbf */

	undefined,		0,		/* edc0 */
	undefined,		0,		/* edc1 */
	undefined,		0,		/* edc2 */
	undefined,		0,		/* edc3 */
	undefined,		0,		/* edc4 */
	undefined,		0,		/* edc5 */
	undefined,		0,		/* edc6 */
	undefined,		0,		/* edc7 */

	undefined,		0,		/* edc8 */
	undefined,		0,		/* edc9 */
	undefined,		0,		/* edca */
	undefined,		0,		/* edcb */
	undefined,		0,		/* edcc */
	undefined,		0,		/* edcd */
	undefined,		0,		/* edce */
	undefined,		0,		/* edcf */

	undefined,		0,		/* edd0 */
	undefined,		0,		/* edd1 */
	undefined,		0,		/* edd2 */
	undefined,		0,		/* edd3 */
	undefined,		0,		/* edd4 */
	undefined,		0,		/* edd5 */
	undefined,		0,		/* edd6 */
	undefined,		0,		/* edd7 */

	undefined,		0,		/* edd8 */
	undefined,		0,		/* edd9 */
	undefined,		0,		/* edda */
	undefined,		0,		/* eddb */
	undefined,		0,		/* eddc */
	undefined,		0,		/* eddd */
	undefined,		0,		/* edde */
	undefined,		0,		/* eddf */

	undefined,		0,		/* ede0 */
	undefined,		0,		/* ede1 */
	undefined,		0,		/* ede2 */
	undefined,		0,		/* ede3 */
	undefined,		0,		/* ede4 */
	undefined,		0,		/* ede5 */
	undefined,		0,		/* ede6 */
	undefined,		0,		/* ede7 */

	undefined,		0,		/* ede8 */
	undefined,		0,		/* ede9 */
	undefined,		0,		/* edea */
	undefined,		0,		/* edeb */
	undefined,		0,		/* edec */
	undefined,		0,		/* eded */
	undefined,		0,		/* edee */
	undefined,		0,		/* edef */

	undefined,		0,		/* edf0 */
	undefined,		0,		/* edf1 */
	undefined,		0,		/* edf2 */
	undefined,		0,		/* edf3 */
	undefined,		0,		/* edf4 */
	undefined,		0,		/* edf5 */
	undefined,		0,		/* edf6 */
	undefined,		0,		/* edf7 */

	undefined,		0,		/* edf8 */
	undefined,		0,		/* edf9 */
	undefined,		0,		/* edfa */
	undefined,		0,		/* edfb */
	undefined,		0,		/* edfc */
	undefined,		0,		/* edfd */
	undefined,		0,		/* edfe */
	undefined,		0,		/* edff */
							/* fd */
	undefined,		0,		/* fd00 */
	undefined,		0,		/* fd01 */
	undefined,		0,		/* fd02 */
	undefined,		0,		/* fd03 */
	undefined,		0,		/* fd04 */
	undefined,		0,		/* fd05 */
	undefined,		0,		/* fd06 */
	undefined,		0,		/* fd07 */

	undefined,		0,		/* fd08 */
	"add	iy,bc",		0,		/* fd09 */
	undefined,		0,		/* fd0a */
	undefined,		0,		/* fd0b */
	undefined,		0,		/* fd0c */
	undefined,		0,		/* fd0d */
	undefined,		0,		/* fd0e */
	undefined,		0,		/* fd0f */

	undefined,		0,		/* fd10 */
	undefined,		0,		/* fd11 */
	undefined,		0,		/* fd12 */
	undefined,		0,		/* fd13 */
	undefined,		0,		/* fd14 */
	undefined,		0,		/* fd15 */
	undefined,		0,		/* fd16 */
	undefined,		0,		/* fd17 */

	undefined,		0,		/* fd18 */
	"add	iy,de",		0,		/* fd19 */
	undefined,		0,		/* fd1a */
	undefined,		0,		/* fd1b */
	undefined,		0,		/* fd1c */
	undefined,		0,		/* fd1d */
	undefined,		0,		/* fd1e */
	undefined,		0,		/* fd1f */

	undefined,		0,		/* fd20 */
	"ld	iy,%02x%02xh",	2,		/* fd21 */
	"ld	(%02x%02xh),iy",2,		/* fd22 */
	"inc	iy",		0,		/* fd23 */
	undefined,		0,		/* fd24 */
	undefined,		0,		/* fd25 */
	undefined,		0,		/* fd26 */
	undefined,		0,		/* fd27 */

	undefined,		0,		/* fd28 */
	"add	iy,iy",		0,		/* fd29 */
	"ld	iy,(%02x%02xh)",2,		/* fd2a */
	"dec	iy",		0,		/* fd2b */
	undefined,		0,		/* fd2c */
	undefined,		0,		/* fd2d */
	undefined,		0,		/* fd2e */
	undefined,		0,		/* fd2f */

	undefined,		0,		/* fd30 */
	undefined,		0,		/* fd31 */
	undefined,		0,		/* fd32 */
	undefined,		0,		/* fd33 */
	"inc	(iy+%02xh)",	1,		/* fd34 */
	"dec	(iy+%02xh)",	1,		/* fd35 */
	"ld	(iy+%02xh),%02xh",2,		/* fd36 */
	undefined,		0,		/* fd37 */

	undefined,		0,		/* fd38 */
	"add	iy,sp",		0,		/* fd39 */
	undefined,		0,		/* fd3a */
	undefined,		0,		/* fd3b */
	undefined,		0,		/* fd3c */
	undefined,		0,		/* fd3d */
	undefined,		0,		/* fd3e */
	undefined,		0,		/* fd3f */

	undefined,		0,		/* fd40 */
	undefined,		0,		/* fd41 */
	undefined,		0,		/* fd42 */
	undefined,		0,		/* fd43 */
	undefined,		0,		/* fd44 */
	undefined,		0,		/* fd45 */
	"ld	b,(iy+%02xh)",	1,		/* fd46 */
	undefined,		0,		/* fd47 */

	undefined,		0,		/* fd48 */
	undefined,		0,		/* fd49 */
	undefined,		0,		/* fd4a */
	undefined,		0,		/* fd4b */
	undefined,		0,		/* fd4c */
	undefined,		0,		/* fd4d */
	"ld	c,(iy+%02xh)",	1,		/* fd4e */
	undefined,		0,		/* fd4f */
	
	undefined,		0,		/* fd50 */
	undefined,		0,		/* fd51 */
	undefined,		0,		/* fd52 */
	undefined,		0,		/* fd53 */
	undefined,		0,		/* fd54 */
	undefined,		0,		/* fd55 */
	"ld	d,(iy+%02xh)",	1,		/* fd56 */
	undefined,		0,		/* fd57 */

	undefined,		0,		/* fd58 */
	undefined,		0,		/* fd59 */
	undefined,		0,		/* fd5a */
	undefined,		0,		/* fd5b */
	undefined,		0,		/* fd5c */
	undefined,		0,		/* fd5d */
	"ld	e,(iy+%02xh)",	1,		/* fd5e */
	undefined,		0,		/* fd5f */
	
	undefined,		0,		/* fd60 */
	undefined,		0,		/* fd61 */
	undefined,		0,		/* fd62 */
	undefined,		0,		/* fd63 */
	undefined,		0,		/* fd64 */
	undefined,		0,		/* fd65 */
	"ld	h,(iy+%02xh)",	1,		/* fd66 */
	undefined,		0,		/* fd67 */

	undefined,		0,		/* fd68 */
	undefined,		0,		/* fd69 */
	undefined,		0,		/* fd6a */
	undefined,		0,		/* fd6b */
	undefined,		0,		/* fd6c */
	undefined,		0,		/* fd6d */
	"ld	l,(iy+%02xh)",	1,		/* fd6e */
	undefined,		0,		/* fd6f */
	
	"ld	(iy+%02xh),b",	1,		/* fd70 */
	"ld	(iy+%02xh),c",	1,		/* fd71 */
	"ld	(iy+%02xh),d",	1,		/* fd72 */
	"ld	(iy+%02xh),e",	1,		/* fd73 */
	"ld	(iy+%02xh),h",	1,		/* fd74 */
	"ld	(iy+%02xh),l",	1,		/* fd75 */
	undefined,		0,		/* fd76 */
	"ld	(iy+%02xh),a",	1,		/* fd77 */

	undefined,		0,		/* fd78 */
	undefined,		0,		/* fd79 */
	undefined,		0,		/* fd7a */
	undefined,		0,		/* fd7b */
	undefined,		0,		/* fd7c */
	undefined,		0,		/* fd7d */
	"ld	a,(iy+%02xh)",	1,		/* fd7e */
	undefined,		0,		/* fd7f */

	undefined,		0,		/* fd80 */
	undefined,		0,		/* fd81 */
	undefined,		0,		/* fd82 */
	undefined,		0,		/* fd83 */
	undefined,		0,		/* fd84 */
	undefined,		0,		/* fd85 */
	"add	a,(iy+%02xh)",	1,		/* fd86 */
	undefined,		0,		/* fd87 */

	undefined,		0,		/* fd88 */
	undefined,		0,		/* fd89 */
	undefined,		0,		/* fd8a */
	undefined,		0,		/* fd8b */
	undefined,		0,		/* fd8c */
	undefined,		0,		/* fd8d */
	"adc	a,(iy+%02xh)",	1,		/* fd8e */
	undefined,		0,		/* fd8f */
	
	undefined,		0,		/* fd90 */
	undefined,		0,		/* fd91 */
	undefined,		0,		/* fd92 */
	undefined,		0,		/* fd93 */
	undefined,		0,		/* fd94 */
	undefined,		0,		/* fd95 */
	"sub	(iy+%02xh)",	1,		/* fd96 */
	undefined,		0,		/* fd97 */

	undefined,		0,		/* fd98 */
	undefined,		0,		/* fd99 */
	undefined,		0,		/* fd9a */
	undefined,		0,		/* fd9b */
	undefined,		0,		/* fd9c */
	undefined,		0,		/* fd9d */
	"sbc	a,(iy+%02xh)",	1,		/* fd9e */
	undefined,		0,		/* fd9f */
	
	undefined,		0,		/* fda0 */
	undefined,		0,		/* fda1 */
	undefined,		0,		/* fda2 */
	undefined,		0,		/* fda3 */
	undefined,		0,		/* fda4 */
	undefined,		0,		/* fda5 */
	"and	(iy+%02xh)",	1,		/* fda6 */
	undefined,		0,		/* fda7 */

	undefined,		0,		/* fda8 */
	undefined,		0,		/* fda9 */
	undefined,		0,		/* fdaa */
	undefined,		0,		/* fdab */
	undefined,		0,		/* fdac */
	undefined,		0,		/* fdad */
	"xor	(iy+%02xh)",	1,		/* fdae */
	undefined,		0,		/* fdaf */
	
	undefined,		0,		/* fdb0 */
	undefined,		0,		/* fdb1 */
	undefined,		0,		/* fdb2 */
	undefined,		0,		/* fdb3 */
	undefined,		0,		/* fdb4 */
	undefined,		0,		/* fdb5 */
	"or	(iy+%02xh)",	1,		/* fdb6 */
	undefined,		0,		/* fdb7 */

	undefined,		0,		/* fdb8 */
	undefined,		0,		/* fdb9 */
	undefined,		0,		/* fdba */
	undefined,		0,		/* fdbb */
	undefined,		0,		/* fdbc */
	undefined,		0,		/* fdbd */
	"cp	(iy+%02xh)",	1,		/* fdbe */
	undefined,		0,		/* fdbf */
	
	undefined,		0,		/* fdc0 */
	undefined,		0,		/* fdc1 */
	undefined,		0,		/* fdc2 */
	undefined,		0,		/* fdc3 */
	undefined,		0,		/* fdc4 */
	undefined,		0,		/* fdc5 */
	undefined,		0,		/* fdc6 */
	undefined,		0,		/* fdc7 */

	undefined,		0,		/* fdc8 */
	undefined,		0,		/* fdc9 */
	undefined,		0,		/* fdca */
	0,	                5,		/* fdcb */
	undefined,		0,		/* fdcc */
	undefined,		0,		/* fdcd */
	undefined,		0,		/* fdce */
	undefined,		0,		/* fdcf */
	
	undefined,		0,		/* fdd0 */
	undefined,		0,		/* fdd1 */
	undefined,		0,		/* fdd2 */
	undefined,		0,		/* fdd3 */
	undefined,		0,		/* fdd4 */
	undefined,		0,		/* fdd5 */
	undefined,		0,		/* fdd6 */
	undefined,		0,		/* fdd7 */

	undefined,		0,		/* fdd8 */
	undefined,		0,		/* fdd9 */
	undefined,		0,		/* fdda */
	undefined,		0,		/* fddb */
	undefined,		0,		/* fddc */
	undefined,		0,		/* fddd */
	undefined,		0,		/* fdde */
	undefined,		0,		/* fddf */
	
	undefined,		0,		/* fde0 */
	"pop	iy",		0,		/* fde1 */
	undefined,		0,		/* fde2 */
	"ex	(sp),iy",	0,		/* fde3 */
	undefined,		0,		/* fde4 */
	"push	iy",		0,		/* fde5 */
	undefined,		0,		/* fde6 */
	undefined,		0,		/* fde7 */

	undefined,		0,		/* fde8 */
	"jp	(iy)",		0,		/* fde9 */
	undefined,		0,		/* fdea */
	undefined,		0,		/* fdeb */
	undefined,		0,		/* fdec */
	undefined,		0,		/* fded */
	undefined,		0,		/* fdee */
	undefined,		0,		/* fdef */
	
	undefined,		0,		/* fdf0 */
	undefined,		0,		/* fdf1 */
	undefined,		0,		/* fdf2 */
	undefined,		0,		/* fdf3 */
	undefined,		0,		/* fdf4 */
	undefined,		0,		/* fdf5 */
	undefined,		0,		/* fdf6 */
	undefined,		0,		/* fdf7 */

	undefined,		0,		/* fdf8 */
	"ld	sp,iy",		0,		/* fdf9 */
	undefined,		0,		/* fdfa */
	undefined,		0,		/* fdfb */
	undefined,		0,		/* fdfc */
	undefined,		0,		/* fdfd */
	undefined,		0,		/* fdfe */
	undefined,		0,		/* fdff */

							/* dd cb */
	undefined,		0,		/* ddcb00 */
	undefined,		0,		/* ddcb01 */
	undefined,		0,		/* ddcb02 */
	undefined,		0,		/* ddcb03 */
	undefined,		0,		/* ddcb04 */
	undefined,		0,		/* ddcb05 */
	"rlc	(ix+%02xh)",	3,		/* ddcb06 */
	undefined,		0,		/* ddcb07 */
	
	undefined,		0,		/* ddcb08 */
	undefined,		0,		/* ddcb09 */
	undefined,		0,		/* ddcb0a */
	undefined,		0,		/* ddcb0b */
	undefined,		0,		/* ddcb0c */
	undefined,		0,		/* ddcb0d */
	"rrc	(ix+%02xh)",	3,		/* ddcb0e */
	undefined,		0,		/* ddcb0f */
	
	undefined,		0,		/* ddcb10 */
	undefined,		0,		/* ddcb11 */
	undefined,		0,		/* ddcb12 */
	undefined,		0,		/* ddcb13 */
	undefined,		0,		/* ddcb14 */
	undefined,		0,		/* ddcb15 */
	"rl	(ix+%02xh)",	3,		/* ddcb16 */
	undefined,		0,		/* ddcb17 */
	
	undefined,		0,		/* ddcb18 */
	undefined,		0,		/* ddcb19 */
	undefined,		0,		/* ddcb1a */
	undefined,		0,		/* ddcb1b */
	undefined,		0,		/* ddcb1c */
	undefined,		0,		/* ddcb1d */
	"rr	(ix+%02xh)",	3,		/* ddcb1e */
	undefined,		0,		/* ddcb1f */
	
	undefined,		0,		/* ddcb20 */
	undefined,		0,		/* ddcb21 */
	undefined,		0,		/* ddcb22 */
	undefined,		0,		/* ddcb23 */
	undefined,		0,		/* ddcb24 */
	undefined,		0,		/* ddcb25 */
	"sla	(ix+%02xh)",	3,		/* ddcb26 */
	undefined,		0,		/* ddcb27 */
	
	undefined,		0,		/* ddcb28 */
	undefined,		0,		/* ddcb29 */
	undefined,		0,		/* ddcb2a */
	undefined,		0,		/* ddcb2b */
	undefined,		0,		/* ddcb2c */
	undefined,		0,		/* ddcb2d */
	"sra	(ix+%02xh)",	3,		/* ddcb2e */
	undefined,		0,		/* ddcb2f */
	
	undefined,		0,		/* ddcb30 */
	undefined,		0,		/* ddcb31 */
	undefined,		0,		/* ddcb32 */
	undefined,		0,		/* ddcb33 */
	undefined,		0,		/* ddcb34 */
	undefined,		0,		/* ddcb35 */
	undefined,		0,		/* ddcb36 */
	undefined,		0,		/* ddcb37 */
	
	undefined,		0,		/* ddcb38 */
	undefined,		0,		/* ddcb39 */
	undefined,		0,		/* ddcb3a */
	undefined,		0,		/* ddcb3b */
	undefined,		0,		/* ddcb3c */
	undefined,		0,		/* ddcb3d */
	"srl	(ix+%02xh)",	3,		/* ddcb3e */
	undefined,		0,		/* ddcb3f */
	
	undefined,		0,		/* ddcb40 */
	undefined,		0,		/* ddcb41 */
	undefined,		0,		/* ddcb42 */
	undefined,		0,		/* ddcb43 */
	undefined,		0,		/* ddcb44 */
	undefined,		0,		/* ddcb45 */
	"bit	0,(ix+%02xh)",	3,		/* ddcb46 */
	undefined,		0,		/* ddcb47 */
	
	undefined,		0,		/* ddcb48 */
	undefined,		0,		/* ddcb49 */
	undefined,		0,		/* ddcb4a */
	undefined,		0,		/* ddcb4b */
	undefined,		0,		/* ddcb4c */
	undefined,		0,		/* ddcb4d */
	"bit	1,(ix+%02xh)",	3,		/* ddcb4e */
	undefined,		0,		/* ddcb4f */
	
	undefined,		0,		/* ddcb50 */
	undefined,		0,		/* ddcb51 */
	undefined,		0,		/* ddcb52 */
	undefined,		0,		/* ddcb53 */
	undefined,		0,		/* ddcb54 */
	undefined,		0,		/* ddcb55 */
	"bit	2,(ix+%02xh)",	3,		/* ddcb56 */
	undefined,		0,		/* ddcb57 */
	
	undefined,		0,		/* ddcb58 */
	undefined,		0,		/* ddcb59 */
	undefined,		0,		/* ddcb5a */
	undefined,		0,		/* ddcb5b */
	undefined,		0,		/* ddcb5c */
	undefined,		0,		/* ddcb5d */
	"bit	3,(ix+%02xh)",	3,		/* ddcb5e */
	undefined,		0,		/* ddcb5f */
	
	undefined,		0,		/* ddcb60 */
	undefined,		0,		/* ddcb61 */
	undefined,		0,		/* ddcb62 */
	undefined,		0,		/* ddcb63 */
	undefined,		0,		/* ddcb64 */
	undefined,		0,		/* ddcb65 */
	"bit	4,(ix+%02xh)",	3,		/* ddcb66 */
	undefined,		0,		/* ddcb67 */
	
	undefined,		0,		/* ddcb68 */
	undefined,		0,		/* ddcb69 */
	undefined,		0,		/* ddcb6a */
	undefined,		0,		/* ddcb6b */
	undefined,		0,		/* ddcb6c */
	undefined,		0,		/* ddcb6d */
	"bit	5,(ix+%02xh)",	3,		/* ddcb6e */
	undefined,		0,		/* ddcb6f */
	
	undefined,		0,		/* ddcb70 */
	undefined,		0,		/* ddcb71 */
	undefined,		0,		/* ddcb72 */
	undefined,		0,		/* ddcb73 */
	undefined,		0,		/* ddcb74 */
	undefined,		0,		/* ddcb75 */
	"bit	6,(ix+%02xh)",	3,		/* ddcb76 */
	undefined,		0,		/* ddcb77 */
	
	undefined,		0,		/* ddcb78 */
	undefined,		0,		/* ddcb79 */
	undefined,		0,		/* ddcb7a */
	undefined,		0,		/* ddcb7b */
	undefined,		0,		/* ddcb7c */
	undefined,		0,		/* ddcb7d */
	"bit	7,(ix+%02xh)",	3,		/* ddcb7e */
	undefined,		0,		/* ddcb7f */
	
	undefined,		0,		/* ddcb80 */
	undefined,		0,		/* ddcb81 */
	undefined,		0,		/* ddcb82 */
	undefined,		0,		/* ddcb83 */
	undefined,		0,		/* ddcb84 */
	undefined,		0,		/* ddcb85 */
	"res	0,(ix+%02xh)",	3,		/* ddcb86 */
	undefined,		0,		/* ddcb87 */
	
	undefined,		0,		/* ddcb88 */
	undefined,		0,		/* ddcb89 */
	undefined,		0,		/* ddcb8a */
	undefined,		0,		/* ddcb8b */
	undefined,		0,		/* ddcb8c */
	undefined,		0,		/* ddcb8d */
	"res	1,(ix+%02xh)",	3,		/* ddcb8e */
	undefined,		0,		/* ddcb8f */
	
	undefined,		0,		/* ddcb90 */
	undefined,		0,		/* ddcb91 */
	undefined,		0,		/* ddcb92 */
	undefined,		0,		/* ddcb93 */
	undefined,		0,		/* ddcb94 */
	undefined,		0,		/* ddcb95 */
	"res	2,(ix+%02xh)",	3,		/* ddcb96 */
	undefined,		0,		/* ddcb97 */
	
	undefined,		0,		/* ddcb98 */
	undefined,		0,		/* ddcb99 */
	undefined,		0,		/* ddcb9a */
	undefined,		0,		/* ddcb9b */
	undefined,		0,		/* ddcb9c */
	undefined,		0,		/* ddcb9d */
	"res	3,(ix+%02xh)",	3,		/* ddcb9e */
	undefined,		0,		/* ddcb9f */
	
	undefined,		0,		/* ddcba0 */
	undefined,		0,		/* ddcba1 */
	undefined,		0,		/* ddcba2 */
	undefined,		0,		/* ddcba3 */
	undefined,		0,		/* ddcba4 */
	undefined,		0,		/* ddcba5 */
	"res	4,(ix+%02xh)",	3,		/* ddcba6 */
	undefined,		0,		/* ddcba7 */
	
	undefined,		0,		/* ddcba8 */
	undefined,		0,		/* ddcba9 */
	undefined,		0,		/* ddcbaa */
	undefined,		0,		/* ddcbab */
	undefined,		0,		/* ddcbac */
	undefined,		0,		/* ddcbad */
	"res	5,(ix+%02xh)",	3,		/* ddcbae */
	undefined,		0,		/* ddcbaf */
	
	undefined,		0,		/* ddcbb0 */
	undefined,		0,		/* ddcbb1 */
	undefined,		0,		/* ddcbb2 */
	undefined,		0,		/* ddcbb3 */
	undefined,		0,		/* ddcbb4 */
	undefined,		0,		/* ddcbb5 */
	"res	6,(ix+%02xh)",	3,		/* ddcbb6 */
	undefined,		0,		/* ddcbb7 */
	
	undefined,		0,		/* ddcbb8 */
	undefined,		0,		/* ddcbb9 */
	undefined,		0,		/* ddcbba */
	undefined,		0,		/* ddcbbb */
	undefined,		0,		/* ddcbbc */
	undefined,		0,		/* ddcbbd */
	"res	7,(ix+%02xh)",	3,		/* ddcbbe */
	undefined,		0,		/* ddcbbf */
	
	undefined,		0,		/* ddcbc0 */
	undefined,		0,		/* ddcbc1 */
	undefined,		0,		/* ddcbc2 */
	undefined,		0,		/* ddcbc3 */
	undefined,		0,		/* ddcbc4 */
	undefined,		0,		/* ddcbc5 */
	"set	0,(ix+%02xh)",	3,		/* ddcbc6 */
	undefined,		0,		/* ddcbc7 */
	
	undefined,		0,		/* ddcbc8 */
	undefined,		0,		/* ddcbc9 */
	undefined,		0,		/* ddcbca */
	undefined,		0,		/* ddcbcb */
	undefined,		0,		/* ddcbcc */
	undefined,		0,		/* ddcbcd */
	"set	1,(ix+%02xh)",	3,		/* ddcbce */
	undefined,		0,		/* ddcbcf */
	
	undefined,		0,		/* ddcbd0 */
	undefined,		0,		/* ddcbd1 */
	undefined,		0,		/* ddcbd2 */
	undefined,		0,		/* ddcbd3 */
	undefined,		0,		/* ddcbd4 */
	undefined,		0,		/* ddcbd5 */
	"set	2,(ix+%02xh)",	3,		/* ddcbd6 */
	undefined,		0,		/* ddcbd7 */
	
	undefined,		0,		/* ddcbd8 */
	undefined,		0,		/* ddcbd9 */
	undefined,		0,		/* ddcbda */
	undefined,		0,		/* ddcbdb */
	undefined,		0,		/* ddcbdc */
	undefined,		0,		/* ddcbdd */
	"set	3,(ix+%02xh)",	3,		/* ddcbde */
	undefined,		0,		/* ddcbdf */
	
	undefined,		0,		/* ddcbe0 */
	undefined,		0,		/* ddcbe1 */
	undefined,		0,		/* ddcbe2 */
	undefined,		0,		/* ddcbe3 */
	undefined,		0,		/* ddcbe4 */
	undefined,		0,		/* ddcbe5 */
	"set	4,(ix+%02xh)",	3,		/* ddcbe6 */
	undefined,		0,		/* ddcbe7 */
	
	undefined,		0,		/* ddcbe8 */
	undefined,		0,		/* ddcbe9 */
	undefined,		0,		/* ddcbea */
	undefined,		0,		/* ddcbeb */
	undefined,		0,		/* ddcbec */
	undefined,		0,		/* ddcbed */
	"set	5,(ix+%02xh)",	3,		/* ddcbee */
	undefined,		0,		/* ddcbef */
	
	undefined,		0,		/* ddcbf0 */
	undefined,		0,		/* ddcbf1 */
	undefined,		0,		/* ddcbf2 */
	undefined,		0,		/* ddcbf3 */
	undefined,		0,		/* ddcbf4 */
	undefined,		0,		/* ddcbf5 */
	"set	6,(ix+%02xh)",	3,		/* ddcbf6 */
	undefined,		0,		/* ddcbf7 */
	
	undefined,		0,		/* ddcbf8 */
	undefined,		0,		/* ddcbf9 */
	undefined,		0,		/* ddcbfa */
	undefined,		0,		/* ddcbfb */
	undefined,		0,		/* ddcbfc */
	undefined,		0,		/* ddcbfd */
	"set	7,(ix+%02xh)",	3,		/* ddcbfe */
	undefined,		0,		/* ddcbff */

							/* fd cb */
	undefined,		0,		/* fdcb00 */
	undefined,		0,		/* fdcb01 */
	undefined,		0,		/* fdcb02 */
	undefined,		0,		/* fdcb03 */
	undefined,		0,		/* fdcb04 */
	undefined,		0,		/* fdcb05 */
	"rlc	(iy+%02xh)",	3,		/* fdcb06 */
	undefined,		0,		/* fdcb07 */
	
	undefined,		0,		/* fdcb08 */
	undefined,		0,		/* fdcb09 */
	undefined,		0,		/* fdcb0a */
	undefined,		0,		/* fdcb0b */
	undefined,		0,		/* fdcb0c */
	undefined,		0,		/* fdcb0d */
	"rrc	(iy+%02xh)",	3,		/* fdcb0e */
	undefined,		0,		/* fdcb0f */
	
	undefined,		0,		/* fdcb10 */
	undefined,		0,		/* fdcb11 */
	undefined,		0,		/* fdcb12 */
	undefined,		0,		/* fdcb13 */
	undefined,		0,		/* fdcb14 */
	undefined,		0,		/* fdcb15 */
	"rl	(iy+%02xh)",	3,		/* fdcb16 */
	undefined,		0,		/* fdcb17 */
	
	undefined,		0,		/* fdcb18 */
	undefined,		0,		/* fdcb19 */
	undefined,		0,		/* fdcb1a */
	undefined,		0,		/* fdcb1b */
	undefined,		0,		/* fdcb1c */
	undefined,		0,		/* fdcb1d */
	"rr	(iy+%02xh)",	3,		/* fdcb1e */
	undefined,		0,		/* fdcb1f */
	
	undefined,		0,		/* fdcb20 */
	undefined,		0,		/* fdcb21 */
	undefined,		0,		/* fdcb22 */
	undefined,		0,		/* fdcb23 */
	undefined,		0,		/* fdcb24 */
	undefined,		0,		/* fdcb25 */
	"sla	(iy+%02xh)",	3,		/* fdcb26 */
	undefined,		0,		/* fdcb27 */
	
	undefined,		0,		/* fdcb28 */
	undefined,		0,		/* fdcb29 */
	undefined,		0,		/* fdcb2a */
	undefined,		0,		/* fdcb2b */
	undefined,		0,		/* fdcb2c */
	undefined,		0,		/* fdcb2d */
	"sra	(iy+%02xh)",	3,		/* fdcb2e */
	undefined,		0,		/* fdcb2f */
	
	undefined,		0,		/* fdcb30 */
	undefined,		0,		/* fdcb31 */
	undefined,		0,		/* fdcb32 */
	undefined,		0,		/* fdcb33 */
	undefined,		0,		/* fdcb34 */
	undefined,		0,		/* fdcb35 */
	undefined,		0,		/* fdcb36 */
	undefined,		0,		/* fdcb37 */
	
	undefined,		0,		/* fdcb38 */
	undefined,		0,		/* fdcb39 */
	undefined,		0,		/* fdcb3a */
	undefined,		0,		/* fdcb3b */
	undefined,		0,		/* fdcb3c */
	undefined,		0,		/* fdcb3d */
	"srl	(iy+%02xh)",	3,		/* fdcb3e */
	undefined,		0,		/* fdcb3f */
	
	undefined,		0,		/* fdcb40 */
	undefined,		0,		/* fdcb41 */
	undefined,		0,		/* fdcb42 */
	undefined,		0,		/* fdcb43 */
	undefined,		0,		/* fdcb44 */
	undefined,		0,		/* fdcb45 */
	"bit	0,(iy+%02xh)",	3,		/* fdcb46 */
	undefined,		0,		/* fdcb47 */
	
	undefined,		0,		/* fdcb48 */
	undefined,		0,		/* fdcb49 */
	undefined,		0,		/* fdcb4a */
	undefined,		0,		/* fdcb4b */
	undefined,		0,		/* fdcb4c */
	undefined,		0,		/* fdcb4d */
	"bit	1,(iy+%02xh)",	3,		/* fdcb4e */
	undefined,		0,		/* fdcb4f */
	
	undefined,		0,		/* fdcb50 */
	undefined,		0,		/* fdcb51 */
	undefined,		0,		/* fdcb52 */
	undefined,		0,		/* fdcb53 */
	undefined,		0,		/* fdcb54 */
	undefined,		0,		/* fdcb55 */
	"bit	2,(iy+%02xh)",	3,		/* fdcb56 */
	undefined,		0,		/* fdcb57 */
	
	undefined,		0,		/* fdcb58 */
	undefined,		0,		/* fdcb59 */
	undefined,		0,		/* fdcb5a */
	undefined,		0,		/* fdcb5b */
	undefined,		0,		/* fdcb5c */
	undefined,		0,		/* fdcb5d */
	"bit	3,(iy+%02xh)",	3,		/* fdcb5e */
	undefined,		0,		/* fdcb5f */
	
	undefined,		0,		/* fdcb60 */
	undefined,		0,		/* fdcb61 */
	undefined,		0,		/* fdcb62 */
	undefined,		0,		/* fdcb63 */
	undefined,		0,		/* fdcb64 */
	undefined,		0,		/* fdcb65 */
	"bit	4,(iy+%02xh)",	3,		/* fdcb66 */
	undefined,		0,		/* fdcb67 */
	
	undefined,		0,		/* fdcb68 */
	undefined,		0,		/* fdcb69 */
	undefined,		0,		/* fdcb6a */
	undefined,		0,		/* fdcb6b */
	undefined,		0,		/* fdcb6c */
	undefined,		0,		/* fdcb6d */
	"bit	5,(iy+%02xh)",	3,		/* fdcb6e */
	undefined,		0,		/* fdcb6f */
	
	undefined,		0,		/* fdcb70 */
	undefined,		0,		/* fdcb71 */
	undefined,		0,		/* fdcb72 */
	undefined,		0,		/* fdcb73 */
	undefined,		0,		/* fdcb74 */
	undefined,		0,		/* fdcb75 */
	"bit	6,(iy+%02xh)",	3,		/* fdcb76 */
	undefined,		0,		/* fdcb77 */
	
	undefined,		0,		/* fdcb78 */
	undefined,		0,		/* fdcb79 */
	undefined,		0,		/* fdcb7a */
	undefined,		0,		/* fdcb7b */
	undefined,		0,		/* fdcb7c */
	undefined,		0,		/* fdcb7d */
	"bit	7,(iy+%02xh)",	3,		/* fdcb7e */
	undefined,		0,		/* fdcb7f */
	
	undefined,		0,		/* fdcb80 */
	undefined,		0,		/* fdcb81 */
	undefined,		0,		/* fdcb82 */
	undefined,		0,		/* fdcb83 */
	undefined,		0,		/* fdcb84 */
	undefined,		0,		/* fdcb85 */
	"res	0,(iy+%02xh)",	3,		/* fdcb86 */
	undefined,		0,		/* fdcb87 */
	
	undefined,		0,		/* fdcb88 */
	undefined,		0,		/* fdcb89 */
	undefined,		0,		/* fdcb8a */
	undefined,		0,		/* fdcb8b */
	undefined,		0,		/* fdcb8c */
	undefined,		0,		/* fdcb8d */
	"res	1,(iy+%02xh)",	3,		/* fdcb8e */
	undefined,		0,		/* fdcb8f */
	
	undefined,		0,		/* fdcb90 */
	undefined,		0,		/* fdcb91 */
	undefined,		0,		/* fdcb92 */
	undefined,		0,		/* fdcb93 */
	undefined,		0,		/* fdcb94 */
	undefined,		0,		/* fdcb95 */
	"res	2,(iy+%02xh)",	3,		/* fdcb96 */
	undefined,		0,		/* fdcb97 */
	
	undefined,		0,		/* fdcb98 */
	undefined,		0,		/* fdcb99 */
	undefined,		0,		/* fdcb9a */
	undefined,		0,		/* fdcb9b */
	undefined,		0,		/* fdcb9c */
	undefined,		0,		/* fdcb9d */
	"res	3,(iy+%02xh)",	3,		/* fdcb9e */
	undefined,		0,		/* fdcb9f */
	
	undefined,		0,		/* fdcba0 */
	undefined,		0,		/* fdcba1 */
	undefined,		0,		/* fdcba2 */
	undefined,		0,		/* fdcba3 */
	undefined,		0,		/* fdcba4 */
	undefined,		0,		/* fdcba5 */
	"res	4,(iy+%02xh)",	3,		/* fdcba6 */
	undefined,		0,		/* fdcba7 */
	
	undefined,		0,		/* fdcba8 */
	undefined,		0,		/* fdcba9 */
	undefined,		0,		/* fdcbaa */
	undefined,		0,		/* fdcbab */
	undefined,		0,		/* fdcbac */
	undefined,		0,		/* fdcbad */
	"res	5,(iy+%02xh)",	3,		/* fdcbae */
	undefined,		0,		/* fdcbaf */
	
	undefined,		0,		/* fdcbb0 */
	undefined,		0,		/* fdcbb1 */
	undefined,		0,		/* fdcbb2 */
	undefined,		0,		/* fdcbb3 */
	undefined,		0,		/* fdcbb4 */
	undefined,		0,		/* fdcbb5 */
	"res	6,(iy+%02xh)",	3,		/* fdcbb6 */
	undefined,		0,		/* fdcbb7 */
	
	undefined,		0,		/* fdcbb8 */
	undefined,		0,		/* fdcbb9 */
	undefined,		0,		/* fdcbba */
	undefined,		0,		/* fdcbbb */
	undefined,		0,		/* fdcbbc */
	undefined,		0,		/* fdcbbd */
	"res	7,(iy+%02xh)",	3,		/* fdcbbe */
	undefined,		0,		/* fdcbbf */
	
	undefined,		0,		/* fdcbc0 */
	undefined,		0,		/* fdcbc1 */
	undefined,		0,		/* fdcbc2 */
	undefined,		0,		/* fdcbc3 */
	undefined,		0,		/* fdcbc4 */
	undefined,		0,		/* fdcbc5 */
	"set	0,(iy+%02xh)",	3,		/* fdcbc6 */
	undefined,		0,		/* fdcbc7 */
	
	undefined,		0,		/* fdcbc8 */
	undefined,		0,		/* fdcbc9 */
	undefined,		0,		/* fdcbca */
	undefined,		0,		/* fdcbcb */
	undefined,		0,		/* fdcbcc */
	undefined,		0,		/* fdcbcd */
	"set	1,(iy+%02xh)",	3,		/* fdcbce */
	undefined,		0,		/* fdcbcf */
	
	undefined,		0,		/* fdcbd0 */
	undefined,		0,		/* fdcbd1 */
	undefined,		0,		/* fdcbd2 */
	undefined,		0,		/* fdcbd3 */
	undefined,		0,		/* fdcbd4 */
	undefined,		0,		/* fdcbd5 */
	"set	2,(iy+%02xh)",	3,		/* fdcbd6 */
	undefined,		0,		/* fdcbd7 */
	
	undefined,		0,		/* fdcbd8 */
	undefined,		0,		/* fdcbd9 */
	undefined,		0,		/* fdcbda */
	undefined,		0,		/* fdcbdb */
	undefined,		0,		/* fdcbdc */
	undefined,		0,		/* fdcbdd */
	"set	3,(iy+%02xh)",	3,		/* fdcbde */
	undefined,		0,		/* fdcbdf */
	
	undefined,		0,		/* fdcbe0 */
	undefined,		0,		/* fdcbe1 */
	undefined,		0,		/* fdcbe2 */
	undefined,		0,		/* fdcbe3 */
	undefined,		0,		/* fdcbe4 */
	undefined,		0,		/* fdcbe5 */
	"set	4,(iy+%02xh)",	3,		/* fdcbe6 */
	undefined,		0,		/* fdcbe7 */
	
	undefined,		0,		/* fdcbe8 */
	undefined,		0,		/* fdcbe9 */
	undefined,		0,		/* fdcbea */
	undefined,		0,		/* fdcbeb */
	undefined,		0,		/* fdcbec */
	undefined,		0,		/* fdcbed */
	"set	5,(iy+%02xh)",	3,		/* fdcbee */
	undefined,		0,		/* fdcbef */
	
	undefined,		0,		/* fdcbf0 */
	undefined,		0,		/* fdcbf1 */
	undefined,		0,		/* fdcbf2 */
	undefined,		0,		/* fdcbf3 */
	undefined,		0,		/* fdcbf4 */
	undefined,		0,		/* fdcbf5 */
	"set	6,(iy+%02xh)",	3,		/* fdcbf6 */
	undefined,		0,		/* fdcbf7 */
	
	undefined,		0,		/* fdcbf8 */
	undefined,		0,		/* fdcbf9 */
	undefined,		0,		/* fdcbfa */
	undefined,		0,		/* fdcbfb */
	undefined,		0,		/* fdcbfc */
	undefined,		0,		/* fdcbfd */
	"set	7,(iy+%02xh)",	3,		/* fdcbfe */
	undefined,		0,		/* fdcbff */
};

int disassemble(pc)
    unsigned short pc;
{
    int	i, j;
    struct opcode	*code;
    int	addr;
    
    addr = pc;
    i = mem_read(pc++);
    if (!major[i].name)
    {
	j = major[i].args;
	i = mem_read(pc++);
	if (!minor[j][i].name)
	{
	    /* dd cb or fd cb; offset comes *before* instruction */
	    j = minor[j][i].args;
            pc++; /* skip over offset */
	    i = mem_read(pc++);
	}
	code = &minor[j][i];
    }
    else
    {
	code = &major[i];
    }
    printf ("%04x\t", addr);
    switch (code->args) {
      case 2:
	printf (code->name, mem_read(pc + 1), mem_read(pc));
	pc += 2;
	break;
      case 1:
	printf (code->name, mem_read(pc));
	pc += 1;
	break;
      case 3: /* 1 arg before instruction */
	printf (code->name, mem_read(pc - 2));
        break;
      case 0:
	printf (code->name);
	break;
      case -1: /* relative addressing */
	printf (code->name, (pc + 1 + (char) mem_read(pc)) & 0xffff);
	pc += 1;
	break;
    } 
    putchar ('\n');
    return pc;  /* return the location of the next instruction */
}



