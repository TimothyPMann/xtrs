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
 * Portions copyright (c) 1996-2020, Timothy P. Mann
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*#define MOUSEDEBUG 1*/
/*#define XDEBUG 1*/
/*#define QDEBUG 1*/

/*
 * trs_xinterface.c
 *
 * X Windows interface for TRS-80 simulator
 */

#define _BSD_SOURCE /* for glibc version < 2.19 */
#define _DEFAULT_SOURCE /* string.h: strcasecmp() */
#define _XOPEN_SOURCE 500 /* string.h: strdup() */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/Xresource.h>

#include "trs_iodefs.h"
#include "trs.h"
#include "z80.h"
#include "trs_disk.h"
#include "trs_uart.h"
#include "trs_imp_exp.h"

#define DEF_FONT1	"-misc-fixed-medium-r-normal--20-200-75-75-*-100-iso8859-1"
#define DEF_WIDEFONT1	"-misc-fixed-medium-r-normal--20-200-75-75-*-200-iso8859-1"
#define DEF_FONT3	"-misc-fixed-medium-r-normal--20-200-75-75-*-100-iso8859-1"
#define DEF_WIDEFONT3	"-misc-fixed-medium-r-normal--20-200-75-75-*-200-iso8859-1"
#define DEF_USEFONT 0

extern char trs_char_data[][MAXCHARS][TRS_CHAR_HEIGHT];

#define EVENT_MASK \
  ExposureMask | KeyPressMask | MapRequest | KeyReleaseMask | \
  StructureNotifyMask | LeaveWindowMask

#define MAX_SCALE 4

/* Private data */
static unsigned char trs_screen[2048];
static int screen_chars = 1024;
static int row_chars = 64;
static int col_chars = 16;
static int resize = 0;
static int top_margin = 0;
static int left_margin = 0;
static int border_width = 2;
static Pixmap trs_char[2][MAXCHARS];
static Pixmap trs_box[2][64];
static Display *display;
static int screen;
static Window window;
static Window help_window;
static int unmap_help_window = 0;
static GC gc;
static GC gc_inv;
static GC gc_xor;
static int currentmode = NORMAL;
static int OrigHeight,OrigWidth;
static int usefont = DEF_USEFONT;
static int cur_char_width = TRS_CHAR_WIDTH;
static int cur_char_height = TRS_CHAR_HEIGHT * 2;
static int text80x24 = 0, screen640x240 = 0;
static XFontStruct *myfont, *mywidefont, *curfont;
static XKeyboardState repeat_state;
static int trs_charset;
static int scale_x = 1;
static int scale_y = 2;

static XrmOptionDescRec opts[] = {
/* Option */    /* Resource */  /* Value from arg? */   /* Value if no arg */
{"-iconic",     "*iconic",      XrmoptionNoArg,         (XPointer)""},
{"-background",	"*background",	XrmoptionSepArg,	(XPointer)NULL},
{"-bg",		"*background",	XrmoptionSepArg,	(XPointer)NULL},
{"-foreground",	"*foreground",	XrmoptionSepArg,	(XPointer)NULL},
{"-fg",		"*foreground",	XrmoptionSepArg,	(XPointer)NULL},
{"-borderwidth","*borderwidth",	XrmoptionSepArg,	(XPointer)NULL},
{"-usefont",	"*usefont",	XrmoptionNoArg,		(XPointer)"on"},
{"-nofont",	"*usefont",	XrmoptionNoArg,		(XPointer)"off"},
{"-font",	"*font",	XrmoptionSepArg,	(XPointer)NULL},
{"-widefont",	"*widefont",	XrmoptionSepArg,	(XPointer)NULL},
{"-display",	"*display",	XrmoptionSepArg,	(XPointer)NULL},
{"-debug",	"*debug",	XrmoptionNoArg,		(XPointer)"on"},
{"-nodebug",	"*debug",	XrmoptionNoArg,		(XPointer)"off"},
{"-romfile1",	"*romfile1",	XrmoptionSepArg,	(XPointer)NULL},
{"-romfile1x",	"*romfile1x",	XrmoptionSepArg,	(XPointer)NULL},
{"-romfile3",	"*romfile3",	XrmoptionSepArg,	(XPointer)NULL},
{"-romfile4p",	"*romfile4p",	XrmoptionSepArg,	(XPointer)NULL},
{"-resize",	"*resize",	XrmoptionNoArg,		(XPointer)"on"},
{"-noresize",	"*resize",	XrmoptionNoArg,		(XPointer)"off"},
{"-doublestep", "*doublestep",  XrmoptionNoArg,         (XPointer)"on"},
{"-nodoublestep","*doublestep", XrmoptionNoArg,         (XPointer)"off"},
{"-model",      "*model",       XrmoptionSepArg,	(XPointer)NULL},
{"-model1",     "*model",       XrmoptionNoArg,		(XPointer)"1"},
{"-model3",     "*model",       XrmoptionNoArg,		(XPointer)"3"},
{"-model4",     "*model",       XrmoptionNoArg,		(XPointer)"4"},
{"-model4p",    "*model",       XrmoptionNoArg,		(XPointer)"4p"},
{"-diskdir",    "*diskdir",     XrmoptionSepArg,	(XPointer)NULL},
{"-delay",      "*delay",       XrmoptionSepArg,	(XPointer)NULL},
{"-autodelay",  "*autodelay",   XrmoptionNoArg,         (XPointer)"on"},
{"-noautodelay","*autodelay",   XrmoptionNoArg,         (XPointer)"off"},
{"-keystretch", "*keystretch",  XrmoptionSepArg,        (XPointer)NULL},
{"-keydelay",   "*keydelay",    XrmoptionSepArg,        (XPointer)NULL},
{"-microlabs",  "*microlabs",   XrmoptionNoArg,         (XPointer)"on"},
{"-nomicrolabs","*microlabs",   XrmoptionNoArg,         (XPointer)"off"},
{"-doubler",    "*doubler",     XrmoptionSepArg,        (XPointer)NULL},
{"-sizemap",    "*sizemap",     XrmoptionSepArg,        (XPointer)NULL},
{"-stepmap",    "*stepmap",     XrmoptionSepArg,        (XPointer)NULL},
{"-charset",    "*charset",     XrmoptionSepArg,        (XPointer)NULL},
{"-truedam",    "*truedam",     XrmoptionNoArg,         (XPointer)"on"},
{"-notruedam",  "*truedam",     XrmoptionNoArg,         (XPointer)"off"},
{"-samplerate", "*samplerate",  XrmoptionSepArg,        (XPointer)NULL},
{"-title",      "*title",       XrmoptionSepArg,        (XPointer)NULL},
{"-scale",      "*scale",       XrmoptionSepArg,        (XPointer)NULL},
{"-scale1",     "*scale",       XrmoptionNoArg,         (XPointer)"1"},
{"-scale2",     "*scale",       XrmoptionNoArg,         (XPointer)"2"},
{"-scale3",     "*scale",       XrmoptionNoArg,         (XPointer)"3"},
{"-scale4",     "*scale",       XrmoptionNoArg,         (XPointer)"4"},
{"-serial",     "*serial",      XrmoptionSepArg,        (XPointer)NULL},
{"-switches",   "*switches",    XrmoptionSepArg,        (XPointer)NULL},
{"-shiftbracket","*shiftbracket",XrmoptionNoArg,        (XPointer)"on"},
{"-noshiftbracket","*shiftbracket",XrmoptionNoArg,      (XPointer)"off"},
{"-emtsafe",    "*emtsafe",     XrmoptionNoArg,         (XPointer)"on"},
{"-noemtsafe",  "*emtsafe",     XrmoptionNoArg,         (XPointer)"off"},
{"-lowercase",  "*lowercase",   XrmoptionNoArg,         (XPointer)"on"},
{"-nolowercase","*lowercase",   XrmoptionNoArg,         (XPointer)"off"},
{"-year",       "*year",        XrmoptionSepArg,        (XPointer)NULL},
};

static int num_opts = (sizeof opts / sizeof opts[0]);

/* Support for Micro Labs Grafyx Solution and Radio Shack hi-res card */

/* True size of graphics memory -- some is offscreen */
#define G_XSIZE 128
#define G_YSIZE 256
char grafyx[(2*G_YSIZE*MAX_SCALE) * (G_XSIZE*MAX_SCALE)];
unsigned char grafyx_unscaled[G_YSIZE][G_XSIZE];

unsigned char grafyx_microlabs = 0;
unsigned char grafyx_x = 0, grafyx_y = 0, grafyx_mode = 0;
unsigned char grafyx_enable = 0;
unsigned char grafyx_overlay = 0;
unsigned char grafyx_xoffset = 0, grafyx_yoffset = 0;

/* Port 0x83 (grafyx_mode) bits */
#define G_ENABLE    1
#define G_UL_NOTEXT 2   /* Micro Labs only */
#define G_RS_WAIT   2   /* Radio Shack only */
#define G_XDEC      4
#define G_YDEC      8
#define G_XNOCLKR   16
#define G_YNOCLKR   32
#define G_XNOCLKW   64
#define G_YNOCLKW   128

/* Port 0xFF (grafyx_m3_mode) bits */
#define G3_COORD    0x80
#define G3_ENABLE   0x40
#define G3_COMMAND  0x20
#define G3_YLOW(v)  (((v)&0x1e)>>1)     

