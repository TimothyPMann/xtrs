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
   Last modified on Tue May  1 20:32:35 PDT 2001 by mann
*/

/*#define KBDEBUG 1*/
/*#define JOYDEBUG 1*/
#define KP_JOYSTICK 1         /* emulate joystick with keypad */
/*#define KPNUM_JOYSTICK 1*/  /* emulate joystick with keypad + NumLock */
#define SHIFT_F1_IS_F13 1     /* use if X reports Shift+F1..F8 as F13..F20 */
/*#define SHIFT_F1_IS_F11 1*/ /* use if X reports Shift+F1..F10 as F11..F20 */

#include "z80.h"
#include "trs.h"
#include <X11/keysym.h>
#include <X11/X.h>

#define TK(a, b) (((a)<<4)+(b))
#define TK_ADDR(tk) (((tk) >> 4)&0xf)
#define TK_DATA(tk) ((tk)&0xf)
#define TK_DOWN(tk) (((tk)&0x10000) == 0)

/* TRS-80 key matrix */
#define TK_AtSign       TK(0, 0)  /* @   */
#define TK_A            TK(0, 1)
#define TK_B            TK(0, 2)
#define TK_C            TK(0, 3)
#define TK_D            TK(0, 4)
#define TK_E            TK(0, 5)
#define TK_F            TK(0, 6)
#define TK_G            TK(0, 7)
#define TK_H            TK(1, 0)
#define TK_I            TK(1, 1)
#define TK_J            TK(1, 2)
#define TK_K            TK(1, 3)
#define TK_L            TK(1, 4)
#define TK_M            TK(1, 5)
#define TK_N            TK(1, 6)
#define TK_O            TK(1, 7)
#define TK_P            TK(2, 0)
#define TK_Q            TK(2, 1)
#define TK_R            TK(2, 2)
#define TK_S            TK(2, 3)
#define TK_T            TK(2, 4)
#define TK_U            TK(2, 5)
#define TK_V            TK(2, 6)
#define TK_W            TK(2, 7)
#define TK_X            TK(3, 0)
#define TK_Y            TK(3, 1)
#define TK_Z            TK(3, 2)
#define TK_LeftBracket  TK(3, 3)  /* [ { */ /* not really on keyboard */
#define TK_Backslash    TK(3, 4)  /* \ | */ /* not really on keyboard */
#define TK_RightBracket TK(3, 5)  /* ] } */ /* not really on keyboard */
#define TK_Caret        TK(3, 6)  /* ^ ~ */ /* not really on keyboard */
#define TK_Underscore   TK(3, 7)  /* _   */ /* not really on keyboard */
#define TK_0            TK(4, 0)  /* 0   */
#define TK_1            TK(4, 1)  /* 1 ! */
#define TK_2            TK(4, 2)  /* 2 " */
#define TK_3            TK(4, 3)  /* 3 # */
#define TK_4            TK(4, 4)  /* 4 $ */
#define TK_5            TK(4, 5)  /* 5 % */
#define TK_6            TK(4, 6)  /* 6 & */
#define TK_7            TK(4, 7)  /* 7 ' */
#define TK_8            TK(5, 0)  /* 8 ( */
#define TK_9            TK(5, 1)  /* 9 ) */
#define TK_Colon        TK(5, 2)  /* : * */
#define TK_Semicolon    TK(5, 3)  /* ; + */
#define TK_Comma        TK(5, 4)  /* , < */
#define TK_Minus        TK(5, 5)  /* - = */
#define TK_Period       TK(5, 6)  /* . > */
#define TK_Slash        TK(5, 7)  /* / ? */
#define TK_Enter        TK(6, 0)
#define TK_Clear        TK(6, 1)
#define TK_Break        TK(6, 2)
#define TK_Up           TK(6, 3)
#define TK_Down         TK(6, 4)
#define TK_Left         TK(6, 5)
#define TK_Right        TK(6, 6)
#define TK_Space        TK(6, 7)
#define TK_LeftShift    TK(7, 0)
#define TK_RightShift   TK(7, 1)  /* M3/4 only; both shifts are 7, 0 on M1 */
#define TK_Ctrl         TK(7, 2)  /* M4 only */
#define TK_CapsLock     TK(7, 3)  /* M4 only */
#define TK_F1           TK(7, 4)  /* M4 only */
#define TK_F2           TK(7, 5)  /* M4 only */
#define TK_F3           TK(7, 6)  /* M4 only */
#define TK_Unused       TK(7, 7)

/* Fake keycodes with special meanings */
#define TK_NULL                 TK(8, 0)
#define TK_Neutral              TK(8, 1)
#define TK_ForceShift           TK(8, 2)
#define TK_ForceNoShift         TK(8, 3)
#define TK_ForceShiftPersistent TK(8, 4)
#define TK_AllKeysUp            TK(8, 5)
#define TK_Joystick             TK(10,  0)
#define TK_North                TK(10,  1)
#define TK_Northeast            TK(10,  9)
#define TK_East                 TK(10,  8)
#define TK_Southeast            TK(10, 10)
#define TK_South                TK(10,  2)
#define TK_Southwest            TK(10,  5)
#define TK_West                 TK(10,  4)
#define TK_Northwest            TK(10,  6)
#define TK_Fire                 TK(10, 16)

typedef struct
{
    int bit_action;
    int shift_action;
} KeyTable;

/* Keysyms in the extended ASCII range 0x0000 - 0x00ff */

