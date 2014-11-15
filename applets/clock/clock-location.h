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
 *    Carlos Garcia Campos <carlosgc@gnome.org>
 *    Dan Winship <danw@src.gnome.org>
 *    Federico Mena Quintero <federico@novell.com>
 *    Giovanni Campagna <gcampagna@src.gnome.org>
 *    Matthias Clasen <mclasen@redhat.com>
 *    Vincent Untz <vuntz@gnome.org>
 */

#ifndef CLOCK_LOCATION_H
#define CLOCK_LOCATION_H

#include <time.h>
#include <glib.h>
#include <glib-object.h>
#include <libgweather/gweather-weather.h>

#include "clock-utils.h"

G_BEGIN_DECLS

#define CLOCK_TYPE_LOCATION         (clock_location_get_type ())
#define CLOCK_LOCATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), \
                                     CLOCK_TYPE_LOCATION,             \
                                     ClockLocation))
#define CLOCK_LOCATION_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    \
                                     CLOCK_TYPE_LOCATION,             \
                                     ClockLocationClass))
#define CLOCK_IS_LOCATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
                                     CLOCK_TYPE_LOCATION))
#define CLOCK_IS_LOCATION_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    \
                                     CLOCK_TYPE_LOCATION))
#define CLOCK_LOCATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  \
                                     CLOCK_TYPE_LOCATION,             \
                                     ClockLocationClass))

typedef struct _ClockLocation        ClockLocation;
typedef struct _ClockLocationClass   ClockLocationClass;
typedef struct _ClockLocationPrivate ClockLocationPrivate;

struct _ClockLocation
{
	GObject               parent;
	ClockLocationPrivate *priv;
};

struct _ClockLocationClass
{
	GObjectClass parent_class;

	void (* weather_updated) (ClockLocation *location,
	                          GWeatherInfo  *info);
	void (* set_current)     (ClockLocation *location);
};

GType                clock_location_get_type              (void);

ClockLocation       *clock_location_new                   (const gchar   *name,
                                                           const gchar   *metar_code,
                                                           gboolean       override_latlon,
                                                           gdouble        latitude,
                                                           gdouble        longitude);

const gchar         *clock_location_get_tzname            (ClockLocation *location);

const char          *clock_location_get_name              (ClockLocation *location);
void                 clock_location_set_name              (ClockLocation *location,
                                                           const gchar   *name);

gchar               *clock_location_get_city              (ClockLocation *location);
const gchar         *clock_location_get_timezone          (ClockLocation *location);
const gchar         *clock_location_get_tzid              (ClockLocation *location);
void                 clock_location_get_coords            (ClockLocation *location,
                                                           gdouble       *latitude,
                                                           gdouble       *longitude);

GDateTime           *clock_location_localtime             (ClockLocation *location);

gboolean             clock_location_is_current            (ClockLocation *location);
void                 clock_location_make_current          (ClockLocation *location,
                                                           GFunc          callback,
                                                           gpointer       data,
                                                           GDestroyNotify destroy);
gboolean             clock_location_is_current_timezone   (ClockLocation *location);

const gchar         *clock_location_get_weather_code      (ClockLocation *location);
GWeatherInfo        *clock_location_get_weather_info      (ClockLocation *location);

glong                clock_location_get_offset            (ClockLocation *location);

GDesktopClockFormat  clock_location_get_clock_format      (ClockLocation *location);

gboolean             clock_location_setup_weather_tooltip (ClockLocation *location,
                                                           GtkTooltip    *tip);

gboolean             clock_location_equal                 (ClockLocation *location1,
                                                           ClockLocation *location2);

GVariant            *clock_location_serialize             (ClockLocation *location);

G_END_DECLS

#endif
