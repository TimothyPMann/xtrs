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
 * trs_xinterface.c
 *
 * X Windows interface for TRS-80 simulator
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <malloc.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/Xresource.h>

#include "trs_iodefs.h"
#include "trs.h"

#define DEF_FONT "fixed"

extern char trs_char_data[MAXCHARS][12];

/* Private data */
static int trs_screen[SCREENCHARS];
static Pixmap trs_char[MAXCHARS];
static Display *display;
static int screen;
static Window window;
static GC gc;
static ScreenMode currentmode = NORMAL;
static int OrigHeight,OrigWidth;
static int usefont = 1;
static int cur_char_width = TRS_CHAR_WIDTH;
static int cur_char_height = TRS_CHAR_HEIGHT;
static XFontStruct *myfont;

static XrmOptionDescRec opts[] = {
{"-background",	"*background",	XrmoptionSepArg,	(caddr_t)NULL},
{"-bg",		"*background",	XrmoptionSepArg,	(caddr_t)NULL},
{"-foreground",	"*foreground",	XrmoptionSepArg,	(caddr_t)NULL},
{"-fg",		"*foreground",	XrmoptionSepArg,	(caddr_t)NULL},
{"-usefont",	"*usefont",	XrmoptionNoArg,		(caddr_t)"on"},
{"-nofont",	"*usefont",	XrmoptionNoArg,		(caddr_t)"off"},
{"-font",	"*font",	XrmoptionSepArg,	(caddr_t)NULL},
{"-display",	"*display",	XrmoptionSepArg,	(caddr_t)NULL},
{"-debug",	"*debug",	XrmoptionNoArg,		(caddr_t)"on"},
{"-romfile",	"*romfile",	XrmoptionSepArg,	(caddr_t)NULL},
};

static int num_opts = (sizeof opts / sizeof opts[0]);

/*
 * Key event queueing routines:
 */
#define KEY_QUEUE_SIZE	(16)
static int key_queue[KEY_QUEUE_SIZE];
static int key_queue_head;
static int key_queue_entries;

static int key_state = 0;

static void clear_key_queue()
{
    key_queue_head = 0;
    key_queue_entries = 0;
}

static void queue_key(state)
    int state;
{
    if(key_queue_entries < KEY_QUEUE_SIZE)
    {
	key_queue[(key_queue_head + key_queue_entries) % KEY_QUEUE_SIZE] =
	  state;
	key_queue_entries++;
    }
}

static int dequeue_key()
{
    int rval = -1;

    if(key_queue_entries > 0)
    {
	rval = key_queue[key_queue_head];
	key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
	key_queue_entries--;
    }
    return rval;
}

/* Private routines */
void bitmap_init();
void screen_init();
void trs_event();
XEvent get_event();

