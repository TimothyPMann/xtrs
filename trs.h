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
   Modified by Timothy Mann, 1996-2008
   $Id$
*/

/*
 * trs.h
 */
#ifndef _TRS_H
#define _TRS_H

#include "z80.h"

#define NORMAL 0
#define EXPANDED 1
#define INVERSE 2
#define ALTERNATE 4

extern char *program_name;
extern int trs_model; /* 1, 3, 4, 5(=4p) */
extern int trs_paused;
extern int trs_autodelay;
void trs_suspend_delay(void);
void trs_restore_delay(void);
extern int trs_continuous; /* 1= run continuously,
			      0= enter debugger after instruction,
			     -1= suppress interrupt and enter debugger */
extern int trs_disk_debug_flags;
extern int trs_io_debug_flags;
extern int trs_emtsafe;

int trs_parse_command_line(int argc, char **argv, int *debug);

void trs_screen_init(void);
void trs_screen_write_char(int position, int char_index);
void trs_screen_expanded(int flag);
void trs_screen_alternate(int flag);
void trs_screen_80x24(int flag);
void trs_screen_inverse(int flag);
void trs_screen_scroll(void);
void trs_screen_refresh(void);

void trs_reset(int poweron);
void trs_exit(void);

void trs_kb_reset(void);
void trs_kb_bracket(int shifted);
int trs_kb_mem_read(int address);
int trs_next_key(int wait);
void trs_kb_heartbeat(void);
void trs_xlate_keysym(int keysym);
void queue_key(int key);
int dequeue_key(void);
void clear_key_queue(void);
void trs_skip_next_kbwait(void);
extern int stretch_amount;

void trs_get_event(int wait);
extern volatile int x_poll_count;

void trs_printer_write(int value);
int trs_printer_read(void);

void trs_cassette_motor(int value);
void trs_cassette_out(int value);
int trs_cassette_in(void);
void trs_cassette_select(int value);
void trs_sound_out(int value);

int trs_joystick_in(void);

extern int trs_rom_size;
extern int trs_rom1_size;
extern int trs_rom3_size;
extern int trs_rom4p_size;
extern unsigned char trs_rom1[];
extern unsigned char trs_rom3[];
extern unsigned char trs_rom4p[];

extern void trs_load_compiled_rom(int size, unsigned char rom[]);
extern void trs_load_rom(char *filename);

unsigned char trs_interrupt_latch_read(void);
unsigned char trs_nmi_latch_read(void);
void trs_interrupt_mask_write(unsigned char);
void trs_nmi_mask_write(unsigned char);
void trs_reset_button_interrupt(int state);
void trs_disk_intrq_interrupt(int state);
void trs_disk_drq_interrupt(int state);
void trs_disk_motoroff_interrupt(int state);
void trs_uart_err_interrupt(int state);
void trs_uart_rcv_interrupt(int state);
void trs_uart_snd_interrupt(int state);
void trs_timer_interrupt(int state);
void trs_timer_init(void);
void trs_timer_off(void);
void trs_timer_on(void);
void trs_timer_speed(int flag);
void trs_cassette_rise_interrupt(int dummy);
void trs_cassette_fall_interrupt(int dummy);
void trs_cassette_clear_interrupts(void);
int trs_cassette_interrupts_enabled(void);
void trs_cassette_update(int dummy);
extern int cassette_default_sample_rate;
void trs_orch90_out(int chan, int value);
void trs_cassette_reset(void);

const char *trs_disk_get_name(int drive);
int trs_disk_set_name(int drive, const char *newname);
int trs_disk_create(const char *newname);
void trs_disk_change_all(void);
void trs_disk_debug(void);
int trs_disk_motoroff(void);

void trs_change_all(void);

void mem_video_page(int which);
void mem_bank(int which);
void mem_map(int which);
void mem_romin(int state);

void trs_debug(void);

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
void grafyx_m3_reset();
int grafyx_m3_active(); /* true if currently intercepting video i/o */
void grafyx_m3_write_mode(int value);
unsigned char grafyx_m3_read_byte(int position);
int grafyx_m3_write_byte(int position, int value);
void hrg_onoff(int enable);
void hrg_write_addr(int addr, int mask);
void hrg_write_data(int data);
int hrg_read_data(void);

void trs_get_mouse_pos(int *x, int *y, unsigned int *buttons);
void trs_set_mouse_pos(int x, int y);
void trs_get_mouse_max(int *x, int *y, unsigned int *sens);
void trs_set_mouse_max(int x, int y, unsigned int sens);
int trs_get_mouse_type(void);

void stringy_init(void);
const char *stringy_get_name(int unit);
int stringy_set_name(int unit, const char *name);
int stringy_create(const char *name);
int stringy_in(int unit);
void stringy_out(int unit, int value);
void stringy_reset(void);
void stringy_change_all(void);

int put_twobyte(Ushort n, FILE* f);
int put_fourbyte(Uint n, FILE* f);
int get_twobyte(Ushort *n, FILE* f);
int get_fourbyte(Uint *n, FILE* f);


#endif /*_TRS_H*/
