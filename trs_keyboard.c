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
   Last modified on Thu Aug 28 17:11:06 PDT 1997 by mann
*/

#include "z80.h"
#include "trs.h"

/* TRS-80 key matrix */
#define TK_AtSign       0, 0  /* @   */
#define TK_A            0, 1
#define TK_B            0, 2
#define TK_C            0, 3
#define TK_D            0, 4
#define TK_E            0, 5
#define TK_F            0, 6
#define TK_G            0, 7
#define TK_H            1, 0
#define TK_I            1, 1
#define TK_J            1, 2
#define TK_K            1, 3
#define TK_L            1, 4
#define TK_M            1, 5
#define TK_N            1, 6
#define TK_O            1, 7
#define TK_P            2, 0
#define TK_Q            2, 1
#define TK_R            2, 2
#define TK_S            2, 3
#define TK_T            2, 4
#define TK_U            2, 5
#define TK_V            2, 6
#define TK_W            2, 7
#define TK_X            3, 0
#define TK_Y            3, 1
#define TK_Z            3, 2
#define TK_LeftBracket  3, 3  /* [ { */ /* not really on keyboard */
#define TK_Backslash    3, 4  /* \ | */ /* not really on keyboard */
#define TK_RightBracket 3, 5  /* ] } */ /* not really on keyboard */
#define TK_Caret        3, 6  /* ^ ~ */ /* not really on keyboard */
#define TK_Underscore   3, 7  /* _   */ /* not really on keyboard */
#define TK_0            4, 0  /* 0   */
#define TK_1            4, 1  /* 1 ! */
#define TK_2            4, 2  /* 2 " */
#define TK_3            4, 3  /* 3 # */
#define TK_4            4, 4  /* 4 $ */
#define TK_5            4, 5  /* 5 % */
#define TK_6            4, 6  /* 6 & */
#define TK_7            4, 7  /* 7 ' */
#define TK_8            5, 0  /* 8 ( */
#define TK_9            5, 1  /* 9 ) */
#define TK_Colon        5, 2  /* : * */
#define TK_Semicolon    5, 3  /* ; + */
#define TK_Comma        5, 4  /* , < */
#define TK_Minus        5, 5  /* - = */
#define TK_Period       5, 6  /* . > */
#define TK_Slash        5, 7  /* / ? */
#define TK_Enter        6, 0
#define TK_Clear        6, 1
#define TK_Break        6, 2
#define TK_Up           6, 3
#define TK_Down         6, 4
#define TK_Left         6, 5
#define TK_Right        6, 6
#define TK_Space        6, 7
#define TK_LeftShift    7, 0
#define TK_RightShift   7, 1  /* M3 only; both shifts are 7, 0 on M1 */
#define TK_Unused       7, 2
#define TK_CapsLock     7, 3  /* M3 only */
#define TK_F1           7, 4  /* M3 only */
#define TK_F2           7, 5  /* M3 only */
#define TK_F3           7, 6  /* M3 only */
#define TK_Ctrl         7, 7  /* M3 only */

#define TK_NULL          -1, 0

#define TK_Neutral              0
#define TK_ForceShift           1
#define TK_ForceNoShift        -1
#define TK_ForceShiftPersistent 2

typedef struct
{
    int address_bit;
    int data_bit;
    int shift_action; /* -1 force off, 0 use true state, 1 force on,
		          2 force on until key up */
} KeyTable;

/* Keysyms in the ASCII range 0x0000 - 0x007f */
/* The range 0x0000-0x001f is not used by X, nor is 0x007f */

