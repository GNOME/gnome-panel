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
#include <libgnome/gnome-i18n.h>

#ifdef HAVE_NETWORK_MANAGER
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <NetworkManager/NetworkManager.h>
#endif

#include "clock-location.h"
#include "clock-marshallers.h"
#include "set-timezone.h"
#include "gweather-xml.h"

G_DEFINE_TYPE (ClockLocation, clock_location, G_TYPE_OBJECT)

typedef struct {
        gchar *name;

        gchar *sys_timezone;
        gchar *timezone;

        gchar *tzname;

        gfloat latitude;
        gfloat longitude;

        gchar *weather_code;
        WeatherInfo *weather_info;
        guint weather_timeout;

	TempUnit temperature_unit;
	SpeedUnit speed_unit;
} ClockLocationPrivate;

enum {
	WEATHER_UPDATED,
	LAST_SIGNAL
};

static guint location_signals[LAST_SIGNAL] = { 0 };

static void clock_location_finalize (GObject *);
static void clock_location_set_tz (ClockLocation *this);
static void clock_location_unset_tz (ClockLocation *this);
static void setup_weather_updates (ClockLocation *loc);
static void add_to_network_monitor (ClockLocation *loc);
static void remove_from_network_monitor (ClockLocation *loc);

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CLOCK_LOCATION_TYPE, ClockLocationPrivate))

ClockLocation *
clock_location_new (const gchar *name, const gchar *timezone,
		    gfloat latitude, gfloat longitude,
		    const gchar *code, WeatherPrefs *prefs)
{
        ClockLocation *this;
        ClockLocationPrivate *priv;

        this = g_object_new (CLOCK_LOCATION_TYPE, NULL);
        priv = PRIVATE (this);

        priv->name = g_strdup (name);
        priv->timezone = g_strdup (timezone);

        priv->sys_timezone = getenv ("TZ");
        if (priv->sys_timezone) {
                priv->sys_timezone = g_strdup (priv->sys_timezone);
        }

        /* initialize priv->tzname */
        clock_location_set_tz (this);
        clock_location_unset_tz (this);

        priv->latitude = latitude;
        priv->longitude = longitude;

        priv->weather_code = g_strdup (code);

	if (prefs) {
		priv->temperature_unit = prefs->temperature_unit;
		priv->speed_unit = prefs->speed_unit;
	}

        setup_weather_updates (this);

        return this;
}

static gboolean
files_are_identical (const char *localtime, struct stat *localtime_s,
                     const char *localtime_data, const gsize localtime_len,
                     char *file, struct stat *file_s)
{
        gsize file_len = -1;
        gchar *file_data = NULL;

        if (localtime_s->st_size != file_s->st_size) {
                return FALSE;
        }

        if (!g_file_get_contents (file, &file_data, &file_len, NULL)) {
                return FALSE;
        }

        if (localtime_len != file_len) {
                g_free (file_data);
                return FALSE;
        }

        if (memcmp (localtime_data, file_data, localtime_len) == 0) {
                g_free (file_data);
                return TRUE;
        }

        g_free (file_data);
        return FALSE;
}

static gchar *
recursive_guess_zone (const char *localtime, struct stat *localtime_s,
                      const char *localtime_data, const gsize localtime_len,
                      char *file, struct stat *file_s, ClockZoneTable *zones)
{
        if (S_ISREG (file_s->st_mode)) {
                gchar *zone = file + strlen (SYSTEM_ZONEINFODIR) + 1;

                /* ignore files that aren't in the Olson database */
                if (!clock_zonetable_get_zone (zones, zone)) {
                        return NULL;
                }

                if (files_are_identical (localtime, localtime_s,
                                         localtime_data, localtime_len,
                                         file, file_s)) {
                        return g_strdup (file + strlen (SYSTEM_ZONEINFODIR) + 1);
                } else {
                        return NULL;
                }
        } else if (S_ISDIR (file_s->st_mode)) {
                GDir *dir = NULL;
                gchar *ret = NULL;

                const gchar *subfile = NULL;
                gchar *subpath = NULL;
                struct stat subpath_s;

                dir = g_dir_open (file, 0, NULL);
                if (dir == NULL) {
                        return NULL;
                }

                while ((subfile = g_dir_read_name (dir)) != NULL) {
                        subpath = g_build_filename (file, subfile, NULL);

                        if (stat (subpath, &subpath_s) == -1) {
                                continue;
                        }

                        ret = recursive_guess_zone (localtime, localtime_s,
                                                    localtime_data, localtime_len,
                                                    subpath, &subpath_s,
                                                    zones);

                        g_free (subpath);

                        if (ret != NULL) {
                                break;
                        }
                }

                g_dir_close (dir);

                return ret;
        }

        return NULL;
}

