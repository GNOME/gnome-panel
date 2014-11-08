/*
 * Copyright (C) 2007      Peter Teichman
 * Copyright (C) 2005-2006 Davyd Madeley
 * Copyright (C) 2014      Alberts Muktupāvels
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
 *    Davyd Madeley  <davyd@madeley.id.au>
 *    Peter Teichman <peter@novell.com>
 */

#include <config.h>
#include <gtk/gtk.h>
#include <math.h>

#include "clock.h"
#include "clock-face.h"

typedef enum _ClockFaceType ClockFaceType;

struct _ClockFacePrivate
{
	ClockLocation *location;
	ClockTime     *time;
	gboolean       show_seconds;

	gint           hour;
	gint           minute;
	gint           second;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClockFace,
                            clock_face,
                            GTK_TYPE_IMAGE)

enum
{
	PROP_0,
	PROP_LOCATION,
	PROP_TIME,
	PROP_SHOW_SECONDS,
	N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

enum _ClockFaceType
{
	CLOCK_FACE_TYPE_MORNING,
	CLOCK_FACE_TYPE_DAY,
	CLOCK_FACE_TYPE_EVENING,
	CLOCK_FACE_TYPE_NIGHT
};

static gboolean
clock_face_draw (GtkWidget *widget,
                 cairo_t   *cr)
{
	ClockFace *face;
	gdouble    hour_length;
	gdouble    minute_length;
	gdouble    second_length;
	gdouble    x;
	gdouble    y;
	gdouble    radius;
	gdouble    width;
	gdouble    height;

	face = CLOCK_FACE (widget);

	if (GTK_WIDGET_CLASS (clock_face_parent_class)->draw)
		GTK_WIDGET_CLASS (clock_face_parent_class)->draw (widget, cr);

	x = gtk_widget_get_allocated_width (widget) / 2;
	y = gtk_widget_get_allocated_height (widget) / 2;
	radius = MIN (x, y) - 5;

	if (x * 2 >= 50) {
		hour_length = 0.45;
		minute_length = 0.6;
		second_length = 0.65;
	} else {
		hour_length = 0.5;
		minute_length = 0.7;
		second_length = 0.8;
	}

	cairo_set_line_width (cr, 1);

	/* hour hand:
	 * the hour hand is rotated 30 degrees (pi/6 r) per hour +
	 * 1/2 a degree (pi/360 r) per minute
	 */
	width = x + radius * hour_length * sin (M_PI / 6 * face->priv->hour +
	                                        M_PI / 360 * face->priv->minute);
	height = y + radius * hour_length * -cos (M_PI / 6 * face->priv->hour +
	                                          M_PI / 360 * face->priv->minute);

	cairo_save (cr);
	cairo_move_to (cr, x, y);
	cairo_line_to (cr, width, height);
	cairo_stroke (cr);
	cairo_restore (cr);

	/* minute hand:
	 * the minute hand is rotated 6 degrees (pi/30 r) per minute
	 */
	width = x + radius * minute_length * sin (M_PI / 30 * face->priv->minute);
	height = y + radius * minute_length * -cos (M_PI / 30 * face->priv->minute);

	cairo_save (cr);
	cairo_move_to (cr, x, y);
	cairo_line_to (cr, width, height);
	cairo_stroke (cr);
	cairo_restore (cr);

	/* second hand:
	 * operates identically to the minute hand
	 */
	if (face->priv->show_seconds) {
		width = x + radius * second_length * sin (M_PI / 30 * face->priv->second);
		height = y + radius * second_length * -cos (M_PI / 30 * face->priv->second);

		cairo_save (cr);
		cairo_set_source_rgb (cr, 0.937, 0.161, 0.161); /* tango red */
		cairo_move_to (cr, x, y);
		cairo_line_to (cr, width, height);
		cairo_stroke (cr);
		cairo_restore (cr);
	}

	return FALSE;
}

/*
 * FIXME: this should be a gsettings setting,
 * currently hardcoded values:
 * 1. morning : 7 - 9
 * 2. day     : 9 - 17
 * 3. evening : 17 - 22
 * 4. night   : 22 - 7
 */
static ClockFaceType
clock_face_get_type_from_hour (gint hour)
{
	ClockFaceType type;

	if (hour < 7)
		type = CLOCK_FACE_TYPE_NIGHT;
	else if (hour < 9)
		type = CLOCK_FACE_TYPE_MORNING;
	else if (hour < 17)
		type = CLOCK_FACE_TYPE_DAY;
	else if (hour < 22)
		type = CLOCK_FACE_TYPE_EVENING;
	else
		type = CLOCK_FACE_TYPE_NIGHT;

	return type;
}

static gchar *
clock_face_image_from_type (ClockFaceType type)
{
	const gchar *faces[4] = {
		"morning",
		"day",
		"evening",
		"night"
	};

	return g_strconcat (CLOCK_RESOURCE_PATH "icons/",
	                    "clock-face-",
	                    faces[(gint) type],
	                    ".svg",
	                    NULL);
}

static GdkPixbuf *
clock_face_get_pixbuf (ClockFace *face)
{
	ClockFaceType  type;
	gchar         *image;
	gint           width;
	gint           height;
	gint           size;
	GError        *error;
	GdkPixbuf     *pixbuf;

	type = clock_face_get_type_from_hour (face->priv->hour);
	image = clock_face_image_from_type (type);

	width = gtk_widget_get_allocated_width (GTK_WIDGET (face));
	height = gtk_widget_get_allocated_height (GTK_WIDGET (face));

	size = MIN (width, height);

	error = NULL;
	pixbuf = gdk_pixbuf_new_from_resource_at_scale (image,
	                                                size,
	                                                size,
	                                                TRUE,
	                                                &error);
	g_free (image);

	if (error) {
		g_warning ("Failed to load clock face: %s", error->message);
		g_error_free (error);
	}

	return pixbuf;
}

static void
clock_face_hour_changed (ClockTime *time,
                         gint       hour,
                         gpointer   user_data)
{
	ClockFace *face;
	GdkPixbuf *pixbuf;

	face = CLOCK_FACE (user_data);

	face->priv->hour = hour;

	pixbuf = clock_face_get_pixbuf (face);
	gtk_image_set_from_pixbuf (GTK_IMAGE (face), pixbuf);
	g_object_unref (pixbuf);
}

static void
clock_face_minute_changed (ClockTime *time,
                           gint       hour,
                           gint       minute,
                           gpointer   user_data)
{
	ClockFace *face;

	face = CLOCK_FACE (user_data);

	if (face->priv->show_seconds)
		return;

	face->priv->hour = hour;
	face->priv->minute = minute;

	gtk_widget_queue_draw (GTK_WIDGET (face));
}

static void
clock_face_second_changed (ClockTime *time,
                           gint       hour,
                           gint       minute,
                           gint       second,
                           gpointer   user_data)
{
	ClockFace *face;

	face = CLOCK_FACE (user_data);

	if (!face->priv->show_seconds)
		return;

	face->priv->hour = hour;
	face->priv->minute = minute;
	face->priv->second = second;

	gtk_widget_queue_draw (GTK_WIDGET (face));
}

static void
clock_face_size_allocate (GtkWidget     *widget,
                          GtkAllocation *allocation)
{
	ClockFace     *face;
	GtkAllocation  old_allocation;
	GdkPixbuf     *pixbuf;

	gtk_widget_get_allocation (widget, &old_allocation);

	GTK_WIDGET_CLASS (clock_face_parent_class)->size_allocate (widget,
	                                                           allocation);

	if (old_allocation.width == allocation->width &&
	    old_allocation.height == allocation->height)
		return;

	face = CLOCK_FACE (widget);

	if (face->priv->hour == -1)
		return;

	pixbuf = clock_face_get_pixbuf (CLOCK_FACE (widget));
	gtk_image_set_from_pixbuf (GTK_IMAGE (widget), pixbuf);
	g_object_unref (pixbuf);
}

static void
clock_face_finalize (GObject *object)
{
	ClockFace *face;

	face = CLOCK_FACE (object);

	g_clear_object (&face->priv->time);
	g_clear_object (&face->priv->location);

	G_OBJECT_CLASS (clock_face_parent_class)->finalize (object);
}

static void
clock_face_set_location (ClockFace     *face,
                         ClockLocation *location)
{
	if (face->priv->location)
		g_object_unref (face->priv->location);

	face->priv->location = g_object_ref (location);
}

static void
clock_face_set_time (ClockFace *face,
                     ClockTime *time)
{
	if (face->priv->time)
		g_object_unref (face->priv->time);

	face->priv->time = g_object_ref (time);

	g_signal_connect (face->priv->time,
	                  "hour-changed",
	                  G_CALLBACK (clock_face_hour_changed),
	                  face);
	g_signal_connect (face->priv->time,
	                  "minute-changed",
	                  G_CALLBACK (clock_face_minute_changed),
	                  face);
	g_signal_connect (face->priv->time,
	                  "second-changed",
	                  G_CALLBACK (clock_face_second_changed),
	                  face);
}

static void
clock_face_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
	ClockFace *face;