KeyTable ascii_key_table[] = {
/* 0x0 */     { TK_NULL, TK_Neutral }, /* undefined keysyms... */
/* 0x1 */     { TK_NULL, TK_Neutral },
/* 0x2 */     { TK_NULL, TK_Neutral },
/* 0x3 */     { TK_NULL, TK_Neutral },
/* 0x4 */     { TK_NULL, TK_Neutral },
/* 0x5 */     { TK_NULL, TK_Neutral },
/* 0x6 */     { TK_NULL, TK_Neutral },
/* 0x7 */     { TK_NULL, TK_Neutral },
/* 0x8 */     { TK_NULL, TK_Neutral },
/* 0x9 */     { TK_NULL, TK_Neutral },
/* 0xa */     { TK_NULL, TK_Neutral },
/* 0xb */     { TK_NULL, TK_Neutral },
/* 0xc */     { TK_NULL, TK_Neutral },
/* 0xd */     { TK_NULL, TK_Neutral },
/* 0xe */     { TK_NULL, TK_Neutral },
/* 0xf */     { TK_NULL, TK_Neutral },
/* 0x10 */    { TK_NULL, TK_Neutral },
/* 0x11 */    { TK_NULL, TK_Neutral },
/* 0x12 */    { TK_NULL, TK_Neutral },
/* 0x13 */    { TK_NULL, TK_Neutral },
/* 0x14 */    { TK_NULL, TK_Neutral },
/* 0x15 */    { TK_NULL, TK_Neutral },
/* 0x16 */    { TK_NULL, TK_Neutral },
/* 0x17 */    { TK_NULL, TK_Neutral },
/* 0x18 */    { TK_NULL, TK_Neutral },
/* 0x19 */    { TK_NULL, TK_Neutral },
/* 0x1a */    { TK_NULL, TK_Neutral },
/* 0x1b */    { TK_NULL, TK_Neutral },
/* 0x1c */    { TK_NULL, TK_Neutral },
/* 0x1d */    { TK_NULL, TK_Neutral },
/* 0x1e */    { TK_NULL, TK_Neutral },
/* 0x1f */    { TK_NULL, TK_Neutral },  /* ...end undefined keysyms */
/* 0x20 */    { TK_Space, TK_Neutral },
/* 0x21 */    { TK_1, TK_ForceShift },
/* 0x22 */    { TK_2, TK_ForceShift },
/* 0x23 */    { TK_3, TK_ForceShift },
/* 0x24 */    { TK_4, TK_ForceShift },
/* 0x25 */    { TK_5, TK_ForceShift },
/* 0x26 */    { TK_6, TK_ForceShift },
/* 0x27 */    { TK_7, TK_ForceShift },
/* 0x28 */    { TK_8, TK_ForceShift },
/* 0x29 */    { TK_9, TK_ForceShift },
/* 0x2a */    { TK_Colon, TK_ForceShift },
/* 0x2b */    { TK_Semicolon, TK_ForceShift },
/* 0x2c */    { TK_Comma, TK_ForceNoShift },
/* 0x2d */    { TK_Minus, TK_ForceNoShift },
/* 0x2e */    { TK_Period, TK_ForceNoShift },
/* 0x2f */    { TK_Slash, TK_ForceNoShift },
/* 0x30 */    { TK_0, TK_ForceNoShift },
/* 0x31 */    { TK_1, TK_ForceNoShift },
/* 0x32 */    { TK_2, TK_ForceNoShift },
/* 0x33 */    { TK_3, TK_ForceNoShift },
/* 0x34 */    { TK_4, TK_ForceNoShift },
/* 0x35 */    { TK_5, TK_ForceNoShift },
/* 0x36 */    { TK_6, TK_ForceNoShift },
/* 0x37 */    { TK_7, TK_ForceNoShift },
/* 0x38 */    { TK_8, TK_ForceNoShift },
/* 0x39 */    { TK_9, TK_ForceNoShift },
/* 0x3a */    { TK_Colon, TK_ForceNoShift },
/* 0x3b */    { TK_Semicolon, TK_ForceNoShift },
/* 0x3c */    { TK_Comma, TK_ForceShift },
/* 0x3d */    { TK_Minus, TK_ForceShift },
/* 0x3e */    { TK_Period, TK_ForceShift },
/* 0x3f */    { TK_Slash, TK_ForceShift },
/* 0x40 */    { TK_AtSign, TK_ForceNoShift },
/* 0x41 */    { TK_A,  TK_ForceShift },
/* 0x42 */    { TK_B,  TK_ForceShift },
/* 0x43 */    { TK_C,  TK_ForceShift },
/* 0x44 */    { TK_D,  TK_ForceShift },
/* 0x45 */    { TK_E,  TK_ForceShift },
/* 0x46 */    { TK_F,  TK_ForceShift },
/* 0x47 */    { TK_G,  TK_ForceShift },
/* 0x48 */    { TK_H,  TK_ForceShift },
/* 0x49 */    { TK_I,  TK_ForceShift },
/* 0x4a */    { TK_J,  TK_ForceShift },
/* 0x4b */    { TK_K,  TK_ForceShift },
/* 0x4c */    { TK_L,  TK_ForceShift },
/* 0x4d */    { TK_M,  TK_ForceShift },
/* 0x4e */    { TK_N,  TK_ForceShift },
/* 0x4f */    { TK_O,  TK_ForceShift },
/* 0x50 */    { TK_P,  TK_ForceShift },
/* 0x51 */    { TK_Q,  TK_ForceShift },
/* 0x52 */    { TK_R,  TK_ForceShift },
/* 0x53 */    { TK_S,  TK_ForceShift },
/* 0x54 */    { TK_T,  TK_ForceShift },
/* 0x55 */    { TK_U,  TK_ForceShift },
/* 0x56 */    { TK_V,  TK_ForceShift },
/* 0x57 */    { TK_W,  TK_ForceShift },
/* 0x58 */    { TK_X,  TK_ForceShift },
/* 0x59 */    { TK_Y,  TK_ForceShift },
/* 0x5a */    { TK_Z,  TK_ForceShift },
/* 0x5b */    { TK_LeftBracket, TK_ForceNoShift },
/* 0x5c */    { TK_Backslash, TK_ForceNoShift },
/* 0x5d */    { TK_RightBracket, TK_ForceNoShift },
/* 0x5e */    { TK_Caret, TK_ForceNoShift },
/* 0x5f */    { TK_Underscore, TK_ForceNoShift },
/* 0x60 */    { TK_AtSign,  TK_ForceShift },
/* 0x61 */    { TK_A, TK_ForceNoShift },
/* 0x62 */    { TK_B, TK_ForceNoShift },
/* 0x63 */    { TK_C, TK_ForceNoShift },
/* 0x64 */    { TK_D, TK_ForceNoShift },
/* 0x65 */    { TK_E, TK_ForceNoShift },
/* 0x66 */    { TK_F, TK_ForceNoShift },
/* 0x67 */    { TK_G, TK_ForceNoShift },
/* 0x68 */    { TK_H, TK_ForceNoShift },
/* 0x69 */    { TK_I, TK_ForceNoShift },
/* 0x6a */    { TK_J, TK_ForceNoShift },
/* 0x6b */    { TK_K, TK_ForceNoShift },
/* 0x6c */    { TK_L, TK_ForceNoShift },
/* 0x6d */    { TK_M, TK_ForceNoShift },
/* 0x6e */    { TK_N, TK_ForceNoShift },
/* 0x6f */    { TK_O, TK_ForceNoShift },
/* 0x70 */    { TK_P, TK_ForceNoShift },
/* 0x71 */    { TK_Q, TK_ForceNoShift },
/* 0x72 */    { TK_R, TK_ForceNoShift },
/* 0x73 */    { TK_S, TK_ForceNoShift },
/* 0x74 */    { TK_T, TK_ForceNoShift },
/* 0x75 */    { TK_U, TK_ForceNoShift },
/* 0x76 */    { TK_V, TK_ForceNoShift },
/* 0x77 */    { TK_W, TK_ForceNoShift },
/* 0x78 */    { TK_X, TK_ForceNoShift },
/* 0x79 */    { TK_Y, TK_ForceNoShift },
/* 0x7a */    { TK_Z, TK_ForceNoShift },
/* 0x7b */    { TK_LeftBracket, TK_ForceShift },
/* 0x7c */    { TK_Backslash, TK_ForceShift },
/* 0x7d */    { TK_RightBracket, TK_ForceShift },
/* 0x7e */    { TK_Caret, TK_ForceShift },
/* 0x7f */    { TK_NULL, TK_Neutral },
/* 0x80 */    { TK_NULL, TK_Neutral },
/* 0x81 */    { TK_NULL, TK_Neutral },
/* 0x82 */    { TK_NULL, TK_Neutral },
/* 0x83 */    { TK_NULL, TK_Neutral },
/* 0x84 */    { TK_NULL, TK_Neutral },
/* 0x85 */    { TK_NULL, TK_Neutral },
/* 0x86 */    { TK_NULL, TK_Neutral },
/* 0x87 */    { TK_NULL, TK_Neutral },
/* 0x88 */    { TK_NULL, TK_Neutral },
/* 0x89 */    { TK_NULL, TK_Neutral },
/* 0x8a */    { TK_NULL, TK_Neutral },
/* 0x8b */    { TK_NULL, TK_Neutral },
/* 0x8c */    { TK_NULL, TK_Neutral },
/* 0x8d */    { TK_NULL, TK_Neutral },
/* 0x8e */    { TK_NULL, TK_Neutral },
/* 0x8f */    { TK_NULL, TK_Neutral },
/* 0x90 */    { TK_NULL, TK_Neutral },
/* 0x91 */    { TK_NULL, TK_Neutral },
/* 0x92 */    { TK_NULL, TK_Neutral },
/* 0x93 */    { TK_NULL, TK_Neutral },
/* 0x94 */    { TK_NULL, TK_Neutral },
/* 0x95 */    { TK_NULL, TK_Neutral },
/* 0x96 */    { TK_NULL, TK_Neutral },
/* 0x97 */    { TK_NULL, TK_Neutral },
/* 0x98 */    { TK_NULL, TK_Neutral },
/* 0x99 */    { TK_NULL, TK_Neutral },
/* 0x9a */    { TK_NULL, TK_Neutral },
/* 0x9b */    { TK_NULL, TK_Neutral },
/* 0x9c */    { TK_NULL, TK_Neutral },
/* 0x9d */    { TK_NULL, TK_Neutral },
/* 0x9e */    { TK_NULL, TK_Neutral },
/* 0x9f */    { TK_NULL, TK_Neutral },
/* 0xa0 */    { TK_NULL, TK_Neutral },
/* 0xa1 */    { TK_NULL, TK_Neutral },
/* 0xa2 */    { TK_NULL, TK_Neutral },
/* 0xa3 */    { TK_NULL, TK_Neutral },
/* 0xa4 */    { TK_NULL, TK_Neutral },
/* 0xa5 */    { TK_NULL, TK_Neutral },
/* 0xa6 */    { TK_NULL, TK_Neutral },
/* 0xa7 */    { TK_NULL, TK_Neutral },
/* 0xa8 */    { TK_NULL, TK_Neutral },
/* 0xa9 */    { TK_NULL, TK_Neutral },
/* 0xaa */    { TK_NULL, TK_Neutral },
/* 0xab */    { TK_NULL, TK_Neutral },
/* 0xac */    { TK_NULL, TK_Neutral },
/* 0xad */    { TK_NULL, TK_Neutral },
/* 0xae */    { TK_NULL, TK_Neutral },
/* 0xaf */    { TK_NULL, TK_Neutral },
/* 0xb0 */    { TK_NULL, TK_Neutral },
/* 0xb1 */    { TK_NULL, TK_Neutral },
/* 0xb2 */    { TK_NULL, TK_Neutral },
/* 0xb3 */    { TK_NULL, TK_Neutral },
/* 0xb4 */    { TK_NULL, TK_Neutral },
/* 0xb5 */    { TK_NULL, TK_Neutral },
/* 0xb6 */    { TK_NULL, TK_Neutral },
/* 0xb7 */    { TK_NULL, TK_Neutral },
/* 0xb8 */    { TK_NULL, TK_Neutral },
/* 0xb9 */    { TK_NULL, TK_Neutral },
/* 0xba */    { TK_NULL, TK_Neutral },
/* 0xbb */    { TK_NULL, TK_Neutral },
/* 0xbc */    { TK_NULL, TK_Neutral },
/* 0xbd */    { TK_NULL, TK_Neutral },
/* 0xbe */    { TK_NULL, TK_Neutral },
/* 0xbf */    { TK_NULL, TK_Neutral },
/* 0xc0 */    { TK_NULL, TK_Neutral },
/* 0xc1 */    { TK_NULL, TK_Neutral },
/* 0xc2 */    { TK_NULL, TK_Neutral },
/* 0xc3 */    { TK_NULL, TK_Neutral },
/* 0xc4 */    { TK_LeftBracket, TK_ForceShift },    /* Ä */
/* 0xc5 */    { TK_NULL, TK_Neutral },
/* 0xc6 */    { TK_NULL, TK_Neutral },
/* 0xc7 */    { TK_NULL, TK_Neutral },
/* 0xc8 */    { TK_NULL, TK_Neutral },
/* 0xc9 */    { TK_NULL, TK_Neutral },
/* 0xca */    { TK_NULL, TK_Neutral },
/* 0xcb */    { TK_NULL, TK_Neutral },
/* 0xcc */    { TK_NULL, TK_Neutral },
/* 0xcd */    { TK_NULL, TK_Neutral },
/* 0xce */    { TK_NULL, TK_Neutral },
/* 0xcf */    { TK_NULL, TK_Neutral },
/* 0xd0 */    { TK_NULL, TK_Neutral },
/* 0xd1 */    { TK_NULL, TK_Neutral },
/* 0xd2 */    { TK_NULL, TK_Neutral },
/* 0xd3 */    { TK_NULL, TK_Neutral },
/* 0xd4 */    { TK_NULL, TK_Neutral },
/* 0xd5 */    { TK_NULL, TK_Neutral },
/* 0xd6 */    { TK_Backslash, TK_ForceShift },      /* Ö */
/* 0xd7 */    { TK_NULL, TK_Neutral },
/* 0xd8 */    { TK_NULL, TK_Neutral },
/* 0xd9 */    { TK_NULL, TK_Neutral },
/* 0xda */    { TK_NULL, TK_Neutral },
/* 0xdb */    { TK_NULL, TK_Neutral },
/* 0xdc */    { TK_RightBracket, TK_ForceShift },   /* Ü */
/* 0xdd */    { TK_NULL, TK_Neutral },
/* 0xde */    { TK_NULL, TK_Neutral },
/* 0xdf */    { TK_Caret, TK_ForceNoShift },        /* ß */
/* 0xe0 */    { TK_NULL, TK_Neutral },
/* 0xe1 */    { TK_NULL, TK_Neutral },
/* 0xe2 */    { TK_NULL, TK_Neutral },
/* 0xe3 */    { TK_NULL, TK_Neutral },
/* 0xe4 */    { TK_LeftBracket, TK_ForceNoShift },  /* ä */
/* 0xe5 */    { TK_NULL, TK_Neutral },
/* 0xe6 */    { TK_NULL, TK_Neutral },
/* 0xe7 */    { TK_NULL, TK_Neutral },
/* 0xe8 */    { TK_NULL, TK_Neutral },
/* 0xe9 */    { TK_NULL, TK_Neutral },
/* 0xea */    { TK_NULL, TK_Neutral },
/* 0xeb */    { TK_NULL, TK_Neutral },
/* 0xec */    { TK_NULL, TK_Neutral },
/* 0xed */    { TK_NULL, TK_Neutral },
/* 0xee */    { TK_NULL, TK_Neutral },
/* 0xef */    { TK_NULL, TK_Neutral },
/* 0xf0 */    { TK_NULL, TK_Neutral },
/* 0xf1 */    { TK_NULL, TK_Neutral },
/* 0xf2 */    { TK_NULL, TK_Neutral },
/* 0xf3 */    { TK_NULL, TK_Neutral },
/* 0xf4 */    { TK_NULL, TK_Neutral },
/* 0xf5 */    { TK_NULL, TK_Neutral },
/* 0xf6 */    { TK_Backslash, TK_ForceNoShift },    /* ö */
/* 0xf7 */    { TK_NULL, TK_Neutral },
/* 0xf8 */    { TK_NULL, TK_Neutral },
/* 0xf9 */    { TK_NULL, TK_Neutral },
/* 0xfa */    { TK_NULL, TK_Neutral },
/* 0xfb */    { TK_NULL, TK_Neutral },
/* 0xfc */    { TK_RightBracket, TK_ForceNoShift }, /* ü */
/* 0xfd */    { TK_NULL, TK_Neutral },
/* 0xfe */    { TK_NULL, TK_Neutral },
/* 0xff */    { TK_NULL, TK_Neutral }
};