XImage image = {
  /*width, height*/    8*G_XSIZE, 2*G_YSIZE,  /* if scale_x=1, scale_y=2 */
  /*xoffset*/          0,
  /*format*/           XYBitmap,
  /*data*/             (char*)grafyx,
  /*byte_order*/       LSBFirst,
  /*bitmap_unit*/      8,
  /*bitmap_bit_order*/ MSBFirst,
  /*bitmap_pad*/       8,
  /*depth*/            1,
  /*bytes_per_line*/   G_XSIZE,  /* if scale = 1 */
  /*bits_per_pixel*/   1,
  /*red_mask*/         1,
  /*green_mask*/       1,
  /*blue_mask*/        1,
  /*obdata*/           NULL,
  /*f*/                { NULL, NULL, NULL, NULL, NULL, NULL }
};

#define HRG_MEMSIZE (1024 * 12)	/* 12k * 8 bit graphics memory */
static unsigned char hrg_screen[HRG_MEMSIZE];
static int hrg_pixel_x[2][6+1];
static int hrg_pixel_y[12+1];
static int hrg_pixel_width[2][6];
static int hrg_pixel_height[12];
static int hrg_enable = 0;
static int hrg_addr = 0;
static void hrg_update_char(int position);

/* Private routines */
void bitmap_init(unsigned long foreground, unsigned long background);
void screen_init(void);

static XrmDatabase x_db = NULL;
static XrmDatabase command_db = NULL;
extern char *program_name;
char *title;

void trs_exit(void)
{
    exit(0);
}


int trs_parse_command_line(int argc, char **argv, int *debug)
{
  char option[512];
  char *type;
  XrmValue value;
  char *xrms, *tmp;
  int stepdefault, i, s[8];

  title = program_name; /* default */

  XrmInitialize();

  /* parse command line options */
  XrmParseCommand(&command_db,opts,num_opts,program_name,&argc,argv);

  (void) sprintf(option, "%s%s", program_name, ".display");
  (void) XrmGetResource(command_db, option, "Xtrs.Display", &type, &value);
  /* open display */
  if ( (display = XOpenDisplay (value.addr)) == NULL) {
    fatal("unable to open display");
    exit(-1);
  }

  /* get defaults from server */
  xrms = XResourceManagerString(display);
  if (xrms != NULL) {
    x_db = XrmGetStringDatabase(xrms);
    XrmMergeDatabases(command_db,&x_db);
  } else {
    x_db = command_db;
  }

  /* get defaults from $HOME/.Xdefaults and $HOME/Xtrs */
  if ((tmp = getenv("HOME"))) {
    sprintf(option, "%s/%s", tmp, ".Xdefaults");
    XrmCombineFileDatabase(option, &x_db, False);
    sprintf(option, "%s/%s", tmp, "Xtrs");
    XrmCombineFileDatabase(option, &x_db, False);
  }

  /* get defaults from /usr/X11/lib/X11/app-defaults/Xtrs */
#ifdef APPDEFAULTS
  sprintf(option, "%s/%s", APPDEFAULTS, "Xtrs");
  XrmCombineFileDatabase(option, &x_db, False);
#endif

  (void) sprintf(option, "%s%s", program_name, ".scale");
  if (XrmGetResource(x_db, option, "Xtrs.Scale", &type, &value)) 
  {
    if (sscanf(value.addr, "%d,%d", &scale_x, &scale_y) < 2)
      scale_y = scale_x * 2;
    if (scale_x <= 0) scale_x = 1;
    if (scale_x > MAX_SCALE) scale_x = MAX_SCALE;
    if (scale_y <= 0) scale_y = 1;
    if (scale_y > MAX_SCALE * 2) scale_y = MAX_SCALE * 2;
  }
  image.width *= scale_x;
  image.height = image.height * scale_y / 2;
  image.bytes_per_line *= scale_x;

  (void) sprintf(option, "%s%s", program_name, ".emtsafe");
  if (XrmGetResource(x_db, option, "Xtrs.Emtsafe", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      trs_emtsafe = True;
    } else if (strcmp(value.addr,"off") == 0) {
      trs_emtsafe = False;
    }
  }

  (void) sprintf(option, "%s%s", program_name, ".debug");
  if (XrmGetResource(x_db, option, "Xtrs.Debug", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      *debug = True;
    } else if (strcmp(value.addr,"off") == 0) {
      *debug = False;
    }
  }

  (void) sprintf(option, "%s%s", program_name, ".autodelay");
  if (XrmGetResource(x_db, option, "Xtrs.Autodelay", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      trs_autodelay = True;
    } else if (strcmp(value.addr,"off") == 0) {
      trs_autodelay = False;
    }
  }

  (void) sprintf(option, "%s%s", program_name, ".model");
  if (XrmGetResource(x_db, option, "Xtrs.Model", &type, &value)) {
    if (strcmp(value.addr, "1") == 0 ||
	strcasecmp(value.addr, "I") == 0) {
      trs_model = 1;
    } else if (strcmp(value.addr, "3") == 0 ||
	       strcasecmp(value.addr, "III") == 0) {
      trs_model = 3;
    } else if (strcmp(value.addr, "4") == 0 ||
	       strcasecmp(value.addr, "IV") == 0) {
      trs_model = 4;
    } else if (strcasecmp(value.addr, "4P") == 0 ||
	       strcasecmp(value.addr, "IVp") == 0) {
      trs_model = 5;
    } else {
      fatal("TRS-80 Model %s not supported", value.addr);
    }
  }

  /* !!Note: charset numbers must match trs_chars.c */
  if (trs_model == 1) {
    char *charset_name = "wider"; /* default */
    (void) sprintf(option, "%s%s", program_name, ".charset");
    if (XrmGetResource(x_db, option, "Xtrs.Charset", &type, &value)) {
      charset_name = (char*) value.addr;
    }
    if (isdigit((unsigned char)*charset_name)) {
      trs_charset = atoi(charset_name);
      cur_char_width = 8 * scale_x;
    } else {
      if (charset_name[0] == 'e'/*early*/) {
	trs_charset = 0;
	cur_char_width = 6 * scale_x;
      } else if (charset_name[0] == 's'/*stock*/) {
	trs_charset = 1;
	cur_char_width = 6 * scale_x;
      } else if (charset_name[0] == 'l'/*lcmod*/) {
	trs_charset = 2;
	cur_char_width = 6 * scale_x;
      } else if (charset_name[0] == 'w'/*wider*/) {
	trs_charset = 3;
	cur_char_width = 8 * scale_x;
      } else if (charset_name[0] == 'g'/*genie or german*/) {
	trs_charset = 10;
	cur_char_width = 8 * scale_x;
      } else {
	fatal("unknown charset name %s", value.addr);
      }
    }
    cur_char_height = TRS_CHAR_HEIGHT * scale_y;
  } else /* trs_model > 1 */ {
    char *charset_name;
    /* Set default */
    if (trs_model == 3) {
      charset_name = "katakana";
    } else {
      charset_name = "international";
    }
    (void) sprintf(option, "%s%s", program_name, ".charset");
    if (XrmGetResource(x_db, option, "Xtrs.Charset", &type, &value)) {
      charset_name = (char*) value.addr;
    }
    if (isdigit((unsigned char)*charset_name)) {
      trs_charset = atoi(charset_name);
    } else {
      if (charset_name[0] == 'k'/*katakana*/) {
	trs_charset = 4 + 3*(trs_model > 3);
      } else if (charset_name[0] == 'i'/*international*/) {
	trs_charset = 5 + 3*(trs_model > 3);
      } else if (charset_name[0] == 'b'/*bold*/) {
	trs_charset = 6 + 3*(trs_model > 3);
      } else {
	fatal("unknown charset name %s", value.addr);
      }
    }
    cur_char_width = TRS_CHAR_WIDTH * scale_x;
    cur_char_height = TRS_CHAR_HEIGHT * scale_y;
  }

  (void) sprintf(option, "%s%s", program_name, ".diskdir");
  if (XrmGetResource(x_db, option, "Xtrs.Diskdir", &type, &value)) {
    trs_disk_dir = strdup(value.addr);
  }
  if (trs_disk_dir[0] == '~' &&
      (trs_disk_dir[1] == '/' || trs_disk_dir[1] == '\0')) {
    char* home = getenv("HOME");
    if (home) {
      char *p = (char*)malloc(strlen(home) + strlen(trs_disk_dir) + 1);
      sprintf(p, "%s/%s", home, trs_disk_dir+1);
      trs_disk_dir = p;
    }
  }

  (void) sprintf(option, "%s%s", program_name, ".delay");
  if (XrmGetResource(x_db, option, "Xtrs.Delay", &type, &value)) {
    z80_state.delay = strtol(value.addr, NULL, 0);
  }

  (void) sprintf(option, "%s%s", program_name, ".keydelay");
  if (XrmGetResource(x_db, option, "Xtrs.Keydelay", &type, &value)) {
    trs_keydelay = strtol(value.addr, NULL, 0);
  }

  (void) sprintf(option, "%s%s", program_name, ".keystretch");
  if (XrmGetResource(x_db, option, "Xtrs.Keystretch", &type, &value)) {
    if (strchr(value.addr, ',')) {
      fatal("-keystretch now takes only one argument; see man page");
    }
    stretch_amount = strtol(value.addr, NULL, 0);
  }

  (void) sprintf(option, "%s%s", program_name, ".microlabs");
  if (XrmGetResource(x_db, option, "Xtrs.Microlabs", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      grafyx_set_microlabs(True);
    } else if (strcmp(value.addr,"off") == 0) {
      grafyx_set_microlabs(False);
    }
  }

  (void) sprintf(option, "%s%s", program_name, ".lowercase");
  if (XrmGetResource(x_db, option, "Xtrs.Lowercase", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      trs_lowercase = 1;
    } else if (strcmp(value.addr,"off") == 0) {
      trs_lowercase = 0;
    }
  }

  (void) sprintf(option, "%s%s", program_name, ".doubler");
  if (XrmGetResource(x_db, option, "Xtrs.Doubler", &type, &value)) {
    switch (*(char*)value.addr) {
    case 'p':
    case 'P':
      trs_disk_doubler = TRSDISK_PERCOM;
      break;
    case 'r':
    case 'R':
    case 't':
    case 'T':
      trs_disk_doubler = TRSDISK_TANDY;
      break;
    case 'b':
    case 'B':
    default:
      trs_disk_doubler = TRSDISK_BOTH;
      break;
    case 'n':
    case 'N':
      trs_disk_doubler = TRSDISK_NODOUBLER;
      break;
    }
  }

  /* Defaults for sizemap */
  s[0] = 5;
  s[1] = 5;
  s[2] = 5;
  s[3] = 5;
  s[4] = 8;
  s[5] = 8;
  s[6] = 8;
  s[7] = 8;
  (void) sprintf(option, "%s%s", program_name, ".sizemap");
  if (XrmGetResource(x_db, option, "Xtrs.Sizemap", &type, &value)) {
    sscanf((char*)value.addr, "%d,%d,%d,%d,%d,%d,%d,%d",
	   &s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6], &s[7]);
  }
  for (i=0; i<=7; i++) {
    if (s[i] != 5 && s[i] != 8) {
      fatal("bad value %d for disk %d size", s[i], i);
    } else {
      trs_disk_setsize(i, s[i]);
    }
  }

  /* Defaults for stepmap */
  (void) sprintf(option, "%s%s", program_name, ".doublestep");
  stepdefault = 1;
  if (XrmGetResource(x_db, option, "Xtrs.doublestep", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      stepdefault = 2;
    } else if (strcmp(value.addr,"off") == 0) {
      stepdefault = 1;
    }
  }

  for (i=0; i<=7; i++) {
    s[i] = stepdefault;
  }
  (void) sprintf(option, "%s%s", program_name, ".stepmap");
  if (XrmGetResource(x_db, option, "Xtrs.Stepmap", &type, &value)) {
    sscanf((char*)value.addr, "%d,%d,%d,%d,%d,%d,%d,%d",
	   &s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6], &s[7]);
  }
  for (i=0; i<=7; i++) {
    if (s[i] != 1 && s[i] != 2) {
      fatal("bad value %d for disk %d single/double step\n", s[i], i);
    } else {
      trs_disk_setstep(i, s[i]);
    }
  }

  (void) sprintf(option, "%s%s", program_name, ".truedam");
  if (XrmGetResource(x_db, option, "Xtrs.Truedam", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      trs_disk_truedam = True;
    } else if (strcmp(value.addr,"off") == 0) {
      trs_disk_truedam = False;
    }
  }

  (void) sprintf(option, "%s%s", program_name, ".samplerate");
  if (XrmGetResource(x_db, option, "Xtrs.Samplerate", &type, &value)) {
    cassette_default_sample_rate = strtol(value.addr, NULL, 0);
  }

  (void) sprintf(option, "%s%s", program_name, ".title");
  if (XrmGetResource(x_db, option, "Xtrs.title", &type, &value)) {
      title = strdup(value.addr);
  }

  (void) sprintf(option, "%s%s", program_name, ".serial");
  if (XrmGetResource(x_db, option, "Xtrs.serial", &type, &value)) {
      trs_uart_name = strdup(value.addr);
  }

  (void) sprintf(option, "%s%s", program_name, ".switches");
  if (XrmGetResource(x_db, option, "Xtrs.serial", &type, &value)) {
      trs_uart_switches = strtol(value.addr, NULL, 0);
  }

  (void) sprintf(option, "%s%s", program_name, ".shiftbracket");
  if (XrmGetResource(x_db, option, "Xtrs.Shiftbracket", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      trs_kb_bracket(True);
    } else if (strcmp(value.addr,"off") == 0) {
      trs_kb_bracket(False);
    }
  } else {
    trs_kb_bracket(trs_model >= 4);
  }

  (void) sprintf(option, "%s%s", program_name, ".year");
  if (XrmGetResource(x_db, option, "Xtrs.Year", &type, &value)) {
    trs_inityear(strtol(value.addr, NULL, 0));
  }

  return argc;
}

