#ifndef __CLOCK_COUNTRY_H__
#define __CLOCK_COUNTRY_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CLOCK_COUNTRY_TYPE         (clock_country_get_type ())
#define CLOCK_COUNTRY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CLOCK_COUNTRY_TYPE, ClockCountry))
#define CLOCK_COUNTRY_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), CLOCK_COUNTRY_TYPE, ClockCountryClass))
#define IS_CLOCK_COUNTRY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CLOCK_COUNTRY_TYPE))
#define IS_CLOCK_COUNTRY_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), CLOCK_COUNTRY_TYPE))
#define CLOCK_COUNTRY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CLOCK_COUNTRY_TYPE, ClockCountryClass))

typedef struct
{
        GObject g_object;
} ClockCountry;

typedef struct
{
        GObjectClass g_object_class;
} ClockCountryClass;

GType clock_country_get_type (void);

ClockCountry *clock_country_new (gchar *code, gchar *name);

gchar *clock_country_get_code (ClockCountry *this);
gchar *clock_country_get_name (ClockCountry *this);
gchar *clock_country_get_l10n_name (ClockCountry *this);

G_END_DECLS
#endif /* __CLOCK_COUNTRY_H__ */
