/* Copyright (c) 2009-2017, Timothy Mann */
/* $Id$ */

/* This software may be copied, modified, and used for any purpose
 * without fee, provided that (1) the above copyright notice is
 * retained, and (2) modified versions are clearly marked as having
 * been modified, with the modifier's name and the date included.  */

/*
 * XXX TODO
 * - need a way to package up trs.glade and find at runtime
 * - design more menus, toolbar, and dialogs.  remove any unused menu items.
 * - create a window for zbx instead of using the launch xterm.
 * - save/load settings to a file (file selectable on command line).  useful to 
 *    replace old way of pre-configured directories with disk?-? symlinks.
 * - implement copy/paste.  can maybe steal some code from sdltrs.
 * - generally, look at sdltrs for ideas and code to steal.
 * - split up this file more
 * - fix any remaining XXX's
 */

/*XXX for command line parsing, should move? */
#define _XOPEN_SOURCE 500 /* string.h: strdup() */
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
/*XXX end */

#define GDK_ENABLE_BROKEN 1 // needed for gdk_image_new_bitmap
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "trs.h"
#include "trs_iodefs.h"
#include "trs_disk.h"
#include "trs_uart.h"
#include "keyrepeat.h"

/*#define MOUSEDEBUG 6*/
/*#define KDEBUG 1*/

#define MAX_SCALE 4

/* currentmode values */
#define NORMAL 0
#define EXPANDED 1
#define INVERSE 2
#define ALTERNATE 4

/* Private data */
static unsigned char trs_screen[2048];
static int screen_chars = 1024;
static int row_chars = 64;
static int col_chars = 16;
static int resize = -1;
static int top_margin = 0;
static int left_margin = 0;
static int border_width = 2;
static int currentmode = NORMAL;
static int cur_screen_width, cur_screen_height;
static int cur_char_height, cur_char_width;
static int text80x24 = 0, screen640x240 = 0;
static int trs_charset;
static int scale_x = 1;
static int scale_y = 0;

GtkWidget *main_window;
GtkWidget *about_dialog;
GtkWidget *keys_window;
GtkWidget *quit_dialog;
GtkWidget *drawing_area;
GdkPixmap *trs_screen_pixmap;
GdkGC     *gc;
GdkGC     *gc_inv;
GdkGC     *gc_xor;
GdkColor  fore_color, back_color, xor_color, null_color;

static GdkPixmap *trs_char[2][2][MAXCHARS];
static GdkPixmap *trs_box[2][64];

/* Support for Micro Labs Grafyx Solution and Radio Shack hi-res card */

/* True size of graphics memory -- some is offscreen */
#define G_XSIZE 128
#define G_YSIZE 256
unsigned char grafyx_unscaled[G_YSIZE][G_XSIZE];
GdkImage *grafyx_image;

int grafyx_microlabs = 0;
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

void grafyx_init(void);

#define HRG_MEMSIZE (1024 * 12)	/* 12k * 8 bit graphics memory */
static unsigned char hrg_screen[HRG_MEMSIZE];
static int hrg_pixel_x[2][6+1];
static int hrg_pixel_y[12+1];
static int hrg_pixel_width[2][6];
static int hrg_pixel_height[12];
static int hrg_enable = 0;
static int hrg_addr = 0;
static void hrg_update_char(int position);


/*
 * Create a pixmap from xbm data, scaled by the given scale_x, scale_y.
 */
GdkPixmap*
pixmap_create_from_data_scale(GdkDrawable *drawable,
			      const gchar *data,
			      gint width, gint height,
			      gint scale_x, gint scale_y,
			      const GdkColor *fore_color,
			      const GdkColor *back_color)
{
  gchar *mydata;
  gchar *mypixels;
  int i, j, k;
  int width8;  // width rounded up to a multiple of 8
  int scaled_width, scaled_width8, scaled_height;
  GdkPixmap *pm;

  width8 = (width + 7) / 8 * 8;
  scaled_width = width * scale_x;
  scaled_width8 = (scaled_width + 7) / 8 * 8;
  scaled_height = height * scale_y;

  mydata = g_malloc(scaled_width8 * scaled_height * 8);
  mypixels= g_malloc(width8 * height * 8);
  
  /* Read the character data */ 
  for (j = 0; j < width8 * height; j += 8) {
    for (i = j + 7; i >= j; i--) {
      *(mypixels + i) = (*(data + (j >> 3)) >> (i - j)) & 1;
    }
  }

  /* Clear out the rescaled character array */
  memset(mydata, 0, scaled_width8 * scaled_height * 4);
  
  /* And prepare our rescaled character. */
  k= 0;
  for (j = 0; j < scaled_height; j++) {
    for (i = 0; i < scaled_width8; i++) {
       *(mydata + (k >> 3)) |= 
	 (*(mypixels + ((int)(j / scale_y) * width8) +
	    (int)(i / scale_x)) << (k & 7));
       k++;
    }
  }

  pm = gdk_pixmap_create_from_data(drawable, mydata,
				   scaled_width, scaled_height, -1,
				   fore_color, back_color);
  g_free(mydata);
  g_free(mypixels);
  return pm;
}


void
font_init(void)
{
  /* Initialize from built-in font bitmaps. */
  int i;
	
  for (i = 0; i < MAXCHARS; i++) {
    trs_char[0][0][i] =
      pixmap_create_from_data_scale(trs_screen_pixmap,
				    trs_char_data[trs_charset][i],
				    cur_char_width / scale_x,
				    cur_char_height / scale_y,
				    scale_x, scale_y,
				    &fore_color, &back_color);
				    
    trs_char[1][0][i] =
      pixmap_create_from_data_scale(trs_screen_pixmap,
				    trs_char_data[trs_charset][i],
				    cur_char_width / scale_x,
				    cur_char_height / scale_y,
				    scale_x * 2, scale_y,
				    &fore_color, &back_color);
    if (trs_model >= 4) {
      trs_char[0][1][i] =
	pixmap_create_from_data_scale(trs_screen_pixmap,
				      trs_char_data[trs_charset][i],
				      cur_char_width / scale_x,
				      cur_char_height / scale_y,
				      scale_x, scale_y,
				      &back_color, &fore_color);
				    
      trs_char[1][1][i] =
	pixmap_create_from_data_scale(trs_screen_pixmap,
				      trs_char_data[trs_charset][i],
				      cur_char_width / scale_x,
				      cur_char_height / scale_y,
				      scale_x * 2, scale_y,
				      &back_color, &fore_color);
    }
  }
}


