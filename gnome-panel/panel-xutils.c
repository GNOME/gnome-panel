/*
 * panel-xutils.c: X related utility methods.
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include "config.h"

#include "panel-xutils.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

static Atom net_wm_window_type        = None;
static Atom net_wm_window_type_dock   = None;
static Atom net_wm_window_type_normal = None;
static Atom net_wm_strut              = None;

void
panel_xutils_set_window_type (GdkWindow             *gdk_window,	
			      PanelXUtilsWindowType  type)
{
	Display *display;
	Window   window;
	Atom     atoms [2];
	int      i = 0;

	g_return_if_fail (GDK_IS_WINDOW (gdk_window));

	display = GDK_WINDOW_XDISPLAY (gdk_window);
	window  = GDK_WINDOW_XWINDOW (gdk_window);

	if (net_wm_window_type == None)
		net_wm_window_type = XInternAtom (display,
						  "_NET_WM_WINDOW_TYPE",
						  False);

	switch (type) {
	case PANEL_XUTILS_TYPE_DOCK:
		if (net_wm_window_type_dock == None)
			net_wm_window_type_dock = XInternAtom (display,
							       "_NET_WM_WINDOW_TYPE_DOCK",
							       False);
		atoms [i++] = net_wm_window_type_dock;
		break;
	case PANEL_XUTILS_TYPE_NORMAL:
		if (net_wm_window_type_normal == None)
			net_wm_window_type_dock = XInternAtom (display,
							       "_NET_WM_WINDOW_TYPE_NORMAL",
							       False);
		atoms [i++] = net_wm_window_type_normal;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	gdk_error_trap_push ();
	XChangeProperty (display, window, net_wm_window_type,
			 XA_ATOM, 32, PropModeReplace,
			 (guchar *) &atoms, i);
	gdk_error_trap_pop ();
}

void
panel_xutils_set_strut (GdkWindow *gdk_window,
			guint32    left,
			guint32    right,
			guint32    bottom,
			guint32    top)
{
	Display *display;
	Window   window;
	guint32  struts [4];

	g_return_if_fail (GDK_IS_WINDOW (gdk_window));

	display = GDK_WINDOW_XDISPLAY (gdk_window);
	window  = GDK_WINDOW_XWINDOW (gdk_window);

	if (net_wm_strut == None)
		net_wm_strut = XInternAtom (display, "_NET_WM_STRUT", False);

        struts [0] = left;
        struts [1] = right;
        struts [2] = top;
        struts [3] = bottom;

	gdk_error_trap_push ();
	XChangeProperty (display, window, net_wm_strut,
			 XA_CARDINAL, 32, PropModeReplace,
			 (guchar *) &struts, 4);
	gdk_error_trap_pop ();
}