static void
save_repeat(void)
{
  XGetKeyboardControl(display, &repeat_state);
  XAutoRepeatOff(display);
  XSync(display, False);
}

static void
restore_repeat(void)
{
  if (repeat_state.global_auto_repeat == AutoRepeatModeOn) {
    XAutoRepeatOn(display);
    XSync(display, False);
  }
}

void trs_fix_size (Window window, int width, int height)
{
  XSizeHints sizehints;

  sizehints.flags = PMinSize | PMaxSize | PBaseSize;
  sizehints.min_width = width;
  sizehints.max_width = width;
  sizehints.base_width = width;

  sizehints.min_height = height;
  sizehints.max_height = height;
  sizehints.base_height = height;

  XSetWMNormalHints(display, window, &sizehints);
}


/*
 * show help
 */

static void show_fallback_help(char const **helpitems) {
  int i;
  for (i = 0; NULL != helpitems[i]; i++) {
    (void) fprintf(stderr, "%s\n", helpitems[i]);
  }
}

void trs_show_help(void)
{
  XWindowAttributes parent_attrs;
  int const help_width = 495;
  int const help_height = 365;
  unsigned int help_xpos, help_ypos;
  unsigned long foreground, background;
  GC help_gc;
  XGCValues values;
  static char const *help_font_name = "9x15";
  XFontStruct *help_font_info;
  int font_height;
  int text_line;
  int i;
  static char const *helpitems[] = {
    "F1: Model 4/4P F1 key",
    "F2: Model 4/4P F2 key",
    "F3: Model 4/4P F3 key",
    "F4: Model 4/4P caps lock key",
    "F5: TRS-80 @ key",
    "F6: TRS-80 0 key",
    "F7: Signal disk change in emulated floppy drive",
    "F8: Exit emulator",
    "F9: Enter zbx debugger",
    "F10: TRS-80 reset button",
    "Shift+F10: Hard reset",
    "",
    "LeftArrow, Backspace, Delete: TRS-80 left arrow key",
    "RightArrow, Tab: TRS-80 right arrow key",
    "UpArrow: TRS-80 up arrow key",
    "DownArrow, Linefeed: TRS-80 down arrow key",
    "",
    "Esc, Break: TRS-80 break key",
    "Home, Clear, LeftMeta: TRS-80 clear key",
    "Control: Model 4/4P control key",
    "RightMeta: Shifted TRS-80 down arrow key",
    "",
    "F11 toggles this help.",
    "See the xtrs(1) manual page for more information.",
    NULL
  };

  foreground = BlackPixel(display, screen);
  background = WhitePixel(display, screen);
  (void) XGetWindowAttributes(display, window, &parent_attrs);
  if ((parent_attrs.width < help_width)
      || (parent_attrs.height < help_height)) {
    error("cannot display help window; parent window too small to contain it");
    show_fallback_help(helpitems);
    return;
  }

  help_xpos = (parent_attrs.width - help_width) / 2;
  help_ypos = (parent_attrs.height - help_height) / 2;
  help_window = XCreateSimpleWindow(display, window, help_xpos, help_ypos,
				    help_width, help_height, 1, foreground,
				    background);
  help_font_info = XLoadQueryFont(display, help_font_name);

  if (NULL == help_font_info) {
    size_t const errbufsize = 256;
    char errbuf[errbufsize];
    (void) snprintf(errbuf, errbufsize, "cannot display help window; cannot"
                    " open font \"%s\"", help_font_name);
    error(errbuf);
    show_fallback_help(helpitems);
    return;
  }

  help_gc = XCreateGC(display, help_window, 0, &values);
  XSetFont(display, help_gc, help_font_info->fid);
  XSetBackground(display, help_gc, WhitePixel(display,screen));
  XSetForeground(display, help_gc, BlackPixel(display,screen));

  font_height = help_font_info->ascent + help_font_info->descent;
  text_line = font_height;

  XMapWindow(display, help_window);

  for (i = 0; NULL != helpitems[i]; i++) {
    size_t len;

    len = strlen(helpitems[i]);
    XDrawString(display, help_window, help_gc, 10, text_line, helpitems[i],
		len);
    text_line += font_height;
  }

  return;
}