/*
 * Create a pixmap for each TRS-80 2x3 box graphics character.
 */
void
boxes_init(int width, int height, int expanded)
{
  int graphics_char, bit;
  GdkRectangle bits[6];

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
      gdk_pixmap_new(trs_screen_pixmap, width, height, -1);

    /* Clear everything */
    gdk_draw_rectangle(trs_box[expanded][graphics_char],
		       gc_inv, TRUE, 0, 0, width, height);

    /* Set the bits */
    for (bit = 0; bit < 6; ++bit) {
      if (graphics_char & (1 << bit)) {
	gdk_draw_rectangle(trs_box[expanded][graphics_char], gc, TRUE,
			   bits[bit].x, bits[bit].y,
			   bits[bit].width, bits[bit].height);
      }
    }
  }
}


/*
 * Command line parsing.  In trs_gtkinterface mostly for historical
 * reasons (the old trs_xinterface looked in the X resource database)
 * and partly because it sets a lot of values that are used here.  Ugh.
 */

int opt_iconic = FALSE;
char *opt_background = NULL;
char *opt_foreground = NULL;
int opt_debug = FALSE;
char *opt_title;
int opt_shiftbracket = -1;
char *opt_charset = NULL;
char *opt_scale = NULL;
int opt_stepdefault = 1;
char *opt_stepmap = NULL;
char *opt_sizemap = NULL;

