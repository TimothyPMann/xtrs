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
   Last modified on Sat Apr  1 04:29:51 PST 2000 by mann
*/

/*#define MOUSEDEBUG 1*/
/*#define XDEBUG 1*/

/*
 * trs_xinterface.c
 *
 * X Windows interface for TRS-80 simulator
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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

#define DEF_FONT1	"-misc-fixed-medium-r-normal--20-200-75-75-*-100-iso8859-1"
#define DEF_WIDEFONT1	"-misc-fixed-medium-r-normal--20-200-75-75-*-200-iso8859-1"
#define DEF_FONT3	"-misc-fixed-medium-r-normal--20-200-75-75-*-100-iso8859-1"
#define DEF_WIDEFONT3	"-misc-fixed-medium-r-normal--20-200-75-75-*-200-iso8859-1"
#define DEF_USEFONT 0

extern char trs_char_data[][MAXCHARS][TRS_CHAR_HEIGHT];
extern char trs_widechar_data[][MAXCHARS][TRS_CHAR_HEIGHT][2];

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
static GC gc;
static GC gc_inv;
static GC gc_xor;
static int currentmode = NORMAL;
static int OrigHeight,OrigWidth;
static int usefont = DEF_USEFONT;
static int cur_char_width = TRS_CHAR_WIDTH;
static int cur_char_height = TRS_CHAR_HEIGHT;
static int text80x24 = 0, screen640x240 = 0;
static XFontStruct *myfont, *mywidefont, *curfont;
static XKeyboardState repeat_state;
static int trs_charset;
static int scale = 1;

static XrmOptionDescRec opts[] = {
/* Option */    /* Resource */  /* Value from arg? */   /* Value if no arg */
{"-background",	"*background",	XrmoptionSepArg,	(caddr_t)NULL},
{"-bg",		"*background",	XrmoptionSepArg,	(caddr_t)NULL},
{"-foreground",	"*foreground",	XrmoptionSepArg,	(caddr_t)NULL},
{"-fg",		"*foreground",	XrmoptionSepArg,	(caddr_t)NULL},
{"-borderwidth","*borderwidth",	XrmoptionSepArg,	(caddr_t)NULL},
{"-usefont",	"*usefont",	XrmoptionNoArg,		(caddr_t)"on"},
{"-nofont",	"*usefont",	XrmoptionNoArg,		(caddr_t)"off"},
{"-font",	"*font",	XrmoptionSepArg,	(caddr_t)NULL},
{"-widefont",	"*widefont",	XrmoptionSepArg,	(caddr_t)NULL},
{"-display",	"*display",	XrmoptionSepArg,	(caddr_t)NULL},
{"-debug",	"*debug",	XrmoptionNoArg,		(caddr_t)"on"},
{"-nodebug",	"*debug",	XrmoptionNoArg,		(caddr_t)"off"},
{"-romfile",	"*romfile",	XrmoptionSepArg,	(caddr_t)NULL},
{"-romfile3",	"*romfile3",	XrmoptionSepArg,	(caddr_t)NULL},
{"-romfile4p",	"*romfile4p",	XrmoptionSepArg,	(caddr_t)NULL},
{"-resize",	"*resize",	XrmoptionNoArg,		(caddr_t)"on"},
{"-noresize",	"*resize",	XrmoptionNoArg,		(caddr_t)"off"},
{"-doublestep", "*doublestep",  XrmoptionNoArg,         (caddr_t)"on"},
{"-nodoublestep","*doublestep", XrmoptionNoArg,         (caddr_t)"off"},
{"-model",      "*model",       XrmoptionSepArg,	(caddr_t)NULL},
{"-model1",     "*model",       XrmoptionNoArg,		(caddr_t)"1"},
{"-model3",     "*model",       XrmoptionNoArg,		(caddr_t)"3"},
{"-model4",     "*model",       XrmoptionNoArg,		(caddr_t)"4"},
{"-model4p",    "*model",       XrmoptionNoArg,		(caddr_t)"4p"},
{"-diskdir",    "*diskdir",     XrmoptionSepArg,	(caddr_t)NULL},
{"-delay",      "*delay",       XrmoptionSepArg,	(caddr_t)NULL},
{"-autodelay",  "*autodelay",   XrmoptionNoArg,         (caddr_t)"on"},
{"-noautodelay","*autodelay",   XrmoptionNoArg,         (caddr_t)"off"},
{"-keystretch", "*keystretch",  XrmoptionSepArg,        (caddr_t)NULL},
{"-microlabs",  "*microlabs",   XrmoptionNoArg,         (caddr_t)"on"},
{"-nomicrolabs","*microlabs",   XrmoptionNoArg,         (caddr_t)"off"},
{"-doubler",    "*doubler",     XrmoptionSepArg,        (caddr_t)NULL},
{"-sizemap",    "*sizemap",     XrmoptionSepArg,        (caddr_t)NULL},
{"-stepmap",    "*stepmap",     XrmoptionSepArg,        (caddr_t)NULL},
{"-charset",    "*charset",     XrmoptionSepArg,        (caddr_t)NULL},
{"-truedam",    "*truedam",     XrmoptionNoArg,         (caddr_t)"on"},
{"-notruedam",  "*truedam",     XrmoptionNoArg,         (caddr_t)"off"},
{"-samplerate", "*samplerate",  XrmoptionSepArg,        (caddr_t)NULL},
{"-title",      "*title",       XrmoptionSepArg,        (caddr_t)NULL},
{"-scale",      "*scale",       XrmoptionSepArg,        (caddr_t)NULL},
{"-scale1",     "*scale",       XrmoptionNoArg,         (caddr_t)"1"},
{"-scale2",     "*scale",       XrmoptionNoArg,         (caddr_t)"2"},
{"-scale3",     "*scale",       XrmoptionNoArg,         (caddr_t)"3"},
{"-scale4",     "*scale",       XrmoptionNoArg,         (caddr_t)"4"},
#if __linux
{"-sb",         "*sb",          XrmoptionSepArg,        (caddr_t)NULL},
#endif /* linux */
};