/* exits if something really bad happens */
void trs_screen_init(void)
{
  Window root_window;
  unsigned long fore_pixel, back_pixel, foreground, background;
  char option[512];
  char *type;
  XrmValue value;
  Colormap color_map;
  XColor cdef;
  XGCValues gcvals;
  Atom wm_delete_window;
  char *fontname = NULL;
  char *widefontname = NULL;
  int len;

  screen = DefaultScreen(display);
  color_map = DefaultColormap(display,screen);
  wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);

  (void) sprintf(option, "%s%s", program_name, ".foreground");
  if (XrmGetResource(x_db, option, "Xtrs.Foreground", &type, &value)) {
    /* debug("foreground is %s\n",value.addr); */
    XParseColor(display, color_map, value.addr, &cdef);
    XAllocColor(display, color_map, &cdef);
    fore_pixel = cdef.pixel;
  } else {
    fore_pixel = WhitePixel(display, screen);
  }

  (void) sprintf(option, "%s%s", program_name, ".background");
  if (XrmGetResource(x_db, option, "Xtrs.Background", &type, &value)) {
    XParseColor(display, color_map, value.addr, &cdef);
    XAllocColor(display, color_map, &cdef);
    back_pixel = cdef.pixel;
  } else {
    back_pixel = BlackPixel(display, screen);
  }

  (void) sprintf(option, "%s%s", program_name, ".borderwidth");
  if (XrmGetResource(x_db, option, "Xtrs.Borderwidth", &type, &value)) {
    if ((border_width = atoi(value.addr)) < 0)
      border_width = 0;
  } else {
    border_width = 2;
  }

  (void) sprintf(option, "%s%s", program_name, ".usefont");
  if (XrmGetResource(x_db, option, "Xtrs.Usefont", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      usefont = 1;
    } else if (strcmp(value.addr,"off") == 0) {
      usefont = 0;
    }
  }

  if (usefont) {
    (void) sprintf(option, "%s%s", program_name, ".font");
    if (XrmGetResource(x_db, option, "Xtrs.Font", &type, &value)) {
      len = strlen(value.addr);
      fontname = malloc(len + 1);
      strcpy(fontname,value.addr);
    } else {
      char *def_font = (trs_model == 1 ? DEF_FONT1 : DEF_FONT3);
      len = strlen(def_font);
      fontname = malloc(len+1);
      strcpy(fontname, def_font);
    }
    (void) sprintf(option, "%s%s", program_name, ".widefont");
    if (XrmGetResource(x_db, option, "Xtrs.Widefont", &type, &value)) {
      len = strlen(value.addr);
      widefontname = malloc(len + 1);
      strcpy(widefontname,value.addr);
    } else {
      char *def_widefont =
	(trs_model == 1 ? DEF_WIDEFONT1 : DEF_WIDEFONT3);
      len = strlen(def_widefont);
      widefontname = malloc(len+1);
      strcpy(widefontname, def_widefont);
    }
  }

  switch (trs_model) {
  case 1:
    (void) sprintf(option, "%s%s", program_name, ".romfile1");
    if (XrmGetResource(x_db, option, "Xtrs.Romfile1", &type, &value)) {
        romfile1 = value.addr;
    }
    (void) sprintf(option, "%s%s", program_name, ".romfile1x");
    if (XrmGetResource(x_db, option, "Xtrs.Romfile1x", &type, &value)) {
        romfile1x = value.addr;
    }
    break;
  case 3: case 4:
    (void) sprintf(option, "%s%s", program_name, ".romfile3");
    if (XrmGetResource(x_db, option, "Xtrs.Romfile3", &type, &value)) {
        romfile3 = value.addr;
    }
    break;
  default: /* 4P */
    (void) sprintf(option, "%s%s", program_name, ".romfile4p");
    if (XrmGetResource(x_db, option, "Xtrs.Romfile4p", &type, &value)) {
        romfile4p = value.addr;
    }
    break;
  }

  (void) sprintf(option, "%s%s", program_name, ".resize");
  if (XrmGetResource(x_db, option, "Xtrs.Resize", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      resize = 1;
    } else if (strcmp(value.addr,"off") == 0) {
      resize = 0;
    }
  } else {
    resize = (trs_model == 3);
  }

  clear_key_queue();		/* init the key queue */

  /* setup root window, and gc */
  root_window = DefaultRootWindow(display);

  /* save keyboard repeat state */
  XGetKeyboardControl(display, &repeat_state);
  atexit(restore_repeat);

  foreground = fore_pixel;
  background = back_pixel;
  gcvals.graphics_exposures = False;

  gc = XCreateGC(display, root_window, GCGraphicsExposures, &gcvals);
  XSetForeground(display, gc, fore_pixel);
  XSetBackground(display, gc, back_pixel);

  gc_inv = XCreateGC(display, root_window, GCGraphicsExposures, &gcvals);
  XSetForeground(display, gc_inv, back_pixel);
  XSetBackground(display, gc_inv, fore_pixel);

  gc_xor = XCreateGC(display, root_window, GCGraphicsExposures, &gcvals);
  XSetForeground(display, gc_xor, back_pixel^fore_pixel);
  XSetBackground(display, gc_xor, 0);
  XSetFunction(display, gc_xor, GXxor);

  if (usefont) {
    if ((myfont = XLoadQueryFont(display,fontname)) == NULL) {
      fatal("can't open font %s!\n", fontname);
    }
    if ((mywidefont = XLoadQueryFont(display,widefontname)) == NULL) {
      fatal("can't open font %s!", widefontname);
    }
    curfont = myfont;
    XSetFont(display,gc,myfont->fid);
    XSetFont(display,gc_inv,myfont->fid);
    cur_char_width =  myfont->max_bounds.width;
    cur_char_height = myfont->ascent + myfont->descent;
  }

  if (trs_model >= 3 && !resize) {
    OrigWidth = cur_char_width * 80 + 2 * border_width;
    left_margin = cur_char_width * (80 - row_chars)/2 + border_width;
    if (usefont) {
      OrigHeight = cur_char_height * 24 + 2 * border_width;
      top_margin = cur_char_height * (24 - col_chars)/2 + border_width;
    } else {
      OrigHeight = TRS_CHAR_HEIGHT4 * scale_y * 24 + 2 * border_width;
      top_margin = (TRS_CHAR_HEIGHT4 * scale_y * 24 -
		    cur_char_height * col_chars)/2 + border_width;
    }
  } else {
    OrigWidth = cur_char_width * row_chars + 2 * border_width;
    left_margin = border_width;
    OrigHeight = cur_char_height * col_chars + 2 * border_width;
    top_margin = border_width;
  }
  window = XCreateSimpleWindow(display, root_window, 400, 400,
			       OrigWidth, OrigHeight, 1, foreground,
			       background);
#if XDEBUG
    debug("XCreateSimpleWindow(%d, %d)\n", OrigWidth, OrigHeight);
#endif /*XDEBUG*/
  trs_fix_size(window, OrigWidth, OrigHeight);
  XStoreName(display,window,title);
  XSelectInput(display, window, EVENT_MASK);
  XSetWMProtocols(display, window, &wm_delete_window, 1);

  (void) sprintf(option, "%s%s", program_name, ".iconic"); 
  if (XrmGetResource(x_db, option, "Xtrs.Iconic", &type, &value)) { 
    XWMHints * hints = XAllocWMHints(); 
    hints->flags = StateHint; 
    hints->initial_state = IconicState; 
    XSetWMHints(display, window, hints); 
    XFree(hints); 
  }

  XMapWindow(display, window);
  bitmap_init(foreground, background);
  screen_init();
  XClearWindow(display,window);
}

KeySym last_key[256];

/* 
 * Get and process X event(s).
 *
 *   If wait is true, we think there's nothing to do right now and
 *   want to give up the CPU until there is something to do.
 *
 *   If wait is false we definitely don't want to block for events.
 *
 * Handle interrupt-driven uart input here too.
 */
