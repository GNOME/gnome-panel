/*
 * Copyright (C) 2014 Alberts Muktupāvels
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *    Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *    Federico Mena Quintero <federico@novell.com>
 *    Giovanni Campagna <gcampagna@src.gnome.org>
 *    Matthias Clasen <mclasen@redhat.com>
 *    Vincent Untz <vuntz@gnome.org>
 */

#ifndef CLOCK_LOCATION_TILE_H
#define CLOCK_LOCATION_TILE_H

#include <gtk/gtk.h>

#include "clock-location.h"

G_BEGIN_DECLS

#define CLOCK_TYPE_LOCATION_TILE         (clock_location_tile_get_type ())
#define CLOCK_LOCATION_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), \
                                          CLOCK_TYPE_LOCATION_TILE,        \
                                          ClockLocationTile))
#define CLOCK_LOCATION_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    \
                                          CLOCK_TYPE_LOCATION_TILE,        \
                                          ClockLocationTileClass))
#define CLOCK_IS_LOCATION_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
                                          CLOCK_TYPE_LOCATION_TILE))
#define CLOCK_IS_LOCATION_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    \
                                          CLOCK_TYPE_LOCATION_TILE))
#define CLOCK_LOCATION_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  \
                                          CLOCK_TYPE_LOCATION_TILE,        \
                                          ClockLocationTileClass))

typedef struct _ClockLocationTile        ClockLocationTile;
typedef struct _ClockLocationTileClass   ClockLocationTileClass;
typedef struct _ClockLocationTilePrivate ClockLocationTilePrivate;

struct _ClockLocationTile
{
	GtkEventBox               parent;
	ClockLocationTilePrivate *priv;
};

struct _ClockLocationTileClass
{
	GtkEventBoxClass parent_class;

	void (* tile_pressed) (ClockLocationTile *tile,
	                       ClockLocation     *location);
};

GType      clock_location_tile_get_type (void);

GtkWidget *clock_location_tile_new      (ClockLocation *location);

G_END_DECLS

#endif