static gchar *
guess_zone_from_tree (const gchar *localtime, ClockZoneTable *zones)
{
        int i;
        struct stat s;
        struct stat dir_s;
        char *ret = NULL;;

        char *localtime_data = NULL;
        gsize localtime_len = -1;

        /* walk the zoneinfo tree and compare with
           /etc/localtime to try to find the current zone */

        i = stat (localtime, &s);
        if (i == -1 || !S_ISREG (s.st_mode)) {
                return NULL;
        }

        i = stat (SYSTEM_ZONEINFODIR, &dir_s);
        if (i == -1 || !S_ISDIR (dir_s.st_mode)) {
                return NULL;
        }

        if (!g_file_get_contents (localtime, &localtime_data,
                                  &localtime_len, NULL)) {
                return NULL;
        }

        ret = recursive_guess_zone (localtime, &s,
                                    localtime_data, localtime_len,
                                    SYSTEM_ZONEINFODIR, &dir_s, zones);

        g_free (localtime_data);

        return ret;
}

static gchar *current_zone = NULL;
static GFileMonitor *monitor = NULL;

static void
parse_etc_sysconfig_clock (void)
{
	gchar *data;
	gsize len;
	gchar **lines;
	gchar *res;
	gint i;
	gchar *p, *q;

	lines = NULL;
	res = NULL;
	if (g_file_test ("/etc/sysconfig/clock",
			 G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		if (!g_file_get_contents ("/etc/sysconfig/clock",
		    			  &data, &len, NULL))
			goto out;

		lines = g_strsplit (data, "\n", 0);
		g_free (data);

		for (i = 0; lines[i] && !res; i++) {
			/* If you are Fedora, uncomment these and comment out the other version
			   if (g_str_has_prefix (lines[i], "ZONE=")) {
			   p = lines[i] + strlen ("ZONE=");
			*/
			if (g_str_has_prefix (lines[i], "TIMEZONE=")) {
				p = lines[i] + strlen ("TIMEZONE=");
				if (p[0] != '\"')
					goto out;
				p++;
				q = strchr (p, '\"');
				q[0] = '\0';
				res = g_strdup (p);
			}
		}
	}

out:
	if (lines)
		g_strfreev (lines);

	g_free (current_zone);
	current_zone = res;
}

static void
monitor_etc_sysconfig_clock (GFileMonitor *handle,
			     GFile *file,
			     GFile *other_file,
			     GFileMonitorEvent event,
			     gpointer user_data)
{
	parse_etc_sysconfig_clock ();
}

static const gchar *
zone_from_etc_sysconfig_clock (void)
{
	if (monitor == NULL) {
		GFile *file;

		parse_etc_sysconfig_clock ();
		
		file = g_file_new_for_path ("/etc/sysconfig/clock");

		monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE,
					       NULL, NULL);

		g_object_unref (file);

		if (monitor)
			g_signal_connect (G_OBJECT (monitor), "changed", 
					  G_CALLBACK (monitor_etc_sysconfig_clock),
					  NULL);
	}

	return current_zone;
}

static gchar *
clock_location_guess_zone (ClockZoneTable *zones)
{
        const char *localtime = "/etc/localtime";
        gchar *linkfile = NULL;
        GError *err = NULL;
	const gchar *zone;

	/* look for /etc/sysconfig/clock */
	if ((zone = zone_from_etc_sysconfig_clock ())) {
		return g_strdup (zone);
	}

        /* guess the current time zone by readlink() on /etc/localtime */
        linkfile = g_file_read_link (localtime, &err);
        if (err) {
                return guess_zone_from_tree (localtime, zones);
        }

        if (strncmp (linkfile, SYSTEM_ZONEINFODIR,
                     strlen (SYSTEM_ZONEINFODIR)) == 0) {
                return g_strdup (linkfile + strlen (SYSTEM_ZONEINFODIR) + 1);
        }

        return NULL;
}

