#ifndef XSTUFF_H

#include <gdk/gdkx.h>

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

void xstuff_init			(void);
Atom xstuff_atom_intern			(Display *display,
					 const char *name);
void xstuff_set_simple_hint		(GdkWindow *w,
					 const char *name,
					 long val);
void xstuff_delete_property		(GdkWindow *window,
					 const char *name);
gboolean xstuff_is_compliant_wm		(void);

gpointer get_typed_property_data	(Display *xdisplay,
					 Window   xwindow,
					 Atom     property,
					 Atom     requested_type,
					 gint    *size_p,
					 guint    expected_format);

void xstuff_set_no_group_and_no_input	(GdkWindow *win);

void xstuff_setup_desktop_area		(int screen,
					 int left,
					 int right,
					 int top,
					 int bottom);
void xstuff_unsetup_desktop_area	(void);
void xstuff_set_pos_size		(GdkWindow *window,
					 int x, int y,
					 int w, int h);
void xstuff_set_wmspec_dock_hints       (GdkWindow *window,
					 gboolean autohide);
void xstuff_set_wmspec_strut		(GdkWindow *window,
					 int left,
					 int right,
					 int top,
					 int bottom);

#endif
