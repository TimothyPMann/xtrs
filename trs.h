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
   Last modified on Fri Sep 25 19:34:17 PDT 1998 by mann
*/

/*
 * trs.h
 */

#include "z80.h"

#define NORMAL 0
#define EXPANDED 1
#define INVERSE 2

extern int trs_model; /* 1, 3, 4, 5(=4p) */
extern int trs_pausing;
extern int trs_autodelay;

extern int trs_parse_command_line(int argc, char **argv, int *debug);

extern void trs_screen_init(void);
extern void trs_screen_write_char(int position, int char_index, int doflush);
extern void trs_screen_write_chars(int *locations, int *values, int count);
extern void trs_screen_expanded(int flag);
extern void trs_screen_80x24(int flag);
extern void trs_screen_inverse(int flag);
extern void trs_screen_scroll(void);
extern void trs_screen_refresh(void);

extern void trs_reset(void);
extern void trs_exit(void);

extern void trs_kb_reset(void);
extern int trs_kb_mem_read(int address);
extern int trs_next_key(int wait);
extern void trs_kb_heartbeat(void);
extern void trs_xlate_keycode(int keycode);
extern void queue_key(int key);
extern int dequeue_key(void);
extern int stretch_amount, stretch_poll, stretch_heartbeat;

extern void trs_get_event(int wait);
extern volatile int x_poll_count;

extern void trs_printer_write(int value);
extern int trs_printer_read(void);

extern void trs_cassette_motor(int value);
extern void trs_cassette_out(int value);
extern int trs_cassette_in(int modesel);
extern void trs_sound_out(int value);
extern void trs_sound_init(int ioport, int vol);

extern int trs_rom_size;
extern int trs_rom1_size;
extern int trs_rom3_size;
extern int trs_rom4p_size;
extern unsigned char trs_rom1[];
extern unsigned char trs_rom3[];
extern unsigned char trs_rom4p[];

extern void trs_load_compiled_rom(int size, unsigned char rom[]);
extern void trs_load_rom(char *filename);

extern unsigned char trs_interrupt_latch_read(void);
extern unsigned char trs_nmi_latch_read(void);
extern void trs_interrupt_mask_write(unsigned char);
extern void trs_nmi_mask_write(unsigned char);
extern void trs_reset_button_interrupt(int state);
extern void trs_disk_intrq_interrupt(int state);
extern void trs_disk_drq_interrupt(int state);
extern void trs_timer_interrupt(int state);
extern void trs_timer_init(void);
extern void trs_timer_off(void);
extern void trs_timer_on(void);
extern void trs_timer_speed(int flag);

extern void trs_disk_change(int drive);
extern void trs_disk_change_all();

extern void mem_video_page(int which);
extern void mem_bank(int which);
extern void mem_map(int which);
extern void mem_romin(int state);

extern void trs_debug(void);

typedef void (*trs_event_func)(int arg);
void trs_schedule_event(trs_event_func f, int arg, int when);
void trs_do_event(void);
void trs_cancel_event(void);

void grafyx_write_x(int value);
void grafyx_write_y(int value);
void grafyx_write_data(int value);
int grafyx_read_data(void);
void grafyx_write_mode(int value);