ClockLocation *
clock_location_new_from_env (ClockZoneTable *zones)
{
        ClockLocation *ret;

        ClockZoneInfo *info;
        ClockCountry *country;

        gchar *name;
        gfloat lat = 0;
        gfloat lon = 0;

        char *zone = getenv ("TZ");

        if (zone == NULL) {
                zone = clock_location_guess_zone (zones);
        }

        if (zone == NULL) {
                /* make a fake location with a null TZ */
                return clock_location_new (_("Unknown Location"), NULL, 0.0, 0.0, NULL, NULL);
        }

        info = clock_zonetable_get_zone (zones, zone);

        if (info == NULL) {
                /* make a fake location with the current TZ */
                return clock_location_new (_("Unknown Location"), zone, 0.0, 0.0, NULL, NULL);
        }

        g_free (zone);

        clock_zoneinfo_get_coords (info, &lat, &lon);

        country = clock_zonetable_get_country (zones, clock_zoneinfo_get_country (info));

        if (country) {
                name = g_strdup_printf (_("%s, %s"),
                                        clock_zoneinfo_get_l10n_city (info),
                                        clock_country_get_l10n_name (country));
        } else {
                name = g_strdup (clock_zoneinfo_get_l10n_city (info));
        }

        ret = clock_location_new (name, clock_zoneinfo_get_name (info),
				  lat, lon, NULL, NULL);

        g_free (name);

        return ret;
}

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
			      _clock_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

        g_type_class_add_private (this_class, sizeof (ClockLocationPrivate));
}

static void
clock_location_init (ClockLocation *this)
{
        ClockLocationPrivate *priv = PRIVATE (this);

        priv->name = NULL;

        priv->sys_timezone = NULL;
        priv->timezone = NULL;

        priv->tzname = NULL;

        priv->latitude = 0;
        priv->longitude = 0;

	priv->temperature_unit = TEMP_UNIT_CENTIGRADE;
	priv->speed_unit = SPEED_UNIT_MS;
}

static void
clock_location_finalize (GObject *g_obj)
{
        ClockLocationPrivate *priv = PRIVATE (g_obj);

	remove_from_network_monitor (CLOCK_LOCATION (g_obj));

        if (priv->name) {
                g_free (priv->name);
                priv->name = NULL;
        }

        if (priv->timezone) {
                g_free (priv->timezone);
                priv->timezone = NULL;
        }

        if (priv->sys_timezone) {
                g_free (priv->sys_timezone);
                priv->sys_timezone = NULL;
        }

        if (priv->tzname) {
                g_free (priv->tzname);
                priv->tzname = NULL;
        }

        if (priv->weather_code) {
                g_free (priv->weather_code);
                priv->weather_code = NULL;
        }

        if (priv->weather_info) {
                weather_info_free (priv->weather_info);
                priv->weather_info = NULL;
        }

        if (priv->weather_timeout) {
                g_source_remove (priv->weather_timeout);
                priv->weather_timeout = 0;
        }

	if (monitor) {
		g_file_monitor_cancel (monitor);
		g_object_unref (monitor);
		monitor = NULL;
	}

        G_OBJECT_CLASS (clock_location_parent_class)->finalize (g_obj);
}

gchar *
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
clock_location_get_timezone (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

        return priv->timezone;
}

void
clock_location_set_timezone (ClockLocation *loc, const gchar *timezone)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

        if (priv->timezone) {
                g_free (priv->timezone);
                priv->timezone = NULL;
        }

        priv->timezone = g_strdup (timezone);
}

gchar *
clock_location_get_tzname (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

        return priv->tzname;
}

void
clock_location_get_coords (ClockLocation *loc, gfloat *latitude,
                               gfloat *longitude)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

        *latitude = priv->latitude;
        *longitude = priv->longitude;
}

void
clock_location_set_coords (ClockLocation *loc, gfloat latitude,
                               gfloat longitude)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

        priv->latitude = latitude;
        priv->longitude = longitude;
}

static void
clock_location_set_tzname (ClockLocation *this, const char *tzname)
{
        ClockLocationPrivate *priv = PRIVATE (this);

        if (priv->tzname) {
                if (strcmp (priv->tzname, tzname) == 0) {
                        return;
                }

                g_free (priv->tzname);
                priv->tzname = NULL;
        }

        if (tzname) {
                priv->tzname = g_strdup (tzname);
        } else {
                priv->tzname = NULL;
        }
}

static void
clock_location_set_tz (ClockLocation *this)
{
        ClockLocationPrivate *priv = PRIVATE (this);

        time_t now_t;
        struct tm now;

        if (priv->timezone == NULL) {
                return;
        }

        setenv ("TZ", priv->timezone, 1);
        tzset();

        now_t = time (NULL);
        localtime_r (&now_t, &now);

        if (daylight && now.tm_isdst) {
                clock_location_set_tzname (this, tzname[1]);
        } else {
                clock_location_set_tzname (this, tzname[0]);
        }
}