	face = CLOCK_FACE (object);

	switch (property_id)
	{
		case PROP_LOCATION:
			clock_face_set_location (face,
			                         g_value_get_object (value));
			break;
		case PROP_TIME:
			clock_face_set_time (face,
			                     g_value_get_object (value));
			break;
		case PROP_SHOW_SECONDS:
			face->priv->show_seconds = g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
clock_face_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
	ClockFace *face;

	face = CLOCK_FACE (object);

	switch (property_id)
	{
		case PROP_LOCATION:
			g_value_set_object (value, face->priv->location);
			break;
		case PROP_TIME:
			g_value_set_object (value, face->priv->time);
			break;
		case PROP_SHOW_SECONDS:
			g_value_set_boolean (value, face->priv->show_seconds);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
clock_face_class_init (ClockFaceClass *class)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	object_class->finalize = clock_face_finalize;
	object_class->set_property = clock_face_set_property;
	object_class->get_property = clock_face_get_property;

	widget_class->draw = clock_face_draw;
	widget_class->size_allocate = clock_face_size_allocate;

	object_properties[PROP_LOCATION] =
		g_param_spec_object ("location",
		                     "location",
		                     "location",
		                     CLOCK_TYPE_LOCATION,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_READWRITE);

	object_properties[PROP_TIME] =
		g_param_spec_object ("time",
		                     "time",
		                     "time",
		                     CLOCK_TYPE_TIME,
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_READWRITE);

	object_properties[PROP_SHOW_SECONDS] =
		g_param_spec_boolean ("show-seconds",
		                      "show-seconds",
		                      "show-seconds",
		                      FALSE,
		                      G_PARAM_READWRITE);

	g_object_class_install_properties (object_class,
	                                   N_PROPERTIES,
	                                   object_properties);
}

static void
clock_face_init (ClockFace *face)
{
	face->priv = clock_face_get_instance_private (face);
	face->priv->hour = -1;
}

GtkWidget *
clock_face_new (ClockLocation *location,
                ClockTime     *time,
                gboolean       show_seconds)
{
	GObject *object;

	object = g_object_new (CLOCK_TYPE_FACE,
	                       "location", location,
	                       "time", time,
	                       "show-seconds", show_seconds,
	                       NULL);

	return GTK_WIDGET (object);
}