/* exits if something really bad happens */
int trs_screen_init(argc,argv,debug)
int argc;
char **argv;
int *debug;
{
    int fd,rc;
    Window root_window;
    unsigned long fore_pixel, back_pixel, foreground, background;
    XrmDatabase x_db = NULL;
    XrmDatabase command_db = NULL;
    char option[512];
    char *type;
    XrmValue value;
    Colormap color_map;
    XColor cdef;
    XGCValues gcvals;
    char *fontname;
    int len;
    
    XrmInitialize();
    /* parse command line options */
    XrmParseCommand(&command_db,opts,num_opts,argv[0],&argc,argv);

    (void) sprintf(option, "%s%s", argv[0], ".display");
    (void) XrmGetResource(command_db, option, "Xtrash.Display", &type, &value);
    /* open display */
    if ( (display = XOpenDisplay (value.addr)) == NULL) {
	printf("Unable to open display.");
	exit(-1);
    }

    /* get defaults from server */
    x_db = XrmGetStringDatabase(XResourceManagerString(display));

    XrmMergeDatabases(command_db,&x_db);

    screen = DefaultScreen(display);
    color_map = DefaultColormap(display,screen);

    (void) sprintf(option, "%s%s", argv[0], ".foreground");
    if (XrmGetResource(x_db, option, "Xtrs.Foreground", &type, &value))
    {
	/* printf("foreground is %s\n",value.addr); */
	XParseColor(display, color_map, value.addr, &cdef);
	XAllocColor(display, color_map, &cdef);
	fore_pixel = cdef.pixel;
    } else {
	fore_pixel = WhitePixel(display, screen);
    }

    (void) sprintf(option, "%s%s", argv[0], ".background");
    if (XrmGetResource(x_db, option, "Xtrs.Background", &type, &value))
    {
	XParseColor(display, color_map, value.addr, &cdef);
	XAllocColor(display, color_map, &cdef);
	back_pixel = cdef.pixel;
    } else {
	back_pixel = BlackPixel(display, screen);
    }

    (void) sprintf(option, "%s%s", argv[0], ".debug");
    if (XrmGetResource(x_db, option, "Xtrs.Debug", &type, &value))
    {
	if (strcmp(value.addr,"on") == 0) {
	    *debug = True;
	}
    }

    (void) sprintf(option, "%s%s", argv[0], ".usefont");
    if (XrmGetResource(x_db, option, "Xtrs.Usefont", &type, &value))
    {
	if (strcmp(value.addr,"on") == 0) {
	    usefont = 1;
	} else if (strcmp(value.addr,"off") == 0) {
	    usefont = 0;
	}
    }

    if (usefont) {
	(void) sprintf(option, "%s%s", argv[0], ".font");
	if (XrmGetResource(x_db, option, "Xtrs.Font", &type, &value))
	{
	    len = strlen(value.addr);
	    fontname = malloc(len + 1);
	    strcpy(fontname,value.addr);
	} else {
	    len = strlen(DEF_FONT);
	    fontname = malloc(len+1);
	    strcpy(fontname,DEF_FONT);
	}
    }

    /* UGLY: romfile garbage */
    if (trs_rom_size > 0) {
	/* use compiled in rom */
	trs_load_compiled_rom();
    } else {
	/* try resources */
	(void) sprintf(option, "%s%s", argv[0], ".romfile");
	if (XrmGetResource(x_db, option, "Xtrs.Romfile", &type, &value)) {
	    trs_load_rom(value.addr);
	} else {
#ifdef DEFAULT_ROM
	    trs_load_rom(DEFAULT_ROM);
#else
	    fprintf(stderr,"%s: rom file not specified!\n",argv[0]);
	    exit(-1);
#endif
	}
    }

    clear_key_queue(); /* init the key queue */

    /* setup root window, and gc */
    root_window = DefaultRootWindow(display);

    /* gc = XCreateGC(display, root_window, 0, NULL); */
    gcvals.graphics_exposures = False;
    gc = XCreateGC(display, root_window, GCGraphicsExposures, &gcvals);

    XSetForeground(display, gc, fore_pixel);
    XSetBackground(display, gc, back_pixel);
    foreground = fore_pixel;
    background = back_pixel;

    if (usefont) {
	if ((myfont = XLoadQueryFont(display,fontname)) == NULL) {
	    fprintf(stderr,"%s: Can't open font %s!\n",argv[0],fontname);
	    exit(-1);
	}
	XSetFont(display,gc,myfont->fid);
	cur_char_width =  myfont->max_bounds.width;
	cur_char_height = myfont->ascent + myfont->descent;
    }

    OrigWidth = cur_char_width * ROW_LENGTH;
    OrigHeight = cur_char_height * COL_LENGTH;
    window = XCreateSimpleWindow(display, root_window, 400, 400,
				 OrigWidth, OrigHeight, 1, foreground,
				 background);
    XStoreName(display,window,argv[0]);
    XSelectInput(display, window, ExposureMask | KeyPressMask | MapRequest |
		 KeyReleaseMask | StructureNotifyMask | ResizeRedirectMask );
    XMapWindow(display, window);

    bitmap_init(foreground, background);

    /* screen_init(); */
    XClearWindow(display,window);

    /* set up event handler */
    signal(SIGIO,trs_event);
    fd = ConnectionNumber(display);
    if (fcntl(fd,F_SETOWN,getpid()) < 0)
	perror("fcntl F_SETOWN");
    rc = fcntl(fd,F_SETFL,FASYNC);
    if (rc != 0)
	perror("fcntl F_SETFL async error");

    return(argc);
}