static void
clock_location_unset_tz (ClockLocation *this)
{
        ClockLocationPrivate *priv = PRIVATE (this);

        if (priv->timezone == NULL) {
                return;
        }

        if (priv->sys_timezone) {
                setenv ("TZ", priv->sys_timezone, 1);
        } else {
                unsetenv ("TZ");
        }
        tzset();
}

void
clock_location_localtime (ClockLocation *loc, struct tm *tm)
{
        time_t now;

        clock_location_set_tz (loc);

        time (&now);
        localtime_r (&now, tm);

        clock_location_unset_tz (loc);
}

gboolean
clock_location_is_current (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);
	const char *zone;

	if ((zone = zone_from_etc_sysconfig_clock ()))
		return strcmp (zone, priv->timezone) == 0;

	return clock_location_get_offset (loc) == 0;
}


glong
clock_location_get_offset (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);
        glong sys_timezone;
	glong offset;

        unsetenv ("TZ");
        tzset ();
        sys_timezone = timezone;

        setenv ("TZ", priv->timezone, 1);
        tzset();

        offset = timezone - sys_timezone;

        if (priv->sys_timezone) {
                setenv ("TZ", priv->sys_timezone, 1);
        } else {
                unsetenv ("TZ");
        }
        tzset();

        return offset;
}

typedef struct {
	ClockLocation *location;
	GFunc callback;
	gpointer data;
	GDestroyNotify destroy;
} MakeCurrentData;

static void
make_current_cb (gpointer data, GError *error)
{
	MakeCurrentData *mcdata = data;
        ClockLocationPrivate *priv = PRIVATE (mcdata->location);

	if (error == NULL) {
		/* FIXME this ugly shortcut is necessary until we move the
 	  	 * current timezone tracking to clock.c and emit the
 	 	 * signal from there
 	 	 */
		g_free (current_zone);
		current_zone = g_strdup (priv->timezone);
	}

	if (mcdata->callback)
		mcdata->callback (mcdata->data, error);
	else
		g_error_free (error);
}

static void
free_make_current_data (gpointer data)
{
	MakeCurrentData *mcdata = data;
	
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
        gchar *filename;
	MakeCurrentData *mcdata;

	mcdata = g_new (MakeCurrentData, 1);

	mcdata->location = g_object_ref (loc);
	mcdata->callback = callback;
	mcdata->data = data;
	mcdata->destroy = destroy;

        filename = g_build_filename (SYSTEM_ZONEINFODIR, priv->timezone, NULL);
        set_system_timezone_async (filename, 
                                   (GFunc)make_current_cb, 
				   mcdata,
                                   free_make_current_data);
        g_free (filename);
}

const gchar *
clock_location_get_weather_code (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

	return priv->weather_code;
}

void
clock_location_set_weather_code (ClockLocation *loc, const gchar *code)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

	g_free (priv->weather_code);
	priv->weather_code = g_strdup (code);

	setup_weather_updates (loc);
}

WeatherInfo *
clock_location_get_weather_info (ClockLocation *loc)
{
        ClockLocationPrivate *priv = PRIVATE (loc);

	return priv->weather_info;
}

static void
weather_info_updated (WeatherInfo *info, gpointer data)
{
	ClockLocation *loc = data;
	ClockLocationPrivate *priv = PRIVATE (loc);

	g_signal_emit (loc, location_signals[WEATHER_UPDATED],
		       0, priv->weather_info);
}

static gboolean
update_weather_info (gpointer data)
{
	ClockLocation *loc = data;
	ClockLocationPrivate *priv = PRIVATE (loc);
	WeatherPrefs prefs = {
		FORECAST_STATE,
		FALSE,
		NULL,
		TEMP_UNIT_CENTIGRADE,
		SPEED_UNIT_MS,
		PRESSURE_UNIT_MB,
		DISTANCE_UNIT_KM
	};

	prefs.temperature_unit = priv->temperature_unit;
	prefs.speed_unit = priv->speed_unit;

	weather_info_abort (priv->weather_info);
        weather_info_update (priv->weather_info,
                             &prefs, weather_info_updated, loc);

	return TRUE;
}