void trs_get_event(int wait)
{
  XEvent event;
  KeySym key;
  char buf[10];
  XComposeStatus status;

  /*
   * Of Enter and Leave events, track which kind we saw last.  X clients
   * like "unclutter" will synthesize an Enter event on their own.
   *
   * Historically, xtrs assumed that every Enter would be preceded by a
   * Leave.  When this was not the case, xtrs would clobber the global
   * autorepeat state to off; it wrongly stored the off state because the
   * xtrs window already had focus.
   *
   * This enum is tri-valued with the zero value being UNDEF because we
   * don't want to fake a Leave event the first time we Enter; that would
   * cause restore_repeat() to read from repeat_state before it has been
   * initialized, possibly storing a bogus global autorepeat state.
   */
  enum enter_leave_t { UNDEF, ENTER, LEAVE };
  static enum enter_leave_t enter_leave;

  if (trs_model > 1) {
    (void)trs_uart_check_avail();
  }

  if (wait) {
    pause();
    trs_paused = 1;
  }

  do {
    if (XCheckTypedEvent(display, ClientMessage, &event)) trs_exit();
    if (!XCheckMaskEvent(display, ~0, &event)) return;

    switch(event.type) {
    case Expose:
#if XDEBUG
      debug("Expose\n");
#endif
      if (event.xexpose.count == 0) {
	while (XCheckMaskEvent(display, ExposureMask, &event)) /*skip*/;
	trs_screen_refresh();
      }
      break;

    case MapNotify:
#if XDEBUG
      debug("MapNotify\n");
#endif
      trs_screen_refresh();
      break;

    case EnterNotify:
#if XDEBUG
      debug("EnterNotify\n");
#endif
      if (enter_leave == ENTER) {
	/*
	 * The last Enter/Leave event we saw was an Enter; pretend we saw a
	 * Leave event first.
	 */
#if XDEBUG
	debug("faking a LeaveNotify event first\n");
#endif
	restore_repeat();
      }
      enter_leave = ENTER;
      save_repeat();
      trs_xlate_keysym(0x10000); /* all keys up */
      break;

    case LeaveNotify:
#if XDEBUG
      debug("LeaveNotify\n");
#endif
      enter_leave = LEAVE;
      restore_repeat();
      trs_xlate_keysym(0x10000); /* all keys up */
      break;

    case KeyPress:
      (void) XLookupString((XKeyEvent *)&event,buf,10,&key,&status);
#if XDEBUG
      debug("KeyPress: state 0x%x, keycode 0x%x, key 0x%x\n",
	      event.xkey.state, event.xkey.keycode, (unsigned int) key);
#endif
      switch (key) {
	/* Trap some function keys here */
      case XK_F11:
	if (!help_window) {
	  trs_show_help();
	} else {
	  /*
	   * Work around the most painful case of
	   * http://bugs.freedesktop.org/show_bug.cgi?id=21454.  This
	   * bug causes infinite key repeat if a key is down when an
	   * application disables key repeat.  When we unmap the help
	   * window, that causes an EnterNotify event for the main
	   * window.  In the EnterNotify event handler we disable key
	   * repeat, which will cause F11 to repeat infinitely if it
	   * is still down.  So we wait until F11 is released before
	   * unmapping the window.
	   */
	  unmap_help_window = 1;
	}
	key = 0;
	trs_skip_next_kbwait();
	break;
      case XK_F10:
	if ((event.xkey.state & ShiftMask) == ShiftMask)
	  trs_reset(1);
	else
	  trs_reset(0);
	key = 0;
	trs_skip_next_kbwait();
	break;
      case XK_F9:
	trs_debug();
	key = 0;
	trs_skip_next_kbwait();
	break;
      case XK_F8:
	trs_exit();
	key = 0;
	trs_skip_next_kbwait();
	break;
      case XK_F7:
	trs_change_all();
	key = 0;
	trs_skip_next_kbwait();
	break;
      default:
	break;
      }
      if ( ((event.xkey.state & (ShiftMask|LockMask))
	    == (ShiftMask|LockMask))
	  && key >= 'A' && key <= 'Z' ) {
	/* Make Shift + CapsLock give lower case */
	key = (int) key + 0x20;
      }
      if (key == XK_Shift_R && trs_model == 1) {
	key = XK_Shift_L;
      }
      if (last_key[event.xkey.keycode] != 0) {
	trs_xlate_keysym(0x10000 | last_key[event.xkey.keycode]);
      }
      last_key[event.xkey.keycode] = key;
      if (key != 0) {
	trs_xlate_keysym(key);
      }
      break;

    case KeyRelease:
      if (unmap_help_window) {
	/* See comment under KeyPress XK_F11 case above. */
	XUnmapWindow(display, help_window);
	help_window = 0;
	unmap_help_window = 0;
      }
      key = last_key[event.xkey.keycode];
      last_key[event.xkey.keycode] = 0;
      if (key != 0) {
	trs_xlate_keysym(0x10000 | key);
      }
#if XDEBUG
      debug("KeyRelease: state 0x%x, keycode 0x%x, last_key 0x%x\n",
	    event.xkey.state, event.xkey.keycode, (unsigned int) key);
#endif
      break;

    default:
#if XDEBUG	    
      debug("Unhandled event: type %d\n", event.type);
#endif
      break;
    }
  } while (!wait);
}

void trs_screen_expanded(int flag)
{
  int bit = flag ? EXPANDED : 0;
  if ((currentmode ^ bit) & EXPANDED) {
    currentmode ^= EXPANDED;
    if (usefont) {
      curfont = (flag ? mywidefont : myfont);
      XSetFont(display,gc,curfont->fid);
      XSetFont(display,gc_inv,curfont->fid);
    }
    XClearWindow(display,window);
    trs_screen_refresh();
  }
}

void trs_screen_inverse(int flag)
{
  int bit = flag ? INVERSE : 0;
  int i;
  if ((currentmode ^ bit) & INVERSE) {
    currentmode ^= INVERSE;
    for (i = 0; i < screen_chars; i++) {
      if (trs_screen[i] & 0x80)
	trs_screen_write_char(i, trs_screen[i]);
    }
  }
}

void trs_screen_alternate(int flag)
{
  int bit = flag ? ALTERNATE : 0;
  int i;
  if ((currentmode ^ bit) & ALTERNATE) {
    currentmode ^= ALTERNATE;
    for (i = 0; i < screen_chars; i++) {
      if (trs_screen[i] >= 0xc0)
	trs_screen_write_char(i, trs_screen[i]);
    }
  }
}

void trs_screen_640x240(int flag)
{
  if (flag == screen640x240) return;
  screen640x240 = flag;
  if (flag) {
    row_chars = 80;
    col_chars = 24;
    if (!usefont) cur_char_height = TRS_CHAR_HEIGHT4 * scale_y;
  } else {
    row_chars = 64;
    col_chars = 16;
    if (!usefont) cur_char_height = TRS_CHAR_HEIGHT * scale_y;
  }
  screen_chars = row_chars * col_chars;
  if (resize) {
    OrigWidth = cur_char_width * row_chars + 2 * border_width;
    OrigHeight = cur_char_height * col_chars + 2 * border_width;
    left_margin = border_width;
    top_margin = border_width;
    trs_fix_size(window, OrigWidth, OrigHeight);
    XResizeWindow(display, window, OrigWidth, OrigHeight);
    XClearWindow(display,window);
    XFlush(display);
#if XDEBUG
    debug("XResizeWindow(%d, %d)\n", OrigWidth, OrigHeight);
#endif /*XDEBUG*/
  } else {
    left_margin = cur_char_width * (80 - row_chars)/2 + border_width;
    if (usefont) {
      top_margin = cur_char_height * (24 - col_chars)/2 + border_width;
    } else {
      top_margin = (TRS_CHAR_HEIGHT4 * scale_y * 24 -
		    cur_char_height * col_chars)/2 + border_width;
    }
    if (left_margin > border_width || top_margin > border_width) {
      XClearWindow(display,window);
    }
  }
  trs_screen_refresh();
}

void trs_screen_80x24(int flag)
{
  if (!grafyx_enable || grafyx_overlay) {
    trs_screen_640x240(flag);
  }
  text80x24 = flag;
}

void screen_init(void)
{
  unsigned i;

  /* initially, screen is blank (i.e. full of spaces) */
  for (i = 0; i < sizeof(trs_screen); i++)
    trs_screen[i] = ' ';
}

void
boxes_init(int foreground, int background, int width, int height, int expanded)
{
  int graphics_char, bit, p;
  XRectangle bits[6];
  XRectangle cur_bits[6];

  /*
   * Calculate what the 2x3 boxes look like.
   */
  bits[0].x = bits[2].x = bits[4].x = 0;
  bits[0].width = bits[2].width = bits[4].width =
    bits[1].x = bits[3].x = bits[5].x =  width / 2;
  bits[1].width = bits[3].width = bits[5].width = width - bits[1].x;

  bits[0].y = bits[1].y = 0;
  bits[0].height = bits[1].height =
    bits[2].y = bits[3].y = height / 3;
  bits[4].y = bits[5].y = (height * 2) / 3;
  bits[2].height = bits[3].height = bits[4].y - bits[2].y;
  bits[4].height = bits[5].height = height - bits[4].y;

  for (graphics_char = 0; graphics_char < 64; ++graphics_char) {
    trs_box[expanded][graphics_char] =
      XCreatePixmap(display, window, width, height,
		    DefaultDepth(display, screen));

    /* Clear everything */
    XSetForeground(display, gc, background);
    XFillRectangle(display, trs_box[expanded][graphics_char],
		   gc, 0, 0, width, height);

    /* Set the bits */
    XSetForeground(display, gc, foreground);

    for (bit = 0, p = 0; bit < 6; ++bit) {
      if (graphics_char & (1 << bit)) {
	cur_bits[p++] = bits[bit];
      }
    }
    XFillRectangles(display, trs_box[expanded][graphics_char],
		    gc, cur_bits, p);
  }
}

/* DPL 20000129
 * This routine creates a rescaled character bitmap, and then
 * calls XCreateBitmapFromData. It then can be used pretty much
 * as a drop-in replacement for XCreateBitmapFromData. 
 */
Pixmap XCreateBitmapFromDataScale(Display *display, Drawable window,
			             char *data, unsigned int width, 
				     unsigned int height,
				     unsigned int scale_x,
				     unsigned int scale_y)
{
  static unsigned char *mydata;
  static unsigned char *mypixels;
  int i, j, k;

  if (mydata == NULL)
  {
    /* 
     * Allocate a bit more room than necessary - There shouldn't be 
     * any proportional characters, but just in case...             
     * These arrays never get released, but they are really not     
     * too big, so we should be OK.
     */
    mydata = (unsigned char *)malloc(width * height *
				     scale_x * scale_y * 4);
    mypixels= (unsigned char *)malloc(width * height * 4);
  }
  
  /* Read the character data */ 
  for (j= 0; j< (int)(width * height); j += 8)
  {
    for (i= j + 7; i >= j; i--)
    {
      *(mypixels + i)= (*(data + (j >> 3)) >> (i - j)) & 1;
    }
  }

  /* Clear out the rescaled character array */
  for (i= 0; i< (int)(width / 8 * height * scale_x * scale_y * 4); i++)
  {
    *(mydata + i)= 0;
  }
  
  /* And prepare our rescaled character. */
  k= 0;
  for (j= 0; j< (int)(height * scale_y); j++)
  {
    for (i= 0; i< (int)(width * scale_x); i++)
    {
       *(mydata + (k >> 3)) = *(mydata + (k >> 3)) |
	 (*(mypixels + ((int)(j / scale_y) * width) +
	    (int)(i / scale_x)) << (k & 7));
       k++;
    }
  }

  return XCreateBitmapFromData(display,window,
			      (char*)mydata,
			      width * scale_x, height * scale_y);
}

