/*
 * panel-background-monitor.c:
 *
 * Copyright (C) 2001, 2002 Ian McKellar <yakk@yakk.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *      Ian McKellar <yakk@yakk.net>
 *      Mark McLoughlin <mark@skynet.ie>
 */

#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "panel-background-monitor.h"
#include "panel-gdk-pixbuf-extensions.h"

enum {
	CHANGED,
	LAST_SIGNAL
};

static GdkFilterReturn panel_background_monitor_xevent_filter (GdkXEvent *xevent,
							       GdkEvent  *event,
							       gpointer   data);

struct _PanelBackgroundMonitorClass {
	GObjectClass   parent_class;
	void         (*changed) (PanelBackgroundMonitor *monitor);
};

struct _PanelBackgroundMonitor {
	GObject    parent_instance;

	Window     xwindow;
	GdkWindow *gdkwindow;

	Atom       xatom;
	GdkAtom    gdkatom;

	GdkPixmap *gdkpixmap;
	GdkPixbuf *gdkpixbuf;

	int        width;
	int        height;
};

static PanelBackgroundMonitor **global_background_monitors = NULL;

static guint signals [LAST_SIGNAL] = { 0 };

static void
panel_background_monitor_finalize (GObject *object)
{
	PanelBackgroundMonitor *monitor;

	monitor = PANEL_BACKGROUND_MONITOR (object);

	gdk_window_remove_filter (
		monitor->gdkwindow, panel_background_monitor_xevent_filter, monitor);

	if (monitor->gdkpixmap)
		g_object_unref (monitor->gdkpixmap);
	monitor->gdkpixmap = NULL;

	if (monitor->gdkpixbuf)
		g_object_unref (monitor->gdkpixbuf);
	monitor->gdkpixbuf = NULL;
}

static void
panel_background_monitor_class_init (PanelBackgroundMonitorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	signals [CHANGED] = 
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (PanelBackgroundMonitorClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	object_class->finalize = panel_background_monitor_finalize;
}

static void
panel_background_monitor_init (PanelBackgroundMonitor *monitor)
{
	monitor->gdkwindow = NULL;
	monitor->xwindow   = None;

	monitor->gdkatom = gdk_atom_intern ("_XROOTPMAP_ID", FALSE);
	monitor->xatom   = gdk_x11_atom_to_xatom (monitor->gdkatom);

	monitor->gdkpixmap = NULL;
	monitor->gdkpixbuf = NULL;
}

GType
panel_background_monitor_get_type (void)
{
	static GType object_type = 0;

	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (PanelBackgroundMonitorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) panel_background_monitor_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (PanelBackgroundMonitor),
			0,              /* n_preallocs */
			(GInstanceInitFunc) panel_background_monitor_init,
		};

		object_type = g_type_register_static (
					G_TYPE_OBJECT, "PanelBackgroundMonitor", &object_info, 0);
	}

	return object_type;
}

static void
panel_background_monitor_connect_to_screen (PanelBackgroundMonitor *monitor,
					    GdkScreen              *screen)
{
	monitor->gdkwindow = gdk_screen_get_root_window (screen);
	monitor->xwindow   = gdk_x11_drawable_get_xid (monitor->gdkwindow);

	gdk_window_add_filter (
		monitor->gdkwindow, panel_background_monitor_xevent_filter, monitor);

	gdk_window_set_events (
		monitor->gdkwindow, 
		gdk_window_get_events (monitor->gdkwindow) | GDK_PROPERTY_CHANGE_MASK);
}

static PanelBackgroundMonitor *
panel_background_monitor_new (GdkScreen *screen)
{
	PanelBackgroundMonitor *monitor;

	monitor = g_object_new (PANEL_TYPE_BACKGROUND_MONITOR, NULL);

	panel_background_monitor_connect_to_screen (monitor, screen);

	return monitor;
}

PanelBackgroundMonitor *
panel_background_monitor_get_for_screen (GdkScreen *screen)
{
	int screen_number;

	screen_number = gdk_screen_get_number (screen);

	if (!global_background_monitors) {
		int n_screens;

		n_screens = gdk_display_get_n_screens (gdk_display_get_default ());

		global_background_monitors = g_new0 (PanelBackgroundMonitor *, n_screens);
	}

	if (!global_background_monitors [screen_number]) {
		global_background_monitors [screen_number] =
				panel_background_monitor_new (screen);

		g_object_add_weak_pointer (
			G_OBJECT (global_background_monitors [screen_number]),
			(void **) &global_background_monitors [screen_number]);

		return global_background_monitors [screen_number];
	}

	return g_object_ref (global_background_monitors [screen_number]);
}

PanelBackgroundMonitor *
panel_background_monitor_get (void)
{
	return panel_background_monitor_get_for_screen (gdk_screen_get_default ());
}

