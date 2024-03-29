#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "clock-face.h"
#include "clock-location-tile.h"
#include "clock-location.h"
#include "clock-timedate1-gen.h"
#include "clock-utils.h"

enum {
	TILE_PRESSED,
	NEED_CLOCK_FORMAT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _ClockLocationTilePrivate {
        ClockLocation *location;

        GDateTime *last_refresh;
	long last_offset;

	GtkWidget *box;
        GtkWidget *clock_face;
        GtkWidget *city_label;
        GtkWidget *time_label;

        GtkWidget *current_stack;
        GtkWidget *current_button;

        GtkWidget *weather_icon;

	gulong location_weather_updated_id;

        GCancellable *cancellable;
        ClockTimedate1Gen *timedate1;

        GPermission *permission;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClockLocationTile, clock_location_tile, GTK_TYPE_BIN)

static void clock_location_tile_finalize (GObject *);

static void clock_location_tile_fill (ClockLocationTile *this);
static void update_weather_icon (ClockLocation *loc, GWeatherInfo *info, gpointer data);
static gboolean weather_tooltip (GtkWidget *widget,
                                 gint x, gint y,
		                 gboolean    keyboard_mode,
		                 GtkTooltip *tooltip,
		                 gpointer    data);

ClockLocationTile *
clock_location_tile_new (ClockLocation *loc)
{
        ClockLocationTile *this;
        ClockLocationTilePrivate *priv;

        this = g_object_new (CLOCK_LOCATION_TILE_TYPE, NULL);
        priv = this->priv;

        priv->location = g_object_ref (loc);

        clock_location_tile_fill (this);

	update_weather_icon (loc, clock_location_get_weather_info (loc), this);
	gtk_widget_set_has_tooltip (priv->weather_icon, TRUE);

	g_signal_connect (priv->weather_icon, "query-tooltip",
			  G_CALLBACK (weather_tooltip), this);
	priv->location_weather_updated_id = g_signal_connect (G_OBJECT (loc), "weather-updated",
							      G_CALLBACK (update_weather_icon), this);

        return this;
}

static void
timedate1_cb (GObject      *object,
              GAsyncResult *res,
              gpointer      user_data)
{
  GError *error;
  ClockTimedate1Gen *timedate1;
  ClockLocationTile *self;

  error = NULL;
  timedate1 = clock_timedate1_gen_proxy_new_for_bus_finish (res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  self = CLOCK_LOCATION_TILE (user_data);

  g_clear_object (&self->priv->cancellable);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  self->priv->timedate1 = timedate1;
}

static void
clock_location_tile_dispose (GObject *object)
{
  ClockLocationTile *self;

  self = CLOCK_LOCATION_TILE (object);

  if (self->priv->cancellable != NULL)
    {
      g_cancellable_cancel (self->priv->cancellable);
      g_clear_object (&self->priv->cancellable);
    }

  g_clear_object (&self->priv->timedate1);
  g_clear_object (&self->priv->permission);

  G_OBJECT_CLASS (clock_location_tile_parent_class)->dispose (object);
}

static void
clock_location_tile_class_init (ClockLocationTileClass *this_class)
{
        GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

        g_obj_class->dispose = clock_location_tile_dispose;
        g_obj_class->finalize = clock_location_tile_finalize;

	signals[TILE_PRESSED] = g_signal_new ("tile-pressed",
					      G_TYPE_FROM_CLASS (g_obj_class),
					      G_SIGNAL_RUN_FIRST,
					      G_STRUCT_OFFSET (ClockLocationTileClass, tile_pressed),
					      NULL,
					      NULL,
					      NULL,
					      G_TYPE_NONE, 0);
	signals[NEED_CLOCK_FORMAT] = g_signal_new ("need-clock-format",
						   G_TYPE_FROM_CLASS (g_obj_class),
						   G_SIGNAL_RUN_LAST,
						   G_STRUCT_OFFSET (ClockLocationTileClass, need_clock_format),
						   NULL,
						   NULL,
						   NULL,
						   G_TYPE_INT, 0);
}

static void
clock_location_tile_init (ClockLocationTile *this)
{
        ClockLocationTilePrivate *priv;

        this->priv = clock_location_tile_get_instance_private (this);
        priv = this->priv;

        priv->location = NULL;

	priv->last_refresh = NULL;
	priv->last_offset = 0;

        priv->clock_face = NULL;
        priv->city_label = NULL;
        priv->time_label = NULL;

        priv->cancellable = g_cancellable_new ();
        clock_timedate1_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                               G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                               "org.freedesktop.timedate1",
                                               "/org/freedesktop/timedate1",
                                               priv->cancellable,
                                               timedate1_cb,
                                               this);
}

static void
clock_location_tile_finalize (GObject *g_obj)
{
        ClockLocationTile *tile;
        ClockLocationTilePrivate *priv;

        tile = CLOCK_LOCATION_TILE (g_obj);
        priv = tile->priv;

	if (priv->last_refresh) {
		g_date_time_unref (priv->last_refresh);
		priv->last_refresh = NULL;
	}

        if (priv->location) {
		g_signal_handler_disconnect (priv->location, priv->location_weather_updated_id);
		priv->location_weather_updated_id = 0;

                g_object_unref (priv->location);
                priv->location = NULL;
        }

        G_OBJECT_CLASS (clock_location_tile_parent_class)->finalize (g_obj);
}

static gboolean
press_on_tile      (GtkWidget             *widget,
                    GdkEventButton        *event,
                    ClockLocationTile *tile)
{
	g_signal_emit (tile, signals[TILE_PRESSED], 0);

        return TRUE;
}

static void
set_timezone_cb (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error;
  ClockLocationTile *self;

  error = NULL;
  clock_timedate1_gen_call_set_timezone_finish (CLOCK_TIMEDATE1_GEN (object),
                                                res,
                                                &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  self = CLOCK_LOCATION_TILE (user_data);

  if (error != NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (NULL,
                                       0,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Failed to set the system timezone"));

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s",
                                                error->message);

      g_signal_connect (dialog,
                        "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);

      gtk_window_present (GTK_WINDOW (dialog));
      g_error_free (error);
      return;
    }

  clock_location_set_current (self->priv->location, TRUE);
}

static void
make_current (GtkWidget         *widget,
              ClockLocationTile *self)
{
  if (clock_location_is_current_timezone (self->priv->location))
    {
      clock_location_set_current (self->priv->location, TRUE);
      return;
    }

  if (self->priv->cancellable != NULL)
    {
      g_cancellable_cancel (self->priv->cancellable);
      g_object_unref (self->priv->cancellable);
    }

  self->priv->cancellable = g_cancellable_new ();

  clock_timedate1_gen_call_set_timezone (self->priv->timedate1,
                                         clock_location_get_timezone_identifier (self->priv->location),
                                         TRUE,
                                         self->priv->cancellable,
                                         set_timezone_cb,
                                         self);
}

static gboolean
enter_or_leave_tile (GtkWidget             *widget,
                     GdkEventCrossing      *event,
                     ClockLocationTile *tile)
{
	ClockLocationTilePrivate *priv;

	priv = tile->priv;

	if (event->mode != GDK_CROSSING_NORMAL) {
		return TRUE;
	}

	if (clock_location_is_current (priv->location)) {
		gtk_stack_set_visible_child_name (GTK_STACK (priv->current_stack), "marker");

		return TRUE;
	}

	if (event->type == GDK_ENTER_NOTIFY) {
		gboolean allowed;
		gboolean can_acquire;

		allowed = FALSE;
		can_acquire = FALSE;

		if (priv->timedate1 != NULL &&
		    priv->permission != NULL) {
			if (g_permission_get_allowed (priv->permission))
				allowed = TRUE;
			if (g_permission_get_can_acquire (priv->permission))
				can_acquire = TRUE;
		}

		if (clock_location_is_current_timezone (priv->location))
			allowed = TRUE;

		if (allowed || can_acquire) {
			const char *tooltip;

			if (allowed) {
				if (!clock_location_is_current_timezone (priv->location))
					tooltip = _("Set location as current location and use its timezone for this computer");
				else
					tooltip = _("Set location as current location");
			} else {
				tooltip = _("Click “Unlock” to set location as current location and use its timezone for this computer");
			}

			gtk_widget_set_sensitive (priv->current_button, allowed);
			gtk_widget_set_tooltip_text (priv->current_button, tooltip);
			gtk_stack_set_visible_child_name (GTK_STACK (priv->current_stack), "button");
		}
		else {
			gtk_stack_set_visible_child_name (GTK_STACK (priv->current_stack), "spacer");
		}
	}
	else {
		if (event->detail != GDK_NOTIFY_INFERIOR) {
			gtk_stack_set_visible_child_name (GTK_STACK (priv->current_stack), "spacer");
		}
	}

	return TRUE;
}

static void
clock_location_tile_fill (ClockLocationTile *this)
{
        ClockLocationTilePrivate *priv;
        GtkWidget *box;
        GtkWidget *tile;
        GtkWidget *head_section;
        GtkStyleContext *context;
        GtkWidget *child;
        GtkWidget *label;

        priv = this->priv;
        priv->box = gtk_event_box_new ();

        gtk_widget_add_events (priv->box, GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect (priv->box, "button-press-event",
                          G_CALLBACK (press_on_tile), this);
        g_signal_connect (priv->box, "enter-notify-event",
                          G_CALLBACK (enter_or_leave_tile), this);
        g_signal_connect (priv->box, "leave-notify-event",
                          G_CALLBACK (enter_or_leave_tile), this);

        tile = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_margin_top (tile, 3);
        gtk_widget_set_margin_bottom (tile, 3);
        gtk_widget_set_margin_start (tile, 3);

        head_section = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

        priv->city_label = gtk_label_new (NULL);
        gtk_widget_set_margin_end (priv->city_label, 3);
        gtk_label_set_xalign (GTK_LABEL (priv->city_label), 0);

        gtk_box_pack_start (GTK_BOX (head_section), priv->city_label, FALSE, FALSE, 0);

        priv->time_label = gtk_label_new (NULL);
        gtk_widget_set_margin_end (priv->time_label, 3);
        gtk_label_set_yalign (GTK_LABEL (priv->time_label), 0);

        priv->weather_icon = gtk_image_new ();
        gtk_widget_set_valign (priv->weather_icon, GTK_ALIGN_START);

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start (GTK_BOX (head_section), box, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), priv->weather_icon, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), priv->time_label, FALSE, FALSE, 0);

        priv->current_stack = gtk_stack_new ();
        gtk_box_pack_end (GTK_BOX (box), priv->current_stack, FALSE, FALSE, 0);
        gtk_widget_show (priv->current_stack);

        priv->current_button = gtk_button_new ();
        context = gtk_widget_get_style_context (priv->current_button);
        gtk_style_context_add_class (context, "calendar-window-button");
        gtk_stack_add_named (GTK_STACK (priv->current_stack), priv->current_button, "button");
        gtk_widget_set_halign (priv->current_button, GTK_ALIGN_END);
        gtk_widget_show (priv->current_button);

        label = gtk_label_new (_("Set"));
        gtk_container_add (GTK_CONTAINER (priv->current_button), label);
        gtk_widget_show (label);

        child = gtk_image_new_from_icon_name ("go-home", GTK_ICON_SIZE_BUTTON);
        gtk_stack_add_named (GTK_STACK (priv->current_stack), child, "marker");
        gtk_widget_set_halign (child, GTK_ALIGN_END);
        gtk_widget_show (child);

        child = gtk_event_box_new ();
        gtk_stack_add_named (GTK_STACK (priv->current_stack), child, "spacer");
        gtk_widget_show (child);

        if (clock_location_is_current (priv->location))
                gtk_stack_set_visible_child_name (GTK_STACK (priv->current_stack), "marker");
        else
                gtk_stack_set_visible_child_name (GTK_STACK (priv->current_stack), "spacer");

        g_signal_connect (priv->current_button, "clicked",
                          G_CALLBACK (make_current), this);

        priv->clock_face = clock_face_new_with_location (priv->location);

        gtk_box_pack_start (GTK_BOX (tile), priv->clock_face, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (tile), head_section, TRUE, TRUE, 0);

        gtk_container_add (GTK_CONTAINER (priv->box), tile);
        gtk_container_add (GTK_CONTAINER (this), priv->box);
}

static gboolean
clock_needs_face_refresh (ClockLocationTile *this)
{
        ClockLocationTilePrivate *priv;
        GDateTime *now;
	gboolean retval;

	priv = this->priv;

	if (!priv->last_refresh)
		return TRUE;

        now = clock_location_localtime (priv->location);

	retval = FALSE;
        if (g_date_time_get_year (now) > g_date_time_get_year (priv->last_refresh)
            || g_date_time_get_month (now) > g_date_time_get_month (priv->last_refresh)
            || g_date_time_get_day_of_month (now) > g_date_time_get_day_of_month (priv->last_refresh)
            || g_date_time_get_hour (now) > g_date_time_get_hour (priv->last_refresh)
            || g_date_time_get_minute (now) > g_date_time_get_minute (priv->last_refresh)) {
		retval = TRUE;
        }

	g_date_time_unref (now);
	return retval;
}

static gboolean
clock_needs_label_refresh (ClockLocationTile *this)
{
        ClockLocationTilePrivate *priv;
	GDateTime *now;
	long offset;
	gboolean retval;

	priv = this->priv;

	if (!priv->last_refresh)
		return TRUE;

        now = clock_location_localtime (priv->location);
	offset = clock_location_get_offset (priv->location);

	retval = FALSE;
        if (g_date_time_get_year (now) > g_date_time_get_year (priv->last_refresh)
            || g_date_time_get_month (now) > g_date_time_get_month (priv->last_refresh)
            || g_date_time_get_day_of_month (now) > g_date_time_get_day_of_month (priv->last_refresh)
            || g_date_time_get_hour (now) > g_date_time_get_hour (priv->last_refresh)
            || g_date_time_get_minute (now) > g_date_time_get_minute (priv->last_refresh)
	    || offset != priv->last_offset) {
		retval = TRUE;
        }

	g_date_time_unref (now);
        return retval;
}

static char *
format_time (GDateTime   *now, 
             const char  *tzname,
             GDesktopClockFormat  clock_format,
	     long         offset)
{
	const char *format;
	GDateTime *local_now;
	char *buf;	
	char *tmp;	
	long hours, minutes;

	local_now = g_date_time_new_now_local ();

	if (g_date_time_get_day_of_week (local_now) !=
	    g_date_time_get_day_of_week (now)) {
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
	}
	else {
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

        g_date_time_unref (local_now);

        buf = g_date_time_format (now, format);
        hours = offset / 3600;
        minutes = labs (offset % 3600) / 60;

	if (hours != 0 && minutes != 0) {
		tmp = g_strdup_printf ("%s <small>%s %+ld:%ld</small>", buf, tzname, hours, minutes);
	}
	else if (hours != 0) {
		tmp = g_strdup_printf ("%s <small>%s %+ld</small>", buf, tzname, hours);
	}
	else {
		tmp = g_strdup_printf ("%s <small>%s</small>", buf, tzname);
	}

	g_free (buf);
	return tmp;
}

static char *
convert_time_to_str (time_t               now,
                     GDesktopClockFormat  clock_format,
                     GTimeZone           *timezone)
{
	const gchar *format;
	GDateTime *utc, *local;
	char *ret;

	if (clock_format == G_DESKTOP_CLOCK_FORMAT_12H) {
                /* Translators: This is a strftime format string.
                 * It is used to display the time in 12-hours format (eg, like
                 * in the US: 8:10 am). The %p expands to am/pm.
                 */
		format = _("%l:%M %p");
	}
	else {
                /* Translators: This is a strftime format string.
                 * It is used to display the time in 24-hours format (eg, like
                 * in France: 20:10).
                 */
		format = _("%H:%M");
	}

	utc = g_date_time_new_from_unix_utc (now);
	local = g_date_time_to_timezone (utc, timezone);

	ret = g_date_time_format (local, format);

	g_date_time_unref (utc);
	g_date_time_unref (local);

	return ret;
}

void
clock_location_tile_refresh (ClockLocationTile *this, gboolean force_refresh)
{
        ClockLocationTilePrivate *priv;
        GtkStack *stack;
        gchar *tmp;
	const char *tzname;
	GDateTime *now;
	long offset;
	int format;

	g_return_if_fail (IS_CLOCK_LOCATION_TILE (this));

	priv = this->priv;
	stack = GTK_STACK (priv->current_stack);

        if (clock_location_is_current (priv->location)) {
		gtk_stack_set_visible_child_name (stack, "marker");
	}
	else {
		if (g_strcmp0 (gtk_stack_get_visible_child_name (stack), "marker") == 0)
			gtk_stack_set_visible_child_name (stack, "spacer");
	}

        if (clock_needs_face_refresh (this)) {
                clock_face_refresh (CLOCK_FACE (priv->clock_face));
        }

        if (!force_refresh && !clock_needs_label_refresh (this)) {
                return;
        }

        now = clock_location_localtime (priv->location);
        tzname = clock_location_get_timezone_abbreviation (priv->location);

	if (priv->last_refresh)
		g_date_time_unref (priv->last_refresh);
	priv->last_refresh = g_date_time_ref (now);
	priv->last_offset = clock_location_get_offset (priv->location);

        tmp = g_strdup_printf ("<big><b>%s</b></big>",
                               clock_location_get_name (priv->location));
        gtk_label_set_markup (GTK_LABEL (priv->city_label), tmp);
        g_free (tmp);

	g_signal_emit (this, signals[NEED_CLOCK_FORMAT], 0, &format);

	offset = - priv->last_offset;

	tmp = format_time (now, tzname, format, offset);

        gtk_label_set_markup (GTK_LABEL (priv->time_label), tmp);

        g_free (tmp);
}

void
weather_info_setup_tooltip (GWeatherInfo *info, ClockLocation *location, GtkTooltip *tooltip,
			    GDesktopClockFormat clock_format)
{
        GdkPixbuf *pixbuf = NULL;
        GtkIconTheme *theme = NULL;
	gchar *conditions, *sky, *wind;
	gchar *temp, *apparent;
	gchar *line1, *line2, *line3, *line4, *tip;
	const gchar *icon_name;
	time_t sunrise_time, sunset_time;
	gchar *sunrise_str, *sunset_str;
	GTimeZone *timezone;
	gdouble unused;
	GWeatherWindDirection unused2;

       	icon_name = gweather_info_get_icon_name (info);
        theme = gtk_icon_theme_get_default ();
        pixbuf = gtk_icon_theme_load_icon (theme, icon_name, 48,
                                           GTK_ICON_LOOKUP_GENERIC_FALLBACK, NULL);
        if (pixbuf)
                gtk_tooltip_set_icon (tooltip, pixbuf);

	conditions = gweather_info_get_conditions (info);
	sky = gweather_info_get_sky (info);
	if (strcmp (conditions, "-") != 0) {
		line1 = g_strdup_printf (_("%s, %s"),
					 conditions, sky);
		g_free (sky);
	} else {
		line1 = sky;
	}
	g_free (conditions);

	temp = gweather_info_get_temp (info);
	apparent = gweather_info_get_apparent (info);
	if (strcmp (apparent, temp) != 0 &&
	    gweather_info_get_value_apparent (info, GWEATHER_TEMP_UNIT_DEFAULT, &unused))
		/* Translators: The two strings are temperatures. */
		line2 = g_strdup_printf (_("%s, feels like %s"), temp, apparent);
	else
		line2 = g_strdup (temp);
	g_free (temp);
	g_free (apparent);

	wind = gweather_info_get_wind (info);
        if (gweather_info_get_value_wind (info, GWEATHER_SPEED_UNIT_DEFAULT, &unused, &unused2))
		line3 = g_strdup_printf ("%s\n", wind);
	else
		line3 = g_strdup ("");

	timezone = clock_location_get_timezone (location);
	if (gweather_info_get_value_sunrise (info, &sunrise_time))
		sunrise_str = convert_time_to_str (sunrise_time, clock_format, timezone);
	else
		sunrise_str = g_strdup ("???");
	if (gweather_info_get_value_sunset (info, &sunset_time))
		sunset_str = convert_time_to_str (sunset_time, clock_format, timezone);
	else
		sunset_str = g_strdup ("???");
	line4 = g_strdup_printf (_("Sunrise: %s / Sunset: %s"),
				 sunrise_str, sunset_str);
	g_free (sunrise_str);
	g_free (sunset_str);

	tip = g_strdup_printf ("<b>%s</b>\n%s\n%s%s", line1, line2, line3, line4);
	gtk_tooltip_set_markup (tooltip, tip);
	g_free (line1);
	g_free (line2);
	g_free (line3);
	g_free (line4);
	g_free (tip);
}

static gboolean
weather_tooltip (GtkWidget  *widget,
		 gint        x,
		 gint	     y,
		 gboolean    keyboard_mode,
		 GtkTooltip *tooltip,
		 gpointer    data)
{
        ClockLocationTile *tile = data;
        ClockLocationTilePrivate *priv = tile->priv;
	GWeatherInfo *info;
	int clock_format;

	info = clock_location_get_weather_info (priv->location);

	if (!info || !gweather_info_is_valid (info))
		return FALSE;

	g_signal_emit (tile, signals[NEED_CLOCK_FORMAT], 0, &clock_format);

	weather_info_setup_tooltip (info, priv->location, tooltip, clock_format);

	return TRUE;
}

static void
update_weather_icon (ClockLocation *loc, GWeatherInfo *info, gpointer data)
{
        ClockLocationTile *tile = data;
        ClockLocationTilePrivate *priv = tile->priv;
        GdkPixbuf *pixbuf = NULL;
        GtkIconTheme *theme = NULL;
        const gchar *icon_name;

        if (!info || !gweather_info_is_valid (info))
                return;

        icon_name = gweather_info_get_icon_name (info);
        theme = gtk_icon_theme_get_default ();
        pixbuf = gtk_icon_theme_load_icon (theme, icon_name, 16,
                                           GTK_ICON_LOOKUP_GENERIC_FALLBACK, NULL);

        if (pixbuf) {
                gtk_image_set_from_pixbuf (GTK_IMAGE (priv->weather_icon), pixbuf);
                gtk_widget_set_margin_end (priv->weather_icon, 6);
        }
}

ClockLocation *
clock_location_tile_get_location (ClockLocationTile *this)
{
	g_return_val_if_fail (IS_CLOCK_LOCATION_TILE (this), NULL);

	return g_object_ref (this->priv->location);
}

void
clock_location_tile_set_permission (ClockLocationTile *self,
                                    GPermission       *permission)
{
  g_clear_object (&self->priv->permission);
  self->priv->permission = g_object_ref (permission);
}