static int num_opts = (sizeof opts / sizeof opts[0]);

/* Support for Micro Labs Grafyx Solution and Radio Shack hi-res card */

/* True size of graphics memory -- some is offscreen */
#define G_XSIZE 128
#define G_YSIZE 256
char grafyx[(2*G_YSIZE*MAX_SCALE) * (G_XSIZE*MAX_SCALE)];
unsigned char grafyx_unscaled[G_YSIZE][G_XSIZE];

unsigned char grafyx_microlabs = 1;
unsigned char grafyx_x = 0, grafyx_y = 0, grafyx_mode = 0;
unsigned char grafyx_enable = 0;
unsigned char grafyx_overlay = 0;
unsigned char grafyx_xoffset = 0, grafyx_yoffset = 0;

/* Port 0x83 bits */
#define G_ENABLE    1
#define G_UL_NOTEXT 2   /* Micro Labs only */
#define G_RS_WAIT   2   /* Radio Shack only */
#define G_XDEC      4
#define G_YDEC      8
#define G_XNOCLKR   16
#define G_YNOCLKR   32
#define G_XNOCLKW   64
#define G_YNOCLKW   128

XImage image = {
  /*width, height*/    8*G_XSIZE, 2*G_YSIZE,  /* if scale = 1 */
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

/*
 * Key event queueing routines
 */
#define KEY_QUEUE_SIZE	(32)
static int key_queue[KEY_QUEUE_SIZE];
static int key_queue_head;
static int key_queue_entries;
static int key_immediate;

/* dummy buffer for stat() call */
struct stat statbuf;

static void clear_key_queue()
{
  key_queue_head = 0;
  key_queue_entries = 0;
}

void queue_key(int state)
{
  key_queue[(key_queue_head + key_queue_entries) % KEY_QUEUE_SIZE] = state;
#if KBDEBUG
  fprintf(stderr, "queue_key 0x%x", state);
#endif
  if (key_queue_entries < KEY_QUEUE_SIZE) {
    key_queue_entries++;
#if KBDEBUG
    fprintf(stderr, "\n");
  } else {
    fprintf(stderr, " (overflow)\n");
#endif
  }
}

int dequeue_key()
{
  int rval = -1;

  if(key_queue_entries > 0)
    {
      rval = key_queue[key_queue_head];
      key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
      key_queue_entries--;
#if KBDEBUG
      fprintf(stderr, "dequeue_key 0x%x\n", rval);
#endif
    }
  return rval;
}

int trs_next_key(int wait)
{
#if KBWAIT
  if (wait) {
    int rval;
    for (;;) {
      if ((rval = dequeue_key()) >= 0) break;
      if ((z80_state.nmi && !z80_state.nmi_seen) ||
	  (z80_state.irq && z80_state.iff1) ||
	  trs_event_scheduled() || key_immediate) {
	rval = -1;
	break;
      }
      trs_paused = 1;
      pause();			/* Wait for SIGALRM or SIGIO */
      trs_get_event(0);
    }
    return rval;
  }
#endif
  return dequeue_key();

}

/* Private routines */
void bitmap_init();
void screen_init();
void trs_event_init();
void trs_event();

static XrmDatabase x_db = NULL;
static XrmDatabase command_db = NULL;
extern char *program_name;
char *title;

int trs_parse_command_line(int argc, char **argv, int *debug)
{
  char option[512];
  char *type;
  XrmValue value;
  char *xrms;
  int stepdefault, i, s[8];

  title = program_name; /* default */

  XrmInitialize();
  /* parse command line options */
  XrmParseCommand(&command_db,opts,num_opts,program_name,&argc,argv);

  (void) sprintf(option, "%s%s", program_name, ".display");
  (void) XrmGetResource(command_db, option, "Xtrs.Display", &type, &value);
  /* open display */
  if ( (display = XOpenDisplay (value.addr)) == NULL) {
    printf("Unable to open display.");
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

  (void) sprintf(option, "%s%s", program_name, ".scale");
  if (XrmGetResource(x_db, option, "Xtrs.Scale", &type, &value)) 
  {
    sscanf(value.addr, "%d", &scale);
    if (scale <= 0) scale = 1;
    if (scale > MAX_SCALE) scale = MAX_SCALE;
  }
  image.width *= scale;
  image.height *= scale;
  image.bytes_per_line *= scale;

#if __linux
  (void) sprintf(option, "%s%s", program_name, ".sb");
  if (XrmGetResource(x_db, option, "Xtrs.Sb", &type, &value)) {
    char *next; int ioport, vol;
    ioport = strtol(value.addr, &next, 0);
    if(*next == ',') {
      next++;
      vol=atoi(next);
      trs_sound_init(ioport, vol); /* requires root privilege */
    }
  }
  setuid(getuid());
#endif /* linux */

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
    if (isdigit(*charset_name)) {
      trs_charset = atoi(charset_name);
      cur_char_width = 8 * scale;
    } else {
      if (charset_name[0] == 'e'/*early*/) {
	trs_charset = 0;
	cur_char_width = 6 * scale;
      } else if (charset_name[0] == 's'/*stock*/) {
	trs_charset = 1;
	cur_char_width = 6 * scale;
      } else if (charset_name[0] == 'l'/*lcmod*/) {
	trs_charset = 2;
	cur_char_width = 6 * scale;
      } else if (charset_name[0] == 'w'/*wider*/) {
	trs_charset = 3;
	cur_char_width = 8 * scale;
      } else if (charset_name[0] == 'g'/*genie or german*/) {
	trs_charset = 10;
	cur_char_width = 8 * scale;
      } else {
	fatal("unknown charset name %s", value.addr);
      }
    }
    cur_char_height = TRS_CHAR_HEIGHT * scale;
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
    if (isdigit(*charset_name)) {
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
    cur_char_width = TRS_CHAR_WIDTH * scale;
    cur_char_height = ((trs_model >= 4) ? TRS_CHAR_HEIGHT4 : TRS_CHAR_HEIGHT)
      * scale;
  }

  (void) sprintf(option, "%s%s", program_name, ".diskdir");
  if (XrmGetResource(x_db, option, "Xtrs.Diskdir", &type, &value)) {
    trs_disk_dir = strdup(value.addr);
  }

  (void) sprintf(option, "%s%s", program_name, ".delay");
  if (XrmGetResource(x_db, option, "Xtrs.Delay", &type, &value)) {
    z80_state.delay = strtol(value.addr, NULL, 0);
  }

  (void) sprintf(option, "%s%s", program_name, ".keystretch");
  if (XrmGetResource(x_db, option, "Xtrs.Keystretch", &type, &value)) {
    sscanf(value.addr, "%d,%d,%d",
	   &stretch_amount, &stretch_poll, &stretch_heartbeat);
  }

  (void) sprintf(option, "%s%s", program_name, ".microlabs");
  if (XrmGetResource(x_db, option, "Xtrs.Microlabs", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      grafyx_set_microlabs(True);
    } else if (strcmp(value.addr,"off") == 0) {
      grafyx_set_microlabs(False);
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

  return argc;
}

static void
save_repeat()
{
  XGetKeyboardControl(display, &repeat_state);
  XAutoRepeatOff(display);
  XSync(display, False);
}

static void
restore_repeat()
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

/* exits if something really bad happens */
void trs_screen_init()
{
  Window root_window;
  unsigned long fore_pixel, back_pixel, foreground, background;
  char option[512];
  char *type;
  XrmValue value;
  Colormap color_map;
  XColor cdef;
  XGCValues gcvals;
  char *fontname = NULL;
  char *widefontname = NULL;
  int len;
  char *romfile = NULL;

  screen = DefaultScreen(display);
  color_map = DefaultColormap(display,screen);

  (void) sprintf(option, "%s%s", program_name, ".foreground");
  if (XrmGetResource(x_db, option, "Xtrs.Foreground", &type, &value)) {
    /* printf("foreground is %s\n",value.addr); */
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

  (void) sprintf(option, "%s%s", program_name, ".resize");
  if (XrmGetResource(x_db, option, "Xtrs.Resize", &type, &value)) {
    if (strcmp(value.addr,"on") == 0) {
      resize = 1;
    } else if (strcmp(value.addr,"off") == 0) {
      resize = 0;
    }
  }

  switch (trs_model) {
  case 1:
    (void) sprintf(option, "%s%s", program_name, ".romfile");
    if (XrmGetResource(x_db, option, "Xtrs.Romfile", &type, &value)) {
      if ((stat(value.addr, &statbuf)) == 0) { /* romfile exists */
        romfile = value.addr;
      }
#ifdef DEFAULT_ROM
    } else if ((stat(DEFAULT_ROM, &statbuf)) == 0) {
      romfile = DEFAULT_ROM;
#endif
    }
    if (romfile != NULL) {
      trs_load_rom(romfile);
    } else if (trs_rom1_size > 0) {
      trs_load_compiled_rom(trs_rom1_size, trs_rom1);
    } else {
      fatal("ROM file not specified!");
    }
    break;
  case 3: case 4:
    (void) sprintf(option, "%s%s", program_name, ".romfile3");
    if (XrmGetResource(x_db, option, "Xtrs.Romfile3", &type, &value)) {
      if ((stat(value.addr, &statbuf)) == 0) { /* romfile exists */
        romfile = value.addr;
      }
#ifdef DEFAULT_ROM3
    } else if ((stat(DEFAULT_ROM3, &statbuf)) == 0) {
      romfile = DEFAULT_ROM3;
#endif
    }
    if (romfile != NULL) {
      trs_load_rom(romfile);
    } else if (trs_rom3_size > 0) {
      trs_load_compiled_rom(trs_rom3_size, trs_rom3);
    } else {
      fatal("ROM file not specified!");
    }
    break;
  default: /* 4P */
    (void) sprintf(option, "%s%s", program_name, ".romfile4p");
    if (XrmGetResource(x_db, option, "Xtrs.Romfile4p", &type, &value)) {
      if ((stat(value.addr, &statbuf)) == 0) { /* romfile exists */
        romfile = value.addr;
      }
#ifdef DEFAULT_ROM4P
    } else if ((stat(DEFAULT_ROM4P, &statbuf)) == 0) {
      romfile = DEFAULT_ROM4P;
#endif
    }
    if (romfile != NULL) {
      trs_load_rom(romfile);
    } else if (trs_rom4p_size > 0) {
      trs_load_compiled_rom(trs_rom4p_size, trs_rom4p);
    } else {
      fatal("ROM file not specified!");
    }
    break;
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

  if (trs_model >= 4 && !resize) {
    OrigWidth = cur_char_width * 80 + 2 * border_width;
    left_margin = cur_char_width * (80 - row_chars)/2 + border_width;
    if (usefont) {
      OrigHeight = cur_char_height * 24 + 2 * border_width;
      top_margin = cur_char_height * (24 - col_chars)/2 + border_width;
    } else {
      OrigHeight = TRS_CHAR_HEIGHT4 * scale * 24 + 2 * border_width;
      top_margin = (TRS_CHAR_HEIGHT4 * scale * 24 -
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
    fprintf(stderr, "XCreateSimpleWindow(%d, %d)\n", OrigWidth, OrigHeight);
#endif XDEBUG
  trs_fix_size(window, OrigWidth, OrigHeight);
  XStoreName(display,window,title);
  XSelectInput(display, window, EVENT_MASK);
  XMapWindow(display, window);

  bitmap_init(foreground, background);

  screen_init();
  XClearWindow(display,window);

#if HAVE_SIGIO
  trs_event_init();
#endif
}

#if HAVE_SIGIO
void trs_event_init()
{
  int fd, rc;
  struct sigaction sa;

  /* set up event handler */
  sa.sa_handler = trs_event;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGIO);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGIO, &sa, NULL);

  fd = ConnectionNumber(display);
  if (fcntl(fd,F_SETOWN,getpid()) < 0)
    perror("fcntl F_SETOWN");
  rc = fcntl(fd,F_SETFL,FASYNC);
  if (rc != 0)
    perror("fcntl F_SETFL async error");
}

/* ARGSUSED */
void trs_event(int signo)
{
  x_poll_count = 0;
}
#endif /*HAVE_SIGIO*/

KeySym last_key[256];

/* 
 * Get and process X event(s).
 *   If wait is true, process one event, blocking until one is available.
 *   If wait is false, process as many events as are available, returning
 *     when none are left.
 */ 
void trs_get_event(int wait)
{
  XEvent event;
  KeySym key;
  char buf[10];
  XComposeStatus status;

  key_immediate = 0;
  do {
    if (wait) {
      XNextEvent(display, &event);
    } else {
      if (!XCheckMaskEvent(display, ~0, &event)) return;
    }

    switch(event.type) {
    case Expose:
#if XDEBUG
      fprintf(stderr,"Expose\n");
#endif
      if (event.xexpose.count == 0) {
	while (XCheckMaskEvent(display, ExposureMask, &event)) /*skip*/;
	trs_screen_refresh();
      }
      break;

    case MapNotify:
#if XDEBUG
      fprintf(stderr,"MapNotify\n");
#endif
      trs_screen_refresh();
      break;

    case EnterNotify:
#if XDEBUG
      fprintf(stderr,"EnterNotify\n");
#endif
      save_repeat();
      trs_xlate_keycode(0x10000); /* all keys up */
      break;

    case LeaveNotify:
#if XDEBUG
      fprintf(stderr,"LeaveNotify\n");
#endif
      restore_repeat();
      trs_xlate_keycode(0x10000); /* all keys up */
      break;

    case KeyPress:
      (void) XLookupString((XKeyEvent *)&event,buf,10,&key,&status);
#if XDEBUG
      fprintf(stderr,"KeyPress: state 0x%x, keycode 0x%x, key 0x%x\n",
	      event.xkey.state, event.xkey.keycode, (unsigned int) key);
#endif
      switch (key) {
	/* Trap some function keys here */
      case XK_F10:
	trs_reset();
	key = 0;
	key_immediate = 1;
	break;
      case XK_F9:
	trs_debug();
	key = 0;
	key_immediate = 1;
	break;
      case XK_F8:
	trs_exit();
	key = 0;
	key_immediate = 1;
	break;
      case XK_F7:
	trs_disk_change_all();
	key = 0;
	key_immediate = 1;
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
	trs_xlate_keycode(0x10000 | last_key[event.xkey.keycode]);
      }
      last_key[event.xkey.keycode] = key;
      if (key != 0) {
	trs_xlate_keycode(key);
      }
      break;

    case KeyRelease:
      key = last_key[event.xkey.keycode];
      last_key[event.xkey.keycode] = 0;
      if (key != 0) {
	trs_xlate_keycode(0x10000 | key);
      }
#if XDEBUG
      fprintf(stderr,"KeyRelease: state 0x%x, keycode 0x%x, last_key 0x%x\n",
	      event.xkey.state, event.xkey.keycode, (unsigned int) key);
#endif
      break;

    default:
#if XDEBUG	    
      fprintf(stderr,"Unhandled event: type %d\n", event.type);
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
	trs_screen_write_char(i,trs_screen[i],False);
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
	trs_screen_write_char(i,trs_screen[i],False);
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
    if (!usefont) cur_char_height = TRS_CHAR_HEIGHT4 * scale;
  } else {
    row_chars = 64;
    col_chars = 16;
    if (!usefont) cur_char_height = TRS_CHAR_HEIGHT * scale;
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
    fprintf(stderr, "XResizeWindow(%d, %d)\n", OrigWidth, OrigHeight);
#endif XDEBUG
  } else {
    left_margin = cur_char_width * (80 - row_chars)/2 + border_width;
    if (usefont) {
      top_margin = cur_char_height * (24 - col_chars)/2 + border_width;
    } else {
      top_margin = (TRS_CHAR_HEIGHT4 * scale * 24 -
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

void screen_init()
{
  int i;

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
 * This routine creates a rescaled charater bitmap, and then
 * calls XCreateBitmapFromData. It then can be used pretty much
 * as a drop-in replacement for XCreateBitmapFromData. 
 */
Pixmap XCreateBitmapFromDataScale(Display *display, Drawable window,
			             char *data, unsigned int width, 
                                     unsigned int height, unsigned int scale)
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
    mydata= (unsigned char *)malloc(width * height * scale * scale * 4);
    mypixels= (unsigned char *)malloc(width * height * 4);
  }
  
  /* Read the character data */ 
  for (j= 0; j< width * height; j += 8)
  {
    for (i= j + 7; i >= j; i--)
    {
      *(mypixels + i)= (*(data + (j >> 3)) >> (i - j)) & 1;
    }
  }

  /* Clear out the rescaled character array */
  for (i= 0; i< width / 8 * height * scale * scale * 4; i++)
  {
    *(mydata + i)= 0;
  }
  
  /* And prepare our rescaled character. */
  k= 0;
  for (j= 0; j< height * scale; j++)
  {
    for (i= 0; i< width * scale; i++)
    {
       *(mydata + (k >> 3)) = *(mydata + (k >> 3)) |
	 (*(mypixels + ((int)(j / scale) * width) +
	    (int)(i / scale)) << (k & 7));
       k++;
    }
  }

  return XCreateBitmapFromData(display,window,
			      (char*)mydata,
			      width * scale, height * scale);
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
				   TRS_CHAR_WIDTH,TRS_CHAR_HEIGHT,scale);
      trs_char[1][i] =
	XCreateBitmapFromDataScale(display,window,
				   (char*)trs_widechar_data[trs_charset][i],
				   TRS_CHAR_WIDTH*2,TRS_CHAR_HEIGHT,scale);
    }
    boxes_init(foreground, background,
	       cur_char_width, TRS_CHAR_HEIGHT * scale, 0);
    boxes_init(foreground, background,
	       cur_char_width*2, TRS_CHAR_HEIGHT * scale, 1);
  }
}

void trs_screen_refresh()
{
  int i, srcx, srcy, dunx, duny;

#if XDEBUG
  fprintf(stderr, "trs_screen_refresh\n");
#endif
  if (grafyx_enable && !grafyx_overlay) {
    srcx = cur_char_width * grafyx_xoffset;
    srcy = 2 * scale * grafyx_yoffset;
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
      trs_screen_write_char(i,trs_screen[i],False);
    }
  }
#if DO_XFLUSH
  XFlush(display);
#endif
}

void trs_screen_write_char(int position, int char_index, Bool doflush)
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
    if (trs_model == 1) {
#if !UPPERCASE
      /* Emulate Radio Shack lowercase mod.  The replacement character
	 generator ROM had another copy of the uppercase characters in
	 the control character positions, to compensate for a bug in the
	 Level II ROM that stores such values instead of uppercase. */
      if (char_index < 0x20) char_index += 0x40;
#endif
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
    srcy = (row*cur_char_height + grafyx_yoffset*2*scale) % (G_YSIZE*2*scale); 
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
#if DO_XFLUSH
  if (doflush)
    XFlush(display);
#endif
}

 /* Copy lines 1 through col_chars-1 to lines 0 through col_chars-2.
    Doesn't need to clear line col_chars-1. */
void trs_screen_scroll()
{
  int i = 0;

  for (i = row_chars; i < screen_chars; i++)
    trs_screen[i-row_chars] = trs_screen[i];

  if (grafyx_enable) {
    if (grafyx_overlay) {
      trs_screen_refresh();
    }
  } else {
    XCopyArea(display,window,window,gc,
	      border_width,cur_char_height+border_width,
	      (cur_char_width*row_chars),(cur_char_height*col_chars),
	      border_width,border_width);
  }
}

void trs_screen_write_chars(int *locations, int *values, int count)
{
  while (count--) {
    trs_screen_write_char(*locations++, *values++ , False);
  }
#if DO_XFLUSH
  XFlush(display);
#endif
}

void grafyx_write_byte(int x, int y, char byte)
{
  int i, j;
  char exp[MAX_SCALE];
  int screen_x = ((x - grafyx_xoffset + G_XSIZE) % G_XSIZE);
  int screen_y = ((y - grafyx_yoffset + G_YSIZE) % G_YSIZE);
  int on_screen = screen_x < row_chars &&
    screen_y < col_chars*cur_char_height/(scale*2);
  if (grafyx_enable && grafyx_overlay && on_screen) {
    /* Erase old byte, preserving text */
    XPutImage(display, window, gc_xor, &image,
	      x*cur_char_width, y*2*scale,
	      left_margin + screen_x*cur_char_width,
	      top_margin + screen_y*2*scale,
	      cur_char_width, 2*scale);
  }

  /* Save new byte in local memory */
  grafyx_unscaled[y][x] = byte;
  switch (scale) {
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
    fatal("scaling graphics by %dx not implemented\n", scale);
  }
  for (j=0; j<2*scale; j++) {
    for (i=0; i<scale; i++) {
      grafyx[(y*2*scale + j)*image.bytes_per_line + x*scale + i] = exp[i];
    }
  }

  if (grafyx_enable && on_screen) {
    /* Draw new byte */
    if (grafyx_overlay) {
      XPutImage(display, window, gc_xor, &image,
		x*cur_char_width, y*2*scale,
		left_margin + screen_x*cur_char_width,
		top_margin + screen_y*2*scale,
		cur_char_width, 2*scale);
    } else {
      XPutImage(display, window, gc, &image,
		x*cur_char_width, y*2*scale,
		left_margin + screen_x*cur_char_width,
		top_margin + screen_y*2*scale,
		cur_char_width, 2*scale);
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

int grafyx_read_data()
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

int grafyx_get_microlabs()
{
  return grafyx_microlabs;
}

void grafyx_set_microlabs(int on_off)
{
  grafyx_microlabs = on_off;
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
  fprintf(stderr, "get_mouse %d %d 0x%x ->", win_x, win_y, mask);
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
  fprintf(stderr, "%d %d 0x%x\n",
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
  fprintf(stderr, "set_mouse %d %d -> %d %d\n", x, y, dest_x, dest_y);
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

int trs_get_mouse_type()
{
  /* !!Note: assuming 3-button mouse */
  return 1;
}