/* Keysyms in the function key range 0xff00 - 0xffff */

KeyTable function_key_table[] = {
/* 0xff00                  */    { TK_NULL, TK_Neutral },
/* 0xff01                  */    { TK_NULL, TK_Neutral },
/* 0xff02                  */    { TK_NULL, TK_Neutral },
/* 0xff03                  */    { TK_NULL, TK_Neutral },
/* 0xff04                  */    { TK_NULL, TK_Neutral },
/* 0xff05                  */    { TK_NULL, TK_Neutral },
/* 0xff06                  */    { TK_NULL, TK_Neutral },
/* 0xff07                  */    { TK_NULL, TK_Neutral },
/* 0xff08   XK_BackSpace   */    { TK_Left, TK_Neutral },
/* 0xff09   XK_Tab         */    { TK_Right, TK_Neutral },
/* 0xff0a   XK_Linefeed    */    { TK_Down, TK_Neutral },
/* 0xff0b   XK_Clear       */    { TK_Clear, TK_Neutral },
/* 0xff0c                  */    { TK_NULL, TK_Neutral },
/* 0xff0d   XK_Return      */    { TK_Enter, TK_Neutral },
/* 0xff0e                  */    { TK_NULL, TK_Neutral },
/* 0xff0f                  */    { TK_NULL, TK_Neutral },
/* 0xff10                  */    { TK_NULL, TK_Neutral },
/* 0xff11                  */    { TK_NULL, TK_Neutral },
/* 0xff12                  */    { TK_NULL, TK_Neutral },
/* 0xff13   XK_Pause       */    { TK_NULL, TK_Neutral },
/* 0xff14   XK_ScrollLock  */    { TK_AtSign, TK_Neutral },
/* 0xff15   XK_Sys_Req     */    { TK_NULL, TK_Neutral },
/* 0xff16                  */    { TK_NULL, TK_Neutral },
/* 0xff17                  */    { TK_NULL, TK_Neutral },
/* 0xff18                  */    { TK_NULL, TK_Neutral },
/* 0xff19                  */    { TK_NULL, TK_Neutral },
/* 0xff1a                  */    { TK_NULL, TK_Neutral },
/* 0xff1b   XK_Escape      */    { TK_Break, TK_Neutral },
/* 0xff1c                  */    { TK_NULL, TK_Neutral },
/* 0xff1d                  */    { TK_NULL, TK_Neutral },
/* 0xff1e                  */    { TK_NULL, TK_Neutral },
/* 0xff1f                  */    { TK_NULL, TK_Neutral },
/* 0xff20   XK_Multi_key   */    { TK_AtSign, TK_Neutral },
/* 0xff21                  */    { TK_NULL, TK_Neutral },
/* 0xff22                  */    { TK_NULL, TK_Neutral },
/* 0xff23                  */    { TK_NULL, TK_Neutral },
/* 0xff24                  */    { TK_NULL, TK_Neutral },
/* 0xff25                  */    { TK_NULL, TK_Neutral },
/* 0xff26                  */    { TK_NULL, TK_Neutral },
/* 0xff27                  */    { TK_NULL, TK_Neutral },
/* 0xff28                  */    { TK_NULL, TK_Neutral },
/* 0xff29                  */    { TK_NULL, TK_Neutral },
/* 0xff2a                  */    { TK_NULL, TK_Neutral },
/* 0xff2b                  */    { TK_NULL, TK_Neutral },
/* 0xff2c                  */    { TK_NULL, TK_Neutral },
/* 0xff2d                  */    { TK_NULL, TK_Neutral },
/* 0xff2e                  */    { TK_NULL, TK_Neutral },
/* 0xff2f                  */    { TK_NULL, TK_Neutral },
/* 0xff30                  */    { TK_NULL, TK_Neutral },
/* 0xff31                  */    { TK_NULL, TK_Neutral },
/* 0xff32                  */    { TK_NULL, TK_Neutral },
/* 0xff33                  */    { TK_NULL, TK_Neutral },
/* 0xff34                  */    { TK_NULL, TK_Neutral },
/* 0xff35                  */    { TK_NULL, TK_Neutral },
/* 0xff36                  */    { TK_NULL, TK_Neutral },
/* 0xff37                  */    { TK_NULL, TK_Neutral },
/* 0xff38                  */    { TK_NULL, TK_Neutral },
/* 0xff39                  */    { TK_NULL, TK_Neutral },
/* 0xff3a                  */    { TK_NULL, TK_Neutral },
/* 0xff3b                  */    { TK_NULL, TK_Neutral },
/* 0xff3c                  */    { TK_NULL, TK_Neutral },
/* 0xff3d                  */    { TK_NULL, TK_Neutral },
/* 0xff3e                  */    { TK_NULL, TK_Neutral },
/* 0xff3f                  */    { TK_NULL, TK_Neutral },
/* 0xff40                  */    { TK_NULL, TK_Neutral },
/* 0xff41                  */    { TK_NULL, TK_Neutral },
/* 0xff42                  */    { TK_NULL, TK_Neutral },
/* 0xff43                  */    { TK_NULL, TK_Neutral },
/* 0xff44                  */    { TK_NULL, TK_Neutral },
/* 0xff45                  */    { TK_NULL, TK_Neutral },
/* 0xff46                  */    { TK_NULL, TK_Neutral },
/* 0xff47                  */    { TK_NULL, TK_Neutral },
/* 0xff48                  */    { TK_NULL, TK_Neutral },
/* 0xff49                  */    { TK_NULL, TK_Neutral },
/* 0xff4a                  */    { TK_NULL, TK_Neutral },
/* 0xff4b                  */    { TK_NULL, TK_Neutral },
/* 0xff4c                  */    { TK_NULL, TK_Neutral },
/* 0xff4d                  */    { TK_NULL, TK_Neutral },
/* 0xff4e                  */    { TK_NULL, TK_Neutral },
/* 0xff4f                  */    { TK_NULL, TK_Neutral },
/* 0xff50   XK_Home        */    { TK_Clear, TK_Neutral },
/* 0xff51   XK_Left        */    { TK_Left, TK_Neutral },
/* 0xff52   XK_Up          */    { TK_Up, TK_Neutral },
/* 0xff53   XK_Right       */    { TK_Right, TK_Neutral },
/* 0xff54   XK_Down        */    { TK_Down, TK_Neutral },
/* 0xff55   XK_Prior, XK_Page_Up  */  { TK_LeftShift, TK_Neutral },
/* 0xff56   XK_Next, XK_Page_Down */  { TK_RightShift, TK_Neutral },
/* 0xff57   XK_End         */    { TK_Unused, TK_Neutral },
/* 0xff58   XK_Begin       */    { TK_NULL, TK_Neutral },
/* 0xff59                  */    { TK_NULL, TK_Neutral },
/* 0xff5a                  */    { TK_NULL, TK_Neutral },
/* 0xff5b                  */    { TK_NULL, TK_Neutral },
/* 0xff5c                  */    { TK_NULL, TK_Neutral },
/* 0xff5d                  */    { TK_NULL, TK_Neutral },
/* 0xff5e                  */    { TK_NULL, TK_Neutral },
/* 0xff5f                  */    { TK_NULL, TK_Neutral },
/* 0xff60   XK_Select      */    { TK_NULL, TK_Neutral },
/* 0xff61   XK_Print       */    { TK_NULL, TK_Neutral },
/* 0xff62   XK_Execute     */    { TK_NULL, TK_Neutral },
/* 0xff63   XK_Insert      */    { TK_Underscore, TK_Neutral },
/* 0xff64                  */    { TK_NULL, TK_Neutral },
/* 0xff65   XK_Undo        */    { TK_NULL, TK_Neutral },
/* 0xff66   XK_Redo        */    { TK_NULL, TK_Neutral },
/* 0xff67   XK_Menu        */    { TK_NULL, TK_Neutral },
/* 0xff68   XK_Find        */    { TK_NULL, TK_Neutral },
/* 0xff69   XK_Cancel      */    { TK_NULL, TK_Neutral },
/* 0xff6a   XK_Help        */    { TK_NULL, TK_Neutral },
/* 0xff6b   XK_Break       */    { TK_Break, TK_Neutral },
/* 0xff6c                  */    { TK_NULL, TK_Neutral },
/* 0xff6d                  */    { TK_NULL, TK_Neutral },
/* 0xff6e                  */    { TK_NULL, TK_Neutral },
/* 0xff6f                  */    { TK_NULL, TK_Neutral },
/* 0xff70                  */    { TK_NULL, TK_Neutral },
/* 0xff71                  */    { TK_NULL, TK_Neutral },
/* 0xff72                  */    { TK_NULL, TK_Neutral },
/* 0xff73                  */    { TK_NULL, TK_Neutral },
/* 0xff74                  */    { TK_NULL, TK_Neutral },
/* 0xff75                  */    { TK_NULL, TK_Neutral },
/* 0xff76                  */    { TK_NULL, TK_Neutral },
/* 0xff77                  */    { TK_NULL, TK_Neutral },
/* 0xff78                  */    { TK_NULL, TK_Neutral },
/* 0xff79                  */    { TK_NULL, TK_Neutral },
/* 0xff7a                  */    { TK_NULL, TK_Neutral },
/* 0xff7b                  */    { TK_NULL, TK_Neutral },
/* 0xff7c                  */    { TK_NULL, TK_Neutral },
/* 0xff7d                  */    { TK_NULL, TK_Neutral },
/* 0xff7e   XK_Mode_switch */    { TK_NULL, TK_Neutral },
/* 0xff7f   XK_Num_Lock    */    { TK_NULL, TK_Neutral },
/* 0xff80   XK_KP_Space    */    { TK_Space, TK_Neutral },
/* 0xff81                  */    { TK_NULL, TK_Neutral },
/* 0xff82                  */    { TK_NULL, TK_Neutral },
/* 0xff83                  */    { TK_NULL, TK_Neutral },
/* 0xff84                  */    { TK_NULL, TK_Neutral },
/* 0xff85                  */    { TK_NULL, TK_Neutral },
/* 0xff86                  */    { TK_NULL, TK_Neutral },
/* 0xff87                  */    { TK_NULL, TK_Neutral },
/* 0xff88                  */    { TK_NULL, TK_Neutral },
/* 0xff89   XK_KP_Tab      */    { TK_Right, TK_Neutral },
/* 0xff8a                  */    { TK_NULL, TK_Neutral },
/* 0xff8b                  */    { TK_NULL, TK_Neutral },
/* 0xff8c                  */    { TK_NULL, TK_Neutral },
/* 0xff8d   XK_KP_Enter    */    { TK_Enter, TK_Neutral },
/* 0xff8e                  */    { TK_NULL, TK_Neutral },
/* 0xff8f                  */    { TK_NULL, TK_Neutral },
/* 0xff90                  */    { TK_NULL, TK_Neutral },
/* 0xff91   XK_KP_F1       */    { TK_F1, TK_Neutral },
/* 0xff92   XK_KP_F2       */    { TK_F2, TK_Neutral },
/* 0xff93   XK_KP_F3       */    { TK_F3, TK_Neutral },
/* 0xff94   XK_KP_F4       */    { TK_CapsLock, TK_Neutral },
#if KP_JOYSTICK
/* 0xff95   XK_KP_Home     */    { TK_Northwest, TK_Neutral },
/* 0xff96   XK_KP_Left     */    { TK_West, TK_Neutral },
/* 0xff97   XK_KP_Up       */    { TK_North, TK_Neutral },
/* 0xff98   XK_KP_Right    */    { TK_East, TK_Neutral },
/* 0xff99   XK_KP_Down     */    { TK_South, TK_Neutral },
/* 0xff9a   XK_KP_Prior, XK_KP_Page_Up  */ { TK_Northeast, TK_Neutral },
/* 0xff9b   XK_KP_Next, XK_KP_Page_Down */ { TK_Southeast, TK_Neutral },
/* 0xff9c   XK_KP_End      */    { TK_Southwest, TK_Neutral },
/* 0xff9d   XK_KP_Begin    */    { TK_Fire, TK_Neutral },
/* 0xff9e   XK_KP_Insert   */    { TK_Fire, TK_Neutral },
#else
/* 0xff95   XK_KP_Home     */    { TK_Clear, TK_Neutral },
/* 0xff96   XK_KP_Left     */    { TK_Left, TK_Neutral },
/* 0xff97   XK_KP_Up       */    { TK_Up, TK_Neutral },
/* 0xff98   XK_KP_Right    */    { TK_Right, TK_Neutral },
/* 0xff99   XK_KP_Down     */    { TK_Down, TK_Neutral },
/* 0xff9a   XK_KP_Prior, XK_KP_Page_Up  */ { TK_LeftShift, TK_Neutral },
/* 0xff9b   XK_KP_Next, XK_KP_Page_Down */ { TK_RightShift, TK_Neutral },
/* 0xff9c   XK_KP_End      */    { TK_Unused, TK_Neutral },
/* 0xff9d   XK_KP_Begin    */    { TK_NULL, TK_Neutral },
/* 0xff9e   XK_KP_Insert   */    { TK_Underscore, TK_Neutral },
#endif
/* 0xff9f   XK_KP_Delete   */    { TK_Left, TK_Neutral },
/* 0xffa0                  */    { TK_NULL, TK_Neutral },
/* 0xffa1                  */    { TK_NULL, TK_Neutral },
/* 0xffa2                  */    { TK_NULL, TK_Neutral },
/* 0xffa3                  */    { TK_NULL, TK_Neutral },
/* 0xffa4                  */    { TK_NULL, TK_Neutral },
/* 0xffa5                  */    { TK_NULL, TK_Neutral },
/* 0xffa6                  */    { TK_NULL, TK_Neutral },
/* 0xffa7                  */    { TK_NULL, TK_Neutral },
/* 0xffa8                  */    { TK_NULL, TK_Neutral },
/* 0xffa9                  */    { TK_NULL, TK_Neutral },
/* 0xffaa   XK_KP_Multiply */    { TK_Colon, TK_ForceShift },
/* 0xffab   XK_KP_Add      */    { TK_Semicolon, TK_ForceShift },
/* 0xffac   XK_KP_Separator*/    { TK_Comma, TK_Neutral },
/* 0xffad   XK_KP_Subtract */    { TK_Minus, TK_Neutral },
/* 0xffae   XK_KP_Decimal  */    { TK_Period, TK_Neutral },
/* 0xffaf   XK_KP_Divide   */    { TK_Slash, TK_Neutral },
#if KPNUM_JOYSTICK
/* 0xffb0   XK_KP_0        */    { TK_Fire, TK_Neutral },
/* 0xffb1   XK_KP_1        */    { TK_Southwest, TK_Neutral },
/* 0xffb2   XK_KP_2        */    { TK_South, TK_Neutral },
/* 0xffb3   XK_KP_3        */    { TK_Southeast, TK_Neutral },
/* 0xffb4   XK_KP_4        */    { TK_West, TK_Neutral },
/* 0xffb5   XK_KP_5        */    { TK_Fire, TK_Neutral },
/* 0xffb6   XK_KP_6        */    { TK_East, TK_Neutral },
/* 0xffb7   XK_KP_7        */    { TK_Northwest, TK_Neutral },
/* 0xffb8   XK_KP_8        */    { TK_North, TK_Neutral },
/* 0xffb9   XK_KP_9        */    { TK_Northeast, TK_Neutral },
#else
/* 0xffb0   XK_KP_0        */    { TK_0, TK_Neutral },
/* 0xffb1   XK_KP_1        */    { TK_1, TK_Neutral },
/* 0xffb2   XK_KP_2        */    { TK_2, TK_Neutral },
/* 0xffb3   XK_KP_3        */    { TK_3, TK_Neutral },
/* 0xffb4   XK_KP_4        */    { TK_4, TK_Neutral },
/* 0xffb5   XK_KP_5        */    { TK_5, TK_Neutral },
/* 0xffb6   XK_KP_6        */    { TK_6, TK_Neutral },
/* 0xffb7   XK_KP_7        */    { TK_7, TK_Neutral },
/* 0xffb8   XK_KP_8        */    { TK_8, TK_Neutral },
/* 0xffb9   XK_KP_9        */    { TK_9, TK_Neutral },
#endif
/* 0xffba                  */    { TK_NULL, TK_Neutral },
/* 0xffbb                  */    { TK_NULL, TK_Neutral },
/* 0xffbc                  */    { TK_NULL, TK_Neutral },
/* 0xffbd   XK_KP_Equal    */    { TK_Minus,  TK_ForceShift },
/* 0xffbe   XK_F1          */    { TK_F1, TK_Neutral },
/* 0xffbf   XK_F2          */    { TK_F2, TK_Neutral },
/* 0xffc0   XK_F3          */    { TK_F3, TK_Neutral },
/* 0xffc1   XK_F4          */    { TK_CapsLock, TK_Neutral },
/* 0xffc2   XK_F5          */    { TK_AtSign, TK_Neutral },
/* 0xffc3   XK_F6          */    { TK_0, TK_Neutral },
/* 0xffc4   XK_F7          */    { TK_NULL, TK_Neutral },
/* 0xffc5   XK_F8          */    { TK_NULL, TK_Neutral },
/* 0xffc6   XK_F9          */    { TK_NULL, TK_Neutral },
/* 0xffc7   XK_F10         */    { TK_NULL, TK_Neutral },
/* In some versions of XFree86, XK_F11 to XK_F20 are produced for the
   shifted F1 to F10 keys, in some XK_F13 to XK_F20 are produced for
   the shifted F1 to F8 keys, and in some the keysyms are not altered.
   Arrgh.
*/
#if SHIFT_F1_IS_F11
/* 0xffc8   XK_F11         */    { TK_F1, TK_Neutral },
/* 0xffc9   XK_F12         */    { TK_F2, TK_Neutral },
/* 0xffca   XK_F13         */    { TK_F3, TK_Neutral },
/* 0xffcb   XK_F14         */    { TK_CapsLock, TK_Neutral },
/* 0xffcc   XK_F15         */    { TK_AtSign, TK_Neutral },
/* 0xffcd   XK_F16         */    { TK_0, TK_Neutral },
/* 0xffce   XK_F17         */    { TK_NULL, TK_Neutral },
/* 0xffcf   XK_F18         */    { TK_NULL, TK_Neutral },
#elif SHIFT_F1_IS_F13
/* 0xffc8   XK_F11         */    { TK_NULL, TK_Neutral },
/* 0xffc9   XK_F12         */    { TK_NULL, TK_Neutral },
/* 0xffca   XK_F13         */    { TK_F1, TK_Neutral },
/* 0xffcb   XK_F14         */    { TK_F2, TK_Neutral },
/* 0xffcc   XK_F15         */    { TK_F3, TK_Neutral },
/* 0xffcd   XK_F16         */    { TK_CapsLock, TK_Neutral },
/* 0xffce   XK_F17         */    { TK_AtSign, TK_Neutral },
/* 0xffcf   XK_F18         */    { TK_0, TK_Neutral },
#else
/* 0xffc8   XK_F11         */    { TK_NULL, TK_Neutral },
/* 0xffc9   XK_F12         */    { TK_NULL, TK_Neutral },
/* 0xffca   XK_F13         */    { TK_NULL, TK_Neutral },
/* 0xffcb   XK_F14         */    { TK_NULL, TK_Neutral },
/* 0xffcc   XK_F15         */    { TK_NULL, TK_Neutral },
/* 0xffcd   XK_F16         */    { TK_NULL, TK_Neutral },
/* 0xffce   XK_F17         */    { TK_NULL, TK_Neutral },
/* 0xffcf   XK_F18         */    { TK_NULL, TK_Neutral },
#endif
/* 0xffd0   XK_F19         */    { TK_NULL, TK_Neutral },
/* 0xffd1   XK_F20         */    { TK_NULL, TK_Neutral },
/* 0xffd2   XK_F21         */    { TK_NULL, TK_Neutral },
/* 0xffd3   XK_F22         */    { TK_NULL, TK_Neutral },
/* 0xffd4   XK_F23         */    { TK_NULL, TK_Neutral },
/* 0xffd5   XK_F24         */    { TK_NULL, TK_Neutral },
/* 0xffd6   XK_F25         */    { TK_NULL, TK_Neutral },
/* 0xffd7   XK_F26         */    { TK_NULL, TK_Neutral },
/* 0xffd8   XK_F27         */    { TK_NULL, TK_Neutral },
/* 0xffd9   XK_F28         */    { TK_NULL, TK_Neutral },
/* 0xffda   XK_F29         */    { TK_NULL, TK_Neutral },
/* 0xffdb   XK_F30         */    { TK_NULL, TK_Neutral },
/* 0xffdc   XK_F31         */    { TK_NULL, TK_Neutral },
/* 0xffdd   XK_F32         */    { TK_NULL, TK_Neutral },
/* 0xffde   XK_F33         */    { TK_NULL, TK_Neutral },
/* 0xffdf   XK_F34         */    { TK_NULL, TK_Neutral },
/* 0xffe0   XK_F35         */    { TK_NULL, TK_Neutral },
/* 0xffe1   XK_Shift_L     */    { TK_LeftShift, TK_Neutral },
/* 0xffe2   XK_Shift_R     */    { TK_RightShift, TK_Neutral },
/* 0xffe3   XK_Control_L   */    { TK_Ctrl, TK_Neutral },
/* 0xffe4   XK_Control_R   */    { TK_Ctrl, TK_Neutral },
/* 0xffe5   XK_Caps_Lock   */    { TK_NULL, TK_Neutral },
/* 0xffe6   XK_Shift_Lock  */    { TK_NULL, TK_Neutral },
/* 0xffe7   XK_Meta_L      */    { TK_Clear, TK_Neutral },
/* 0xffe8   XK_Meta_R      */    { TK_Down, TK_ForceShiftPersistent },
/* 0xffe9   XK_Alt_L       */    { TK_Clear, TK_Neutral },
/* 0xffea   XK_Alt_R       */    { TK_Down, TK_ForceShiftPersistent },
/* 0xffeb   XK_Super_L     */    { TK_NULL, TK_Neutral },
/* 0xffec   XK_Super_R     */    { TK_NULL, TK_Neutral },
/* 0xffed   XK_Hyper_L     */    { TK_NULL, TK_Neutral },
/* 0xffee   XK_Hyper_R     */    { TK_NULL, TK_Neutral },
/* 0xffef                  */    { TK_NULL, TK_Neutral },
/* 0xfff0                  */    { TK_NULL, TK_Neutral },
/* 0xfff1                  */    { TK_NULL, TK_Neutral },
/* 0xfff2                  */    { TK_NULL, TK_Neutral },
/* 0xfff3                  */    { TK_NULL, TK_Neutral },
/* 0xfff4                  */    { TK_NULL, TK_Neutral },
/* 0xfff5                  */    { TK_NULL, TK_Neutral },
/* 0xfff6                  */    { TK_NULL, TK_Neutral },
/* 0xfff7                  */    { TK_NULL, TK_Neutral },
/* 0xfff8                  */    { TK_NULL, TK_Neutral },
/* 0xfff9                  */    { TK_NULL, TK_Neutral },
/* 0xfffa                  */    { TK_NULL, TK_Neutral },
/* 0xfffb                  */    { TK_NULL, TK_Neutral },
/* 0xfffc                  */    { TK_NULL, TK_Neutral },
/* 0xfffd                  */    { TK_NULL, TK_Neutral },
/* 0xfffe                  */    { TK_NULL, TK_Neutral },
/* 0xffff   XK_Delete      */    { TK_Left, TK_Neutral }
};

