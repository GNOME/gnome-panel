/*
 *   multiscreen-stuff: Multiscreen and Xinerama support for the panel.
 *
 *   Copyright (C) 2001 George Lebl <jirka@5z.com>
 *                 2002 Sun Microsystems Inc.
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

#include "multiscreen-stuff.h"
#include "panel-util.h"
#include "panel.h"
#include "basep-widget.h"
#include "foobar-widget.h"

extern GSList *panel_list;

static int            screens     = 0;
static int           *monitors    = NULL;
static GdkRectangle **geometries  = NULL;
static gboolean	      initialized = FALSE;

static void
multiscreen_screen_size_changed (GdkScreen *screen)
{
	GSList *l;

	multiscreen_reinit ();

	for (l = panel_list; l; l = l->next) {
		PanelData *panel_data = l->data;

		g_assert (panel_data->panel != NULL);

		if (BASEP_IS_WIDGET (panel_data->panel))
			basep_widget_screen_size_changed (
				BASEP_WIDGET (panel_data->panel), screen);

		else if (FOOBAR_IS_WIDGET (panel_data->panel))
			foobar_widget_screen_size_changed (
				FOOBAR_WIDGET (panel_data->panel), screen);
	}
}

static void
multiscreen_support_init (void)
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

		g_signal_connect (screen, "size_changed",
				  G_CALLBACK (multiscreen_screen_size_changed), NULL);

		monitors   [i] = gdk_screen_get_n_monitors (screen);
		geometries [i] = g_new0 (GdkRectangle, monitors [i]);

		for (j = 0; j < monitors [i]; j++)
			gdk_screen_get_monitor_geometry (
				screen, j, &geometries [i][j]);
	}
}

void
multiscreen_init (void)
{
	if (initialized)
		return;

	if (g_getenv ("FAKE_XINERAMA_PANEL")) {
		int width, height;

		width  = gdk_screen_get_width  (gdk_screen_get_default ());
		height = gdk_screen_get_height (gdk_screen_get_default ());

		/* fake xinerama setup for debugging */
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

	multiscreen_support_init ();

	initialized = TRUE;
}

void
multiscreen_reinit (void)
{
	if (monitors)
		g_free (monitors);

	if (geometries) {
		int i;

		for (i = 0; i < screens; i++)
			g_free (geometries[i]);
		g_free (geometries);
	}

	initialized = FALSE;
	multiscreen_init ();
}
	
int
multiscreen_screens ()
{
	g_return_val_if_fail (initialized, 1);

	return screens;
}

int
multiscreen_monitors (int screen)
{
	g_return_val_if_fail (initialized, 1);
	g_return_val_if_fail (screen >= 0 && screen < screens, 1);

	return monitors [screen];
}

int
multiscreen_x (int screen,
	       int monitor)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0 && screen < screens, 0);
	g_return_val_if_fail (monitor >= 0 || monitor < monitors [screen], 0);

	return geometries [screen][monitor].x;
}

int
multiscreen_y (int screen,
	       int monitor)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0 && screen < screens, 0);
	g_return_val_if_fail (monitor >= 0 || monitor < monitors [screen], 0);

	return geometries [screen][monitor].y;
}

int
multiscreen_width (int screen,
		   int monitor)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0 && screen < screens, 0);
	g_return_val_if_fail (monitor >= 0 || monitor < monitors [screen], 0);

	return geometries [screen][monitor].width;
}

int
multiscreen_height (int screen,
		    int monitor)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0 && screen < screens, 0);
	g_return_val_if_fail (monitor >= 0 || monitor < monitors [screen], 0);

	return geometries [screen][monitor].height;
}

int
multiscreen_locate_coords (int screen,
			   int x,
			   int y)
{
	int i;

	for (i = 0; i < monitors [screen]; i++)
		if (x >= geometries [screen][i].x &&
		    x <  geometries [screen][i].x + geometries [screen][i].width &&
		    y >= geometries [screen][i].y &&
		    y <  geometries [screen][i].y + geometries [screen][i].height)
			return i;

	return -1;
}

int
multiscreen_locate_widget (int        screen,
			   GtkWidget *widget)
{
	return panel_monitor_from_toplevel (
				gtk_widget_get_toplevel (widget));
}