struct option options[] = {
  /* Name, takes argument?, store int value at, value to store */
  {"iconic",         FALSE, &opt_iconic,       TRUE  },
  {"noiconic",       FALSE, &opt_iconic,       FALSE },
  {"background",     TRUE,  NULL,              0     },
  {"bg",	     TRUE,  NULL,              0     },
  {"foreground",     TRUE,  NULL,              0     },
  {"fg",             TRUE,  NULL,              0     },
  {"title",          TRUE,  NULL,              0     },
  {"borderwidth",    TRUE,  NULL,              0     },
  {"scale",          TRUE,  NULL,              0     },
  {"scale1",         FALSE, &scale_x,          1     },
  {"scale2",         FALSE, &scale_x,          2     },
  {"scale3",         FALSE, &scale_x,          3     },
  {"scale4",         FALSE, &scale_x,          4     },
  {"resize",	     FALSE, &resize,           TRUE  },
  {"noresize",	     FALSE, &resize,           FALSE },
  {"charset",        TRUE,  NULL,              0     },
  {"microlabs",      FALSE, &grafyx_microlabs, TRUE  },
  {"nomicrolabs",    FALSE, &grafyx_microlabs, FALSE },
  {"debug",	     FALSE, &opt_debug,        TRUE  },
  {"nodebug",        FALSE, &opt_debug,        FALSE },
  {"romfile",	     TRUE,  NULL,              0     },
  {"romfile3",	     TRUE,  NULL,              0     },
  {"romfile4p",      TRUE,  NULL,              0     },
  {"model",          TRUE,  NULL,              0     },
  {"model1",         FALSE, &trs_model,        1     },
  {"model3",         FALSE, &trs_model,        3     },
  {"model4",         FALSE, &trs_model,        4     },
  {"model4p",        FALSE, &trs_model,        5     },
  {"delay",          TRUE,  NULL,              0     },
  {"autodelay",      FALSE, &trs_autodelay,    TRUE  },
  {"noautodelay",    FALSE, &trs_autodelay,    FALSE },
  {"keystretch",     TRUE,  NULL,              0     },
  {"shiftbracket",   FALSE, &opt_shiftbracket, TRUE  },
  {"noshiftbracket", FALSE, &opt_shiftbracket, FALSE },
  {"diskdir",        TRUE,  NULL,              0     },
  {"doubler",        TRUE,  NULL,              0     },
  {"doublestep",     FALSE, &opt_stepdefault,  2     },
  {"nodoublestep",   FALSE, &opt_stepdefault,  1     },
  {"stepmap",        TRUE,  NULL,              0     },
  {"sizemap",        TRUE,  NULL,              0     },
  {"truedam",        FALSE, &trs_disk_truedam, TRUE  },
  {"notruedam",      FALSE, &trs_disk_truedam, FALSE },
  {"samplerate",     TRUE,  NULL,              0     },
  {"serial",         TRUE,  NULL,              0     },
  {"switches",       TRUE,  NULL,              0     },
  {"emtsafe",        FALSE, &trs_emtsafe,      TRUE  },
  {"noemtsafe",      FALSE, &trs_emtsafe,      FALSE },
  {NULL, 0, 0, 0}
};

  
int
trs_parse_command_line(int argc, char **argv, int *debug)
{
  int i;
  int s[8];

  gtk_init(&argc, &argv);

  opterr = 0;
  for (;;) {
    int c;
    int option_index = 0;
    const char *name;

    c = getopt_long_only(argc, argv, "", options, &option_index);
    if (c == -1) break;
    if (c == '?') {
      fatal("unrecognized option %s", argv[optind - 1]);
    }
    name = options[option_index].name;
    if (strcmp(name, "background") == 0 ||
               strcmp(name, "bg") == 0) {
      opt_background = optarg;
    } else if (strcmp(name, "foreground") == 0 ||
               strcmp(name, "fg") == 0) {
      opt_foreground = optarg;
    } else if (strcmp(name, "title") == 0) {
      opt_title = optarg;
    } else if (strcmp(name, "borderwidth") == 0) {
      border_width = strtoul(optarg, NULL, 0);
    } else if (strcmp(name, "scale") == 0) {
      sscanf(optarg, "%u,%u", &scale_x, &scale_y);
    } else if (strcmp(name, "charset") == 0) {
      opt_charset = optarg;
    } else if (strcmp(name, "romfile") == 0) {
      romfile = optarg;
    } else if (strcmp(name, "romfile3") == 0) {
      romfile3 = optarg;
    } else if (strcmp(name, "romfile4p") == 0) {
      romfile4p = optarg;
    } else if (strcmp(name, "model") == 0) {
      if (strcmp(optarg, "1") == 0 ||
	  strcasecmp(optarg, "I") == 0) {
	trs_model = 1;
      } else if (strcmp(optarg, "3") == 0 ||
		 strcasecmp(optarg, "III") == 0) {
	trs_model = 3;
      } else if (strcmp(optarg, "4") == 0 ||
		 strcasecmp(optarg, "IV") == 0) {
	trs_model = 4;
      } else if (strcasecmp(optarg, "4P") == 0 ||
		 strcasecmp(optarg, "IVp") == 0) {
	trs_model = 5;
      } else {
	fatal("TRS-80 Model %s not supported", optarg);
      }
    } else if (strcmp(name, "delay") == 0) {
      z80_state.delay = strtol(optarg, NULL, 0);
    } else if (strcmp(name, "keystretch") == 0) {
      stretch_amount = strtol(optarg, NULL, 0);
    } else if (strcmp(name, "diskdir") == 0) {
      trs_disk_dir = strdup(optarg);
      if (trs_disk_dir[0] == '~' &&
	  (trs_disk_dir[1] == '/' || trs_disk_dir[1] == '\0')) {
	char* home = getenv("HOME");
	if (home) {
	  char *p = (char*)malloc(strlen(home) + strlen(trs_disk_dir) + 1);
	  sprintf(p, "%s/%s", home, trs_disk_dir+1);
	  trs_disk_dir = p;
	}
      }
    } else if (strcmp(name, "doubler") == 0) {
      switch (optarg[0]) {
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
	trs_disk_doubler = TRSDISK_BOTH;
	break;
      case 'n':
      case 'N':
	trs_disk_doubler = TRSDISK_NODOUBLER;
	break;
      default:
	fatal("unrecognized doubler type %s\n", optarg);
      }
    } else if (strcmp(name, "stepmap") == 0) {
      opt_stepmap = optarg;
    } else if (strcmp(name, "sizemap") == 0) {
      opt_sizemap = optarg;
    } else if (strcmp(name, "samplerate") == 0) {
      cassette_default_sample_rate = strtol(optarg, NULL, 0);
    } else if (strcmp(name, "serial") == 0) {
      trs_uart_name = strdup(optarg);
    } else if (strcmp(name, "switches") == 0) {
      trs_uart_switches = strtol(optarg, NULL, 0);
    }
  }
  if (optind != argc) {
    fatal("unrecognized argument %s", argv[optind]);
  }

  /*
   * Some additional processing needed after all options are parsed.
   * In some cases the order is important; e.g., trs_model must be known.
   */
  *debug = opt_debug;

  if (resize == -1) {
    resize = (trs_model == 3);
  }

  if (opt_shiftbracket == -1) {
    opt_shiftbracket = trs_model >= 4;
  }
  trs_kb_bracket(opt_shiftbracket);

  if (scale_y == 0) scale_y = 2 * scale_x;

  /* Note: charset numbers must match trs_chars.c */
  if (trs_model == 1) {
    if (opt_charset == NULL) {
      opt_charset = "wider"; /* default */
    }
    if (isdigit(*opt_charset)) {
      trs_charset = strtol(opt_charset, NULL, 0);
      cur_char_width = 8 * scale_x;
    } else {
      if (opt_charset[0] == 'e'/*early*/) {
	trs_charset = 0;
	cur_char_width = 6 * scale_x;
      } else if (opt_charset[0] == 's'/*stock*/) {
	trs_charset = 1;
	cur_char_width = 6 * scale_x;
      } else if (opt_charset[0] == 'l'/*lcmod*/) {
	trs_charset = 2;
	cur_char_width = 6 * scale_x;
      } else if (opt_charset[0] == 'w'/*wider*/) {
	trs_charset = 3;
	cur_char_width = 8 * scale_x;
      } else if (opt_charset[0] == 'g'/*genie or german*/) {
	trs_charset = 10;
	cur_char_width = 8 * scale_x;
      } else {
	fatal("unknown charset name %s", opt_charset);
      }
    }
    cur_char_height = TRS_CHAR_HEIGHT * scale_y;
  } else /* trs_model > 1 */ {
    if (opt_charset == NULL) {
      /* default */
      opt_charset = (trs_model == 3) ? "katakana" : "international";
    }
    if (isdigit(*opt_charset)) {
      trs_charset = strtol(opt_charset, NULL, 0);
    } else {
      if (opt_charset[0] == 'k'/*katakana*/) {
	trs_charset = 4 + 3*(trs_model > 3);
      } else if (opt_charset[0] == 'i'/*international*/) {
	trs_charset = 5 + 3*(trs_model > 3);
      } else if (opt_charset[0] == 'b'/*bold*/) {
	trs_charset = 6 + 3*(trs_model > 3);
      } else {
	fatal("unknown charset name %s", opt_charset);
      }
    }
    cur_char_width = TRS_CHAR_WIDTH * scale_x;
    cur_char_height = TRS_CHAR_HEIGHT * scale_y;
  }

  for (i = 0; i <= 7; i++) {
    s[i] = opt_stepdefault;
  }
  if (opt_stepmap) {
    sscanf(opt_stepmap, "%d,%d,%d,%d,%d,%d,%d,%d",
           &s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6], &s[7]);
  }
  for (i = 0; i <= 7; i++) {
    if (s[i] != 1 && s[i] != 2) {
      fatal("bad value %d for disk %d single/double step\n", s[i], i);
    } else {
      trs_disk_setstep(i, s[i]);
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
  if (opt_sizemap) {
    sscanf(opt_sizemap, "%d,%d,%d,%d,%d,%d,%d,%d",
	   &s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6], &s[7]);
  }
  for (i = 0; i <= 7; i++) {
    if (s[i] != 5 && s[i] != 8) {
      fatal("bad value %d for disk %d size", s[i], i);
    } else {
      trs_disk_setsize(i, s[i]);
    }
  }

  return 1;
}


