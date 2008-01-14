#include "clock-country.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <libgnome/gnome-i18n.h>

G_DEFINE_TYPE (ClockCountry, clock_country, G_TYPE_OBJECT)

typedef struct {
        gchar *code;
        gchar *name;
} ClockCountryPrivate;

static void clock_country_finalize (GObject *);

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CLOCK_COUNTRY_TYPE, ClockCountryPrivate))

ClockCountry *
clock_country_new (gchar *code, gchar *name)
{
        ClockCountry *this;
        ClockCountryPrivate *priv;

        this = g_object_new (CLOCK_COUNTRY_TYPE, NULL);
        priv = PRIVATE (this);

        priv->code = g_strdup (code);
        priv->name = g_strdup (name);

        return this;
}

static void
clock_country_class_init (ClockCountryClass *this_class)
{
        GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

        g_obj_class->finalize = clock_country_finalize;

        g_type_class_add_private (this_class, sizeof (ClockCountryPrivate));
}

static void
clock_country_init (ClockCountry *this)
{
        ClockCountryPrivate *priv = PRIVATE (this);

        priv->code = NULL;
        priv->name = NULL;
}

static void
clock_country_finalize (GObject *g_obj)
{
        ClockCountryPrivate *priv = PRIVATE (g_obj);

        if (priv->code) {
                g_free (priv->code);
                priv->code = NULL;
        }

        if (priv->name) {
                g_free (priv->name);
                priv->name = NULL;
        }

        G_OBJECT_CLASS (clock_country_parent_class)->finalize (g_obj);
}

gchar *
clock_country_get_code (ClockCountry *this)
{
        ClockCountryPrivate *priv = PRIVATE (this);

        return priv->code;
}

gchar *
clock_country_get_name (ClockCountry *this)
{
        ClockCountryPrivate *priv = PRIVATE (this);

        return priv->name;
}

gchar *
clock_country_get_l10n_name (ClockCountry *this)
{
        ClockCountryPrivate *priv = PRIVATE (this);

	/* FMQ: It is broken to read from Evolution's text domain */
        return dgettext (EVOLUTION_TEXTDOMAIN, priv->name);
}