static int keystate[8] = { 0, };
static int force_shift = TK_Neutral;
static int joystate = 0;

/* Avoid changing state too fast so keystrokes aren't lost. */
#define STRETCH_AMOUNT 4000
static tstate_t key_stretch_timeout;
int stretch_amount = STRETCH_AMOUNT;

void trs_kb_reset()
{
  key_stretch_timeout = z80_state.t_count;
}

int key_heartbeat = 0;
void trs_kb_heartbeat()
{
  /* Don't hold keys in queue too long */
  key_heartbeat++;
}

void trs_kb_bracket(int shifted)
{
  /* Set the shift state for the emulation of the "[ {", "\ |", 
     "] }", "^ ~", and "_ DEL" keys.  Some Model 4 keyboard drivers
     decode these with [ shifted and { unshifted, etc., while most
     other keyboard drivers either ignore them or decode them with
     [ unshifted and { shifted.  We default to the latter.  Note that
     these keys didn't exist on real machines anyway.
  */
  int i;
  for (i=0x5b; i<=0x5f; i++) {
    ascii_key_table[i].shift_action =
      shifted ? TK_ForceShift : TK_ForceNoShift;
  }
  for (i=0x7b; i<=0x7f; i++) {
    ascii_key_table[i].shift_action =
      shifted ? TK_ForceNoShift : TK_ForceShift;
  }
}