void trs_exit()
{
  gtk_exit(0);
}


void
trs_screen_init(void)
{
  GtkBuilder *builder;
  GError *err = NULL;      

  builder = gtk_builder_new();
  if (gtk_builder_add_from_file(builder, "xtrs.glade", &err) == 0) {
    fatal(err->message);
  }
   
  main_window = GTK_WIDGET(gtk_builder_get_object(builder,
						  "main_window"));
  drawing_area = GTK_WIDGET(gtk_builder_get_object(builder,
						   "drawing_area"));
  about_dialog = GTK_WIDGET(gtk_builder_get_object(builder,
						   "about_dialog"));
  keys_window = GTK_WIDGET(gtk_builder_get_object(builder,
						  "keys_window"));
  quit_dialog = GTK_WIDGET(gtk_builder_get_object(builder,
						  "quit_dialog"));

  /*
   * Work around glade-3 not letting me connect to the delete-event
   * signal for dialogs, instead leaving it with the default
   * that destroys the dialog.  XXX is this still a problem?
   */
  g_signal_connect(GTK_OBJECT(about_dialog), "delete-event",
		   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);
  g_signal_connect(GTK_OBJECT(keys_window), "delete-event",
		   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);
  g_signal_connect(GTK_OBJECT(quit_dialog), "delete-event",
		   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);

  gtk_builder_connect_signals(builder, NULL);
  g_object_unref(G_OBJECT(builder));

  if (trs_model >= 3 && !resize) {
    cur_screen_width = cur_char_width * 80 + 2 * border_width;
    left_margin = cur_char_width * (80 - row_chars) / 2 + border_width;
    cur_screen_height = TRS_CHAR_HEIGHT4 * scale_y * 24 + 2 * border_width;
    top_margin = (TRS_CHAR_HEIGHT4 * scale_y * 24 -
		  cur_char_height * col_chars) / 2 + border_width;

  } else {
    cur_screen_width = cur_char_width * row_chars + 2 * border_width;
    left_margin = border_width;
    cur_screen_height = cur_char_height * col_chars + 2 * border_width;
    top_margin = border_width;
  }
  gtk_widget_set_size_request(drawing_area,
			      cur_screen_width, cur_screen_height);

  if (opt_background) {
    if (!gdk_color_parse(opt_background, &back_color)) {
      fatal("unrecognized color %s", opt_background);
    }
    if (!gdk_colormap_alloc_color(gdk_colormap_get_system(),
				  &back_color, FALSE, TRUE)) {
      fatal("failed to allocate color %s", opt_background);
    }
    
  } else {
    gdk_color_black(gdk_colormap_get_system(), &back_color);
  }
  if (opt_foreground) {
    if (!gdk_color_parse(opt_foreground, &fore_color)) {
      fatal("unrecognized color %s", opt_foreground);
    }
    if (!gdk_colormap_alloc_color(gdk_colormap_get_system(),
				  &fore_color, FALSE, TRUE)) {
      fatal("failed to allocate color %s", opt_foreground);
    }
  } else {
    gdk_color_white(gdk_colormap_get_system(), &fore_color);
  }

  /*
   * The xor_color is a magic color that swaps fore_color and
   * back_color when it is xored in, and null_color is a color that
   * causes no change when xor'ed in.  It is the actual pixel value
   * that gets xor'ed, so we must construct the xor_color pixel value
   * here by xor'ing the fore_color and back_color pixel values, not
   * by xor'ing the RGB components and asking gdk_colormap_alloc_color
   * to allocate the closest matching color.  Similarly, the null_color
   * simply has pixel value 0.
   */
  xor_color.pixel = fore_color.pixel ^ back_color.pixel;
  null_color.pixel = 0;

  if (opt_iconic) {
    gtk_window_iconify(GTK_WINDOW(main_window));
  }

  gtk_widget_realize(main_window);

  gdk_window_set_title(main_window->window,
		       opt_title ? opt_title : program_name);

  trs_screen_pixmap =
    gdk_pixmap_new(main_window->window,
		   640 * scale_x + 2 * border_width,
		   240 * scale_y + 2 * border_width, -1);

  gc = gdk_gc_new(trs_screen_pixmap);
  gdk_gc_set_foreground(gc, &fore_color);
  gdk_gc_set_background(gc, &back_color);
  gdk_gc_set_function(gc, GDK_COPY);
  
  gc_inv = gdk_gc_new(trs_screen_pixmap);
  gdk_gc_set_foreground(gc_inv, &back_color);
  gdk_gc_set_background(gc_inv, &fore_color);
  gdk_gc_set_function(gc_inv, GDK_COPY);

  gc_xor = gdk_gc_new(trs_screen_pixmap);
  gdk_gc_set_foreground(gc_xor, &xor_color);
  gdk_gc_set_background(gc_xor, &null_color);
  gdk_gc_set_function(gc_xor, GDK_XOR);

  gdk_draw_rectangle(trs_screen_pixmap, gc_inv, TRUE,
		     0, 0, cur_screen_width, cur_screen_height);

  font_init();

  boxes_init(cur_char_width, TRS_CHAR_HEIGHT * scale_y, 0);
  boxes_init(cur_char_width * 2, TRS_CHAR_HEIGHT * scale_y, 1);

  grafyx_init();

  gtk_settings_set_string_property (gtk_settings_get_default (),
				    "gtk-menu-bar-accel", "F12", NULL);

  gtk_widget_show(main_window);
}