KeyTable ascii_key_table[] = {
/* 0x0 */     TK_NULL, 0,  /* undefined keysyms... */
/* 0x1 */     TK_NULL, 0,
/* 0x2 */     TK_NULL, 0,
/* 0x3 */     TK_NULL, 0,
/* 0x4 */     TK_NULL, 0,
/* 0x5 */     TK_NULL, 0,
/* 0x6 */     TK_NULL, 0,
/* 0x7 */     TK_NULL, 0,
/* 0x8 */     TK_NULL, 0,
/* 0x9 */     TK_NULL, 0,
/* 0xa */     TK_NULL, 0,
/* 0xb */     TK_NULL, 0,
/* 0xc */     TK_NULL, 0,
/* 0xd */     TK_NULL, 0,
/* 0xe */     TK_NULL, 0,
/* 0xf */     TK_NULL, 0,
/* 0x10 */    TK_NULL, 0,
/* 0x11 */    TK_NULL, 0,
/* 0x12 */    TK_NULL, 0,
/* 0x13 */    TK_NULL, 0,
/* 0x14 */    TK_NULL, 0,
/* 0x15 */    TK_NULL, 0,
/* 0x16 */    TK_NULL, 0,
/* 0x17 */    TK_NULL, 0,
/* 0x18 */    TK_NULL, 0,
/* 0x19 */    TK_NULL, 0,
/* 0x1a */    TK_NULL, 0,
/* 0x1b */    TK_NULL, 0,
/* 0x1c */    TK_NULL, 0,
/* 0x1d */    TK_NULL, 0,
/* 0x1e */    TK_NULL, 0,
/* 0x1f */    TK_NULL, 0,  /* ...end undefined keysyms */
/* 0x20 */    TK_Space, 0,
/* 0x21 */    TK_1, 1,
/* 0x22 */    TK_2, 1,
/* 0x23 */    TK_3, 1,
/* 0x24 */    TK_4, 1,
/* 0x25 */    TK_5, 1,
/* 0x26 */    TK_6, 1,
/* 0x27 */    TK_7, 1,
/* 0x28 */    TK_8, 1,
/* 0x29 */    TK_9, 1,
/* 0x2a */    TK_Colon, 1,
/* 0x2b */    TK_Semicolon, 1,
/* 0x2c */    TK_Comma, -1,
/* 0x2d */    TK_Minus, -1,
/* 0x2e */    TK_Period, -1,
/* 0x2f */    TK_Slash, -1,
/* 0x30 */    TK_0, -1,
/* 0x31 */    TK_1, -1,
/* 0x32 */    TK_2, -1,
/* 0x33 */    TK_3, -1,
/* 0x34 */    TK_4, -1,
/* 0x35 */    TK_5, -1,
/* 0x36 */    TK_6, -1,
/* 0x37 */    TK_7, -1,
/* 0x38 */    TK_8, -1,
/* 0x39 */    TK_9, -1,
/* 0x3a */    TK_Colon, -1,
/* 0x3b */    TK_Semicolon, -1,
/* 0x3c */    TK_Comma, 1,
/* 0x3d */    TK_Minus, 1,
/* 0x3e */    TK_Period, 1,
/* 0x3f */    TK_Slash, 1,
/* 0x40 */    TK_AtSign, -1,
/* 0x41 */    TK_A,  1,
/* 0x42 */    TK_B,  1,
/* 0x43 */    TK_C,  1,
/* 0x44 */    TK_D,  1,
/* 0x45 */    TK_E,  1,
/* 0x46 */    TK_F,  1,
/* 0x47 */    TK_G,  1,
/* 0x48 */    TK_H,  1,
/* 0x49 */    TK_I,  1,
/* 0x4a */    TK_J,  1,
/* 0x4b */    TK_K,  1,
/* 0x4c */    TK_L,  1,
/* 0x4d */    TK_M,  1,
/* 0x4e */    TK_N,  1,
/* 0x4f */    TK_O,  1,
/* 0x50 */    TK_P,  1,
/* 0x51 */    TK_Q,  1,
/* 0x52 */    TK_R,  1,
/* 0x53 */    TK_S,  1,
/* 0x54 */    TK_T,  1,
/* 0x55 */    TK_U,  1,
/* 0x56 */    TK_V,  1,
/* 0x57 */    TK_W,  1,
/* 0x58 */    TK_X,  1,
/* 0x59 */    TK_Y,  1,
/* 0x5a */    TK_Z,  1,
/* 0x5b */    TK_LeftBracket, -1,
/* 0x5c */    TK_Backslash, -1,
/* 0x5d */    TK_RightBracket, -1,
/* 0x5e */    TK_Caret, -1,
/* 0x5f */    TK_Underscore, -1,
/* 0x60 */    TK_AtSign,  1,
/* 0x61 */    TK_A, -1,
/* 0x62 */    TK_B, -1,
/* 0x63 */    TK_C, -1,
/* 0x64 */    TK_D, -1,
/* 0x65 */    TK_E, -1,
/* 0x66 */    TK_F, -1,
/* 0x67 */    TK_G, -1,
/* 0x68 */    TK_H, -1,
/* 0x69 */    TK_I, -1,
/* 0x6a */    TK_J, -1,
/* 0x6b */    TK_K, -1,
/* 0x6c */    TK_L, -1,
/* 0x6d */    TK_M, -1,
/* 0x6e */    TK_N, -1,
/* 0x6f */    TK_O, -1,
/* 0x70 */    TK_P, -1,
/* 0x71 */    TK_Q, -1,
/* 0x72 */    TK_R, -1,
/* 0x73 */    TK_S, -1,
/* 0x74 */    TK_T, -1,
/* 0x75 */    TK_U, -1,
/* 0x76 */    TK_V, -1,
/* 0x77 */    TK_W, -1,
/* 0x78 */    TK_X, -1,
/* 0x79 */    TK_Y, -1,
/* 0x7a */    TK_Z, -1,
/* 0x7b */    TK_LeftBracket, 1,
/* 0x7c */    TK_Backslash, 1,
/* 0x7d */    TK_RightBracket, 1,
/* 0x7e */    TK_Caret, 1,
/* 0x7f */    TK_NULL, 0  /* undefined keysym */
};

