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

#ifdef HAVE_NETWORK_MANAGER
#include <NetworkManager/NetworkManager.h>
#endif

#include "clock-location.h"
#include "clock-marshallers.h"
#include "set-timezone.h"
#include "system-timezone.h"

G_DEFINE_TYPE (ClockLocation, clock_location, G_TYPE_OBJECT)

typedef struct {
        gchar *name;

	GWeatherLocation *world;
	GWeatherLocation *loc;

        SystemTimezone *systz;

        gdouble latitude;
        gdouble longitude;

        GWeatherInfo *weather_info;
	gint          weather_timeout;
	gint          weather_retry_time;
} ClockLocationPrivate;

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
static void setup_weather_updates (ClockLocation *loc);
static void add_to_network_monitor (ClockLocation *loc);
static void remove_from_network_monitor (ClockLocation *loc);

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CLOCK_LOCATION_TYPE, ClockLocationPrivate))

ClockLocation *
clock_location_new (GWeatherLocation *world,
		    const char       *name,
		    const char       *metar_code,
		    gboolean          override_latlon,
		    gdouble           latitude,
		    gdouble           longitude)
{
        ClockLocation *this;
        ClockLocationPrivate *priv;

        this = g_object_new (CLOCK_LOCATION_TYPE, NULL);
        priv = PRIVATE (this);

	priv->world = gweather_location_ref (world);
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
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	location_signals[SET_CURRENT] = 
		g_signal_new ("set-current",
			      G_OBJECT_CLASS_TYPE (g_obj_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ClockLocationClass, set_current),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

        g_type_class_add_private (this_class, sizeof (ClockLocationPrivate));
}

static void
clock_location_init (ClockLocation *this)
{
        ClockLocationPrivate *priv = PRIVATE (this);

        priv->systz = system_timezone_new ();

        priv->latitude = 0;
        priv->longitude = 0;
}

static void
clock_location_finalize (GObject *g_obj)
{
        ClockLocationPrivate *priv = PRIVATE (g_obj);

	remove_from_network_monitor (CLOCK_LOCATION (g_obj));

	g_free (priv->name);

	gweather_location_unref (priv->world);
	gweather_location_unref (priv->loc);

	if (priv->weather_timeout)
		g_source_remove (priv->weather_timeout);

        if (priv->systz) {
                g_object_unref (priv->systz);
                priv->systz = NULL;
        }

        if (priv->weather_info) {
                g_object_unref (priv->weather_info);
                priv->weather_info = NULL;
        }

        G_OBJECT_CLASS (clock_location_parent_class)->finalize (g_obj);
}

const gchar *
clock_location_get_name (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

        return priv->name;
}

void
clock_location_set_name (ClockLocation *loc, const gchar *name)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

        if (priv->name) {
                g_free (priv->name);
                priv->name = NULL;
        }

        priv->name = g_strdup (name);
}

gchar *
clock_location_get_city (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

        return gweather_location_get_city_name (priv->loc);
}

const gchar *
clock_location_get_timezone (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);
	GWeatherTimezone *tz;

	tz = gweather_location_get_timezone (priv->loc);
        return gweather_timezone_get_name (tz);
}

const gchar *
clock_location_get_tzname (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);
	GWeatherTimezone *tz;

	tz = gweather_location_get_timezone (priv->loc);
        return gweather_timezone_get_tzid (tz);
}

void
clock_location_get_coords (ClockLocation *loc,
			   gdouble *latitude,
			   gdouble *longitude)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

        *latitude = priv->latitude;
        *longitude = priv->longitude;
}

GDateTime *
clock_location_localtime (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);
	GWeatherTimezone *wtz;
	GTimeZone *tz;
	GDateTime *dt;

	wtz = gweather_location_get_timezone (priv->loc);

	tz = g_time_zone_new (gweather_timezone_get_tzid (wtz));
	dt = g_date_time_new_now (tz);

	g_time_zone_unref (tz);
	return dt;
}

