#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include <glib.h>
#include <gio/gio.h>

#include "clock-location.h"
#include "set-timezone.h"

struct _ClockLocationPrivate {
        gchar *name;

	GnomeWallClock   *wall_clock;

	GWeatherLocation *world;
	GWeatherLocation *loc;

	GTimeZone        *tz;

        gdouble latitude;
        gdouble longitude;

        GWeatherInfo *weather_info;
	gint          weather_timeout;
	gint          weather_retry_time;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClockLocation, clock_location, G_TYPE_OBJECT)

#define WEATHER_TIMEOUT_BASE 30
#define WEATHER_TIMEOUT_MAX  1800
#define WEATHER_EMPTY_CODE   "-"

enum {
	WEATHER_UPDATED,
	SET_CURRENT,
	LAST_SIGNAL
};

static guint location_signals[LAST_SIGNAL] = { 0 };

static void clock_location_finalize (GObject *);
static gboolean update_weather_info (gpointer user_data);
static void setup_weather_updates (ClockLocation *loc);

static GTimeZone *
get_gweather_timezone (ClockLocation *loc)
{
	GTimeZone *tz;
	GWeatherLocation *gloc;

	gloc = g_object_ref (loc->priv->loc);
	tz = gweather_location_get_timezone (gloc);

	if (tz == NULL) {
		GWeatherLocation *tmp;

		/* Some weather stations do not have timezone information.
		 * In this case, we need to find the nearest city. */
		while (gweather_location_get_level (gloc) >= GWEATHER_LOCATION_CITY) {
			tmp = gloc;

			gloc = gweather_location_get_parent (gloc);

			g_object_unref (tmp);
		}

		tmp = gloc;
		gloc = gweather_location_find_nearest_city (gloc,
		                                            loc->priv->latitude,
		                                            loc->priv->longitude);
		g_object_unref (tmp);

		if (gloc == NULL) {
			g_warning ("Could not find the nearest city for location \"%s\"",
			           gweather_location_get_name (loc->priv->loc));
			return g_time_zone_new_utc ();
		}

		tz = gweather_location_get_timezone (gloc);
		tz = g_time_zone_ref (tz);
		g_object_unref (gloc);
	} else {
		tz = g_time_zone_ref (tz);
		g_object_unref (gloc);
	}

	return tz;
}

ClockLocation *
clock_location_new (GnomeWallClock   *wall_clock,
                    GWeatherLocation *world,
                    const char       *name,
                    const char       *metar_code,
                    gboolean          override_latlon,
                    gdouble           latitude,
                    gdouble           longitude)
{
        ClockLocation *this;
        ClockLocationPrivate *priv;

        this = g_object_new (CLOCK_LOCATION_TYPE, NULL);
        priv = this->priv;

	priv->wall_clock = g_object_ref (wall_clock);
	priv->world = g_object_ref (world);
	priv->loc = gweather_location_find_by_station_code (priv->world,
							    metar_code);

	if (name && *name) {
		priv->name = g_strdup (name);
	} else {
		priv->name = g_strdup (gweather_location_get_name (priv->loc));
	}

	if (override_latlon) {
		priv->latitude = latitude;
		priv->longitude = longitude;
	} else {
		gweather_location_get_coords (priv->loc, &priv->latitude, &priv->longitude);
	}

	priv->tz = get_gweather_timezone (this);

	if (priv->tz == NULL) {
		g_warning ("Failed to get timezone for - %s, falling back to UTC!",
		           priv->name);

		priv->tz = g_time_zone_new_utc ();
	}

        setup_weather_updates (this);

        return this;
}

static ClockLocation *current_location = NULL;

