#ifndef __MULTIHEAD_HACKS__
#define __MULTIHEAD_HACKS__

/* Multihead support:
 *    Remove this when we require gtk+ with multihead.
 */

#include "config.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#ifndef HAVE_GTK_MULTIHEAD

typedef struct _GdkDisplay GdkDisplay;

#define gtk_window_get_screen(a)	NULL
#define gdk_drawable_get_screen(a)	NULL
#define gdk_screen_get_default()	NULL
#define gdk_display_get_default()	NULL
#define gdk_display_get_screen(a,b)	NULL
#define gdk_screen_get_number(a)	0

#define gdk_screen_get_width(a)		gdk_screen_width ()
#define gdk_screen_get_height(a)	gdk_screen_height ()
#define gdk_screen_get_root_window(a)	gdk_get_default_root_window ()

#define gtk_window_set_screen(a,b)
#define gtk_menu_set_screen(a, b)

#define GDK_DISPLAY_XDISPLAY(a)		GDK_DISPLAY ()

#endif

G_END_DECLS

#endif /* __MULTIHEAD_HACKS__ */
