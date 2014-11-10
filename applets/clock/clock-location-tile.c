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
 *    Federico Mena Quintero <federico@novell.com>
 *    Frank Solensky <frank@src.gnome.org>
 *    Frédéric Crozat <fcrozat@src.gnome.org>
 *    Giovanni Campagna <gcampagna@src.gnome.org>
 *    Jens Granseuer <jensgr@gmx.net>
 *    Jeremy Bicha <jbicha@ubuntu.com>
 *    Matthias Clasen <mclasen@redhat.com>
 *    Milan Bouchet-Valat <nalimilan@club.fr>
 *    Vincent Untz <vuntz@gnome.org>
 */

#include <config.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "clock-face.h"
#include "clock-location-tile.h"
#include "clock-time.h"
#include "set-timezone.h"

struct _ClockLocationTilePrivate
{
	ClockLocation *location;
	ClockTime     *time;

	gulong         weather_updated_id;
	gulong         clock_format_id;

	GtkWidget     *time_label;

	GtkWidget     *button_label;
	GtkWidget     *button;
	GtkWidget     *marker;
	GtkWidget     *spacer;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClockLocationTile,
                            clock_location_tile,
                            GTK_TYPE_EVENT_BOX)

enum
{
	TILE_PRESSED,
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

static void
make_current_cb (gpointer  user_data,
                 GError   *error)
{
	GtkWidget *dialog;

	if (!error)
		return;

	dialog = gtk_message_dialog_new (NULL,
	                                 0,
	                                 GTK_MESSAGE_ERROR,
	                                 GTK_BUTTONS_CLOSE,
	                                 _("Failed to set the system timezone"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
	                                          "%s",
	                                          error->message);
	g_error_free (error);

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_window_present (GTK_WINDOW (dialog));
}

static void
make_current (GtkWidget *widget,
              gpointer   user_data)
{
	ClockLocationTile *tile;

	tile = CLOCK_LOCATION_TILE (user_data);

	clock_location_make_current (tile->priv->location,
	                             (GFunc) make_current_cb,
	                             NULL,
	                             NULL);
}

static gboolean
clock_location_tile_enter_or_leave (GtkWidget        *widget,
                                    GdkEventCrossing *event,
                                    gpointer          user_data)
{
	ClockLocationTile *tile;

	if (event->mode != GDK_CROSSING_NORMAL)
		return TRUE;

	tile = CLOCK_LOCATION_TILE (user_data);

	if (clock_location_is_current (tile->priv->location)) {
		gtk_widget_hide (tile->priv->button);
		gtk_widget_show (tile->priv->marker);
		gtk_widget_hide (tile->priv->spacer);

		return TRUE;
	}

	if (event->type == GDK_ENTER_NOTIFY) {
		gint can_set;

		if (clock_location_is_current_timezone (tile->priv->location)) {
			can_set = 2;
		} else {
			can_set = can_set_system_timezone ();
		}

		if (can_set != 0) {
			const gchar *markup;

			if (can_set == 1) {
				markup = _("<small>Set...</small>");
			} else {
				markup = _("<small>Set</small>");
			}

			gtk_label_set_markup (GTK_LABEL (tile->priv->button_label),
			                      markup);

			gtk_widget_show (tile->priv->button);
			gtk_widget_hide (tile->priv->marker);
			gtk_widget_hide (tile->priv->spacer);
		} else {
			gtk_widget_hide (tile->priv->button);
			gtk_widget_hide (tile->priv->marker);
			gtk_widget_show (tile->priv->spacer);
		}
	} else {
		if (event->detail != GDK_NOTIFY_INFERIOR) {
			gtk_widget_hide (tile->priv->button);
			gtk_widget_hide (tile->priv->marker);
			gtk_widget_show (tile->priv->spacer);
		}
	}

	return TRUE;
}

static gboolean
show_weather_tooltip (GtkWidget  *widget,
                      gint        x,
                      gint        y,
                      gboolean    keyboard_mode,
                      GtkTooltip *tooltip,
                      gpointer    user_data)
{
	ClockLocationTile *tile;

	tile = CLOCK_LOCATION_TILE (user_data);

	return clock_location_setup_weather_tooltip (tile->priv->location,
	                                             tooltip);
}

static void
update_weather_icon (ClockLocation *location,
                     GWeatherInfo  *info,
                     gpointer       user_data)
{
	GtkWidget   *icon;
	const gchar *icon_name;

	icon = GTK_WIDGET (user_data);

	if (!info || !gweather_info_is_valid (info)) {
		gtk_widget_hide (icon);
		return;
	}

	icon_name = gweather_info_get_icon_name (info);
	gtk_image_set_from_icon_name (GTK_IMAGE (icon),
	                              icon_name,
	                              GTK_ICON_SIZE_INVALID);
	gtk_image_set_pixel_size (GTK_IMAGE (icon), 16);
	gtk_widget_show (icon);
}

static GtkWidget *
clock_location_tile_get_weather_icon (ClockLocationTile *tile)
{
	ClockLocation *location;
	GtkWidget     *icon;

	location = tile->priv->location;
	icon = gtk_image_new ();

	gtk_widget_set_no_show_all (icon, TRUE);
	gtk_widget_set_has_tooltip (icon, TRUE);
	gtk_widget_set_valign (icon, GTK_ALIGN_START);

	g_signal_connect (icon, "query-tooltip",
	                  G_CALLBACK (show_weather_tooltip), tile);

	tile->priv->weather_updated_id =
		g_signal_connect (location, "weather-updated",
		                  G_CALLBACK (update_weather_icon), icon);

	update_weather_icon (location,
	                     clock_location_get_weather_info (location),
	                     icon);

	return icon;
}

static GtkWidget *
clock_location_tile_get_title (ClockLocation *location)
{
	GtkWidget *label;
	gchar     *markup;

	label = gtk_label_new (NULL);
	markup = g_strdup_printf ("<big><b>%s</b></big>",
	                          clock_location_get_name (location));

	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);

	gtk_label_set_xalign (GTK_LABEL (label), 0);

	return label;
}

static gchar *
format_time (ClockLocationTile *tile)
{
	GDateTime           *time;
	GDateTime           *time_local;
	const gchar         *tzname;
	glong                offset;
	gint                 day_of_week;
	gint                 day_of_week_local;
	GDesktopClockFormat  clock_format;
	const gchar         *format;
	gchar               *buf;
	glong                hours;
	glong                minutes;
	gchar               *tmp;

	time = clock_location_localtime (tile->priv->location);
	time_local = g_date_time_new_now_local ();
	tzname = clock_location_get_tzname (tile->priv->location);
	offset = -clock_location_get_offset (tile->priv->location);

	day_of_week = g_date_time_get_day_of_week (time);
	day_of_week_local = g_date_time_get_day_of_week (time_local);

	clock_format = clock_location_get_clock_format (tile->priv->location);

	if (day_of_week != day_of_week_local) {
		if (clock_format == G_DESKTOP_CLOCK_FORMAT_12H) {
			/* Translators: This is a strftime format string.
			 * It is used to display the time in 12-hours format
			 * (eg, like in the US: 8:10 am), when the local
			 * weekday differs from the weekday at the location
			 * (the %A expands to the weekday). The %p expands to
			 * am/pm. */
			format = _("%l:%M <small>%p (%A)</small>");
		}
		else {
			/* Translators: This is a strftime format string.
			 * It is used to display the time in 24-hours format
			 * (eg, like in France: 20:10), when the local
			 * weekday differs from the weekday at the location
			 * (the %A expands to the weekday). */
			format = _("%H:%M <small>(%A)</small>");
		}
	} else {
		if (clock_format == G_DESKTOP_CLOCK_FORMAT_12H) {
			/* Translators: This is a strftime format string.
			 * It is used to display the time in 12-hours format
			 * (eg, like in the US: 8:10 am). The %p expands to
			 * am/pm. */
			format = _("%l:%M <small>%p</small>");
		}
		else {
			/* Translators: This is a strftime format string.
			 * It is used to display the time in 24-hours format
			 * (eg, like in France: 20:10). */
			format = _("%H:%M");
		}
	}

	g_date_time_unref (time_local);

	buf = g_date_time_format (time, format);
	g_date_time_unref (time);

	hours = offset / 3600;
	minutes = labs (offset % 3600) / 60;

	if (hours != 0 && minutes != 0) {
		tmp = g_strdup_printf ("%s <small>%s %+ld:%ld</small>", buf, tzname, hours, minutes);
	} else if (hours != 0) {
		tmp = g_strdup_printf ("%s <small>%s %+ld</small>", buf, tzname, hours);
	} else {
		tmp = g_strdup_printf ("%s <small>%s</small>", buf, tzname);
	}

	g_free (buf);

	return tmp;
}

static GtkWidget *
clock_location_tile_get_time (ClockLocationTile *tile)
{
	GtkWidget *label;
	gchar     *markup;

	label = gtk_label_new (NULL);
	markup = format_time (tile);

	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);

	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_label_set_yalign (GTK_LABEL (label), 0);