void bitmap_init(unsigned long foreground, unsigned long background)
{
  if (usefont) {
    int dwidth, dheight;

    boxes_init(foreground, background,
	       cur_char_width, cur_char_height, 0);
    dwidth = 2*cur_char_width;
    dheight = 2*cur_char_height;
    if (dwidth > mywidefont->max_bounds.width) {
      dwidth = mywidefont->max_bounds.width;
    }
    if (dheight > mywidefont->ascent + mywidefont->descent) {
      dheight = mywidefont->ascent + mywidefont->descent;
    }
    boxes_init(foreground, background, dwidth, dheight, 1);
  } else {
    /* Initialize from built-in font bitmaps. */
    int i;
	
    for (i = 0; i < MAXCHARS; i++) {
      trs_char[0][i] =
	XCreateBitmapFromDataScale(display,window,
				   trs_char_data[trs_charset][i],
				   TRS_CHAR_WIDTH,TRS_CHAR_HEIGHT,
				   scale_x,scale_y);
      trs_char[1][i] =
	XCreateBitmapFromDataScale(display,window,
				   trs_char_data[trs_charset][i],
				   TRS_CHAR_WIDTH,TRS_CHAR_HEIGHT,
				   scale_x*2,scale_y);
    }
    boxes_init(foreground, background,
	       cur_char_width, TRS_CHAR_HEIGHT * scale_y, 0);
    boxes_init(foreground, background,
	       cur_char_width*2, TRS_CHAR_HEIGHT * scale_y, 1);
  }
}

void trs_screen_refresh(void)
{
  int i, srcx, srcy, dunx, duny;

#if XDEBUG
  debug("trs_screen_refresh\n");
#endif
  if (grafyx_enable && !grafyx_overlay) {
    srcx = cur_char_width * grafyx_xoffset;
    srcy = scale_y * grafyx_yoffset;
    XPutImage(display, window, gc, &image,
	      srcx, srcy,
	      left_margin, top_margin,
	      cur_char_width*row_chars,
	      cur_char_height*col_chars);
    /* Draw wrapped portions if any */
    dunx = image.width - srcx;
    if (dunx < cur_char_width*row_chars) {
      XPutImage(display, window, gc, &image,
		0, srcy,
		left_margin + dunx, top_margin,
		cur_char_width*row_chars - dunx,
		cur_char_height*col_chars);
    }
    duny = image.height - srcy;
    if (duny < cur_char_height*col_chars) {
      XPutImage(display, window, gc, &image,
		srcx, 0,
		left_margin, top_margin + duny,
		cur_char_width*row_chars,
		cur_char_height*col_chars - duny);
      if (dunx < cur_char_width*row_chars) {
	XPutImage(display, window, gc, &image,
		  0, 0,
		  left_margin + dunx, top_margin + duny,
		  cur_char_width*row_chars - dunx,
		  cur_char_height*col_chars - duny);
      }
    }
  } else {
    for (i = 0; i < screen_chars; i++) {
      trs_screen_write_char(i, trs_screen[i]);
    }
  }
}

void trs_screen_write_char(int position, int char_index)
{
  int row,col,destx,desty;
  int plane;
  char temp_char;

  trs_screen[position] = char_index;
  if (position >= screen_chars) {
    return;
  }
  if ((currentmode & EXPANDED) && (position & 1)) {
    return;
  }
  if (grafyx_enable && !grafyx_overlay) {
    return;
  }
  row = position / row_chars;
  col = position - (row * row_chars);
  destx = col * cur_char_width + left_margin;
  desty = row * cur_char_height + top_margin;

  if (trs_model == 1 && char_index >= 0xc0) {
    /* On Model I, 0xc0-0xff is another copy of 0x80-0xbf */
    char_index -= 0x40;
  }
  if (char_index >= 0x80 && char_index <= 0xbf && !(currentmode & INVERSE)) {
    /* Use graphics character bitmap instead of font */
    switch (currentmode & EXPANDED) {
    case NORMAL:
      XCopyArea(display,
		trs_box[0][char_index-0x80],window,gc,0,0,
		cur_char_width,cur_char_height,destx,desty);
      break;
    case EXPANDED:
      /* use expanded graphics character bitmap instead of font */
      XCopyArea(display,
		trs_box[1][char_index-0x80],window,gc,0,0,
		cur_char_width*2,cur_char_height,destx,desty);
      break;
    } 
  } else if (usefont) {
    /* Draw character using a font */
    if (trs_model == 1 && trs_lowercase) {
      /* Emulate Radio Shack lowercase mod.  The replacement character
	 generator ROM had another copy of the uppercase characters in
	 the control character positions, to compensate for a bug in the
	 Level II ROM that stores such values instead of uppercase. */
      if (char_index < 0x20) char_index += 0x40;
    }
    if (trs_model > 1 && char_index >= 0xc0 &&
	(currentmode & (ALTERNATE+INVERSE)) == 0) {
      char_index -= 0x40;
    }
    desty += curfont->ascent;
    if (currentmode & INVERSE) {
      temp_char = (char)(char_index & 0x7f);
      XDrawImageString(display, window,
		       (char_index & 0x80) ? gc_inv : gc,
		       destx, desty, &temp_char, 1);
    } else {
      temp_char = (char)char_index;
      XDrawImageString(display, window, gc,
		       destx, desty, &temp_char, 1);
    }
  } else {
    /* Draw character using a builtin bitmap */
    if (trs_model > 1 && char_index >= 0xc0 &&
	(currentmode & (ALTERNATE+INVERSE)) == 0) {
      char_index -= 0x40;
    }
    plane = 1;
    switch (currentmode & ~ALTERNATE) {
    case NORMAL:
      XCopyPlane(display,trs_char[0][char_index],
		 window,gc,0,0,cur_char_width,
		 cur_char_height,destx,desty,plane);
      break;
    case EXPANDED:
      XCopyPlane(display,trs_char[1][char_index],
		 window,gc,0,0,cur_char_width*2,
		 cur_char_height,destx,desty,plane);
      break;
    case INVERSE:
      XCopyPlane(display,trs_char[0][char_index & 0x7f],window,
		 (char_index & 0x80) ? gc_inv : gc,
		 0,0,cur_char_width,cur_char_height,destx,desty,plane);
      break;
    case EXPANDED+INVERSE:
      XCopyPlane(display,trs_char[1][char_index & 0x7f],window,
		 (char_index & 0x80) ? gc_inv : gc,
		 0,0,cur_char_width*2,cur_char_height,destx,desty,plane);
      break;
    }
  }
  if (grafyx_enable) {
    /* assert(grafyx_overlay); */
    int srcx, srcy, duny;
    srcx = ((col+grafyx_xoffset) % G_XSIZE)*cur_char_width;
    srcy = (row*cur_char_height + grafyx_yoffset*scale_y)
	   % (G_YSIZE*scale_y); 
    XPutImage(display, window, gc_xor, &image, srcx, srcy,
	      destx, desty, cur_char_width, cur_char_height);
    /* Draw wrapped portion if any */
    duny = image.height - srcy;
    if (duny < cur_char_height) {
      XPutImage(display, window, gc_xor, &image,
		srcx, 0,
		destx, desty + duny,
		cur_char_width, cur_char_height - duny);
    }
  }
  if (hrg_enable) {
    hrg_update_char(position);
  }
}

 /* Copy lines 1 through col_chars-1 to lines 0 through col_chars-2.
    Doesn't need to clear line col_chars-1. */
void trs_screen_scroll(void)
{
  int i = 0;

  for (i = row_chars; i < screen_chars; i++)
    trs_screen[i-row_chars] = trs_screen[i];

  if (grafyx_enable) {
    if (grafyx_overlay) {
      trs_screen_refresh();
    }
  } else if (hrg_enable) {
    trs_screen_refresh();
  } else {
    XCopyArea(display,window,window,gc,
	      left_margin,cur_char_height+top_margin,
	      (cur_char_width*row_chars),(cur_char_height*col_chars),
	      left_margin,top_margin);
  }
}

