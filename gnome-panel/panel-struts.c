/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2003,2004 Rob Adams
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.

 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-struts.h"

#include "panel-multiscreen.h"
#include "panel-xutils.h"

#include <gdk/gdkx.h>

typedef struct {
        PanelToplevel    *toplevel;

	int               monitor;
	
        PanelOrientation  orientation;
	GdkRectangle      geometry;
        int               strut_size;
        int               strut_start;
        int               strut_end;

	GdkRectangle      allocated_geometry;
        int               allocated_strut_size;
        int               allocated_strut_start;
        int               allocated_strut_end;
} PanelStrut;


static GSList *panel_struts_list = NULL;


static inline PanelStrut *
panel_struts_find_strut (PanelToplevel *toplevel)
{
	GSList *l;

	for (l = panel_struts_list; l; l = l->next) {
		PanelStrut *strut = l->data;

		if (strut->toplevel == toplevel)
			break;
	}

	return l ? l->data : NULL;
}

static void
panel_struts_get_monitor_geometry (int        monitor,
				   int       *x,
				   int       *y,
				   int       *width,
				   int       *height)
{
        GdkScreen * screen;

        screen = gdk_screen_get_default ();

        *x      = panel_multiscreen_x      (screen, monitor);
        *y      = panel_multiscreen_y      (screen, monitor);
        *width  = panel_multiscreen_width  (screen, monitor);
        *height = panel_multiscreen_height (screen, monitor);
}

static PanelStrut *
panel_struts_intersect (GSList       *struts,
			GdkRectangle *geometry,
			int           skip)
{
	GSList *l;
	int     i;

	i = 0;
	for (l = struts; l; l = l->next) {
		PanelStrut *strut = l->data;
		int         x1, y1, x2, y2;

		x1 = MAX (strut->allocated_geometry.x, geometry->x);
		y1 = MAX (strut->allocated_geometry.y, geometry->y);

		x2 = MIN (strut->allocated_geometry.x + strut->allocated_geometry.width,
			  geometry->x + geometry->width);
		y2 = MIN (strut->allocated_geometry.y + strut->allocated_geometry.height,
			  geometry->y + geometry->height);

		if (x2 - x1 > 0 && y2 - y1 > 0 && ++i > skip)
			break;
	}

	return l ? l->data : NULL;
}

