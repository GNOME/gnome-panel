#ifndef XSTUFF_H

#include <gdk/gdkx.h>

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

void xstuff_init			(void);
void xstuff_set_simple_hint		(GdkWindow *w,
					 GdkAtom atom,
					 long val);
void xstuff_setup_kde_dock_thingie	(GdkWindow *w);

gboolean xstuff_is_compliant_wm		(void);

gpointer get_typed_property_data	(Display *xdisplay,
					 Window   xwindow,
					 Atom     property,
					 Atom     requested_type,
					 gint    *size_p,
					 guint    expected_format);
gboolean send_client_message_1L		(Window recipient,
					 Window event_window,
					 Atom   message_type,
					 long   event_mask,
					 glong  long1);

void xstuff_go_through_client_list	(void);

void xstuff_set_no_group_and_no_input	(GdkWindow *win);

gboolean xstuff_nautilus_desktop_present (void);
void xstuff_setup_desktop_area		(int screen,
					 int left,
					 int right,
					 int top,
					 int bottom);
void xstuff_unsetup_desktop_area	(void);
void xstuff_set_pos_size		(GdkWindow *window,
					 int x, int y,
					 int w, int h);
void xstuff_set_wmspec_dock_hints       (GdkWindow *window);

#endif