void grafyx_write_byte(int x, int y, char byte)
{
  int i, j;
  char exp[MAX_SCALE] = { 0 };
  int screen_x = ((x - grafyx_xoffset + G_XSIZE) % G_XSIZE);
  int screen_y = ((y - grafyx_yoffset + G_YSIZE) % G_YSIZE);
  int on_screen = screen_x < row_chars &&
    screen_y < col_chars*cur_char_height/scale_y;

  if (grafyx_enable && grafyx_overlay && on_screen) {
    /* Erase old byte, preserving text */
    XPutImage(display, window, gc_xor, &image,
	      x*cur_char_width, y*scale_y,
	      left_margin + screen_x*cur_char_width,
	      top_margin + screen_y*scale_y,
	      cur_char_width, scale_y);
  }

  /* Save new byte in local memory */
  grafyx_unscaled[y][x] = byte;
  switch (scale_x) {
  case 1:
    exp[0] = byte;
    break;
  case 2:
    exp[1] = ((byte & 0x01) + ((byte & 0x02) << 1)
	      + ((byte & 0x04) << 2) + ((byte & 0x08) << 3)) * 3;
    exp[0] = (((byte & 0x10) >> 4) + ((byte & 0x20) >> 3)
	      + ((byte & 0x40) >> 2) + ((byte & 0x80) >> 1)) * 3;
    break;
  case 3:
    exp[2] = ((byte & 0x01) + ((byte & 0x02) << 2)
	      + ((byte & 0x04) << 4)) * 7;
    exp[1] = (((byte & 0x08) >> 2) + (byte & 0x10)
	      + ((byte & 0x20) << 2)) * 7 + ((byte & 0x04) >> 2);
    exp[0] = (((byte & 0x40) >> 4) + ((byte & 0x80) >> 2)) * 7
           + ((byte & 0x20) >> 5) * 3;
    break;
  case 4:
    exp[3] = ((byte & 0x01) + ((byte & 0x02) << 3)) * 15;
    exp[2] = (((byte & 0x04) >> 2) + ((byte & 0x08) << 1)) * 15;
    exp[1] = (((byte & 0x10) >> 4) + ((byte & 0x20) >> 1)) * 15;
    exp[0] = (((byte & 0x40) >> 6) + ((byte & 0x80) >> 3)) * 15;
    break;
  default:
    fatal("scaling graphics by %dx not implemented\n", scale_x);
  }
  for (j=0; j<scale_y; j++) {
    for (i=0; i<scale_x; i++) {
      grafyx[(y*scale_y + j)*image.bytes_per_line + x*scale_x + i] = exp[i];
    }
  }

  if (grafyx_enable && on_screen) {
    /* Draw new byte */
    if (grafyx_overlay) {
      XPutImage(display, window, gc_xor, &image,
		x*cur_char_width, y*scale_y,
		left_margin + screen_x*cur_char_width,
		top_margin + screen_y*scale_y,
		cur_char_width, scale_y);
    } else {
      XPutImage(display, window, gc, &image,
		x*cur_char_width, y*scale_y,
		left_margin + screen_x*cur_char_width,
		top_margin + screen_y*scale_y,
		cur_char_width, scale_y);
    }
  }
}

void grafyx_write_x(int value)
{
  grafyx_x = value;
}

void grafyx_write_y(int value)
{
  grafyx_y = value;
}

void grafyx_write_data(int value)
{
  grafyx_write_byte(grafyx_x % G_XSIZE, grafyx_y, value);
  if (!(grafyx_mode & G_XNOCLKW)) {
    if (grafyx_mode & G_XDEC) {
      grafyx_x--;
    } else {
      grafyx_x++;
    }
  }
  if (!(grafyx_mode & G_YNOCLKW)) {
    if (grafyx_mode & G_YDEC) {
      grafyx_y--;
    } else {
      grafyx_y++;
    }
  }
}

int grafyx_read_data(void)
{
  int value = grafyx_unscaled[grafyx_y][grafyx_x % G_XSIZE];
  if (!(grafyx_mode & G_XNOCLKR)) {
    if (grafyx_mode & G_XDEC) {
      grafyx_x--;
    } else {
      grafyx_x++;
    }
  }
  if (!(grafyx_mode & G_YNOCLKR)) {
    if (grafyx_mode & G_YDEC) {
      grafyx_y--;
    } else {
      grafyx_y++;
    }
  }
  return value;
}

void grafyx_write_mode(int value)
{
  unsigned char old_enable = grafyx_enable;
  unsigned char old_overlay = grafyx_overlay;

  grafyx_enable = value & G_ENABLE;
  if (grafyx_microlabs) {
    grafyx_overlay = (value & G_UL_NOTEXT) == 0;
  }
  grafyx_mode = value;
  trs_screen_640x240((grafyx_enable && !grafyx_overlay) || text80x24);
  if (old_enable != grafyx_enable || 
      (grafyx_enable && old_overlay != grafyx_overlay)) {
    
    trs_screen_refresh();
  }
}

void grafyx_write_xoffset(int value)
{
  unsigned char old_xoffset = grafyx_xoffset;
  grafyx_xoffset = value % G_XSIZE;
  if (grafyx_enable && old_xoffset != grafyx_xoffset) {
    trs_screen_refresh();
  }
}

void grafyx_write_yoffset(int value)
{
  unsigned char old_yoffset = grafyx_yoffset;
  grafyx_yoffset = value;
  if (grafyx_enable && old_yoffset != grafyx_yoffset) {
    trs_screen_refresh();
  }
}

void grafyx_write_overlay(int value)
{
  unsigned char old_overlay = grafyx_overlay;
  grafyx_overlay = value & 1;
  if (grafyx_enable && old_overlay != grafyx_overlay) {
    trs_screen_640x240((grafyx_enable && !grafyx_overlay) || text80x24);
    trs_screen_refresh();
  }
}

int grafyx_get_microlabs(void)
{
  return grafyx_microlabs;
}

void grafyx_set_microlabs(int on_off)
{
  grafyx_microlabs = on_off;
}

/* Model III MicroLabs support */
void grafyx_m3_reset(void)
{
  if (grafyx_microlabs) grafyx_m3_write_mode(0);
}

void grafyx_m3_write_mode(int value)
{
  int enable = (value & G3_ENABLE) != 0;
  int changed = (enable != grafyx_enable);
  grafyx_enable = enable;
  grafyx_overlay = enable;
  grafyx_mode = value;
  grafyx_y = G3_YLOW(value);
  if (changed) trs_screen_refresh();
}

int grafyx_m3_write_byte(int position, int byte)
{
  if (grafyx_microlabs && (grafyx_mode & G3_COORD)) {
    int x = (position % 64);
    int y = (position / 64) * 12 + grafyx_y;
    grafyx_write_byte(x, y, byte);
    return 1;
  } else {
    return 0;
  }
}

unsigned char grafyx_m3_read_byte(int position)
{
  if (grafyx_microlabs && (grafyx_mode & G3_COORD)) {
    int x = (position % 64);
    int y = (position / 64) * 12 + grafyx_y;
    return grafyx_unscaled[y][x];
  } else {
    return trs_screen[position];
  }
}

int grafyx_m3_active(void)
{
  return (trs_model == 3 && grafyx_microlabs && (grafyx_mode & G3_COORD));
}

/*
 * Support for Model I HRG1B 384*192 graphics card
 * (sold in Germany for Model I and Video Genie by RB-Elektronik).
 *
 * Assignment of ports is as follows:
 *    Port 0x00 (out): switch graphics screen off (value ignored).
 *    Port 0x01 (out): switch graphics screen on (value ignored).
 *    Port 0x02 (out): select screen memory address (LSB).
 *    Port 0x03 (out): select screen memory address (MSB).
 *    Port 0x04 (in):  read byte from screen memory.
 *    Port 0x05 (out): write byte to screen memory.
 * (The real hardware decodes only address lines A0-A2 and A7, so
 * that there are several "shadow" ports in the region 0x08-0x7d.
 * However, these undocumented ports are not implemented here.)
 *
 * The 16-bit memory address (port 2 and 3) is used for subsequent
 * read or write operations. It corresponds to a position on the
 * graphics screen, in the following way:
 *    Bits 0-5:   character column address (0-63)
 *    Bits 6-9:   character row address (0-15)
 *                (i.e. bits 0-9 are the "PRINT @" position.)
 *    Bits 10-13: address of line within character cell (0-11)
 *    Bits 14-15: not used
 *
 *      <----port 2 (LSB)---->  <-------port 3 (MSB)------->
 * Bit: 0  1  2  3  4  5  6  7  8  9  10  11  12  13  14  15
 *      <-column addr.->  <row addr>  <-line addr.->  <n.u.>
 *
 * Reading from port 4 or writing to port 5 will access six
 * neighbouring pixels corresponding (from left to right) to bits
 * 0-5 of the data byte. Bits 6 and 7 are present in memory, but
 * are ignored.
 *
 * In expanded mode (32 chars per line), the graphics screen has
 * only 192*192 pixels. Pixels with an odd column address (i.e.
 * every second group of 6 pixels) are suppressed.
 */

/* Initialize HRG. */
static void
hrg_init(void)
{
  int i;

  /* Precompute arrays of pixel sizes and offsets. */
  for (i = 0; i <= 6; i++) {
    hrg_pixel_x[0][i] = cur_char_width * i / 6;
    hrg_pixel_x[1][i] = cur_char_width*2 * i / 6;
    if (i != 0) {
      hrg_pixel_width[0][i-1] = hrg_pixel_x[0][i] - hrg_pixel_x[0][i-1];
      hrg_pixel_width[1][i-1] = hrg_pixel_x[1][i] - hrg_pixel_x[1][i-1];
    }
  }
  for (i = 0; i <= 12; i++) {
    hrg_pixel_y[i] = cur_char_height * i / 12;
    if (i != 0)
      hrg_pixel_height[i-1] = hrg_pixel_y[i] - hrg_pixel_y[i-1];
  }
  if (cur_char_width % 6 != 0 || cur_char_height % 12 != 0)
    error("character size %d*%d not a multiple of 6*12 HRG raster",
	  cur_char_width, cur_char_height);
}