static int
panel_struts_allocation_overlapped (PanelStrut   *strut,
				    PanelStrut   *overlap,
				    GdkRectangle *geometry,
				    gboolean     *moved_down,
				    int           skip)
{
	int overlap_x1, overlap_y1, overlap_x2, overlap_y2;

	overlap_x1 = overlap->allocated_geometry.x;
	overlap_y1 = overlap->allocated_geometry.y;
	overlap_x2 = overlap->allocated_geometry.x + overlap->allocated_geometry.width;
	overlap_y2 = overlap->allocated_geometry.y + overlap->allocated_geometry.height;

	if (strut->orientation == overlap->orientation) {
		int old_x, old_y;

		old_x = geometry->x;
		old_y = geometry->y;

		switch (strut->orientation) {
		case PANEL_ORIENTATION_TOP:
			geometry->y = overlap_y2;
			strut->allocated_strut_size += geometry->y - old_y;
			break;
		case PANEL_ORIENTATION_BOTTOM:
			geometry->y = overlap_y1 - geometry->height;
			strut->allocated_strut_size += old_y - geometry->y;
			break;
		case PANEL_ORIENTATION_LEFT:
			geometry->x = overlap_x2;
			strut->allocated_strut_size += geometry->x - old_x;
			break;
		case PANEL_ORIENTATION_RIGHT:
			geometry->x = overlap_x1 - geometry->width;
			strut->allocated_strut_size += old_x - geometry->x;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	} else {
		if (strut->orientation & PANEL_HORIZONTAL_MASK ||
		    overlap->orientation & PANEL_VERTICAL_MASK)
			return ++skip;

		switch (overlap->orientation) {
		case PANEL_ORIENTATION_TOP:
			geometry->y = overlap_y2;
			*moved_down = TRUE;
			break;
		case PANEL_ORIENTATION_BOTTOM:
			if (!*moved_down)
				geometry->y = overlap_y1 - geometry->height;
			else if (overlap_y1 > geometry->y)
				geometry->height = overlap_y1 - geometry->y;
			else
				return ++skip;
			break;
		case PANEL_ORIENTATION_LEFT:
		case PANEL_ORIENTATION_RIGHT:
		default:
			g_assert_not_reached ();
			break;
		}

		strut->allocated_strut_start = geometry->y;
		strut->allocated_strut_end   = geometry->y + geometry->height - 1;
	}

	return skip;
}

static gboolean
panel_struts_allocate_struts (PanelToplevel *toplevel,
			      int            monitor)
{
	GSList   *allocated = NULL;
	GSList   *l;
	gboolean  toplevel_changed = FALSE;

	for (l = panel_struts_list; l; l = l->next) {
		PanelStrut   *strut = l->data;
		PanelStrut   *overlap;
		GdkRectangle  geometry;
		int           monitor_x, monitor_y;
		int           monitor_width, monitor_height;
		gboolean      moved_down;
		int           skip;

		if (strut->monitor != monitor)
			continue;

		panel_struts_get_monitor_geometry (strut->monitor,
						   &monitor_x, &monitor_y,
						   &monitor_width, &monitor_height);

		strut->allocated_strut_size  = strut->strut_size;
		strut->allocated_strut_start = strut->strut_start;
		strut->allocated_strut_end   = strut->strut_end;

		geometry = strut->geometry;

		moved_down = FALSE;
		skip = 0;
		while ((overlap = panel_struts_intersect (allocated, &geometry, skip)))
			skip = panel_struts_allocation_overlapped (
				strut, overlap, &geometry, &moved_down, skip);

		if (strut->orientation & PANEL_VERTICAL_MASK) {
			if (geometry.y < monitor_y) {
				geometry.height = geometry.y + geometry.height - monitor_y;
				geometry.y      = monitor_y;
			}
				
			if (geometry.y + geometry.height > monitor_y + monitor_height)
				geometry.height = monitor_y + monitor_height - geometry.y;
		}

		if (strut->allocated_geometry.x      != geometry.x     ||
		    strut->allocated_geometry.y      != geometry.y     ||
		    strut->allocated_geometry.width  != geometry.width ||
		    strut->allocated_geometry.height != geometry.height) {
			strut->allocated_geometry = geometry;

			if (strut->toplevel == toplevel)
				toplevel_changed = TRUE;
			else
				gtk_widget_queue_resize (GTK_WIDGET (strut->toplevel));
		}

		allocated = g_slist_append (allocated, strut);
	}

	g_slist_free (allocated);

	return toplevel_changed;
}

void
panel_struts_set_window_hint (PanelToplevel *toplevel)
{
	GdkScreen  *screen;
	Screen     *xscreen;
	GtkWidget  *widget;
	PanelStrut *strut;
	int         strut_size;
	int         monitor_x, monitor_y, monitor_width, monitor_height;
	int         screen_width, screen_height;
	int         leftmost, rightmost, topmost, bottommost;

	widget = GTK_WIDGET (toplevel);

	if (!gtk_widget_get_realized (widget))
		return;

	if (!(strut = panel_struts_find_strut (toplevel))) {
		panel_struts_unset_window_hint (toplevel);
		return;
	}

	strut_size = strut->allocated_strut_size;

	screen = gdk_screen_get_default ();
	xscreen = gdk_x11_screen_get_xscreen (screen);

	screen_width = WidthOfScreen (xscreen);
	screen_height = HeightOfScreen (xscreen);

	panel_struts_get_monitor_geometry (strut->monitor,
					   &monitor_x,
					   &monitor_y,
					   &monitor_width,
					   &monitor_height);

        panel_multiscreen_is_at_visible_extreme (strut->monitor,
                                                 &leftmost,
                                                 &rightmost,
                                                 &topmost,
                                                 &bottommost);

	switch (strut->orientation) {
	case PANEL_ORIENTATION_TOP:
		if (monitor_y > 0)
			strut_size += monitor_y;
		if (!topmost) strut_size = 0;
		break;
	case PANEL_ORIENTATION_BOTTOM:
		if (monitor_y + monitor_height < screen_height)
			strut_size += screen_height - (monitor_y + monitor_height);
		if (!bottommost) strut_size = 0;
		break;
	case PANEL_ORIENTATION_LEFT:
		if (leftmost && monitor_x > 0)
			strut_size += monitor_x;
		if (!leftmost) strut_size = 0;
		break;
	case PANEL_ORIENTATION_RIGHT:
		if (monitor_x + monitor_width < screen_width)
			strut_size += screen_width - (monitor_x + monitor_width);
		if (!rightmost) strut_size = 0;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	panel_xutils_set_strut (gtk_widget_get_window (widget),
				strut->orientation,
				strut_size,
				strut->allocated_strut_start,
				strut->allocated_strut_end,
				&strut->allocated_geometry);
}

void
panel_struts_unset_window_hint (PanelToplevel *toplevel)
{
	if (!gtk_widget_get_realized (GTK_WIDGET (toplevel)))
		return;

	panel_xutils_unset_strut (gtk_widget_get_window (GTK_WIDGET (toplevel)));
}

static inline int
orientation_to_order (PanelOrientation orientation)
{
        switch (orientation) {
        case PANEL_ORIENTATION_TOP:
                return 1;
        case PANEL_ORIENTATION_BOTTOM:
                return 2;
        case PANEL_ORIENTATION_LEFT:
                return 3;
        case PANEL_ORIENTATION_RIGHT:
                return 4;
        default:
                g_assert_not_reached ();
                return -1;
        }
}

/* Sort in order of
 *   1) monitor
 *   2) top, bottom, left, right
 *   3) strut_start ascending
 *   4) strut_end descending
 */
static int
panel_struts_compare (const PanelStrut *s1,
		      const PanelStrut *s2)
{
	if (s1->monitor != s2->monitor)
		return s1->monitor - s2->monitor;

        if (s1->orientation != s2->orientation)
                return orientation_to_order (s1->orientation) -
			orientation_to_order (s2->orientation);

        if (s1->strut_start != s2->strut_start)
                return s1->strut_start - s2->strut_start;

        if (s1->strut_end != s2->strut_end)
                return s2->strut_end - s1->strut_end;

        return 0;
}

static void
get_strut_geometry (int               monitor,
                    PanelOrientation  orientation,
                    int               strut_size,
                    int               strut_start,
                    int               strut_end,
                    GdkRectangle     *geometry)
{
  int monitor_x;
  int monitor_y;
  int monitor_width;
  int monitor_height;

  panel_struts_get_monitor_geometry (monitor,
                                     &monitor_x,
                                     &monitor_y,
                                     &monitor_width,
                                     &monitor_height);

  switch (orientation)
    {
      case PANEL_ORIENTATION_TOP:
        *geometry = (GdkRectangle) {
          .x = strut_start,
          .y = monitor_y,
          .width = strut_end - strut_start + 1,
          .height = strut_size
        };
        break;

      case PANEL_ORIENTATION_BOTTOM:
        *geometry = (GdkRectangle) {
          .x = strut_start,
          .y = monitor_y + monitor_height - strut_size,
          .width = strut_end - strut_start + 1,
          .height = strut_size
        };
        break;

      case PANEL_ORIENTATION_LEFT:
        *geometry = (GdkRectangle) {
          .x = monitor_x,
          .y = strut_start,
          .width = strut_size,
          .height = strut_end - strut_start + 1
        };
        break;

      case PANEL_ORIENTATION_RIGHT:
        *geometry = (GdkRectangle) {
          .x = monitor_x + monitor_width - strut_size,
          .y = strut_start,
          .width = strut_size,
          .height = strut_end - strut_start + 1
        };
        break;

      default:
        g_assert_not_reached ();
        break;
    }
}

gboolean
panel_struts_register_strut (PanelToplevel    *toplevel,
			     int               monitor,
			     PanelOrientation  orientation,
			     int               strut_size,
			     int               strut_start,
			     int               strut_end)
{
	GdkRectangle geometry;
	PanelStrut *strut;
	gboolean    new_strut = FALSE;

	get_strut_geometry (monitor,
	                    orientation,
	                    strut_size,
	                    strut_start,
	                    strut_end,
	                    &geometry);

	if (!(strut = panel_struts_find_strut (toplevel))) {
		strut = g_new0 (PanelStrut, 1);
		new_strut = TRUE;

	} else if (strut->toplevel    == toplevel    &&
		   strut->orientation == orientation &&
		   strut->monitor     == monitor     &&
		   strut->strut_size  == strut_size  &&
		   strut->strut_start == strut_start &&
		   strut->strut_end   == strut_end   &&
		   gdk_rectangle_equal (&strut->geometry, &geometry))
		return FALSE;

	strut->toplevel    = toplevel;
	strut->orientation = orientation;
	strut->monitor     = monitor;
	strut->strut_size  = strut_size;
	strut->strut_start = strut_start;
	strut->strut_end   = strut_end;
	strut->geometry    = geometry;
	
	if (new_strut)
		panel_struts_list = g_slist_append (panel_struts_list, strut);

	panel_struts_list = g_slist_sort (panel_struts_list,
					  (GCompareFunc) panel_struts_compare);

	return panel_struts_allocate_struts (toplevel, monitor);
}

void
panel_struts_unregister_strut (PanelToplevel *toplevel)
{
	PanelStrut *strut;
	int         monitor;

	if (!(strut = panel_struts_find_strut (toplevel)))
		return;

	monitor = strut->monitor;

	panel_struts_list = g_slist_remove (panel_struts_list, strut);
	g_free (strut);

	panel_struts_allocate_struts (toplevel, monitor);
}

gboolean
panel_struts_update_toplevel_geometry (PanelToplevel *toplevel,
				       int           *x,
				       int           *y,
				       int           *width,
				       int           *height)
{
	PanelStrut *strut;

	g_return_val_if_fail (x != NULL, FALSE);
	g_return_val_if_fail (y != NULL, FALSE);
	
	if (!(strut = panel_struts_find_strut (toplevel)))
		return FALSE;

	*x += strut->allocated_geometry.x - strut->geometry.x;
	*y += strut->allocated_geometry.y - strut->geometry.y;

	if (width != NULL && *width != -1)
		*width  += strut->allocated_geometry.width  - strut->geometry.width;
	if (height != NULL && *height != -1)
		*height += strut->allocated_geometry.height - strut->geometry.height;

	return TRUE;
}