	return label;
}

static void
update_clock_face_size (GtkWidget     *widget,
                        GtkAllocation *allocation,
                        gpointer       user_data)
{
	GtkWidget *face;
	guint      size;

	face = GTK_WIDGET (user_data);
	size = allocation->height;

	gtk_widget_set_size_request (face, size, size);
}

static GtkWidget *
clock_location_tile_get_button (ClockLocationTile *tile)
{
	const gchar *tooltip;
	GtkWidget   *button;
	GtkWidget   *label;

	button = gtk_button_new ();
	gtk_widget_set_no_show_all (button, TRUE);
	gtk_widget_hide (button);

	tooltip = _("Set location as current location and use its timezone for this computer");
	gtk_widget_set_tooltip_text (button, tooltip);

	label = gtk_label_new (NULL);
	gtk_container_add (GTK_CONTAINER (button), label);
	gtk_widget_show (label);

	g_signal_connect (button, "clicked",
	                  G_CALLBACK (make_current), tile);

	tile->priv->button = button;
	tile->priv->button_label = label;

	return button;
}

static GtkWidget *
clock_location_tile_get_marker (ClockLocationTile *tile)
{
	GtkWidget *marker;

	marker = gtk_image_new_from_icon_name ("go-home",
	                                       GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_no_show_all (marker, TRUE);
	if (clock_location_is_current (tile->priv->location))
		gtk_widget_show (marker);

	tile->priv->marker = marker;

	return marker;
}

static void
clock_location_tile_fill (ClockLocationTile *tile)
{
	GtkWidget    *box1;
	GtkWidget    *face;
	GtkWidget    *info;
	GtkWidget    *city;
	GtkWidget    *box2;
	GtkWidget    *icon;
	GtkWidget    *time;
	GtkWidget    *button;
	GtkWidget    *marker;
	GtkWidget    *strut;
	GtkSizeGroup *button_group;
	GtkSizeGroup *group;

	gtk_container_set_border_width (GTK_CONTAINER (tile), 3);

	box1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add (GTK_CONTAINER (tile), box1);

	face = clock_face_new (tile->priv->location,
	                       tile->priv->time,
	                       FALSE);
	gtk_box_pack_start (GTK_BOX (box1), face, FALSE, FALSE, 0);
	gtk_widget_show (face);

	g_signal_connect (box1, "size-allocate",
	                  G_CALLBACK (update_clock_face_size), face);

	info = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	gtk_box_pack_start (GTK_BOX (box1), info, TRUE, TRUE, 0);
	gtk_widget_show (info);

	city = clock_location_tile_get_title (tile->priv->location);
	gtk_box_pack_start (GTK_BOX (info), city, FALSE, FALSE, 0);
	gtk_widget_show (city);

	box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (info), box2, FALSE, FALSE, 0);
	gtk_widget_show (box2);

	icon = clock_location_tile_get_weather_icon (tile);
	gtk_box_pack_start (GTK_BOX (box2), icon, FALSE, FALSE, 0);

	time = clock_location_tile_get_time (tile);
	gtk_box_pack_start (GTK_BOX (box2), time, FALSE, FALSE, 0);
	gtk_widget_show (time);
	tile->priv->time_label = time;

	button = clock_location_tile_get_button (tile);
	gtk_box_pack_end (GTK_BOX (box2), button, FALSE, FALSE, 0);
	tile->priv->button = button;

	marker = clock_location_tile_get_marker (tile);
	gtk_box_pack_end (GTK_BOX (box2), marker, FALSE, FALSE, 0);
	tile->priv->marker = marker;

	tile->priv->spacer = gtk_event_box_new ();
	gtk_widget_set_no_show_all (tile->priv->spacer, TRUE);
	gtk_box_pack_start (GTK_BOX (box2), tile->priv->spacer, FALSE, FALSE, 0);

	strut = gtk_event_box_new ();
	gtk_box_pack_start (GTK_BOX (box2), strut, TRUE, TRUE, 0);

	button_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
	gtk_size_group_set_ignore_hidden (button_group, FALSE);
	gtk_size_group_add_widget (button_group, strut);
	gtk_size_group_add_widget (button_group, tile->priv->button);
	g_object_unref (button_group);

	group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
	gtk_size_group_set_ignore_hidden (group, FALSE);
	gtk_size_group_add_widget (group, tile->priv->button);
	gtk_size_group_add_widget (group, tile->priv->marker);
	gtk_size_group_add_widget (group, tile->priv->spacer);
	g_object_unref (group);
}