/* Switch HRG on (1) or off (0). */
void
hrg_onoff(int enable)
{
  static int init = 0;

  if ((hrg_enable!=0) == (enable!=0)) return; /* State does not change. */

  if (!init) {
    hrg_init();
    init = 1;
  }
  hrg_enable = enable;
  trs_screen_refresh();
}

/* Write address to latch. */
void
hrg_write_addr(int addr, int mask)
{
  hrg_addr = (hrg_addr & ~mask) | (addr & mask);
}

/* Write byte to HRG memory. */
void
hrg_write_data(int data)
{
  int old_data;
  int position, line;
  int bits0, bits1;

  if (hrg_addr >= HRG_MEMSIZE) return; /* nonexistent address */
  old_data = hrg_screen[hrg_addr];
  hrg_screen[hrg_addr] = data;

  if (!hrg_enable) return;
  if ((currentmode & EXPANDED) && (hrg_addr & 1)) return;
  if ((data &= 0x3f) == (old_data &= 0x3f)) return;

  position = hrg_addr & 0x3ff;	/* bits 0-9: "PRINT @" screen position */
  line = hrg_addr >> 10;	/* vertical offset inside character cell */
  bits0 = ~data & old_data;	/* pattern to clear */
  bits1 = data & ~old_data;	/* pattern to set */

  if (bits0 == 0
      || trs_screen[position] == 0x20
      || trs_screen[position] == 0x80
      /*|| (trs_screen[position] < 0x80 && line >= 8 && !usefont)*/
      ) {
    /* Only additional bits set, or blank text character.
       No need for update of text. */
    int destx = (position % row_chars) * cur_char_width + left_margin;
    int desty = (position / row_chars) * cur_char_height + top_margin
      + hrg_pixel_y[line];
    int *x = hrg_pixel_x[(currentmode&EXPANDED)!=0];
    int *w = hrg_pixel_width[(currentmode&EXPANDED)!=0];
    int h = hrg_pixel_height[line];
    XRectangle rect0[3];    /* 6 bits => max. 3 groups of adjacent "0" bits */
    XRectangle rect1[3];
    int n0 = 0;
    int n1 = 0;
    int flag = 0;
    int j, b;

    /* Compute arrays of rectangles to clear and to set. */
    for (j = 0, b = 1; j < 6; j++, b <<= 1) {
      if (bits0 & b) {
	if (flag >= 0) {	/* Start new rectangle. */
	  rect0[n0].x = destx + x[j];
	  rect0[n0].y = desty;
	  rect0[n0].width = w[j];
	  rect0[n0].height = h;
	  n0++;
	  flag = -1;
	}
	else {			/* Increase width of rectangle. */
	  rect0[n0-1].width += w[j];
	}
      }
      else if (bits1 & b) {
	if (flag <= 0) {
	  rect1[n1].x = destx + x[j];
	  rect1[n1].y = desty;
	  rect1[n1].width = w[j];
	  rect1[n1].height = h;
	  n1++;
	  flag = 1;
	}
	else {
	  rect1[n1-1].width += w[j];
	}
      }
      else {
	flag = 0;
      }
    }
    if (n0 != 0) XFillRectangles(display, window, gc_inv, rect0, n0);
    if (n1 != 0) XFillRectangles(display, window, gc,     rect1, n1);
  }
  else {
    /* Unfortunately, HRG1B combines text and graphics with an
       (inclusive) OR. Thus, in the general case, we cannot erase
       the old graphics byte without losing the text information.
       Call trs_screen_write_char to restore the text character
       (erasing the graphics). This function will in turn call
       hrg_update_char and restore 6*12 graphics pixels. Sigh. */
    trs_screen_write_char(position, trs_screen[position]);
  }
}

/* Read byte from HRG memory. */
int
hrg_read_data(void)
{
  if (hrg_addr >= HRG_MEMSIZE) return 0xff; /* nonexistent address */
  return hrg_screen[hrg_addr];
}

/* Update graphics at given screen position.
   Called by trs_screen_write_char. */
static void
hrg_update_char(int position)
{
  int destx = (position % row_chars) * cur_char_width + left_margin;
  int desty = (position / row_chars) * cur_char_height + top_margin;
  int *x = hrg_pixel_x[(currentmode&EXPANDED)!=0];
  int *w = hrg_pixel_width[(currentmode&EXPANDED)!=0];
  XRectangle rect[3*12];
  int byte;
  int prev_byte = 0;
  int n = 0;
  int np = 0;
  int i, j, flag;

  /* Compute array of rectangles. */
  for (i = 0; i < 12; i++) {
    if ((byte = hrg_screen[position+(i<<10)] & 0x3f) == 0) {
    }
    else if (byte != prev_byte) {
      np = n;
      flag = 0;
      for (j = 0; j < 6; j++) {
	if (!(byte & 1<<j)) {
	  flag = 0;
	}
	else if (!flag) {	/* New rectangle. */
	  rect[n].x = destx + x[j];
	  rect[n].y = desty + hrg_pixel_y[i];
	  rect[n].width = w[j];
	  rect[n].height = hrg_pixel_height[i];
	  n++;
	  flag = 1;
	}
	else {			/* Increase width. */
	  rect[n-1].width += w[j];
	}
      }
    }
    else {			/* Increase heights. */
      for (j = np; j < n; j++)
	rect[j].height += hrg_pixel_height[i];
    }
    prev_byte = byte;
  }
  if (n != 0)
    XFillRectangles(display, window, gc, rect, n);
}


/*---------- X mouse support --------------*/

int mouse_x_size = 640, mouse_y_size = 240;
int mouse_sens = 3;
int mouse_last_x = -1, mouse_last_y = -1;
unsigned int mouse_last_buttons;
int mouse_old_style = 0;

void trs_get_mouse_pos(int *x, int *y, unsigned int *buttons)
{
  Window root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  XQueryPointer(display, window, &root, &child,
		&root_x, &root_y, &win_x, &win_y, &mask);
#if MOUSEDEBUG
  debug("get_mouse %d %d 0x%x ->", win_x, win_y, mask);
#endif
  if (win_x >= 0 && win_x < OrigWidth &&
      win_y >= 0 && win_y < OrigHeight) {
    /* Mouse is within emulator window */
    if (win_x < left_margin) win_x = left_margin;
    if (win_x >= OrigWidth - left_margin) win_x = OrigWidth - left_margin - 1;
    if (win_y < top_margin) win_y = top_margin;
    if (win_y >= OrigHeight - top_margin) win_y = OrigHeight - top_margin - 1;
    *x = mouse_last_x = (win_x - left_margin)
                        * mouse_x_size
                        / (OrigWidth - 2*left_margin);
    *y = mouse_last_y = (win_y - top_margin) 
                        * mouse_y_size
                        / (OrigHeight - 2*top_margin);
    mouse_last_buttons = 7;
    /* !!Note: assuming 3-button mouse */
    if (mask & Button1Mask) mouse_last_buttons &= ~4;
    if (mask & Button2Mask) mouse_last_buttons &= ~2;
    if (mask & Button3Mask) mouse_last_buttons &= ~1;
  }
  *x = mouse_last_x;
  *y = mouse_last_y;
  *buttons = mouse_last_buttons;
#if MOUSEDEBUG
  debug("%d %d 0x%x\n",
	  mouse_last_x, mouse_last_y, mouse_last_buttons);
#endif
}

void trs_set_mouse_pos(int x, int y)
{
  int dest_x, dest_y;
  if (x == mouse_last_x && y == mouse_last_y) {
    /* Kludge: Ignore warp if it says to move the mouse to where we
       last said it was. In general someone could really want to do that,
       but with MDRAW, gratuitous warps to the last location occur frequently.
    */
    return;
  }
  dest_x = left_margin + x * (OrigWidth - 2*left_margin) / mouse_x_size;
  dest_y = top_margin  + y * (OrigHeight - 2*top_margin) / mouse_y_size;

#if MOUSEDEBUG
  debug("set_mouse %d %d -> %d %d\n", x, y, dest_x, dest_y);
#endif
  XWarpPointer(display, window, window, 0, 0, OrigWidth, OrigHeight,
	       dest_x, dest_y);
}

void trs_get_mouse_max(int *x, int *y, unsigned int *sens)
{
  *x = mouse_x_size - (mouse_old_style ? 0 : 1);
  *y = mouse_y_size - (mouse_old_style ? 0 : 1);
  *sens = mouse_sens;
}

void trs_set_mouse_max(int x, int y, unsigned int sens)
{
  if ((x & 1) == 0 && (y & 1) == 0) {
    /* "Old style" mouse drivers took the size here; new style take
       the maximum. As a heuristic kludge, we assume old style if
       the values are even, new style if not. */
    mouse_old_style = 1;
  }
  mouse_x_size = x + (mouse_old_style ? 0 : 1);
  mouse_y_size = y + (mouse_old_style ? 0 : 1);
  mouse_sens = sens;
}

int trs_get_mouse_type(void)
{
  /* !!Note: assuming 3-button mouse */
  return 1;
}