void
trs_screen_write_char(int position, int char_index)
{
  int row, col, destx, desty, expanded, width, height;

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
  expanded = (currentmode & EXPANDED) != 0;
  width = cur_char_width * (expanded + 1);
  height = cur_char_height;

  if (trs_model == 1 && char_index >= 0xc0) {
    /* On Model I, 0xc0-0xff is another copy of 0x80-0xbf */
    char_index -= 0x40;
  }

  if (char_index >= 0x80 && char_index <= 0xbf && !(currentmode & INVERSE)) {
    /* Use box graphics character bitmap */
    gdk_draw_drawable(trs_screen_pixmap, gc,
		      trs_box[expanded][char_index - 0x80],
		      0, 0, destx, desty, width, height);
  } else {
    int inverse = 0;
    /* Use regular character bitmap */
    if (trs_model > 1 && char_index >= 0xc0 &&
	(currentmode & (ALTERNATE+INVERSE)) == 0) {
      char_index -= 0x40;
    }
    if ((currentmode & INVERSE) && (char_index & 0x80)) {
      inverse = 1;
      char_index &= 0x7f;
    }
    gdk_draw_drawable(trs_screen_pixmap, gc,
		      trs_char[expanded][inverse][char_index],
		      0, 0, destx, desty, width, height);
  }

  /* Overlay grafyx on character */
  if (grafyx_enable) {
    /* assert(grafyx_overlay); */
    int srcx, srcy, duny;
    srcx = ((col + grafyx_xoffset) % G_XSIZE) * cur_char_width;
    srcy = (row * cur_char_height + grafyx_yoffset * scale_y)
	   % (G_YSIZE * scale_y); 
    duny = G_YSIZE * scale_y - srcy;
    gdk_draw_image(trs_screen_pixmap, gc_xor, grafyx_image,
		   srcx, srcy, destx, desty,
		   cur_char_width, MIN(duny, cur_char_height));
    /* Draw wrapped portion if any */
    if (duny < cur_char_height) {
      gdk_draw_image(trs_screen_pixmap, gc_xor, grafyx_image, srcx, 0,
		     destx, desty + duny,
		     cur_char_width, cur_char_height - duny);
    }
  }

  if (hrg_enable) {
    hrg_update_char(position);
  }

  gtk_widget_queue_draw_area(drawing_area, 		      
			     destx, desty, width, height);
}


void
trs_screen_refresh()
{
  int i;
  int srcx, srcy, dunx, duny;

  if (grafyx_enable && !grafyx_overlay) {
    srcx = cur_char_width * grafyx_xoffset;
    srcy = scale_y * grafyx_yoffset;
    dunx = (G_XSIZE * scale_x * 8) - srcx;
    duny = (G_YSIZE * scale_y) - srcy;
    gdk_draw_image(trs_screen_pixmap, gc, grafyx_image,
		   srcx, srcy,
		   left_margin, top_margin,
		   MIN(dunx, cur_char_width * row_chars),
		   MIN(duny, cur_char_height * col_chars));
    /* Draw wrapped portions if any */
    if (dunx < cur_char_width * row_chars) {
      gdk_draw_image(trs_screen_pixmap, gc, grafyx_image,
		     0, srcy,
		     left_margin + dunx, top_margin,
		     cur_char_width * row_chars - dunx,
		     MIN(duny, cur_char_height * col_chars));
    }
    if (duny < cur_char_height * col_chars) {
      gdk_draw_image(trs_screen_pixmap, gc, grafyx_image,
		     srcx, 0,
		     left_margin, top_margin + duny,
		     MIN(dunx, cur_char_width * row_chars),
		     cur_char_height * col_chars - duny);
      if (dunx < cur_char_width * row_chars) {
	gdk_draw_image(trs_screen_pixmap, gc, grafyx_image,
		       0, 0,
		       left_margin + dunx, top_margin + duny,
		       cur_char_width * row_chars - dunx,
		       cur_char_height * col_chars - duny);
      }
    }
    gtk_widget_queue_draw_area(drawing_area, 0, 0,
			       cur_screen_width, cur_screen_height);

  } else {
    for (i = 0; i < screen_chars; i++) {
      trs_screen_write_char(i, trs_screen[i]);
    }
  }
}


/*
 * Copies lines 1 to col_chars - 1 to lines 0 to col_chars - 2.
 * Does not clear line col_chars - 1.
 */
void trs_screen_scroll()
{
  int i = 0;

  for (i = row_chars; i < screen_chars; i++)
    trs_screen[i - row_chars] = trs_screen[i];

  if (grafyx_enable) {
    if (grafyx_overlay) {
      trs_screen_refresh();
    }
  } else if (hrg_enable) {
    trs_screen_refresh();
  } else {
    gdk_draw_drawable(trs_screen_pixmap, gc, trs_screen_pixmap,
		      0, top_margin + cur_char_height, 0, top_margin,
		      cur_screen_width,
		      cur_screen_height - cur_char_height - top_margin * 2);
    gtk_widget_queue_draw(drawing_area);

  }
}