static gboolean
clock_location_tile_button_press (GtkWidget      *widget,
                                  GdkEventButton *event,
                                  gpointer        user_data)
{
	ClockLocationTile *tile;

	tile = CLOCK_LOCATION_TILE (user_data);

	g_signal_emit (tile, object_signals[TILE_PRESSED], 0,
	               tile->priv->location);

	return TRUE;
}

static void
update_time_label (ClockLocationTile *tile)
{
	gchar *markup;

	markup = format_time (tile);

	gtk_label_set_markup (GTK_LABEL (tile->priv->time_label),
	                      markup);

	g_free (markup);
}

static void
clock_location_tile_clock_format_changed (ClockLocation       *location,
                                          GDesktopClockFormat  clock_format,
                                          gpointer             user_data)
{
	ClockLocationTile *tile;

	tile = CLOCK_LOCATION_TILE (user_data);

	update_time_label (tile);
}

static void
clock_location_tile_minute_changed (ClockTime *time,
                                    gint       hour,
                                    gint       minute,
                                    gpointer   user_data)
{
	ClockLocationTile *tile;

	tile = CLOCK_LOCATION_TILE (user_data);

	update_time_label (tile);
}

static void
clock_location_tile_set_location (ClockLocationTile *tile,
                                  ClockLocation     *location)
{
	if (tile->priv->location)
		g_object_unref (tile->priv->location);

	if (tile->priv->time)
		g_object_unref (tile->priv->time);

	tile->priv->location = g_object_ref (location);
	tile->priv->time = clock_time_new (location);

	tile->priv->clock_format_id =
		g_signal_connect (tile->priv->location, "notify::clock-format",
		                  G_CALLBACK (clock_location_tile_clock_format_changed), tile);

	g_signal_connect (tile->priv->time, "minute-changed",
	                  G_CALLBACK (clock_location_tile_minute_changed), tile);
}