static void
clock_location_class_init (ClockLocationClass *this_class)
{
        GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

        g_obj_class->finalize = clock_location_finalize;

        location_signals[WEATHER_UPDATED] =
		g_signal_new ("weather-updated",
			      G_OBJECT_CLASS_TYPE (g_obj_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ClockLocationClass, weather_updated),
			      NULL, NULL,
			      NULL,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	location_signals[SET_CURRENT] = 
		g_signal_new ("set-current",
			      G_OBJECT_CLASS_TYPE (g_obj_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ClockLocationClass, set_current),
			      NULL, NULL,
			      NULL,
			      G_TYPE_NONE, 0);
}

static void
network_changed (GNetworkMonitor *monitor,
                 gboolean         available,
                 ClockLocation   *loc)
{
        if (available) {
                loc->priv->weather_retry_time = WEATHER_TIMEOUT_BASE;
                update_weather_info (loc);
        }
}

static void
clock_location_init (ClockLocation *this)
{
        ClockLocationPrivate *priv;
        GNetworkMonitor *monitor;

        priv = this->priv = clock_location_get_instance_private (this);

        priv->latitude = 0;
        priv->longitude = 0;

        monitor = g_network_monitor_get_default();
        g_signal_connect (monitor, "network-changed",
                          G_CALLBACK (network_changed), this);
}

static void
clock_location_finalize (GObject *g_obj)
{
        ClockLocation *loc;
        ClockLocationPrivate *priv;
        GNetworkMonitor *monitor;

        loc = CLOCK_LOCATION (g_obj);
        priv = loc->priv;

	monitor = g_network_monitor_get_default ();
	g_signal_handlers_disconnect_by_func (monitor,
	                                      G_CALLBACK (network_changed),
	                                      CLOCK_LOCATION (g_obj));

	g_free (priv->name);

	g_object_unref (priv->wall_clock);

	g_object_unref (priv->world);
	g_object_unref (priv->loc);

	g_time_zone_unref (priv->tz);

	if (priv->weather_timeout)
		g_source_remove (priv->weather_timeout);

        if (priv->weather_info) {
                g_object_unref (priv->weather_info);
                priv->weather_info = NULL;
        }

        G_OBJECT_CLASS (clock_location_parent_class)->finalize (g_obj);
}

const gchar *
clock_location_get_name (ClockLocation *loc)
{
        return loc->priv->name;
}

void
clock_location_set_name (ClockLocation *loc, const gchar *name)
{
        ClockLocationPrivate *priv;

        priv = loc->priv;

        if (priv->name) {
                g_free (priv->name);
                priv->name = NULL;
        }

        priv->name = g_strdup (name);
}

gchar *
clock_location_get_city (ClockLocation *loc)
{
        return gweather_location_get_city_name (loc->priv->loc);
}

GTimeZone *
clock_location_get_timezone (ClockLocation *self)
{
  return self->priv->tz;
}

const char *
clock_location_get_timezone_identifier (ClockLocation *self)
{
  return g_time_zone_get_identifier (self->priv->tz);
}

const char *
clock_location_get_timezone_abbreviation (ClockLocation *self)
{
  GDateTime *dt;
  gint64 now;
  int interval;

  dt = g_date_time_new_now_local ();
  now = g_date_time_to_unix (dt);
  g_date_time_unref (dt);

  interval = g_time_zone_find_interval (self->priv->tz,
                                        G_TIME_TYPE_STANDARD,
                                        now);

  return g_time_zone_get_abbreviation (self->priv->tz, interval);
}

void
clock_location_get_coords (ClockLocation *loc,
			   gdouble *latitude,
			   gdouble *longitude)
{
        *latitude = loc->priv->latitude;
        *longitude = loc->priv->longitude;
}

GDateTime *
clock_location_localtime (ClockLocation *loc)
{
  return g_date_time_new_now (loc->priv->tz);
}

gboolean
clock_location_is_current_timezone (ClockLocation *loc)
{
	GTimeZone *timezone;
	const char *zone;

	timezone = gnome_wall_clock_get_timezone (loc->priv->wall_clock);
	zone = g_time_zone_get_identifier (timezone);

	if (zone)
		return strcmp (zone, g_time_zone_get_identifier (loc->priv->tz)) == 0;
	else
		return clock_location_get_offset (loc) == 0;
}

gboolean
clock_location_is_current (ClockLocation *loc)
{
	if (current_location == loc)
		return TRUE;
	else if (current_location != NULL)
		return FALSE;

	if (clock_location_is_current_timezone (loc)) {
		/* Note that some code in clock.c depends on the fact that
		 * calling this function can set the current location if
		 * there's none */
		current_location = loc;
		g_object_add_weak_pointer (G_OBJECT (current_location), 
					   (gpointer *)&current_location);
		g_signal_emit (current_location, location_signals[SET_CURRENT],
			       0, NULL);

		return TRUE;
	}

	return FALSE;
}

glong
clock_location_get_offset (ClockLocation *loc)
{
  GDateTime *datetime;
  gint64 now;
  GTimeZone *timezone;
  int interval;
  gint32 system_offset;
  gint32 location_offset;

  datetime = g_date_time_new_now_local ();
  now = g_date_time_to_unix (datetime);
  g_date_time_unref (datetime);

  timezone = gnome_wall_clock_get_timezone (loc->priv->wall_clock);
  interval = g_time_zone_find_interval (timezone, G_TIME_TYPE_STANDARD, now);
  system_offset = g_time_zone_get_offset (timezone, interval);

  timezone = loc->priv->tz;
  interval = g_time_zone_find_interval (timezone, G_TIME_TYPE_STANDARD, now);
  location_offset = g_time_zone_get_offset (timezone, interval);

  return system_offset - location_offset;
}

typedef struct {
	ClockLocation *location;
	GFunc callback;
	gpointer data;
	GDestroyNotify destroy;
} MakeCurrentData;

static void
make_current_cb (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	MakeCurrentData *mcdata = user_data;
	GError *error = NULL;

	set_system_timezone_finish (result, &error);

	if (error == NULL) {
		if (current_location)
			g_object_remove_weak_pointer (G_OBJECT (current_location), 
						      (gpointer *)&current_location);
		current_location = mcdata->location;
		g_object_add_weak_pointer (G_OBJECT (current_location), 
					   (gpointer *)&current_location);
		g_signal_emit (current_location, location_signals[SET_CURRENT],
			       0, NULL);
	}

	if (mcdata->callback)
		mcdata->callback (mcdata->data, error);
	else
		g_error_free (error);

	if (mcdata->destroy)
		mcdata->destroy (mcdata->data);
	
	g_object_unref (mcdata->location);
	g_free (mcdata);
}

void
clock_location_make_current (ClockLocation *loc,
                             GFunc          callback,
                             gpointer       data,
                             GDestroyNotify destroy)
{
	MakeCurrentData *mcdata;

        if (loc == current_location) {
                if (destroy)
                        destroy (data);
                return;
        }

	if (clock_location_is_current_timezone (loc)) {
		if (current_location)
			g_object_remove_weak_pointer (G_OBJECT (current_location), 
						      (gpointer *)&current_location);
		current_location = loc;
		g_object_add_weak_pointer (G_OBJECT (current_location), 
					   (gpointer *)&current_location);
		g_signal_emit (current_location, location_signals[SET_CURRENT],
			       0, NULL);
		if (callback)
               		callback (data, NULL);
		if (destroy)
			destroy (data);	
		return;
	}

	mcdata = g_new (MakeCurrentData, 1);

	mcdata->location = g_object_ref (loc);
	mcdata->callback = callback;
	mcdata->data = data;
	mcdata->destroy = destroy;

	set_system_timezone_async (g_time_zone_get_identifier (loc->priv->tz),
	                           make_current_cb,
	                           mcdata);
}

const gchar *
clock_location_get_weather_code (ClockLocation *loc)
{
	return gweather_location_get_code (loc->priv->loc);
}

GWeatherInfo *
clock_location_get_weather_info (ClockLocation *loc)
{
	return loc->priv->weather_info;
}

static void
set_weather_update_timeout (ClockLocation *loc)
{
	ClockLocationPrivate *priv;
	guint timeout;

	priv = loc->priv;

	if (!gweather_info_network_error (priv->weather_info)) {
		/* The last update succeeded; set the next update to
		 * happen in half an hour, and reset the retry timer.
		 */
		timeout = WEATHER_TIMEOUT_MAX;
		priv->weather_retry_time = WEATHER_TIMEOUT_BASE;
	} else {
		/* The last update failed; set the next update
		 * according to the retry timer, and exponentially
		 * back off the retry timer.
		 */
		timeout = priv->weather_retry_time;
		priv->weather_retry_time *= 2;
		if (priv->weather_retry_time > WEATHER_TIMEOUT_MAX)
			priv->weather_retry_time = WEATHER_TIMEOUT_MAX;
	}

	if (priv->weather_timeout)
		g_source_remove (priv->weather_timeout);
	priv->weather_timeout =
		g_timeout_add_seconds (timeout, update_weather_info, loc);
}

static void
weather_info_updated (GWeatherInfo *info, gpointer data)
{
	ClockLocation *loc = data;

	set_weather_update_timeout (loc);
	g_signal_emit (loc, location_signals[WEATHER_UPDATED],
		       0, loc->priv->weather_info);
}

static gboolean
update_weather_info (gpointer user_data)
{
	ClockLocation *loc = user_data;

	gweather_info_abort (loc->priv->weather_info);
	gweather_info_update (loc->priv->weather_info);

	return TRUE;
}

static void
setup_weather_updates (ClockLocation *loc)
{
	ClockLocationPrivate *priv;
	const char *contact_info;
	GWeatherProvider providers;

	priv = loc->priv;

	g_clear_object (&priv->weather_info);

	if (priv->weather_timeout) {
		g_source_remove (priv->weather_timeout);
		priv->weather_timeout = 0;
	}

	priv->weather_info = gweather_info_new (priv->loc);

	gweather_info_set_application_id (priv->weather_info, "org.gnome.gnome-panel");

	contact_info = "https://gitlab.gnome.org/GNOME/gnome-panel/-/raw/master/gnome-panel.doap";
	gweather_info_set_contact_info (priv->weather_info, contact_info);

	providers = GWEATHER_PROVIDER_METAR | GWEATHER_PROVIDER_IWIN;
	gweather_info_set_enabled_providers (priv->weather_info, providers);

	g_signal_connect (priv->weather_info, "updated",
			  G_CALLBACK (weather_info_updated), loc);

	set_weather_update_timeout (loc);

	gweather_info_update (priv->weather_info);
}
