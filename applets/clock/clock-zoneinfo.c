#include "clock-zoneinfo.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <stdio.h>
#include <string.h>

G_DEFINE_TYPE (ClockZoneInfo, clock_zoneinfo, G_TYPE_OBJECT)

typedef struct {
        gchar *name;
        gchar *l10n_name;
        gchar *country;
#ifdef MEMORY_DOESNT_MATTER
        gchar *city;
#endif
        gchar *l10n_city;
#ifdef MEMORY_DOESNT_MATTER
        gchar *comment;
#endif

        gfloat latitude;
        gfloat longitude;
} ClockZoneInfoPrivate;

static void clock_zoneinfo_finalize (GObject *);

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CLOCK_ZONEINFO_TYPE, ClockZoneInfoPrivate))

ClockZoneInfo *
clock_zoneinfo_new (const gchar *zone,
                    const gchar *country,
                    const gchar *comment,
		    gfloat latitude, gfloat longitude)
{
        ClockZoneInfo *this;
        ClockZoneInfoPrivate *priv;
        int i;
#ifdef MEMORY_DOESNT_MATTER
        const gchar *city;
#endif
        gchar *l10n_city;

        this = g_object_new (CLOCK_ZONEINFO_TYPE, NULL);
        priv = PRIVATE (this);

        priv->name = g_strdup (zone);
        priv->country = g_strdup (country);

#ifdef MEMORY_DOESNT_MATTER
        city = g_strrstr (zone, "/");
        if (city == NULL)
                priv->city = g_strdup (zone);
        else
                priv->city = g_strdup (city + 1);

        /* replace underscores in the city with spaces */
        for (i = 0; priv->city[i] != '\0'; i++) {
                if (priv->city[i] == '_') {
                        priv->city[i] = ' ';
                }
        }
#endif

        priv->l10n_name = g_strdup (dgettext (EVOLUTION_TEXTDOMAIN,
                                              priv->name));

        /* replace underscores in the localized zone name with spaces */
        for (i = 0; priv->l10n_name[i] != '\0'; i++) {
                if (priv->l10n_name[i] == '_') {
                        priv->l10n_name[i] = ' ';
                }
        }

        l10n_city = g_strrstr (priv->l10n_name, "/");
        if (l10n_city == NULL)
                priv->l10n_city = g_strdup (priv->l10n_name);
        else
                priv->l10n_city = g_strdup (l10n_city + 1);

#ifdef MEMORY_DOESNT_MATTER
        priv->comment = g_strdup (comment);
#endif

        priv->latitude = latitude;
        priv->longitude = longitude;

        return this;
}

static void
clock_zoneinfo_class_init (ClockZoneInfoClass *this_class)
{
        GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

        g_obj_class->finalize = clock_zoneinfo_finalize;

        g_type_class_add_private (this_class, sizeof (ClockZoneInfoPrivate));
}

static void
clock_zoneinfo_init (ClockZoneInfo *this)
{
        ClockZoneInfoPrivate *priv = PRIVATE (this);

        priv->name = NULL;
        priv->l10n_name = NULL;
        priv->country = NULL;
#ifdef MEMORY_DOESNT_MATTER
        priv->city = NULL;
#endif
        priv->l10n_city = NULL;
#ifdef MEMORY_DOESNT_MATTER
        priv->comment = NULL;
#endif

        priv->latitude = 0.0;
        priv->longitude = 0.0;
}

static void
clock_zoneinfo_finalize (GObject *g_obj)
{
        ClockZoneInfoPrivate *priv = PRIVATE (g_obj);

        if (priv->name) {
                g_free (priv->name);
                priv->name = NULL;
        }

        if (priv->l10n_name) {
                g_free (priv->l10n_name);
                priv->l10n_name = NULL;
        }

        if (priv->country) {
                g_free (priv->country);
                priv->country = NULL;
        }

#ifdef MEMORY_DOESNT_MATTER
        if (priv->city) {
                g_free (priv->city);
                priv->city = NULL;
        }
#endif

        if (priv->l10n_city) {
                g_free (priv->l10n_city);
                priv->l10n_city = NULL;
        }

#ifdef MEMORY_DOESNT_MATTER
        if (priv->comment) {
                g_free (priv->comment);
                priv->comment = NULL;
        }
#endif

        G_OBJECT_CLASS (clock_zoneinfo_parent_class)->finalize (g_obj);
}

const gchar *
clock_zoneinfo_get_name (ClockZoneInfo *this)
{
        ClockZoneInfoPrivate *priv = PRIVATE (this);

        return priv->name;
}

const gchar *
clock_zoneinfo_get_l10n_name (ClockZoneInfo *this)
{
        ClockZoneInfoPrivate *priv = PRIVATE (this);

        return priv->l10n_name;
}

const gchar *
clock_zoneinfo_get_country (ClockZoneInfo *this)
{
        ClockZoneInfoPrivate *priv = PRIVATE (this);

        return priv->country;
}

#ifdef MEMORY_DOESNT_MATTER
const gchar *
clock_zoneinfo_get_city (ClockZoneInfo *this)
{
        ClockZoneInfoPrivate *priv = PRIVATE (this);

        return priv->city;
}
#endif

const gchar *
clock_zoneinfo_get_l10n_city (ClockZoneInfo *this)
{
        ClockZoneInfoPrivate *priv = PRIVATE (this);

        return priv->l10n_city;
}

void
clock_zoneinfo_get_coords (ClockZoneInfo *this,
			   gfloat *lat, gfloat *lon)
{
        ClockZoneInfoPrivate *priv = PRIVATE (this);

        *lat = priv->latitude;
        *lon = priv->longitude;
}