void trs_screen_expanded(int flag)
{
  int bit = flag ? EXPANDED : 0;
  if ((currentmode ^ bit) & EXPANDED) {
    currentmode ^= EXPANDED;
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
    cur_char_height = TRS_CHAR_HEIGHT4 * scale_y;
  } else {
    row_chars = 64;
    col_chars = 16;
    cur_char_height = TRS_CHAR_HEIGHT * scale_y;
  }
  screen_chars = row_chars * col_chars;
  if (resize) {
    cur_screen_width = cur_char_width * row_chars + 2 * border_width;
    left_margin = border_width;
    cur_screen_height = cur_char_height * col_chars + 2 * border_width;
    top_margin = border_width;
    gtk_widget_set_size_request(drawing_area,
				cur_screen_width, cur_screen_height);
  } else {
    left_margin = cur_char_width * (80 - row_chars) / 2 + border_width;
    top_margin = (TRS_CHAR_HEIGHT4 * scale_y * 24 -
		  cur_char_height * col_chars) / 2 + border_width;
    if (left_margin > border_width || top_margin > border_width) {
      gdk_draw_rectangle(trs_screen_pixmap, gc_inv, TRUE,
			 0, 0, cur_screen_width, cur_screen_height);
      gtk_widget_queue_draw(drawing_area);
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


/* 
 * Get and process GTK event(s).
 *
 *   If wait is true, we think there's nothing to do right now and
 *   want to give up the CPU until there is something to do.
 *   Unfortunately we can't simply call gtk_main_iteration_do in
 *   blocking mode, because (currently) we don't timer ticks via GTK.
 *   Instead, trs_interrupt.c uses an itimer and gets a SIGALRM
 *   callback, and that doesn't cause gtk_main_iteration_do to
 *   unblock.  So instead we pause() and then call
 *   gtk_main_iteration_do in nonblocking mode.
 *
 *   If wait is false we definitely don't want to block for events.
 *
 * Handle interrupt-driven uart input here too.
 *
 */ 
void trs_get_event(int wait)
{
  if (trs_model > 1) {
    (void)trs_uart_check_avail();
  }
  if (wait) {
    pause();
    trs_paused = 1;
  }
  do {
    gtk_main_iteration_do(FALSE);
  } while (gtk_events_pending());
}


void
on_reset_menu_item_activate(GtkMenuItem *menuitem,
			    gpointer user_data)
{
  trs_reset(0);
}

void
on_hard_reset_menu_item_activate(GtkMenuItem *menuitem,
				 gpointer user_data)
{
  trs_reset(1);
}

//XXX not used
void
on_disk_change_menu_item_activate(GtkMenuItem *menuitem,
				  gpointer user_data)
{
  trs_change_all();
}


void
on_debugger_menu_item_activate(GtkMenuItem *menuitem,
			       gpointer user_data)
{
  trs_debug();
}

void
on_quit_menu_item_activate(GtkMenuItem *menuitem,
			   gpointer user_data)
{
  gtk_widget_show(quit_dialog);
}

void
on_quit_dialog_response(GtkMenuItem *dialog,
			gint response_id,
			gpointer user_data)
{
  gtk_widget_hide(quit_dialog);
}


typedef const char *get_name_func(int unit);
typedef int set_name_func(int unit, const char *newname);
typedef int create_func(const char *newname);

void
choose_file(GtkMenuItem *menuitem,
	    char *title_fmt,
	    char *remove_lbl,
	    char *insert_lbl,
	    get_name_func get_name,
	    set_name_func set_name,
	    create_func create)
{
  const gchar *label;
  int unit;
  const gchar *title;
  const char *old_filename;
  GtkWidget *chooser;
  GtkResponseType response;
  const gchar *filename;
  int ires;

  label = gtk_menu_item_get_label(menuitem);
  unit = strtoul(&label[1], NULL, 0);
  title = g_strdup_printf(title_fmt, unit);
  old_filename = get_name(unit);

  chooser = gtk_file_chooser_dialog_new(title,
					GTK_WINDOW(main_window),
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					remove_lbl, GTK_RESPONSE_REJECT,
					insert_lbl, GTK_RESPONSE_OK,
					NULL);
  if (old_filename) {
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), old_filename);
  }
  if (strcmp(trs_disk_dir, ".") != 0) {
    gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(chooser),
					 trs_disk_dir, NULL);
  }

  response = gtk_dialog_run(GTK_DIALOG(chooser));
  switch (response) {
  case GTK_RESPONSE_OK:
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    ires = set_name(unit, filename);

    if (ires == ENOENT) {
      GtkWidget *message =
	gtk_message_dialog_new(GTK_WINDOW(main_window),
			       GTK_DIALOG_DESTROY_WITH_PARENT,
			       GTK_MESSAGE_QUESTION,
			       GTK_BUTTONS_YES_NO,
			       "File '%s' does not exist; create it?",
			       filename);
      response = gtk_dialog_run(GTK_DIALOG(message));
      if (response == GTK_RESPONSE_YES) {
	ires = create(filename);
	if (ires == 0) {
	  ires = set_name(unit, filename);
	}
      } else {
	ires = 0;
      }
      gtk_widget_destroy(message);
    }

    if (ires != 0) {
      const gchar *msg = 
	(ires == -1) ? "Unrecognized file format" : g_strerror(ires);
      GtkWidget *message =
	gtk_message_dialog_new(GTK_WINDOW(main_window),
			       GTK_DIALOG_DESTROY_WITH_PARENT,
			       GTK_MESSAGE_ERROR,
			       GTK_BUTTONS_CLOSE,
			       "Error opening file '%s': %s",
			       filename, msg);
      gtk_dialog_run(GTK_DIALOG(message));
      gtk_widget_destroy(message);
    }
    g_free((gpointer)filename);
    break;

  case GTK_RESPONSE_REJECT:
    set_name(unit, NULL);
    break;

  case GTK_RESPONSE_CANCEL:
  default:
    break;
  }

  gtk_widget_destroy(chooser); //XXX hide and reuse instead?
  g_free((gpointer)title);
}

void
on_floppy_menu_item_activate(GtkMenuItem *menuitem,
			     gpointer user_data)
{
  choose_file(menuitem, "Floppy disk in drive %u",
	      "Remove disk", "Insert disk",
	      trs_disk_get_name, trs_disk_set_name, trs_disk_create);
}


void
on_hard_menu_item_activate(GtkMenuItem *menuitem,
			   gpointer user_data)
{
  choose_file(menuitem, "Emulated hard drive %u",
	      "Disconnect drive", "Connect drive",
	      trs_hard_get_name, trs_hard_set_name, trs_hard_create);
}

void
on_stringy_menu_item_activate(GtkMenuItem *menuitem,
			      gpointer user_data)
{
  choose_file(menuitem, "Stringy floppy wafer in drive %u",
	      "Remove wafer", "Insert wafer",
	      stringy_get_name, stringy_set_name, stringy_create);
}

void
on_about_menu_item_activate(GtkMenuItem *menuitem,
			    gpointer user_data)
{
  gtk_widget_show(about_dialog);
}


void
on_about_dialog_response(GtkMenuItem *dialog,
			 gint response_id,
			 gpointer user_data)
{
  gtk_widget_hide(about_dialog);
}

void
on_keys_menu_item_activate(GtkMenuItem *menuitem,
			   gpointer user_data)
{
  if (gtk_widget_get_visible(keys_window))
    gtk_widget_hide(keys_window);
  else
    gtk_widget_show(keys_window);
}

void
on_keys_window_response(GtkMenuItem *menuitem,
			gint response_id,
			gpointer user_data)
{
  gtk_widget_hide(keys_window);
}

