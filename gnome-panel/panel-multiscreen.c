/*
 * panel-multiscreen.c: Multi-screen and Xinerama support for the panel.
 *
 * Copyright (C) 2001 George Lebl <jirka@5z.com>
 *               2002 Sun Microsystems Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * Authors: George Lebl <jirka@5z.com>,
 *          Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-multiscreen.h"

static int            screens     = 0;
static int           *monitors    = NULL;
static GdkRectangle **geometries  = NULL;
static gboolean	      initialized = FALSE;

static void
panel_multiscreen_support_init (void)
{
	GdkDisplay *display;
	int         i;

	display = gdk_display_get_default ();
	screens = gdk_display_get_n_screens (display);

	monitors   = g_new0 (int, screens);
	geometries = g_new0 (GdkRectangle *, screens);

	for (i = 0; i < screens; i++) {
		GdkScreen *screen;
		int        j;

		screen = gdk_display_get_screen (display, i);

		g_signal_connect (screen, "size-changed",
				  G_CALLBACK (panel_multiscreen_reinit), NULL);

		monitors   [i] = gdk_screen_get_n_monitors (screen);
		geometries [i] = g_new0 (GdkRectangle, monitors [i]);

		for (j = 0; j < monitors [i]; j++)
			gdk_screen_get_monitor_geometry (
				screen, j, &geometries [i][j]);
	}
}

void
panel_multiscreen_init (void)
{
	if (initialized)
		return;

	if (g_getenv ("PANEL_FAKE_XINERAMA")) {
		GdkScreen *screen;
		int        width, height;

		screen = gdk_screen_get_default ();

		width  = gdk_screen_get_width  (screen);
		height = gdk_screen_get_height (screen);

		screens = 1;
		monitors = g_new0 (int, screens);
		geometries = g_new0 (GdkRectangle *, screens);
		monitors [0] = 2;
		geometries [0] = g_new0 (GdkRectangle, monitors [0]);

		geometries [0][0].x      = 0;
		geometries [0][0].y      = 0;
		geometries [0][0].width  = width / 2;
		geometries [0][0].height = height;

		geometries [0][1].x      = width / 2;
		geometries [0][1].y      = 0;
		geometries [0][1].width  = width - geometries [0][1].x;
		geometries [0][1].height = height;

		initialized = TRUE;

		return;
	}

	panel_multiscreen_support_init ();

	initialized = TRUE;
}

void
panel_multiscreen_reinit (void)
{
	GList *toplevels, *l;

	if (monitors)
		g_free (monitors);

	if (geometries) {
		int i;

		for (i = 0; i < screens; i++)
			g_free (geometries[i]);
		g_free (geometries);
	}

	initialized = FALSE;
	panel_multiscreen_init ();

	toplevels = gtk_window_list_toplevels ();

	for (l = toplevels; l; l = l->next)
		gtk_widget_queue_resize (l->data);

	g_list_free (toplevels);
}
	
int
panel_multiscreen_screens ()
{
	return screens;
}

int
panel_multiscreen_monitors (GdkScreen *screen)
{
	int n_screen;

	n_screen = gdk_screen_get_number (screen);

	g_return_val_if_fail (n_screen >= 0 && n_screen < screens, 1);

	return monitors [n_screen];
}

int
panel_multiscreen_x (GdkScreen *screen,
		     int        monitor)
{
	int n_screen;

	n_screen = gdk_screen_get_number (screen);

	g_return_val_if_fail (n_screen >= 0 && n_screen < screens, 0);
	g_return_val_if_fail (monitor >= 0 || monitor < monitors [n_screen], 0);

	return geometries [n_screen][monitor].x;
}

int
panel_multiscreen_y (GdkScreen *screen,
		     int        monitor)
{
	int n_screen;

	n_screen = gdk_screen_get_number (screen);

	g_return_val_if_fail (n_screen >= 0 && n_screen < screens, 0);
	g_return_val_if_fail (monitor >= 0 || monitor < monitors [n_screen], 0);

	return geometries [n_screen][monitor].y;
}

int
panel_multiscreen_width (GdkScreen *screen,
			 int        monitor)
{
	int n_screen;

	n_screen = gdk_screen_get_number (screen);

	g_return_val_if_fail (n_screen >= 0 && n_screen < screens, 0);
	g_return_val_if_fail (monitor >= 0 || monitor < monitors [n_screen], 0);

	return geometries [n_screen][monitor].width;
}

int
panel_multiscreen_height (GdkScreen *screen,
			  int        monitor)
{
	int n_screen;

	n_screen = gdk_screen_get_number (screen);

	g_return_val_if_fail (n_screen >= 0 && n_screen < screens, 0);
	g_return_val_if_fail (monitor >= 0 || monitor < monitors [n_screen], 0);

	return geometries [n_screen][monitor].height;
}

int
panel_multiscreen_locate_coords (GdkScreen *screen,
				 int        x,
				 int        y)
{
	int n_screen;
	int i;

	n_screen = gdk_screen_get_number (screen);

	for (i = 0; i < monitors [n_screen]; i++)
		if (x >= geometries [n_screen][i].x &&
		    x <  geometries [n_screen][i].x + geometries [n_screen][i].width &&
		    y >= geometries [n_screen][i].y &&
		    y <  geometries [n_screen][i].y + geometries [n_screen][i].height)
			return i;

	return -1;
}

int
panel_multiscreen_locate_widget_monitor (GtkWidget *widget)
{
	GtkWidget *toplevel;
	int        retval = -1;

	toplevel = gtk_widget_get_toplevel (widget);
	if (!toplevel)
		return -1;
	
	g_object_get (toplevel, "monitor", &retval, NULL);

	return retval;
}