static GObject *
clock_location_tile_constructor (GType                  gtype,
                                 guint                  n_properties,
                                 GObjectConstructParam *properties)
{
	GObject           *object;
	ClockLocationTile *tile;

	object = G_OBJECT_CLASS (clock_location_tile_parent_class)->constructor (gtype,
	                                                                         n_properties,
	                                                                         properties);
	tile = CLOCK_LOCATION_TILE (object);

	gtk_widget_add_events (GTK_WIDGET (tile),
	                       GDK_BUTTON_PRESS_MASK |
	                       GDK_ENTER_NOTIFY_MASK |
	                       GDK_LEAVE_NOTIFY_MASK);

	g_signal_connect (tile, "button-press-event",
	                  G_CALLBACK (clock_location_tile_button_press), tile);
	g_signal_connect (tile, "enter-notify-event",
	                  G_CALLBACK (clock_location_tile_enter_or_leave), tile);
	g_signal_connect (tile, "leave-notify-event",
	                  G_CALLBACK (clock_location_tile_enter_or_leave), tile);

	clock_location_tile_fill (tile);

	return object;
}

static void
clock_location_tile_finalize (GObject *object)
{
	ClockLocationTile *tile;

	tile = CLOCK_LOCATION_TILE (object);

	if (tile->priv->weather_updated_id > 0) {
		g_signal_handler_disconnect (tile->priv->location,
		                             tile->priv->weather_updated_id);
		tile->priv->weather_updated_id = 0;
	}

	if (tile->priv->clock_format_id > 0) {
		g_signal_handler_disconnect (tile->priv->location,
		                             tile->priv->clock_format_id);
		tile->priv->clock_format_id = 0;
	}

	g_clear_object (&tile->priv->time);
	g_clear_object (&tile->priv->location);

	G_OBJECT_CLASS (clock_location_tile_parent_class)->finalize (object);
}

static void
clock_location_tile_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
	ClockLocationTile *tile;

	tile = CLOCK_LOCATION_TILE (object);

	switch (property_id)
	{
		case PROP_LOCATION:
			clock_location_tile_set_location (tile,
			                                  g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
clock_location_tile_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
	ClockLocationTile *tile;

	tile = CLOCK_LOCATION_TILE (object);

	switch (property_id)
	{
		case PROP_LOCATION:
			g_value_set_object (value, tile->priv->location);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
clock_location_tile_class_init (ClockLocationTileClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->constructor = clock_location_tile_constructor;
	object_class->finalize = clock_location_tile_finalize;
	object_class->set_property = clock_location_tile_set_property;
	object_class->get_property = clock_location_tile_get_property;

	object_signals[TILE_PRESSED] =
		g_signal_new ("tile-pressed",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_FIRST,
		              G_STRUCT_OFFSET (ClockLocationTileClass, tile_pressed),
		              NULL,
		              NULL,
		              NULL,
		              G_TYPE_NONE,
		              1,
		              CLOCK_TYPE_LOCATION);

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
clock_location_tile_init (ClockLocationTile *tile)
{
	tile->priv = clock_location_tile_get_instance_private (tile);
}

GtkWidget *
clock_location_tile_new (ClockLocation *location)
{
	GObject *object;

	object = g_object_new (CLOCK_TYPE_LOCATION_TILE,
	                       "location", location,
	                       NULL);

	return GTK_WIDGET (object);
}