gboolean
clock_location_is_current_timezone (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);
	GWeatherTimezone *wtz;
	const char *zone;

	wtz = gweather_location_get_timezone (priv->loc);

	zone = system_timezone_get (priv->systz);

	if (zone)
		return strcmp (zone, gweather_timezone_get_tzid (wtz)) == 0;
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
        ClockLocationPrivate *priv = PRIVATE (loc);
	GWeatherTimezone *wtz;

	wtz = gweather_location_get_timezone (priv->loc);
	return gweather_timezone_get_offset (wtz);
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
        ClockLocationPrivate *priv = PRIVATE (loc);
	MakeCurrentData *mcdata;
	GWeatherTimezone *wtz;

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

	wtz = gweather_location_get_timezone (priv->loc);
        set_system_timezone_async (gweather_timezone_get_tzid (wtz),
                                   make_current_cb,
                                   mcdata);
}

const gchar *
clock_location_get_weather_code (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

	return gweather_location_get_code (priv->loc);
}

GWeatherInfo *
clock_location_get_weather_info (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

	return priv->weather_info;
}

static gboolean update_weather_info (gpointer data);

static void
set_weather_update_timeout (ClockLocation *loc)
{
	ClockLocationPrivate *priv = PRIVATE (loc);
	guint timeout;

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
	ClockLocationPrivate *priv = PRIVATE (loc);

	set_weather_update_timeout (loc);
	g_signal_emit (loc, location_signals[WEATHER_UPDATED],
		       0, priv->weather_info);
}

static gboolean
update_weather_info (gpointer data)
{
	ClockLocation *loc = data;
	ClockLocationPrivate *priv = PRIVATE (loc);

	gweather_info_abort (priv->weather_info);
        gweather_info_update (priv->weather_info);

	return TRUE;
}

static GList *locations = NULL;

#ifdef HAVE_NETWORK_MANAGER
static void
update_weather_infos (void)
{
	GList *l;

	for (l = locations; l; l = l->next) {
		ClockLocation *loc = l->data;
		ClockLocationPrivate *priv = PRIVATE (loc);

		priv->weather_retry_time = WEATHER_TIMEOUT_BASE;
		update_weather_info (loc);
	}
}

static void
network_monitor_signal (GDBusProxy  *proxy,
			const gchar *sender_name,
			const gchar *signal_name,
			GVariant    *parameters,
			gpointer     user_data)
{
	if (g_str_equal (signal_name, "StateChanged")) {
		if (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(u)"))) {
			guint32 state;

			g_variant_get (parameters, "(u)", &state);

			if (state == NM_STATE_CONNECTED) {
				update_weather_infos ();
			}
		}
	}
}

static void
setup_network_monitor (void)
{
	static GDBusProxy *proxy;
	GError *error = NULL;

        if (proxy == NULL) {
		proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
						       NULL, NM_DBUS_SERVICE, NM_DBUS_PATH, NM_DBUS_INTERFACE,
						       NULL, &error);

		if (proxy == NULL) {
			g_warning ("Couldn't create NetworkManager proxy: %s",
				   error->message);
			g_error_free (error);
			return;
		}

		g_signal_connect (proxy, "g-signal", G_CALLBACK (network_monitor_signal), NULL);
	}
}
#endif

static void
add_to_network_monitor (ClockLocation *loc)
{
#ifdef HAVE_NETWORK_MANAGER
	setup_network_monitor ();
#endif

	if (!g_list_find (locations, loc))
		locations = g_list_prepend (locations, loc);
}

static void
remove_from_network_monitor (ClockLocation *loc)
{
	locations = g_list_remove (locations, loc);
}

static void
setup_weather_updates (ClockLocation *loc)
{
	ClockLocationPrivate *priv = PRIVATE (loc);

	g_clear_object (&priv->weather_info);

	if (priv->weather_timeout) {
		g_source_remove (priv->weather_timeout);
		priv->weather_timeout = 0;
	}

	priv->weather_info = gweather_info_new (priv->loc, GWEATHER_FORECAST_LIST);

	g_signal_connect (priv->weather_info, "updated",
			  G_CALLBACK (weather_info_updated), loc);

	set_weather_update_timeout (loc);

	add_to_network_monitor (loc);
}

