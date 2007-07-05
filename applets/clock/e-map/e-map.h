/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Map widget.
 *
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Authors: Hans Petter Jansson <hpj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef E_MAP_H
#define E_MAP_H

#include <gtk/gtkwidget.h>

#define TYPE_E_MAP            (e_map_get_type ())
#define E_MAP(obj)            (GTK_CHECK_CAST ((obj), TYPE_E_MAP, EMap))
#define E_MAP_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_E_MAP, EMapClass))
#define IS_E_MAP(obj)         (GTK_CHECK_TYPE ((obj), TYPE_E_MAP))
#define IS_E_MAP_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_E_MAP))

typedef struct _EMap EMap;
typedef struct _EMapClass EMapClass;
typedef struct _EMapPoint EMapPoint;

struct _EMap
{
	GtkWidget widget;

	/* Private data */
	gpointer priv;
};

struct _EMapClass
{
	GtkWidgetClass parent_class;

	/* Notification signals */
	void (*zoom_fit) (EMap * view);

	/* GTK+ scrolling interface */
	void (*set_scroll_adjustments) (GtkWidget * widget,
					GtkAdjustment * hadj,
					GtkAdjustment * vadj);
};

/* The definition of Dot */

struct _EMapPoint
{
	gchar *name;  /* Can be NULL */
	double longitude, latitude;
	guint32 rgba;
	gpointer user_data;
};


/* --- Widget --- */

GtkType e_map_get_type (void);

EMap *e_map_new (void);

/* Stop doing redraws when map data changes (e.g. by modifying points) */
void e_map_freeze (EMap *map);

/* Do an immediate repaint, and start doing realtime repaints again */
void e_map_thaw (EMap *map);

/* --- Coordinate translation --- */

/* Translates window-relative coords to lat/long */
void e_map_window_to_world (EMap *map,
			    double win_x, double win_y,
			    double *world_longitude, double *world_latitude);

/* Translates lat/long to window-relative coordinates. Note that the
 * returned coordinates can be negative or greater than the current size
 * of the allocation area */
void e_map_world_to_window (EMap *map,
			    double world_longitude, double world_latitude,
			    double *win_x, double *win_y);

/* --- Zoom --- */

double e_map_get_magnification (EMap *map);

/* Pass TRUE if we want the smooth zoom hack */
void e_map_set_smooth_zoom (EMap *map, gboolean state);

/* TRUE if smooth zoom hack will be employed */
gboolean e_map_get_smooth_zoom (EMap *map);

/* NB: Function definition will change shortly */
void e_map_zoom_to_location (EMap *map, double longitude, double latitude);

/* Zoom to mag factor 1.0 */
void e_map_zoom_out (EMap *map);

/* --- Points --- */

EMapPoint *e_map_add_point (EMap *map, gchar *name,
			    double longitude, double latitude,
			    guint32 color_rgba);

void e_map_remove_point (EMap *map, EMapPoint *point);

void e_map_point_get_location (EMapPoint *point,
			       double *longitude, double *latitude);

gchar *e_map_point_get_name (EMapPoint *point);

guint32 e_map_point_get_color_rgba (EMapPoint *point);

void e_map_point_set_color_rgba (EMap *map, EMapPoint *point, guint32 color_rgba);

void e_map_point_set_data (EMapPoint *point, gpointer data);

gpointer e_map_point_get_data (EMapPoint *point);

gboolean e_map_point_is_in_view (EMap *map, EMapPoint *point);

EMapPoint *e_map_get_closest_point (EMap *map, double longitude, double latitude,
				    gboolean in_view);

#endif