gboolean
on_drawing_area_expose_event(GtkWidget *widget,
			     GdkEventExpose  *event,
			     gpointer user_data)
{
  gdk_draw_drawable(widget->window,
		    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		    trs_screen_pixmap,
		    event->area.x, event->area.y,
		    event->area.x, event->area.y,
		    event->area.width, event->area.height);
  return FALSE;
}

guint last_keysym[1 << 16];

gboolean
on_drawing_area_focus_in_event(GtkWidget *widget,
			       GdkEventFocus *event,
			       gpointer user_data)
{
  /*
   * These bugs were fixed in 2009, so I've #if'ed out the 22515
   * workaround and won't worry about lacking a 21454 workaround.
   */
#if WORKAROUND_X_REPEAT_BUG
#if KDEBUG
  debug("focus in\n");
#endif
  /*
   * Disable repeat instead of relying on GDK detectable autorepeat to
   * work around https://bugs.freedesktop.org/show_bug.cgi?id=22515
   *
   * I haven't found a good workaround for
   * https://bugs.freedesktop.org/show_bug.cgi?id=21454
   */
  disable_repeat(drawing_area->window);
#endif
  return FALSE;
}

gboolean
on_drawing_area_focus_out_event(GtkWidget *widget,
				GdkEventFocus *event,
				gpointer user_data)
{
#if WORKAROUND_X_REPEAT_BUG
#if KDEBUG
  debug("focus out\n");
#endif
  restore_repeat(drawing_area->window);
  trs_xlate_keysym(0x10000); /* all keys up */
  memset(last_keysym, 0, sizeof(last_keysym));
#endif
  return FALSE;
}


gboolean
on_drawing_area_key_press_event(GtkWidget *widget,
				GdkEventKey *event,
				gpointer user_data)
{
  guint keysym = event->keyval;

#if KDEBUG
  debug("keypress keysym %#x keycode %#x time %#x\n",
	event->keyval, event->hardware_keycode, event->time);
#endif
  switch (keysym) {
    /* Trap some function keys here */
    /* XXX Remove this and use an accelerator instead, as F8/F9/F10 do */
  case GDK_F7:
    trs_change_all();
    keysym = 0;
    break;
  default:
    break;
  }

  if ( ((event->state & (GDK_SHIFT_MASK|GDK_LOCK_MASK))
	== (GDK_SHIFT_MASK|GDK_LOCK_MASK))
       && keysym >= 'A' && keysym <= 'Z' ) {
    /* Make Shift + CapsLock give lower case */
    keysym = (int) keysym + 0x20;
  }
  if (keysym == GDK_Shift_R && trs_model == 1) {
    keysym = GDK_Shift_L;
  }
  if (last_keysym[event->hardware_keycode] != 0) {
    /*
     * We think this hardware key is already pressed.
     * Assume we are getting X key repeat and ignore it.
     */
    return TRUE;
  }
  last_keysym[event->hardware_keycode] = keysym;
  if (keysym != 0) {
    trs_xlate_keysym(keysym);
  }
  return TRUE;
}


gboolean
on_drawing_area_key_release_event(GtkWidget *widget,
				  GdkEventKey *event,
				  gpointer user_data)
{
  guint keysym;

#if KDEBUG
  debug("keyrelease keysym %#x keycode %#x time %#x\n",
	event->keyval, event->hardware_keycode, event->time);
#endif
  keysym = last_keysym[event->hardware_keycode];
  last_keysym[event->hardware_keycode] = 0;
  if (keysym != 0) {
    /*
     * A keysym generated from this hardware keycode is still
     * pressed; generate a keysym release for it.
     */
    trs_xlate_keysym(0x10000 | keysym);
  }
  return TRUE;
}


/* --- Support for Grafyx Solution and Radio Shack hires graphics --- */

void grafyx_write_byte(int x, int y, char byte)
{
  int h, i, j;
  int screen_x = ((x - grafyx_xoffset + G_XSIZE) % G_XSIZE);
  int screen_y = ((y - grafyx_yoffset + G_YSIZE) % G_YSIZE);
  int on_screen = screen_x < row_chars &&
    screen_y < col_chars*cur_char_height/scale_y;

  if (grafyx_enable && grafyx_overlay && on_screen) {
    /* Erase old byte, preserving text */
    gdk_draw_image(trs_screen_pixmap, gc_xor, grafyx_image,
		   x * cur_char_width, y * scale_y,
		   left_margin + screen_x * cur_char_width,
		   top_margin + screen_y * scale_y,
		   cur_char_width, scale_y);
  }

  /* Save new byte in local memory */
  grafyx_unscaled[y][x] = byte;

#if 1
  /* Update pixels in image */
  for (j = 0; j < scale_y; j++) {
    for (i = 0; i < scale_x; i++) {
      for (h = 0; h < 8; h++) {
	gdk_image_put_pixel(grafyx_image,
			    (screen_x * scale_x + i) * 8 + h,
			    screen_y * scale_y + j,
			    (byte & (0x80 >> h)) ? 1 : 0);
      }
    }
  }
#else
  //XXX Is this faster?  Not implemented yet for scale_x > 1
  for (j = 0; j < scale_y; j++) {
    ((Uchar *) grafyx_image->mem)[grafyx_image->bpl * (y * scale_y + j) + 
				  + x] = byte;
  }
#endif

  if (grafyx_enable && on_screen) {
    /* Draw new byte */
    gdk_draw_image(trs_screen_pixmap,
		   grafyx_overlay ? gc_xor : gc,
		   grafyx_image,
		   x * cur_char_width, y * scale_y,
		   left_margin + screen_x * cur_char_width,
		   top_margin + screen_y * scale_y,
		   cur_char_width, scale_y);

    gtk_widget_queue_draw_area(drawing_area,
			       left_margin + screen_x * cur_char_width,
			       top_margin + screen_y * scale_y,
			       cur_char_width, scale_y);
  }
}

