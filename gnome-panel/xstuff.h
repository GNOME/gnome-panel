#ifndef __XSTUFF_H__
#define __XSTUFF_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

void xstuff_init			(void);
void xstuff_delete_property		(GdkWindow *window,
					 const char *name);
gboolean xstuff_is_compliant_wm		(void);
gboolean xstuff_net_wm_supports         (const char *hint);

void xstuff_set_no_group_and_no_input	(GdkWindow *win);

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

void xstuff_zoom_animate                (GtkWidget    *widget,
					 GdkRectangle *opt_src_rect);

#endif /* __XSTUFF_H__ */
