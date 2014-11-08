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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *    Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 */

#include <config.h>

#include "clock-time.h"

struct _ClockTimePrivate
{
	ClockLocation *location;

	guint          timeout_id;

	gint           hour;
	gint           minute;
	gint           second;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClockTime,
                            clock_time,
                            G_TYPE_OBJECT)

enum
{
	HOUR_CHANGED,
	MINUTE_CHANGED,
	SECOND_CHANGED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_LOCATION,
	N_PROPERTIES
};

static guint object_signals[LAST_SIGNAL] = { 0, };
static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static gboolean
clock_time_timeout (gpointer user_data)
{
	ClockTime *time;
	GDateTime *date_time;
	gint       hour;
	gint       minute;
	gint       second;

	time = CLOCK_TIME (user_data);

	if (!time->priv->location)
		return TRUE;

	date_time = clock_location_localtime (time->priv->location);

	hour = g_date_time_get_hour (date_time);
	minute = g_date_time_get_minute (date_time);
	second = g_date_time_get_second (date_time);

	g_date_time_unref (date_time);

	if (time->priv->hour != hour)
		g_signal_emit (time,
		               object_signals[HOUR_CHANGED],
		               0,
		               hour);

	if (time->priv->minute != minute)
		g_signal_emit (time,
		               object_signals[MINUTE_CHANGED],
		               0,
		               hour,
		               minute);

	if (time->priv->second != second)
		g_signal_emit (time,
		               object_signals[SECOND_CHANGED],
		               0,
		               hour,
		               minute,
		               second);

	time->priv->hour = hour;
	time->priv->minute = minute;
	time->priv->second = second;

	return TRUE;
}

static void
clock_time_finalize (GObject *object)
{
	ClockTime *time;

	time = CLOCK_TIME (object);

	if (time->priv->timeout_id > 0) {
		g_source_remove (time->priv->timeout_id);
		time->priv->timeout_id = 0;
	}

	g_clear_object (&time->priv->location);

	G_OBJECT_CLASS (clock_time_parent_class)->finalize (object);
}

static void
clock_time_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
	ClockTime     *time;
	ClockLocation *location;

	time = CLOCK_TIME (object);

	switch (property_id)
	{
		case PROP_LOCATION:
			if (time->priv->location)
				g_object_unref (time->priv->location);

			location = g_value_get_object (value);

			time->priv->location = g_object_ref (location);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
clock_time_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
	ClockTime *time;

	time = CLOCK_TIME (object);

	switch (property_id)
	{
		case PROP_LOCATION:
			g_value_set_object (value, time->priv->location);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
clock_time_class_init (ClockTimeClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = clock_time_finalize;
	object_class->set_property = clock_time_set_property;
	object_class->get_property = clock_time_get_property;

	object_signals[HOUR_CHANGED] =
		g_signal_new ("hour-changed",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (ClockTimeClass, hour_changed),
		              NULL,
		              NULL,
		              NULL,
		              G_TYPE_NONE,
		              1,
		              G_TYPE_INT);

	object_signals[MINUTE_CHANGED] =
		g_signal_new ("minute-changed",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (ClockTimeClass, minute_changed),
		              NULL,
		              NULL,
		              NULL,
		              G_TYPE_NONE,
		              2,
		              G_TYPE_INT,
		              G_TYPE_INT);

	object_signals[SECOND_CHANGED] =
		g_signal_new ("second-changed",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (ClockTimeClass, second_changed),
		              NULL,
		              NULL,
		              NULL,
		              G_TYPE_NONE,
		              3,
		              G_TYPE_INT,
		              G_TYPE_INT,
		              G_TYPE_INT);

	object_properties[PROP_LOCATION] =
		g_param_spec_object ("location",
		                     "location",
		                     "location",
		                     CLOCK_TYPE_LOCATION,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_READWRITE);

	g_object_class_install_properties (object_class,
	                                   N_PROPERTIES,
	                                   object_properties);
}

static void
clock_time_init (ClockTime *time)
{
	time->priv = clock_time_get_instance_private (time);

	time->priv->hour = -1;
	time->priv->minute = -1;
	time->priv->second = -1;

	time->priv->timeout_id = g_timeout_add (100,
	                                        (GSourceFunc) clock_time_timeout,
	                                        time);
}

ClockTime *
clock_time_new (ClockLocation *location)
{
	GObject *object;

	object = g_object_new (CLOCK_TYPE_TIME,
	                       "location", location,
	                       NULL);

	return CLOCK_TIME (object);
}
