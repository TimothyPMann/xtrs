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
 * trs.h
 */
typedef enum {
    NORMAL = 0,
    EXPANDED = 1
} ScreenMode;

extern int trs_screen_init();
extern void trs_screen_write_char(/*int position, int char_index, Bool doflush*/);
extern void trs_screen_write_chars(/*int *locations, int *values,int count*/);
extern void trs_screen_mode_select(/*ScreenMode mode*/);
extern void trs_screen_scroll();
extern void trs_screen_refresh();
extern void trs_refresh();
extern int trs_kb_poll();
extern void trs_reset();
extern void trs_exit();

extern void trs_printer_write();
extern int trs_printer_read();

extern void trs_cassette_out();
extern int trs_cassette_in();

extern int trs_rom_size;
extern unsigned char trs_rom[];

extern void trs_load_compiled_rom();
extern void trs_load_rom(/*char *filename*/);