/* Emulate joystick with the keypad */
int trs_emulate_joystick(int key_down, int bit_action)
{
  if (bit_action < TK_Joystick) return 0;
  if (key_down) {
    joystate |= (bit_action & 0x1f);
  } else {
    joystate &= ~(bit_action & 0x1f);
  }
  return 1;
}

int trs_joystick_in()
{
#if JOYDEBUG
  debug("joy %02x ", joystate);
#endif
  return ~joystate;
}

void trs_xlate_keysym(int keysym)
{
    int key_down;
    KeyTable* kt;
    static int shift_action = TK_Neutral;

    if (keysym == 0x10000) {
	/* force all keys up */
	queue_key(TK_AllKeysUp);
	shift_action = TK_Neutral;
	return;
    }

    key_down = (keysym & 0x10000) == 0;
    if (keysym & 0xff00) {
      kt = &function_key_table[keysym & 0xff];
    } else {
      kt = &ascii_key_table[keysym & 0xff];
    }
    if (kt->bit_action == TK_NULL) return;
    if (trs_emulate_joystick(key_down, kt->bit_action)) return;

    if (key_down) {
      if (shift_action != TK_ForceShiftPersistent &&
	  shift_action != kt->shift_action) {
	shift_action = kt->shift_action;
	queue_key(shift_action);
      }
      queue_key(kt->bit_action);
    } else {
      queue_key(kt->bit_action | 0x10000);
      if (shift_action != TK_Neutral &&
	  shift_action == kt->shift_action) {
	shift_action = TK_Neutral;
	queue_key(shift_action);
      }
    }
}

