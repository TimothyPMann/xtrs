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
   Last modified on Wed May 17 21:45:39 PDT 2000 by mann
*/

/*
 * trs.h
 */

#include "z80.h"

#define NORMAL 0
#define EXPANDED 1
#define INVERSE 2
#define ALTERNATE 4

extern int trs_model; /* 1, 3, 4, 5(=4p) */
extern int trs_paused;
extern int trs_autodelay;
void trs_suspend_delay(void);
void trs_restore_delay(void);
extern int trs_continuous; /* 1= run continuously,
			      0= enter debugger after instruction,
			     -1= suppress interrupt and enter debugger */

extern int trs_parse_command_line(int argc, char **argv, int *debug);

extern void trs_screen_init(void);
extern void trs_screen_write_char(int position, int char_index, int doflush);
extern void trs_screen_write_chars(int *locations, int *values, int count);
extern void trs_screen_expanded(int flag);
extern void trs_screen_alternate(int flag);
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
extern int trs_cassette_in(void);
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
extern void trs_disk_motoroff_interrupt(int state);
extern void trs_timer_interrupt(int state);
extern void trs_timer_init(void);
extern void trs_timer_off(void);
extern void trs_timer_on(void);
extern void trs_timer_speed(int flag);
extern void trs_cassette_rise_interrupt(int dummy);
extern void trs_cassette_fall_interrupt(int dummy);
extern void trs_cassette_clear_interrupts(void);
extern int trs_cassette_interrupts_enabled(void);
extern void trs_cassette_update(int dummy);
extern int cassette_default_sample_rate;
extern void trs_orch90_out(int chan, int value);
extern void trs_cassette_reset(void);

extern void trs_disk_change(int drive);
extern void trs_disk_change_all(void);
extern void trs_disk_debug(void);
extern int trs_disk_motoroff(void);

extern void mem_video_page(int which);
extern void mem_bank(int which);
extern void mem_map(int which);
extern void mem_romin(int state);

extern void trs_debug(void);

typedef void (*trs_event_func)(int arg);
void trs_schedule_event(trs_event_func f, int arg, int tstates);
void trs_schedule_event_us(trs_event_func f, int arg, int us);
void trs_do_event(void);
void trs_cancel_event(void);
trs_event_func trs_event_scheduled(void);

void grafyx_write_x(int value);
void grafyx_write_y(int value);
void grafyx_write_data(int value);
int grafyx_read_data(void);
void grafyx_write_mode(int value);
void grafyx_write_xoffset(int value);
void grafyx_write_yoffset(int value);
void grafyx_write_overlay(int value);
void grafyx_set_microlabs(int on_off);
int grafyx_get_microlabs(void);

void trs_get_mouse_pos(int *x, int *y, unsigned int *buttons);
void trs_set_mouse_pos(int x, int y);
void trs_get_mouse_max(int *x, int *y, unsigned int *sens);
void trs_set_mouse_max(int x, int y, unsigned int sens);
int trs_get_mouse_type(void);

void sb_set_volume(int vol);
int sb_get_volume(void);

