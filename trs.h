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
   Last modified on Tue Sep 30 13:45:02 PDT 1997 by mann
*/

/*
 * trs.h
 */

#include "z80.h"

#define NORMAL 0
#define EXPANDED 1
#define INVERSE 2

extern int trs_model; /* 1, 3, 4 */

extern int trs_parse_command_line(/* int argc, char **argv, int *debug */);

extern void trs_screen_init();
extern void trs_screen_write_char(/*int position, int char_index, Bool doflush*/);
extern void trs_screen_write_chars(/*int *locations, int *values, int count*/);
extern void trs_screen_expanded(/*int flag*/);
extern void trs_screen_80x24(/*int flag*/);
extern void trs_screen_inverse(/*int flag*/);
extern void trs_screen_scroll();
extern void trs_screen_refresh();

extern void trs_reset();
extern void trs_exit();

extern void trs_kb_reset();
extern int trs_kb_mem_read(/* int address */);
extern int trs_next_key(/*int wait*/);
extern void trs_kb_heartbeat();

extern void trs_get_event(/*int wait*/);
extern int x_poll_count;

extern void trs_printer_write();
extern int trs_printer_read();

extern void trs_cassette_out();
extern int trs_cassette_in();

extern int trs_rom_size;
extern int trs_rom1_size;
extern int trs_rom3_size;
extern unsigned char trs_rom1[];
extern unsigned char trs_rom3[];

extern void trs_load_compiled_rom(/*int size, unsigned char rom[]*/);
extern void trs_load_rom(/*char *filename*/);

extern unsigned char trs_interrupt_latch_read();
extern unsigned char trs_nmi_latch_read();
extern void trs_interrupt_mask_write(/*unsigned char*/);
extern void trs_nmi_mask_write(/*unsigned char*/);
extern void trs_reset_button_interrupt(/*int state*/);
extern void trs_disk_intrq_interrupt(/*int state*/);
extern void trs_disk_drq_interrupt(/*int state*/);
extern void trs_timer_interrupt(/*int state*/);
extern void trs_timer_init();
extern void trs_timer_off();
extern void trs_timer_on();
extern void trs_timer_speed(/* int flag */);

extern void trs_disk_change(/*int drive*/);
extern void trs_disk_change_all();

extern Uchar *mem_pointer(/*int address*/);
extern void mem_video_page(/* int which */);
extern void mem_bank(/* int which */);
extern void mem_map(/* int which */);

extern void trs_debug();