static void change_keystate(int action)
{
    int key_down;
    int i;
#ifdef KBDEBUG
    debug("change_keystate: action 0x%x\n", action);
#endif

    switch (action) {
      case TK_AllKeysUp:
	/* force all keys up */
	for (i=0; i<7; i++) {
	    keystate[i] = 0;
	}
	force_shift = TK_Neutral;
	break;

      case TK_Neutral:
      case TK_ForceShift:
      case TK_ForceNoShift:
      case TK_ForceShiftPersistent:
	force_shift = action;
	break;

      default:
	key_down = TK_DOWN(action);
	if (key_down) {
	    keystate[TK_ADDR(action)] |= (1 << TK_DATA(action));
	} else {
	    keystate[TK_ADDR(action)] &= ~(1 << TK_DATA(action));
	}
    }
}

static int kb_mem_value(int address)
{
    int i, bitpos, data = 0;

    for (i=0, bitpos=1; i<7; i++, bitpos<<=1) {
	if (address & bitpos) {
	    data |= keystate[i];
	}
    }
    if (address & 0x80) {
	int tmp = keystate[7];
	if (trs_model == 1) {
	    if (force_shift == TK_ForceNoShift) {
		/* deactivate shift key */
		tmp &= ~1;
	    } else if (force_shift != TK_Neutral) {
		/* activate shift key */
		tmp |= 1;
	    }
	} else {
	    if (force_shift == TK_ForceNoShift) {
		/* deactivate both shift keys */
		tmp &= ~3;
	    } else if (force_shift != TK_Neutral) {
		/* if no shift keys are down, activate left shift key */
		if ((tmp & 3) == 0) tmp |= 1;
	    }
	}
	data |= tmp;
    }
    return data;
}

