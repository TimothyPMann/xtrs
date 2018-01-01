/*
 * These bugs were fixed in 2009, so I've #if'ed out the 22515
 * workaround and won't worry about lacking a 21454 workaround.
 */
#if WORKAROUND_X_REPEAT_BUG
/*
 * GDK is supposed to either set or emulate the DetectableAutorepeat
 * feature, which suppresses the extra key release that X autorepeat
 * normally generates.  It's then easy for applications to filter out
 * the extra key press events if desired, because the same key is
 * pressed again without having been released.
 *
 * Unfortunately, I am still getting the key release events!  This
 * appears to be a bug in the version of X included in Ubuntu 9.04:
 * Calling XkbSetDetectableAutoRepeat reports that the feature is
 * supported, but in fact it does not work.  GDK would emulate the
 * feature if it knew it needed to, but this bug fools it into not
 * doing so.  I've reported the bug at
 * https://bugs.freedesktop.org/show_bug.cgi?id=22515
 *
 * Because detectable autorepeat is supposed to work, GDK 2.0 does not
 * wrap XAutoRepeatOff and friends.  Hence this ugly code to reach
 * under the covers and invoke them.
 */

#include <gdk/gdkx.h>
#include "keyrepeat.h"

/*#define REPEAT_DEBUG 1*/

static int repeat_saved = FALSE;
static XKeyboardState repeat_state;

void
disable_repeat(GdkWindow *window)
{
  Display *display = GDK_WINDOW_XDISPLAY(window);
  if (!repeat_saved) {
    XGetKeyboardControl(display, &repeat_state);
    repeat_saved = TRUE;
#if REPEAT_DEBUG
    printf("*** saved repeat state %d\n",
	   repeat_state.global_auto_repeat == AutoRepeatModeOn);
#endif
  }
  XAutoRepeatOff(display);
  XSync(display, False);
#if REPEAT_DEBUG
  printf("*** disabled repeat\n");
#endif
}

void
restore_repeat(GdkWindow *window)
{
  Display *display = GDK_WINDOW_XDISPLAY(window);
  if (repeat_saved) {
    if (repeat_state.global_auto_repeat == AutoRepeatModeOn) {
      XAutoRepeatOn(display);
      XSync(display, False);
    }
#if REPEAT_DEBUG
     printf("*** restored repeat state %d\n",
	   repeat_state.global_auto_repeat == AutoRepeatModeOn);
#endif
  }
}
#endif