/* Keysyms in the function key range 0xff00 - 0xffff */

KeyTable function_key_table[] = {
/* 0xff00                  */    TK_NULL, 0,
/* 0xff01                  */    TK_NULL, 0,
/* 0xff02                  */    TK_NULL, 0,
/* 0xff03                  */    TK_NULL, 0,
/* 0xff04                  */    TK_NULL, 0,
/* 0xff05                  */    TK_NULL, 0,
/* 0xff06                  */    TK_NULL, 0,
/* 0xff07                  */    TK_NULL, 0,
/* 0xff08   XK_BackSpace   */    TK_Left, 0,
/* 0xff09   XK_Tab         */    TK_Right, 0,
/* 0xff0a   XK_Linefeed    */    TK_Down, 0,
/* 0xff0b   XK_Clear       */    TK_Clear, 0,
/* 0xff0c                  */    TK_NULL, 0,
/* 0xff0d   XK_Return      */    TK_Enter, 0,
/* 0xff0e                  */    TK_NULL, 0,
/* 0xff0f                  */    TK_NULL, 0,
/* 0xff10                  */    TK_NULL, 0,
/* 0xff11                  */    TK_NULL, 0,
/* 0xff12                  */    TK_NULL, 0,
/* 0xff13   XK_Pause       */    TK_NULL, 0,
/* 0xff14   XK_ScrollLock  */    TK_AtSign, 0,
/* 0xff15   XK_Sys_Req     */    TK_NULL, 0,
/* 0xff16                  */    TK_NULL, 0,
/* 0xff17                  */    TK_NULL, 0,
/* 0xff18                  */    TK_NULL, 0,
/* 0xff19                  */    TK_NULL, 0,
/* 0xff1a                  */    TK_NULL, 0,
/* 0xff1b   XK_Escape      */    TK_Break, 0,
/* 0xff1c                  */    TK_NULL, 0,
/* 0xff1d                  */    TK_NULL, 0,
/* 0xff1e                  */    TK_NULL, 0,
/* 0xff1f                  */    TK_NULL, 0,
/* 0xff20   XK_Multi_key   */    TK_AtSign, 0,
/* 0xff21                  */    TK_NULL, 0,
/* 0xff22                  */    TK_NULL, 0,
/* 0xff23                  */    TK_NULL, 0,
/* 0xff24                  */    TK_NULL, 0,
/* 0xff25                  */    TK_NULL, 0,
/* 0xff26                  */    TK_NULL, 0,
/* 0xff27                  */    TK_NULL, 0,
/* 0xff28                  */    TK_NULL, 0,
/* 0xff29                  */    TK_NULL, 0,
/* 0xff2a                  */    TK_NULL, 0,
/* 0xff2b                  */    TK_NULL, 0,
/* 0xff2c                  */    TK_NULL, 0,
/* 0xff2d                  */    TK_NULL, 0,
/* 0xff2e                  */    TK_NULL, 0,
/* 0xff2f                  */    TK_NULL, 0,
/* 0xff30                  */    TK_NULL, 0,
/* 0xff31                  */    TK_NULL, 0,
/* 0xff32                  */    TK_NULL, 0,
/* 0xff33                  */    TK_NULL, 0,
/* 0xff34                  */    TK_NULL, 0,
/* 0xff35                  */    TK_NULL, 0,
/* 0xff36                  */    TK_NULL, 0,
/* 0xff37                  */    TK_NULL, 0,
/* 0xff38                  */    TK_NULL, 0,
/* 0xff39                  */    TK_NULL, 0,
/* 0xff3a                  */    TK_NULL, 0,
/* 0xff3b                  */    TK_NULL, 0,
/* 0xff3c                  */    TK_NULL, 0,
/* 0xff3d                  */    TK_NULL, 0,
/* 0xff3e                  */    TK_NULL, 0,
/* 0xff3f                  */    TK_NULL, 0,
/* 0xff40                  */    TK_NULL, 0,
/* 0xff41                  */    TK_NULL, 0,
/* 0xff42                  */    TK_NULL, 0,
/* 0xff43                  */    TK_NULL, 0,
/* 0xff44                  */    TK_NULL, 0,
/* 0xff45                  */    TK_NULL, 0,
/* 0xff46                  */    TK_NULL, 0,
/* 0xff47                  */    TK_NULL, 0,
/* 0xff48                  */    TK_NULL, 0,
/* 0xff49                  */    TK_NULL, 0,
/* 0xff4a                  */    TK_NULL, 0,
/* 0xff4b                  */    TK_NULL, 0,
/* 0xff4c                  */    TK_NULL, 0,
/* 0xff4d                  */    TK_NULL, 0,
/* 0xff4e                  */    TK_NULL, 0,
/* 0xff4f                  */    TK_NULL, 0,
/* 0xff50   XK_Home        */    TK_Clear, 0,
/* 0xff51   XK_Left        */    TK_Left, 0,
/* 0xff52   XK_Up          */    TK_Up, 0,
/* 0xff53   XK_Right       */    TK_Right, 0,
/* 0xff54   XK_Down        */    TK_Down, 0,
/* 0xff55   XK_Prior, XK_Page_Up  */  TK_NULL, 0,
/* 0xff56   XK_Next, XK_Page_Down */  TK_NULL, 0,
/* 0xff57   XK_End         */    TK_Unused, 0,
/* 0xff58   XK_Begin       */    TK_NULL, 0,
/* 0xff59                  */    TK_NULL, 0,
/* 0xff5a                  */    TK_NULL, 0,
/* 0xff5b                  */    TK_NULL, 0,
/* 0xff5c                  */    TK_NULL, 0,
/* 0xff5d                  */    TK_NULL, 0,
/* 0xff5e                  */    TK_NULL, 0,
/* 0xff5f                  */    TK_NULL, 0,
/* 0xff60   XK_Select      */    TK_NULL, 0,
/* 0xff61   XK_Print       */    TK_NULL, 0,
/* 0xff62   XK_Execute     */    TK_NULL, 0,
/* 0xff63   XK_Insert      */    TK_Underscore, 0,
/* 0xff64                  */    TK_NULL, 0,
/* 0xff65   XK_Undo        */    TK_NULL, 0,
/* 0xff66   XK_Redo        */    TK_NULL, 0,
/* 0xff67   XK_Menu        */    TK_NULL, 0,
/* 0xff68   XK_Find        */    TK_NULL, 0,
/* 0xff69   XK_Cancel      */    TK_NULL, 0,
/* 0xff6a   XK_Help        */    TK_NULL, 0,
/* 0xff6b   XK_Break       */    TK_Break, 0,
/* 0xff6c                  */    TK_NULL, 0,
/* 0xff6d                  */    TK_NULL, 0,
/* 0xff6e                  */    TK_NULL, 0,
/* 0xff6f                  */    TK_NULL, 0,
/* 0xff70                  */    TK_NULL, 0,
/* 0xff71                  */    TK_NULL, 0,
/* 0xff72                  */    TK_NULL, 0,
/* 0xff73                  */    TK_NULL, 0,
/* 0xff74                  */    TK_NULL, 0,
/* 0xff75                  */    TK_NULL, 0,
/* 0xff76                  */    TK_NULL, 0,
/* 0xff77                  */    TK_NULL, 0,
/* 0xff78                  */    TK_NULL, 0,
/* 0xff79                  */    TK_NULL, 0,
/* 0xff7a                  */    TK_NULL, 0,
/* 0xff7b                  */    TK_NULL, 0,
/* 0xff7c                  */    TK_NULL, 0,
/* 0xff7d                  */    TK_NULL, 0,
/* 0xff7e   XK_Mode_switch */    TK_NULL, 0,
/* 0xff7f   XK_Num_Lock    */    TK_NULL, 0,
/* 0xff80   XK_KP_Space    */    TK_Space, 0,
/* 0xff81                  */    TK_NULL, 0,
/* 0xff82                  */    TK_NULL, 0,
/* 0xff83                  */    TK_NULL, 0,
/* 0xff84                  */    TK_NULL, 0,
/* 0xff85                  */    TK_NULL, 0,
/* 0xff86                  */    TK_NULL, 0,
/* 0xff87                  */    TK_NULL, 0,
/* 0xff88                  */    TK_NULL, 0,
/* 0xff89   XK_KP_Tab      */    TK_Right, 0,
/* 0xff8a                  */    TK_NULL, 0,
/* 0xff8b                  */    TK_NULL, 0,
/* 0xff8c                  */    TK_NULL, 0,
/* 0xff8d   XK_KP_Enter    */    TK_Enter, 0,
/* 0xff8e                  */    TK_NULL, 0,
/* 0xff8f                  */    TK_NULL, 0,
/* 0xff90                  */    TK_NULL, 0,
/* 0xff91   XK_KP_F1       */    TK_F1, 0,
/* 0xff92   XK_KP_F2       */    TK_F2, 0,
/* 0xff93   XK_KP_F3       */    TK_F3, 0,
/* 0xff94   XK_KP_F4       */    TK_CapsLock, 0,
/* 0xff95   XK_KP_Home     */    TK_Clear, 0,
/* 0xff96   XK_KP_Left     */    TK_Left, 0,
/* 0xff97   XK_KP_Up       */    TK_Up, 0,
/* 0xff98   XK_KP_Right    */    TK_Right, 0,
/* 0xff99   XK_KP_Down     */    TK_Down, 0,
/* 0xff9a   XK_KP_Prior, XK_KP_Page_Up  */ TK_NULL, 0,
/* 0xff9b   XK_KP_Next, XK_KP_Page_Down */ TK_NULL, 0,
/* 0xff9c   XK_KP_End      */    TK_Unused, 0,
/* 0xff9d   XK_KP_Begin    */    TK_NULL, 0,
/* 0xff9e   XK_KP_Insert   */    TK_Underscore, 0,
/* 0xff9f   XK_KP_Delete   */    TK_Left, 0,
/* 0xffa0                  */    TK_NULL, 0,
/* 0xffa1                  */    TK_NULL, 0,
/* 0xffa2                  */    TK_NULL, 0,
/* 0xffa3                  */    TK_NULL, 0,
/* 0xffa4                  */    TK_NULL, 0,
/* 0xffa5                  */    TK_NULL, 0,
/* 0xffa6                  */    TK_NULL, 0,
/* 0xffa7                  */    TK_NULL, 0,
/* 0xffa8                  */    TK_NULL, 0,
/* 0xffa9                  */    TK_NULL, 0,
/* 0xffaa   XK_KP_Multiply */    TK_Colon, 1,
/* 0xffab   XK_KP_Add      */    TK_Semicolon, 1,
/* 0xffac   XK_KP_Separator*/    TK_Comma, 0,
/* 0xffad   XK_KP_Subtract */    TK_Minus, 0,
/* 0xffae   XK_KP_Decimal  */    TK_Period, 0,
/* 0xffaf   XK_KP_Divide   */    TK_Slash, 0,
/* 0xffb0   XK_KP_0        */    TK_0, 0,
/* 0xffb1   XK_KP_1        */    TK_1, 0,
/* 0xffb2   XK_KP_2        */    TK_2, 0,
/* 0xffb3   XK_KP_3        */    TK_3, 0,
/* 0xffb4   XK_KP_4        */    TK_4, 0,
/* 0xffb5   XK_KP_5        */    TK_5, 0,
/* 0xffb6   XK_KP_6        */    TK_6, 0,
/* 0xffb7   XK_KP_7        */    TK_7, 0,
/* 0xffb8   XK_KP_8        */    TK_8, 0,
/* 0xffb9   XK_KP_9        */    TK_9, 0,
/* 0xffba                  */    TK_NULL, 0,
/* 0xffbb                  */    TK_NULL, 0,
/* 0xffbc                  */    TK_NULL, 0,
/* 0xffbd   XK_KP_Equal    */    TK_Minus,  1,
/* 0xffbe   XK_F1          */    TK_F1, 0,
/* 0xffbf   XK_F2          */    TK_F2, 0,
/* 0xffc0   XK_F3          */    TK_F3, 0,
/* 0xffc1   XK_F4          */    TK_CapsLock, 0,
/* 0xffc2   XK_F5          */    TK_AtSign, 0,
/* 0xffc3   XK_F6          */    TK_0, 0,
/* 0xffc4   XK_F7          */    TK_NULL, 0,
/* 0xffc5   XK_F8          */    TK_NULL, 0,
/* 0xffc6   XK_F9          */    TK_NULL, 0,
/* 0xffc7   XK_F10         */    TK_NULL, 0,
/* 0xffc8   XK_F11         */    TK_RightShift, 0,
/* 0xffc9   XK_F12         */    TK_Unused, 0,
/* 0xffca   XK_F13         */    TK_Underscore, 0,
/* 0xffcb   XK_F14         */    TK_RightShift, 0,
/* 0xffcc   XK_F15         */    TK_NULL, 0,
/* 0xffcd   XK_F16         */    TK_NULL, 0,
/* 0xffce   XK_F17         */    TK_NULL, 0,
/* 0xffcf   XK_F18         */    TK_NULL, 0,
/* 0xffd0   XK_F19         */    TK_NULL, 0,
/* 0xffd1   XK_F20         */    TK_NULL, 0,
/* 0xffd2   XK_F21         */    TK_NULL, 0,
/* 0xffd3   XK_F22         */    TK_NULL, 0,
/* 0xffd4   XK_F23         */    TK_NULL, 0,
/* 0xffd5   XK_F24         */    TK_NULL, 0,
/* 0xffd6   XK_F25         */    TK_NULL, 0,
/* 0xffd7   XK_F26         */    TK_NULL, 0,
/* 0xffd8   XK_F27         */    TK_NULL, 0,
/* 0xffd9   XK_F28         */    TK_NULL, 0,
/* 0xffda   XK_F29         */    TK_NULL, 0,
/* 0xffdb   XK_F30         */    TK_NULL, 0,
/* 0xffdc   XK_F31         */    TK_NULL, 0,
/* 0xffdd   XK_F32         */    TK_NULL, 0,
/* 0xffde   XK_F33         */    TK_NULL, 0,
/* 0xffdf   XK_F34         */    TK_NULL, 0,
/* 0xffe0   XK_F35         */    TK_NULL, 0,
/* 0xffe1   XK_Shift_L     */    TK_LeftShift, 0,
/* 0xffe2   XK_Shift_R     */    TK_RightShift, 0,
/* 0xffe3   XK_Control_L   */    TK_Ctrl, 0,
/* 0xffe4   XK_Control_R   */    TK_Ctrl, 0,
/* 0xffe5   XK_Caps_Lock   */    TK_NULL, 0,
/* 0xffe6   XK_Shift_Lock  */    TK_NULL, 0,
/* 0xffe7   XK_Meta_L      */    TK_Clear, 0,
/* 0xffe8   XK_Meta_R      */    TK_Down, 2,
/* 0xffe9   XK_Alt_L       */    TK_Clear, 0,
/* 0xffea   XK_Alt_R       */    TK_Down, 2,
/* 0xffeb   XK_Super_L     */    TK_NULL, 0,
/* 0xffec   XK_Super_R     */    TK_NULL, 0,
/* 0xffed   XK_Hyper_L     */    TK_NULL, 0,
/* 0xffee   XK_Hyper_R     */    TK_NULL, 0,
/* 0xffef                  */    TK_NULL, 0,
/* 0xfff0                  */    TK_NULL, 0,
/* 0xfff1                  */    TK_NULL, 0,
/* 0xfff2                  */    TK_NULL, 0,
/* 0xfff3                  */    TK_NULL, 0,
/* 0xfff4                  */    TK_NULL, 0,
/* 0xfff5                  */    TK_NULL, 0,
/* 0xfff6                  */    TK_NULL, 0,
/* 0xfff7                  */    TK_NULL, 0,
/* 0xfff8                  */    TK_NULL, 0,
/* 0xfff9                  */    TK_NULL, 0,
/* 0xfffa                  */    TK_NULL, 0,
/* 0xfffb                  */    TK_NULL, 0,
/* 0xfffc                  */    TK_NULL, 0,
/* 0xfffd                  */    TK_NULL, 0,
/* 0xfffe                  */    TK_NULL, 0,
/* 0xffff   XK_Delete      */    TK_Left, 0	
};

