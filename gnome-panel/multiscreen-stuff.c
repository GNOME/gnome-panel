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

#include "multiscreen-stuff.h"

/* some globals */
static int		screens = 1;
static GdkRectangle *	rectangles = NULL;
static gboolean		initialized = FALSE;


void
multiscreen_init (void)
{
	if (initialized)
		return;

	/* fake xinerama setup for debugging */
	/*screens = 2;
	rectangles = g_new0 (GdkRectangle, 2);
	rectangles[1].x = 0;
	rectangles[1].y = 0;
	rectangles[1].width = gdk_screen_width () / 2;
	rectangles[1].height = gdk_screen_height () / 2;
	rectangles[0].x = gdk_screen_width () / 2;
	rectangles[0].y = gdk_screen_height () / 2;
	rectangles[0].width = gdk_screen_width () / 2;
	rectangles[0].height = gdk_screen_height () / 2;*/

	/* no xinerama */
	screens = 1;
	rectangles = g_new0 (GdkRectangle, 1);
	rectangles[0].x = 0;
	rectangles[0].y = 0;
	rectangles[0].width = gdk_screen_width ();
	rectangles[0].height = gdk_screen_height ();

	initialized = TRUE;
}

int
multiscreen_screens (void)
{
	g_return_val_if_fail (initialized, 0);

	return screens;
}

int
multiscreen_x (int screen)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0 && screen <= screens, 0);

	return rectangles[screen].x;
}

int
multiscreen_y (int screen)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0 && screen <= screens, 0);

	return rectangles[screen].y;
}

int
multiscreen_width (int screen)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0 && screen <= screens, 0);

	return rectangles[screen].width;
}

int
multiscreen_height (int screen)
{
	g_return_val_if_fail (initialized, 0);
	g_return_val_if_fail (screen >= 0 && screen <= screens, 0);

	return rectangles[screen].height;
}