/* ARGSUSED */
void trs_event(signo)
    int signo;
{
    fd_set rd_mask;
    int fd,nfds,nfound;
    struct timeval zero_timeout,*timeout;

    sigblock(sigmask(SIGIO));
    zero_timeout.tv_sec = 0;
    zero_timeout.tv_usec = 0;

    while (Death && Taxes) {
	FD_ZERO(&rd_mask);
	fd = ConnectionNumber(display);
	FD_SET(fd,&rd_mask);
	timeout = &zero_timeout;
	nfds = fd + 1;
	nfound = select(nfds,&rd_mask,NULL,NULL,timeout);
	if (nfound == 0) {
	    sigsetmask(0);
	    return;
	}
	(void) get_event();
    }
}

XEvent get_event()
{
    XEvent event;
    KeySym key;
    char buf[10];
    XComposeStatus status;
    XWindowChanges xwc;

    XNextEvent(display,&event);
    switch(event.type) {
	case Expose:
	    trs_screen_refresh();
	    break;
	case ResizeRequest:
	    xwc.width = OrigWidth;
	    xwc.height = OrigHeight;
	    XConfigureWindow(display,event.xresizerequest.window,
			     (CWWidth | CWHeight),&xwc);
	    XFlush(display);
	    break;
	case ConfigureNotify:
	    break;
	case MapNotify:
	    trs_screen_refresh();
	    break;
	case KeyPress:
	    (void) XLookupString((XKeyEvent *)&event,buf,10,&key,&status);
	    switch (key) {
		case XK_c:
		    if (event.xkey.state & ControlMask)
			key_state = 0x3; /* ctrl-c is break */
		    else
			key_state = (int) key;
		    break;
		case XK_l:
		    if (event.xkey.state & ControlMask)
			key_state = 0x7f; /* ctrl-l is clear */
		    else
			key_state = (int) key;
		    break;
		case XK_r:
		    if (event.xkey.state & ControlMask)
			trs_reset(); /* ctrl-r is reset button */
		    else
			key_state = (int) key;
		    break;
		case XK_q:
		    if (event.xkey.state & ControlMask)
			trs_exit(); /* ctrl-q quits the program */
		    else
			key_state = (int) key;
		    break;
		case XK_Break:
		    key_state = 0x3;
		    break;
		case XK_Return:
		case XK_KP_Enter:
		    key_state = 0x0d;
		    break;
		case XK_Up:
		    key_state = 0x11;
		    break;
		case XK_Down:
		    key_state = 0x12;
		    break;
		case XK_Left:
		case XK_BackSpace:
		case XK_Delete:
		    key_state = 0x13;
		    break;
		case XK_Right:
		    key_state = 0x14;
		    break;
		case XK_Clear:
		    key_state = 0x7f;
		    break;
		default:
		    if(key < 0x80)
			key_state = (int) key;
		    break;
	    }
	    queue_key(key_state);
	    break;
	case KeyRelease:
	    (void) XLookupString((XKeyEvent *)&event,buf,10,&key,&status);
	    key_state = 0;
	    break;
	default:
#ifdef XDEBUG	    
	    fprintf(stderr,"Unhandled event, type is %d \n",event.type);
#endif
	    break;
    }
    return(event);
}

void trs_screen_mode_select(mode)
    ScreenMode mode;
{
    currentmode = mode;
}

void screen_init()
{
    int i;

    /* initially, screen is blank (i.e. full of 32's) */
    for (i = 0; i < SCREENCHARS; i++)
	trs_screen[i] = 32;
}