int trs_kb_mem_read(int address)
{
    int key = -1;
    int i, wait;
    static int recursion = 0;
    static int timesseen;

    /* Prevent endless recursive calls to this routine (by mem_read_word
       below) if REG_SP happens to point to keyboard memory. */
    if (recursion) return 0;

    /* Avoid delaying key state changes in queue for too long */
    if (key_heartbeat > 2) {
      do {
	key = trs_next_key(0);
	if (key >= 0) {
	  change_keystate(key);
	  timesseen = 1;
	}
      } while (key >= 0);
    }

    /* After each key state change, impose a timeout before the next one
       so that the Z-80 program doesn't miss any by polling too rarely,
       and so that we don't tickle the bugs in some common TRS-80 keyboard
       drivers that strike if two keys change simultaneously */
    if (key_stretch_timeout - z80_state.t_count > TSTATE_T_MID) {

	/* Check if we are in the system keyboard driver, called from
	   the wait-for-input routine.  If so, and there are no
	   keystrokes queued, and the current state has been seen by
	   at least 16 such reads, then trs_next_key will pause the
	   process to avoid burning host CPU needlessly.

	   The test below works on both Model I and III and is
	   insensitive to what keyboard driver is being used, as long
	   as it is called through the wait-for-key routine at ROM
	   address 0x0049 and has not pushed too much on the stack yet
	   when it first reads from the key matrix.  The search is
	   needed (at least) for NEWDOS80, which pushes 2 extra bytes
	   on the stack.  */
	wait = 0;
	if (timesseen++ >= 16) {
	  recursion = 1;
	  for (i=0; i<=4; i+=2) {
	    if (mem_read_word(REG_SP + 2 + i) == 0x4015) {
	      wait = mem_read_word(REG_SP + 10 + i) == 0x004c;
	      break;
	    }
	  }
	  recursion = 0;
	}
	/* Get the next key */
	key = trs_next_key(wait);
	key_stretch_timeout = z80_state.t_count + stretch_amount;
    }

    if (key >= 0) {
      change_keystate(key);
      timesseen = 1;
    }
    key_heartbeat = 0;
    return kb_mem_value(address);
}

