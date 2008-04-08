#ifndef __CLOCK_ZONEINFO_H__
#define __CLOCK_ZONEINFO_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#ifdef HAVE_SOLARIS
#define SYSTEM_ZONEINFODIR "/usr/share/lib/zoneinfo/tab"
#else
#define SYSTEM_ZONEINFODIR "/usr/share/zoneinfo"
#endif

#define CLOCK_ZONEINFO_TYPE         (clock_zoneinfo_get_type ())
#define CLOCK_ZONEINFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CLOCK_ZONEINFO_TYPE, ClockZoneInfo))
#define CLOCK_ZONEINFO_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), CLOCK_ZONEINFO_TYPE, ClockZoneInfoClass))
#define IS_CLOCK_ZONEINFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CLOCK_ZONEINFO_TYPE))
#define IS_CLOCK_ZONEINFO_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), CLOCK_ZONEINFO_TYPE))
#define CLOCK_ZONEINFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CLOCK_ZONEINFO_TYPE, ClockZoneInfoClass))

typedef struct
{
        GObject g_object;
} ClockZoneInfo;

typedef struct
{
        GObjectClass g_object_class;
} ClockZoneInfoClass;

GType clock_zoneinfo_get_type (void);

ClockZoneInfo *clock_zoneinfo_new (const gchar *zone,
                                   const gchar *country,
				   const gchar *comment,
				   gfloat latitude, gfloat longitude);


const gchar *clock_zoneinfo_get_city (ClockZoneInfo *this);
const gchar *clock_zoneinfo_get_l10n_city (ClockZoneInfo *this);
const gchar *clock_zoneinfo_get_country (ClockZoneInfo *this);
const gchar *clock_zoneinfo_get_name (ClockZoneInfo *this);
const gchar *clock_zoneinfo_get_l10n_name (ClockZoneInfo *this);
void clock_zoneinfo_get_coords (ClockZoneInfo *this,
				gfloat *lat, gfloat *lon);

G_END_DECLS
#endif /* __CLOCK_ZONEINFO_H__ */