static int keystate[8] = { 0, };
static int force_shift = 0;
static int stretch = 0;

/* Avoid changing state too fast so keystrokes aren't lost. */
#define STRETCH_AMOUNT 16

void trs_kb_reset()
{
    /* Be responsive right after a reset */
    stretch = 0;
}

void trs_kb_heartbeat()
{
    /* Be responsive if we are polled rarely */
    stretch--;
}

static void change_keystate(key)
     int key;
{
    int key_down;
    KeyTable* kt;

    key_down = (key & 0x10000) == 0;
    if (key & 0xff00) {
	kt = &function_key_table[key & 0xff];
    } else {
	kt = &ascii_key_table[key & 0x7f];
    }
    if (kt->address_bit == -1) return;

    if (key_down) {
	keystate[kt->address_bit] |= (1 << kt->data_bit);
	if (force_shift != 2) force_shift = kt->shift_action;
    } else {
	keystate[kt->address_bit] &= ~(1 << kt->data_bit);
	if (force_shift == kt->shift_action) force_shift = 0;
    }

#ifdef KBDEBUG
    fprintf(stderr, "change_keystate: key 0x%x address %d data %d shift %d\n",
	    key, kt->address_bit, kt->data_bit, kt->shift_action);
#endif
}

static int kb_mem_value(address)
     int address;
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
	    if (force_shift < 0) {
		/* deactivate shift key */
		tmp &= ~1;
	    } else if (force_shift > 0) {
		/* activate shift key */
		tmp |= 1;
	    }
	} else {
	    if (force_shift < 0) {
		/* deactivate both shift keys */
		tmp &= ~3;
	    } else if (force_shift > 0) {
		/* if no shift keys are down, activate left shift key */
		if ((tmp & 3) == 0) tmp |= 1;
	    }
	}
	data |= tmp;
    }
    return data;
}

int trs_kb_mem_read(address)
    int address;
{
    int key = -1;
    int i, wait;

    if (--stretch < 0) {
	stretch = STRETCH_AMOUNT;

	/* Check if we are in the system keyboard driver, called from
           the wait-for-input routine.  The test below works on both
           Model I and III and is insentitive to what keyboard driver
           is being used, as long as it is called through the
           wait-for-key routine at ROM address 0x0049 and has not
           pushed too much on the stack yet when it first reads from
           the key matrix.  The search is needed (at least) for
           NEWDOS80, which pushes 2 extra bytes on the stack.
	*/
	wait = 0;
	for (i=0; i<=4; i+=2) {
	    if (mem_read_word(REG_SP + 2 + i) == 0x4015) {
		wait = mem_read_word(REG_SP + 10 + i) == 0x004c;
		break;
	    }
	}
	key = trs_next_key(wait);
    }

    if (key >= 0) change_keystate(key);
    return kb_mem_value(address);
}

