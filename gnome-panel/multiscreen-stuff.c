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

#include "multihead-hacks.h"

static int            screens     = 0;
static int           *monitors    = NULL;
static GdkRectangle **geometries  = NULL;
static gboolean	      initialized = FALSE;

#ifdef HAVE_GTK_MULTIHEAD
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

		monitors   [i] = gdk_screen_get_n_monitors (screen);
		geometries [i] = g_new0 (GdkRectangle, monitors [i]);

		for (j = 0; j < monitors [i]; j++)
			gdk_screen_get_monitor_geometry (
				screen, j, &geometries [i][j]);
	}
}

#elif defined(HAVE_LIBXINERAMA)

#include <gdk/gdkx.h>
#include <X11/extensions/Xinerama.h>

static void
multiscreen_support_init (void)
{
	gboolean have_xinerama;

        gdk_flush ();
        gdk_error_trap_push ();
        have_xinerama = XineramaIsActive (GDK_DISPLAY ());
        gdk_flush ();
        if (gdk_error_trap_pop ())
                have_xinerama = FALSE;

        if (g_getenv ("GNOME_PANEL_NO_XINERAMA"))
                have_xinerama = FALSE;

	screens    = 1;
	monitors   = g_new0 (int, screens);
	geometries = g_new0 (GdkRectangle *, screens);

        if (have_xinerama) {
                XineramaScreenInfo *xmonitors;
                int                 n_monitors;
		int                 i;

                xmonitors = XineramaQueryScreens (GDK_DISPLAY (), &n_monitors);
		g_assert (n_monitors > 0);

                monitors [0]   = n_monitors;
                geometries [0] = g_new0 (GdkRectangle, n_monitors);

                for (i = 0; i < n_monitors; i++) {
                        geometries [0][i].x      = xmonitors [i].x_org;
                        geometries [0][i].y      = xmonitors [i].y_org;
                        geometries [0][i].width  = xmonitors [i].width;
                        geometries [0][i].height = xmonitors [i].height;
                }

                XFree (xmonitors);
        } else {
		monitors [0]   = 1;
		geometries [0] = g_new0 (GdkRectangle, monitors [0]);

		geometries [0][0].x      = 0;
		geometries [0][0].y      = 0;
		geometries [0][0].width  = gdk_screen_width ();
		geometries [0][0].height = gdk_screen_height ();
	}
}

#else /* !defined (HAVE_GTK_MULTIHEAD) && !defined(HAVE_LIBXINERAMA) */

static void
multiscreen_support_init (void)
{
	screens    = 1;
	monitors   = g_new0 (int, screens);
	geometries = g_new0 (GdkRectangle *, screens);

	monitors [0]   = 1;
	geometries [0] = g_new0 (GdkRectangle, monitors [0]);

	geometries [0][0].x      = 0;
	geometries [0][0].y      = 0;
	geometries [0][0].width  = gdk_screen_width ();
	geometries [0][0].height = gdk_screen_height ();
}
#endif

void
multiscreen_init (void)
{
	if (initialized)
		return;

	if (g_getenv ("FAKE_XINERAMA_PANEL")) {
		int width, height;

#ifdef HAVE_GTK_MULTIHEAD
		width  = gdk_screen_get_width  (gdk_screen_get_default ());
		height = gdk_screen_get_height (gdk_screen_get_default ());
#else
		width  = gdk_screen_width  ();
		height = gdk_screen_height ();
#endif

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