static gchar *
rad2dms (gfloat lat, gfloat lon)
{
	gchar h, h2;
	gfloat d, deg, min, d2, deg2, min2;

	h = lat > 0 ? 'N' : 'S';
	d = fabs (lat);
	deg = floor (d);
	min = floor (60 * (d - deg));
	h2 = lon > 0 ? 'E' : 'W';
	d2 = fabs (lon);
	deg2 = floor (d2);
	min2 = floor (60 * (d2 - deg2));
	return g_strdup_printf ("%02d-%02d%c %02d-%02d%c",
				(int)deg, (int)min, h,
				(int)deg2, (int)min2, h2);
}

static GList *locations = NULL;

static void
update_weather_infos (void)
{
	GList *l;

	for (l = locations; l; l = l->next) {
		update_weather_info (l->data);
	}
}

#ifdef HAVE_NETWORK_MANAGER
static void
state_notify (DBusPendingCall *pending, gpointer data)
{
	DBusMessage *msg = dbus_pending_call_steal_reply (pending);

	if (!msg)
		return;

	if (dbus_message_get_type (msg) == DBUS_MESSAGE_TYPE_METHOD_RETURN) {
		dbus_uint32_t result;

		if (dbus_message_get_args (msg, NULL, 
					   DBUS_TYPE_UINT32, &result,
					   DBUS_TYPE_INVALID)) {
			if (result == NM_STATE_CONNECTED) {
				update_weather_infos ();
			}
		}
	}

	dbus_message_unref (msg);
}

static void 
check_network (DBusConnection *connection)
{
	DBusMessage *message;
	DBusPendingCall *reply;

	message = dbus_message_new_method_call (NM_DBUS_SERVICE,
						NM_DBUS_PATH,
						NM_DBUS_INTERFACE,
						"state");
	if (dbus_connection_send_with_reply (connection, message, &reply, -1)) {
		dbus_pending_call_set_notify (reply, state_notify, NULL, NULL);
		dbus_pending_call_unref (reply);
	}
	
	dbus_message_unref (message);
}

static DBusHandlerResult
filter_func (DBusConnection *connection,
             DBusMessage    *message,
             void           *user_data)
{
	if (dbus_message_is_signal (message,
				    NM_DBUS_INTERFACE, 
				    "StateChanged")) {
		check_network (connection);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
setup_network_monitor (void)
{
        GError *error;
	DBusError derror;
        static DBusGConnection *bus = NULL;
	DBusConnection *dbus;

        if (bus == NULL) {
                error = NULL;
                bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
                if (bus == NULL) {
                        g_warning ("Couldn't connect to system bus: %s",
                                   error->message);
                        g_error_free (error);

			return;
                }

		dbus_error_init (&derror);
		dbus = dbus_g_connection_get_connection (bus);
                dbus_connection_add_filter (dbus, filter_func, NULL, NULL);
                dbus_bus_add_match (dbus,
                                    "type='signal',"
				    "interface='" NM_DBUS_INTERFACE "'",
                                    &derror);
		if (dbus_error_is_set (&derror)) {
			g_warning ("Couldn't register signal handler: %s: %s",
				   derror.name, derror.message);
			dbus_error_free (&derror);
		}
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
	WeatherLocation *wl;
	WeatherPrefs prefs = {
		FORECAST_STATE,
		FALSE,
		NULL,
		TEMP_UNIT_CENTIGRADE,
		SPEED_UNIT_MS,
		PRESSURE_UNIT_MB,
		DISTANCE_UNIT_KM
	};

	gchar *dms;

	prefs.temperature_unit = priv->temperature_unit;
	prefs.speed_unit = priv->speed_unit;

        if (priv->weather_info) {
                weather_info_free (priv->weather_info);
                priv->weather_info = NULL;
        }

	if (priv->weather_timeout) {
		g_source_remove (priv->weather_timeout);
		priv->weather_timeout = 0;
	}

	if (!priv->weather_code || strcmp (priv->weather_code, "-") == 0)
		return;

	dms = rad2dms (priv->latitude, priv->longitude);
	wl = weather_location_new (priv->name, priv->weather_code,
				   NULL, NULL, dms);

	priv->weather_info =
		weather_info_new (wl, &prefs, weather_info_updated, loc);

	priv->weather_timeout =
		g_timeout_add_seconds (1800, update_weather_info, loc);

	weather_location_free (wl);
	g_free (dms);

	add_to_network_monitor (loc);
}

void
clock_location_set_weather_prefs (ClockLocation *loc,
				  WeatherPrefs *prefs)
{
	ClockLocationPrivate *priv = PRIVATE (loc);

	priv->temperature_unit = prefs->temperature_unit;
	priv->speed_unit = prefs->speed_unit;

	update_weather_info (loc);
}

