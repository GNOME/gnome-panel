#ifndef __CLOCK_LOCATION_H__
#define __CLOCK_LOCATION_H__

#include <time.h>
#include <glib.h>
#include <glib-object.h>
#include <libgweather/gweather.h>
#include <libgnome-desktop/gnome-wall-clock.h>

G_BEGIN_DECLS

#define CLOCK_LOCATION_TYPE         (clock_location_get_type ())
#define CLOCK_LOCATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CLOCK_LOCATION_TYPE, ClockLocation))
#define CLOCK_LOCATION_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), CLOCK_LOCATION_TYPE, ClockLocationClass))
#define IS_CLOCK_LOCATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CLOCK_LOCATION_TYPE))
#define IS_CLOCK_LOCATION_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), CLOCK_LOCATION_TYPE))
#define CLOCK_LOCATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CLOCK_LOCATION_TYPE, ClockLocationClass))

typedef struct _ClockLocationPrivate ClockLocationPrivate;

typedef struct
{
        GObject g_object;

        ClockLocationPrivate *priv;
} ClockLocation;

typedef struct
{
        GObjectClass g_object_class;

	void (* weather_updated) (ClockLocation *location, GWeatherInfo *info);

	void (* set_current) (ClockLocation *location);
} ClockLocationClass;

GType clock_location_get_type (void);

ClockLocation *clock_location_new (GnomeWallClock   *wall_clock,
                                   GWeatherLocation *world,
                                   const gchar      *name,
                                   const gchar      *metar_code,
                                   gboolean          override_latlon,
                                   gdouble           latitude,
                                   gdouble           longitude);

GTimeZone  *clock_location_get_timezone (ClockLocation *self);
const char *clock_location_get_timezone_identifier (ClockLocation *self);
const char *clock_location_get_timezone_abbreviation (ClockLocation *self);

const char *clock_location_get_name (ClockLocation *loc);
void clock_location_set_name (ClockLocation *loc, const gchar *name);

gchar *clock_location_get_city (ClockLocation *loc);
void clock_location_get_coords (ClockLocation *loc, gdouble *latitude, gdouble *longitude);

GDateTime *clock_location_localtime (ClockLocation *loc);

gboolean clock_location_is_current (ClockLocation *loc);
void clock_location_make_current (ClockLocation *loc,
				  GFunc          callback,
				  gpointer       data,
				  GDestroyNotify destroy);
gboolean clock_location_is_current_timezone (ClockLocation *loc);

const gchar *clock_location_get_weather_code (ClockLocation *loc);
GWeatherInfo *clock_location_get_weather_info (ClockLocation *loc);

glong clock_location_get_offset (ClockLocation *loc);

G_END_DECLS
#endif /* __CLOCK_LOCATION_H__ */
