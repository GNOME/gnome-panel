/*
 *   multiscreen-stuff: Xinerama (and in the future multidisplay)
 *   support for the panel
 *
 *   Copyright (C) 2001 George Lebl <jirka@5z.com>
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
 */

#include <config.h>
#include <gnome.h>

#include "panel-include.h"
#include "foobar-widget.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#ifdef HAVE_LIBXINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "multiscreen-stuff.h"

/* some globals */
static int		screens = 1;
static GdkRectangle *	rectangles = NULL;
static gboolean		initialized = FALSE;

void
multiscreen_init (void)
{
#ifdef HAVE_LIBXINERAMA
	gboolean have_xinerama = FALSE;
#endif

	if (initialized)
		return;

	if (g_getenv ("FAKE_XINERAMA_PANEL") != NULL) {
		/* fake xinerama setup for debugging */
		screens = 2;
		rectangles = g_new0 (GdkRectangle, 2);
		rectangles[1].x = 0;
		rectangles[1].y = 0;
		rectangles[1].width = gdk_screen_width () / 2;
		rectangles[1].height = gdk_screen_height () / 2;
		rectangles[0].x = gdk_screen_width () / 2;
		rectangles[0].y = gdk_screen_height () / 2;
		rectangles[0].width = gdk_screen_width () - rectangles[0].x;
		rectangles[0].height = gdk_screen_height () - rectangles[0].y;

		initialized = TRUE;

		return;
	}

#ifdef HAVE_LIBXINERAMA
	gdk_flush ();
	gdk_error_trap_push ();
	have_xinerama = XineramaIsActive (GDK_DISPLAY ());
	gdk_flush ();
	if (gdk_error_trap_pop () != 0)
		have_xinerama = FALSE;

	if (have_xinerama) {
		int screen_num, i;
		XineramaScreenInfo *xscreens =
			XineramaQueryScreens (GDK_DISPLAY (),
					      &screen_num);


		if (screen_num <= 0) {
			/* EEEEEK!, should never happen */
			goto no_xinerama;
		}

		rectangles = g_new0 (GdkRectangle, screen_num);
		screens = screen_num;

		for (i = 0; i < screen_num; i++) {
			rectangles[i].x = xscreens[i].x_org;
			rectangles[i].y = xscreens[i].y_org;
			rectangles[i].width = xscreens[i].width;
			rectangles[i].height = xscreens[i].height;
		}

		XFree (xscreens);
	} else
#endif
	{
#ifdef HAVE_LIBXINERAMA
no_xinerama:
#endif
		/* no xinerama */
		screens = 1;
		rectangles = g_new0 (GdkRectangle, 1);
		rectangles[0].x = 0;
		rectangles[0].y = 0;
		rectangles[0].width = gdk_screen_width ();
		rectangles[0].height = gdk_screen_height ();
	}

	initialized = TRUE;
}

int
multiscreen_screens (void)
{
	g_return_val_if_fail (initialized, 1);

	return screens;
}

int
multiscreen_x (int screen)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0, 0);

	if (screen < screens)
		return rectangles[screen].x;
	else
		return gdk_screen_width () + 10;
}

int
multiscreen_y (int screen)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0, 0);

	if (screen < screens)
		return rectangles[screen].y;
	else
		return gdk_screen_height () + 10;
}

int
multiscreen_width (int screen)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0, 0);

	if (screen < screens)
		return rectangles[screen].width;
	else
		return gdk_screen_width ();
}

int
multiscreen_height (int screen)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0, 0);

	if (screen < screens)
		return rectangles[screen].height;
	else
		return gdk_screen_height ();
}

int
multiscreen_screen_from_pos (int x, int y)
{
	int i;
	for (i = 0; i < screens; i++) {
		if (x >= rectangles[i].x &&
		    x <= rectangles[i].x + rectangles[i].width &&
		    y >= rectangles[i].y &&
		    y <= rectangles[i].y + rectangles[i].height)
			return i;
	}
	return -1;
}

int
multiscreen_screen_from_panel (GtkWidget *widget)
{
	g_return_val_if_fail (widget != NULL, 0);
	g_return_val_if_fail (IS_BASEP_WIDGET (widget) ||
			      IS_FOOBAR_WIDGET (widget), 0);
	
	if (IS_BASEP_WIDGET (widget))
		return BASEP_WIDGET (widget)->screen;
	else if (IS_FOOBAR_WIDGET (widget))
		return FOOBAR_WIDGET (widget)->screen;
	else
		return 0;
}
