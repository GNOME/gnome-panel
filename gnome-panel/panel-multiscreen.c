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

#include "config.h"
#include "panel-multiscreen.h"
#include "panel-monitor.h"

static guint         reinit_id   = 0;

static void panel_multiscreen_reinit (void);

static gboolean
panel_multiscreen_reinit_idle (gpointer data)
{
	panel_multiscreen_reinit ();
	reinit_id = 0;

	return FALSE;
}

static void
panel_multiscreen_queue_reinit (void)
{
	if (reinit_id)
		return;

	reinit_id = g_idle_add (panel_multiscreen_reinit_idle, NULL);
}

void
panel_multiscreen_init (void)
{
  GdkScreen *screen;

  screen = gdk_screen_get_default ();

  /* We connect to both signals to be on the safe side, but in
   * theory, it should be enough to only connect to
   * monitors-changed. Since we'll likely get two signals, we do
   * the real callback in the idle loop. */
  g_signal_connect (screen, "size-changed",
                    G_CALLBACK(panel_multiscreen_queue_reinit), NULL);
  g_signal_connect (screen, "monitors-changed",
                    G_CALLBACK(panel_multiscreen_queue_reinit), NULL);
}

static void
panel_multiscreen_reinit (void)
{
	GList     *toplevels, *l;

	toplevels = gtk_window_list_toplevels ();

	for (l = toplevels; l; l = l->next)
		gtk_widget_queue_resize (l->data);

	g_list_free (toplevels);
}

typedef struct {
	int x0;
	int y0;
	int x1;
	int y1;
} MonitorBounds;

static inline void
get_monitor_bounds (int            n_monitor,
                    MonitorBounds *bounds)
{
	g_assert (bounds != NULL);

	int x, y, height, width;

	panel_monitor_get_geometry (n_monitor,
	                            &x, &y, &height, &width);

	bounds->x0 = x;
	bounds->y0 = y;
	bounds->x1 = bounds->x0 + width;
	bounds->y1 = bounds->y0 + height;
}

/* determines whether a given monitor is along the visible
 * edge of the logical screen.
 */
void
panel_multiscreen_is_at_visible_extreme (int        monitor_index,
                                         gboolean  *leftmost,
                                         gboolean  *rightmost,
                                         gboolean  *topmost,
                                         gboolean  *bottommost)
{
	MonitorBounds monitor;
	int i, n_monitors;

	n_monitors = gdk_display_get_n_monitors (gdk_display_get_default ());

	*leftmost   = TRUE;
	*rightmost  = TRUE;
	*topmost    = TRUE;
	*bottommost = TRUE;

	g_return_if_fail (monitor_index >= 0 && monitor_index < n_monitors);

	get_monitor_bounds (monitor_index, &monitor);

	/* go through each monitor and try to find one either right,
	 * below, above, or left of the specified monitor
	 */

	for (i = 0; i < n_monitors; i++) {
		MonitorBounds iter;

		if (i == monitor_index) continue;

		get_monitor_bounds (i, &iter);

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