void bitmap_init(foreground, background)
    unsigned long foreground, background;
{
    if(!usefont) {
	/* Initialize from built-in font bitmaps. */
	int i;
	
	for (i = 0; i < MAXCHARS; i++) {
	    trs_char[i] =
		XCreateBitmapFromData(display,window,trs_char_data[i],
				      TRS_CHAR_WIDTH,TRS_CHAR_HEIGHT);
	}
    } else {
	int graphics_char, bit, p;
	XRectangle bits[6];
	XRectangle cur_bits[6];

	/*
	 * Calculate what the 2x3 boxes look like.
	 */
	bits[0].x = bits[2].x = bits[4].x = 0;
	bits[0].width = bits[2].width = bits[4].width =
	  bits[1].x = bits[3].x = bits[5].x =  cur_char_width / 2;
	bits[1].width = bits[3].width = bits[5].width =
	  cur_char_width - bits[1].x;
	bits[0].y = bits[1].y = 0;
	bits[0].height = bits[1].height =
	  bits[2].y = bits[3].y = cur_char_height / 3;

	bits[4].y = bits[5].y = (cur_char_height * 2) / 3;
	bits[2].height = bits[3].height = bits[4].y - bits[2].y;

	bits[4].height = bits[5].height = cur_char_height - bits[4].y;

	for(graphics_char = 128; graphics_char < 192; ++graphics_char)
	{
	    trs_char[graphics_char] =
		XCreatePixmap(display,window,cur_char_width,cur_char_height,
			      DefaultDepth(display,screen));	

	    /* Clear everything */
	    XSetForeground(display, gc, background);
  	    XFillRectangle(display,trs_char[graphics_char],gc,
			   0,0,cur_char_width,cur_char_height);

	    /* Set the bits */
	    XSetForeground(display, gc, foreground);

	    for(bit = 0, p = 0; bit < 6; ++bit)
	    {
		if(graphics_char & (1 << bit))
		{
		    cur_bits[p++] = bits[bit];
		}
	    }
	    XFillRectangles(display,trs_char[graphics_char],gc,cur_bits,p);
	}
    }
}

void trs_screen_refresh()
{
    int i;

    for (i = 0; i < SCREENCHARS; i++) {
	trs_screen_write_char(i,trs_screen[i],False);
    }
    XFlush(display);
}

void trs_screen_write_char(position, char_index, doflush)
int position;
int char_index;
Bool doflush;
{
    int row,col,destx,desty;
    int plane;
    char temp_char;

    trs_screen[position] = char_index;
    row = position / ROW_LENGTH;
    col = position - (row * ROW_LENGTH);
    destx = col * cur_char_width;
    desty = row * cur_char_height;
    if (usefont) {
	if(char_index >= 128) {
	    /* a graphics character */
	    plane = 1;
	    XCopyArea(display,trs_char[char_index],window,gc,0,0,
		      cur_char_width,cur_char_height,destx,desty);
	} else {
	    desty += myfont->ascent;
	    temp_char = (char)char_index;
	    XDrawImageString(display,window,gc,destx,desty,
			     &temp_char,1);
	}
    } else {
        plane = 1;
	XCopyPlane(display,trs_char[char_index],window,gc,0,0,TRS_CHAR_WIDTH,
		   TRS_CHAR_HEIGHT,destx,desty,plane);
    }
    if (doflush)
	XFlush(display);
}

 /* Copy lines 1 through 15 to lines 0 through 14.  Doesn't need to
    clear line 15. */
void trs_screen_scroll()
{
    int i = 0;

    XCopyArea(display,window,window,gc,0,cur_char_height,
	      (cur_char_width*ROW_LENGTH),(cur_char_height*COL_LENGTH),0,0);
    for (i = 64; i < SCREENCHARS; i++)
	trs_screen[i-64] = trs_screen[i];
}

void trs_screen_write_chars(locations, values, count)
int *locations;
int *values;
int count;
{
    while(count--)
    {
        trs_screen_write_char(*locations++, *values++ , False);
    }
    XFlush(display);
}


int trs_kb_poll()
{
    /* if (key_state != 0)
	printf("key is %d\n",key_state); */

    /* Flush the queued keys and just return the current key state. */
    clear_key_queue();
    return(key_state);
}

int trs_next_key()
{
    XEvent event;
    fd_set rd_mask;
    int fd,nfds;
    int rval;

    /* I don't think this is quite right -- if a key comes in between
       when we try to dequeue and when we block, we'll block unnecessarily. */

    if((rval = dequeue_key()) < 0)
    {
	sigblock(sigmask(SIGIO));
	
	while (Death && Taxes) {
	    FD_ZERO(&rd_mask);
	    fd = ConnectionNumber(display);
	    FD_SET(fd,&rd_mask);
	    nfds = fd + 1;
	    /* block till something happens */
	    (void) select(nfds,&rd_mask,NULL,NULL,NULL);
	    event = get_event();
#ifdef DEBUG
	    fprintf(stderr, "%d\n", event.type);
#endif
	    if (event.type == KeyPress) {
#ifdef DEBUG
		fprintf(stderr, "Key!\n");
#endif
		break;
	    }
	}
	sigsetmask(0);

	rval = dequeue_key();
    }
    return rval;
}