void grafyx_init(void)
{
  int x, y;
  gpointer data;

  /*
   * The "deprecated" and "broken" function gdk_image_new_bitmap is
   * the only way I can find to create a GdkImage with depth 1.
   * Otherwise I can only get one that matches the depth of the real
   * display hardware.  Depth 1 images are useful because
   * gdk_draw_image (like XPutImage) colorizes them while drawing,
   * using the colors from the supplied gc.
   */
  data = malloc(G_XSIZE * scale_x * G_YSIZE * scale_y); //not g_malloc()!
  grafyx_image = gdk_image_new_bitmap(gdk_visual_get_system(), data, 
				      G_XSIZE * 8 * scale_x, G_YSIZE * scale_y);

  for (x = 0; x < G_XSIZE; x++) {
    for (y = 0; y < G_YSIZE; y++) {
      grafyx_write_byte(x, y, 0);
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

/* Model III MicroLabs support */
void grafyx_m3_reset()
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

int grafyx_m3_active()
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

static void
fill_rectangles(GdkGC *gc,
		GdkRectangle *rectangles,
		gint nrectangles)
{
  gint i;
  GdkRectangle *r;

  for (i = 0, r = rectangles; i < nrectangles; i++, r++) {
    gdk_draw_rectangle(trs_screen_pixmap, gc, TRUE,
		       r->x, r->y, r->width, r->height);
    gdk_window_invalidate_rect(drawing_area->window, r, FALSE);
  }
}


/* Initialize HRG. */
static void
hrg_init()
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
      /*|| (trs_screen[position] < 0x80 && line >= 8)*/
      ) {
    /* Only additional bits set, or blank text character.
       No need for update of text. */
    int destx = (position % row_chars) * cur_char_width + left_margin;
    int desty = (position / row_chars) * cur_char_height + top_margin
      + hrg_pixel_y[line];
    int *x = hrg_pixel_x[(currentmode&EXPANDED)!=0];
    int *w = hrg_pixel_width[(currentmode&EXPANDED)!=0];
    int h = hrg_pixel_height[line];
    GdkRectangle rect0[3];    /* 6 bits => max. 3 groups of adjacent "0" bits */
    GdkRectangle rect1[3];
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
    if (n0 != 0) {
      fill_rectangles(gc_inv, rect0, n0);
    }
    if (n1 != 0) {
      fill_rectangles(gc, rect1, n1);
    }
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
hrg_read_data()
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
  GdkRectangle rect[3*12];
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
  if (n != 0) {
    fill_rectangles(gc, rect, n);
  }
}


/*---------- X mouse support --------------*/

int mouse_x_size = 640, mouse_y_size = 240;
int mouse_sens = 3;
int mouse_last_x = -1, mouse_last_y = -1;
unsigned int mouse_last_buttons;
int mouse_old_style = 0;

void trs_get_mouse_pos(int *x, int *y, unsigned int *buttons)
{
  gint win_x, win_y;
  GdkModifierType mask;
  /*
   * In the version of gtk+ I'm using, gdk_window_get_pointer is
   * buggy: it always returns NULL instead of the current gdk window
   * as documented.  Some other related functions handle that properly
   * but their API does not return the modifier mask.  Arrgh.
   */
  gdk_window_get_pointer(drawing_area->window, &win_x, &win_y, &mask);
  if (win_x >= 0 && win_x < cur_screen_width &&
      win_y >= 0 && win_y < cur_screen_height) {
    /* Mouse is within emulator window */
    if (win_x < left_margin) win_x = left_margin;
    if (win_x >= cur_screen_width - left_margin)
      win_x = cur_screen_width - left_margin - 1;
    if (win_y < top_margin) win_y = top_margin;
    if (win_y >= cur_screen_height - top_margin)
      win_y = cur_screen_height - top_margin - 1;
    mouse_last_x = (win_x - left_margin)
      * mouse_x_size
      / (cur_screen_width - 2 * left_margin);
    mouse_last_y = (win_y - top_margin) 
      * mouse_y_size
      / (cur_screen_height - 2 * top_margin);
    mouse_last_buttons = 7;
    /* Note: assuming 3-button mouse */
    if (mask & GDK_BUTTON1_MASK) mouse_last_buttons &= ~4;
    if (mask & GDK_BUTTON2_MASK) mouse_last_buttons &= ~2;
    if (mask & GDK_BUTTON3_MASK) mouse_last_buttons &= ~1;
#if (MOUSEDEBUG & 1)
    debug("get_mouse %d %d 0x%x -> %d %d 0x%x\n",
	  win_x, win_y, mask, mouse_last_x, mouse_last_y, mouse_last_buttons);
#endif
  }
  *x = mouse_last_x;
  *y = mouse_last_y;
  *buttons = mouse_last_buttons;
}

void trs_set_mouse_pos(int x, int y)
{
  GdkScreen *screen = gdk_drawable_get_screen(main_window->window);
  GdkDisplay *display = gdk_screen_get_display(screen);
  gint dest_x, dest_y, src_x, src_y, win_x, win_y;

  if (x == mouse_last_x && y == mouse_last_y) {
    /* Kludge: Ignore warp if it says to move the mouse to where we
       last moved it or said it was. In general someone could really
       want to do that, but with MDRAW, gratuitous warps to the last
       location occur frequently.
    */
    return;
  }
#if (MOUSEDEBUG & 2)
  debug("set_mouse (%d, %d) -> (%d, %d)\n", mouse_last_x, mouse_last_y, x, y);
#endif

  mouse_last_x = x;
  mouse_last_y = y;
  dest_x = left_margin + x * (cur_screen_width - 2*left_margin) / mouse_x_size;
  dest_y = top_margin  + y * (cur_screen_height - 2*top_margin) / mouse_y_size;

  gdk_display_sync(display);
  if (gdk_display_get_window_at_pointer(display, &src_x, &src_y) ==
      drawing_area->window) {
    gdk_window_get_origin(drawing_area->window, &win_x, &win_y);
    gdk_display_warp_pointer(display, screen, win_x + dest_x, win_y + dest_y);

#if (MOUSEDEBUG & 4)
    debug("warp_pointer win@(%d, %d), (%d, %d) -> (%d, %d)\n",
	  win_x, win_y, src_x, src_y, dest_x, dest_y);
#endif
  }
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
  /* Assume 3-button mouse */
  return 1;
}