static GdkFilterReturn
panel_background_monitor_xevent_filter (GdkXEvent *xevent,
					GdkEvent  *event,
					gpointer   data)
{
	PanelBackgroundMonitor *monitor;
	XEvent                 *xev;

	g_return_val_if_fail (PANEL_IS_BACKGROUND_MONITOR (data), GDK_FILTER_CONTINUE);

	monitor = PANEL_BACKGROUND_MONITOR (data);
	xev     = (XEvent *) xevent;

	if (xev->type == PropertyNotify &&
	    xev->xproperty.atom == monitor->xatom &&
	    xev->xproperty.window == monitor->xwindow) {
	    	if (monitor->gdkpixmap)
			g_object_unref (monitor->gdkpixmap);
		monitor->gdkpixmap = NULL;
		if (monitor->gdkpixbuf)
			g_object_unref (monitor->gdkpixbuf);
		monitor->gdkpixbuf = NULL;
		g_signal_emit (monitor, signals [CHANGED], 0);
	}

	return GDK_FILTER_CONTINUE;
}

static void
panel_background_monitor_setup_pixmap (PanelBackgroundMonitor *monitor)
{
	Pixmap	*prop_data = NULL;
	GdkAtom	 prop_type;

	gdk_property_get (
		monitor->gdkwindow, monitor->gdkatom, 0, 0, 10, 
		FALSE, &prop_type, NULL, NULL, (guchar **) &prop_data);

	if ((prop_type == GDK_TARGET_PIXMAP) && prop_data && prop_data [0]) {
		g_assert (monitor->gdkpixmap == NULL);
		monitor->gdkpixmap = gdk_pixmap_foreign_new (prop_data [0]);

		if (!monitor->gdkpixmap)
			g_warning ("couldn't get background pixmap\n");
	}

	g_free (prop_data);
}

static GdkPixbuf *
panel_background_monitor_tile_background (PanelBackgroundMonitor *monitor,
					  int                     width,
					  int                     height)
{
	GdkPixbuf *retval;
	ArtIRect   rect;
	int        tilewidth, tileheight;

	retval = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);

	tilewidth  = gdk_pixbuf_get_width (monitor->gdkpixbuf);
	tileheight = gdk_pixbuf_get_height (monitor->gdkpixbuf);

	rect.x0 = 0;
	rect.y0 = 0;
	rect.x1 = width;
	rect.y1 = height;

	panel_gdk_pixbuf_draw_to_pixbuf_tiled (
		monitor->gdkpixbuf, retval,
		rect, tilewidth, tileheight,
		0, 0, 255, GDK_INTERP_NEAREST);

	return retval;
}

static void 
panel_background_monitor_setup_pixbuf (PanelBackgroundMonitor *monitor)
{
	GdkColormap *colormap = NULL;
	int          rwidth, rheight;
	int          pwidth, pheight;

	if (!monitor->gdkpixmap)
		panel_background_monitor_setup_pixmap (monitor);

	if (!monitor->gdkpixmap)
		return;

	gdk_drawable_get_size (
		GDK_DRAWABLE (monitor->gdkpixmap), &pwidth, &pheight);

	gdk_drawable_get_size (
		GDK_DRAWABLE (monitor->gdkwindow), &rwidth, &rheight);

	monitor->width  = MIN (pwidth,  rwidth);
	monitor->height = MIN (pheight, rheight);

	colormap = gdk_drawable_get_colormap (monitor->gdkwindow);

	g_assert (monitor->gdkpixbuf == NULL);
	monitor->gdkpixbuf = gdk_pixbuf_get_from_drawable (
					NULL, monitor->gdkpixmap, colormap,
					0, 0, 0, 0, 
					monitor->width, monitor->height);

	if (monitor->width < rwidth || monitor->height < rheight) {
		GdkPixbuf *tiled;

		tiled = panel_background_monitor_tile_background (
						monitor, rwidth, rheight);

		g_object_unref (monitor->gdkpixbuf);
		monitor->gdkpixbuf = tiled;

		monitor->width  = rwidth;
		monitor->height = rheight;
	}
}

GdkPixbuf *
panel_background_monitor_get_region (PanelBackgroundMonitor *monitor,
				     int                     x,
				     int                     y,
				     int                     width,
				     int                     height)
{
	GdkPixbuf *pixbuf, *tmpbuf;
	int        subwidth, subheight;
	int        subx, suby;

	if (!monitor->gdkpixbuf)
		panel_background_monitor_setup_pixbuf (monitor);

	if (!monitor->gdkpixbuf)
		return NULL;

	subwidth  = MIN (width,  monitor->width - x);
	subheight = MIN (height, monitor->height - y);

	subx = MAX (x, 0);
	suby = MAX (y, 0);

	if ((subwidth <= 0) || (subheight <= 0) ||
	    (monitor->width-x < 0) || (monitor->height-y < 0) )
		/* region is completely offscreen */
		return gdk_pixbuf_new (
				GDK_COLORSPACE_RGB, FALSE, 8, width, height);

	pixbuf = gdk_pixbuf_new_subpixbuf (
			monitor->gdkpixbuf, subx, suby, subwidth, subheight);

	/* FIXME: don't handle regions off the top or left edge
	 */
	if ((subwidth < width) || (subheight < height)) {
		tmpbuf = gdk_pixbuf_new (
				GDK_COLORSPACE_RGB, FALSE, 8, width, height);
		gdk_pixbuf_composite (
			pixbuf, tmpbuf, 0, 0, subwidth, subheight,
			0.0, 0.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
		g_object_unref (pixbuf);
		pixbuf = tmpbuf;
	}

	return pixbuf;
}
