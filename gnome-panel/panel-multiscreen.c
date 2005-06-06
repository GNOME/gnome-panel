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

#include <string.h>

extern char **environ;

static int            screens     = 0;
static int           *monitors    = NULL;
static GdkRectangle **geometries  = NULL;
static gboolean	      initialized = FALSE;

void
panel_multiscreen_init (void)
{
	GdkDisplay *display;
	int         i;

	if (initialized)
		return;

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

	initialized = TRUE;
}

void
panel_multiscreen_reinit (void)
{
	GdkDisplay *display;
	GList      *toplevels, *l;
	int         new_screens;
	int         i;

	if (monitors)
		g_free (monitors);

	if (geometries) {
		int i;

		for (i = 0; i < screens; i++)
			g_free (geometries[i]);
		g_free (geometries);
	}

	display = gdk_display_get_default ();
	/* Don't use the screens variable since in the future, we might
	 * want to call this function when a screen appears/disappears. */
	new_screens = gdk_display_get_n_screens (display);

	for (i = 0; i < new_screens; i++) {
		GdkScreen *screen;

		screen = gdk_display_get_screen (display, i);
		g_signal_handlers_disconnect_by_func (screen,
						      panel_multiscreen_reinit,
						      NULL);
	}

	initialized = FALSE;
	panel_multiscreen_init ();

	toplevels = gtk_window_list_toplevels ();

	for (l = toplevels; l; l = l->next)
		gtk_widget_queue_resize (l->data);

	g_list_free (toplevels);
}
	
int
panel_multiscreen_screens (void)
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

typedef struct {
	int x0;
	int y0;
	int x1;
	int y1;
} MonitorBounds;

static inline void
get_monitor_bounds (int            n_screen,
		    int            n_monitor,
		    MonitorBounds *bounds)
{
	g_assert (n_screen >= 0 && n_screen < screens);
	g_assert (n_monitor >= 0 || n_monitor < monitors [n_screen]);
	g_assert (bounds != NULL);

	bounds->x0 = geometries [n_screen][n_monitor].x;
	bounds->y0 = geometries [n_screen][n_monitor].y;
	bounds->x1 = bounds->x0 + geometries [n_screen][n_monitor].width;
	bounds->y1 = bounds->y0 + geometries [n_screen][n_monitor].height;
}

/* determines whether a given monitor is along the visible
 * edge of the logical screen.
 */
void
panel_multiscreen_is_at_visible_extreme (GdkScreen *screen,
					 int        n_monitor,
					 gboolean  *leftmost,
					 gboolean  *rightmost,
					 gboolean  *topmost,
					 gboolean  *bottommost)
{
	MonitorBounds monitor;
	int           n_screen, i;

	n_screen = gdk_screen_get_number (screen);

	*leftmost   = TRUE;
	*rightmost  = TRUE;
	*topmost    = TRUE;
	*bottommost = TRUE;

	g_return_if_fail (n_screen >= 0 && n_screen < screens);
	g_return_if_fail (n_monitor >= 0 || n_monitor < monitors [n_screen]);

	get_monitor_bounds (n_screen, n_monitor, &monitor);
	
	/* go through each monitor and try to find one either right,
	 * below, above, or left of the specified monitor
	 */

	for (i = 0; i < monitors [n_screen]; i++) {
		MonitorBounds iter;

		if (i == n_monitor) continue;

		get_monitor_bounds (n_screen, i, &iter);

		if ((iter.y0 >= monitor.y0 && iter.y0 <  monitor.y1) ||
		    (iter.y1 >  monitor.y0 && iter.y1 <= monitor.y1)) {
			if (iter.x0 < monitor.x0)
				*leftmost = FALSE;
			if (iter.x1 > monitor.x1)
				*rightmost = FALSE;
		}

		if ((iter.x0 >= monitor.x0 && iter.x0 <  monitor.x1) ||
		    (iter.x1 >  monitor.x0 && iter.x1 <= monitor.x1)) {
			if (iter.y0 < monitor.y0)
				*topmost = FALSE;
			if (iter.y1 > monitor.y1)
				*bottommost = FALSE;
		}
	}
}

char **
panel_make_environment_for_screen (GdkScreen  *screen,
				   char      **envp)
{
	char **retval = NULL;
	char  *display_name;
	int    display_index = -1;
	int    i, env_len;

	g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

	if (envp == NULL)
		envp = environ;

	for (env_len = 0; envp[env_len]; env_len++)
		if (strncmp (envp[env_len], "DISPLAY", strlen ("DISPLAY")) == 0)
			display_index = env_len;

	retval = g_new (char *, env_len + 1);
	retval[env_len] = NULL;

	display_name = gdk_screen_make_display_name (screen);

	for (i = 0; i < env_len; i++)
		if (i == display_index)
			retval[i] = g_strconcat ("DISPLAY=", display_name, NULL);
		else
			retval[i] = g_strdup (envp[i]);

	g_assert (i == env_len);

	g_free (display_name);

	return retval;
}
