#include "clock-zonetable.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>

#include "clock-country.h"
#include "clock-zoneinfo.h"

#ifdef HAVE_SOLARIS
#define ZONETAB_FILE SYSTEM_ZONEINFODIR"/zone_sun.tab"
#define ISO3166_FILE SYSTEM_ZONEINFODIR"/country.tab"
#else
#define ZONETAB_FILE SYSTEM_ZONEINFODIR"/zone.tab"
#define ISO3166_FILE SYSTEM_ZONEINFODIR"/iso3166.tab"
#endif

G_DEFINE_TYPE (ClockZoneTable, clock_zonetable, G_TYPE_OBJECT)

typedef struct {
        GList *list;
        GHashTable *table;
        GHashTable *l10n_table;

        GList *country_list;
        GHashTable *country_table;
} ClockZoneTablePrivate;

static GObject *clock_zonetable_constructor (GType                  type,
                                             guint                  n_construct_properties,
                                             GObjectConstructParam *construct_properties);
static void clock_zonetable_finalize (GObject *);
static void clock_zonetable_load_zonetab (ClockZoneTable *this);
static void clock_zonetable_load_iso3166 (ClockZoneTable *this);

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CLOCK_ZONETABLE_TYPE, ClockZoneTablePrivate))

ClockZoneTable *
clock_zonetable_new (void)
{
        ClockZoneTable *this;

        this = g_object_new (CLOCK_ZONETABLE_TYPE, NULL);

        return this;
}

static void
clock_zonetable_class_init (ClockZoneTableClass *this_class)
{
        GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

        g_obj_class->constructor = clock_zonetable_constructor;
        g_obj_class->finalize = clock_zonetable_finalize;

        g_type_class_add_private (this_class, sizeof (ClockZoneTablePrivate));

        /* FIXME: find a good way to not use Evolution's gettext domain without
         * duplicating all the strings */
        bindtextdomain (EVOLUTION_TEXTDOMAIN, GNOMELOCALEDIR);
        bind_textdomain_codeset (EVOLUTION_TEXTDOMAIN, "UTF-8");
}

static void
clock_zonetable_init (ClockZoneTable *this)
{
        ClockZoneTablePrivate *priv = PRIVATE (this);

        priv->list = NULL;
        priv->table = NULL;
        priv->l10n_table = NULL;

        priv->country_list = NULL;
        priv->country_table = NULL;
}

static GObject *
clock_zonetable_constructor (GType                  type,
                             guint                  n_construct_properties,
                             GObjectConstructParam *construct_properties)
{
        static GObject *obj = NULL;

        /* This is a singleton, we don't need to have it per-applet */
        if (obj)
                return g_object_ref (obj);

        obj = G_OBJECT_CLASS (clock_zonetable_parent_class)->constructor (
                                                type,
                                                n_construct_properties,
                                                construct_properties);


        clock_zonetable_load_zonetab (CLOCK_ZONETABLE (obj));
        clock_zonetable_load_iso3166 (CLOCK_ZONETABLE (obj));
        /* FIXME: add some file monitoring here to reload the files? */

        return obj;
}

static void
clock_zonetable_finalize (GObject *g_obj)
{
        ClockZoneTablePrivate *priv = PRIVATE (g_obj);

        if (priv->list) {
                g_list_free (priv->list);
                priv->list = NULL;
        }

        if (priv->table) {
                g_hash_table_destroy (priv->table);
                priv->table = NULL;
        }

        if (priv->l10n_table) {
                g_hash_table_destroy (priv->l10n_table);
                priv->l10n_table = NULL;
        }

        if (priv->country_list) {
                g_list_free (priv->country_list);
                priv->country_list = NULL;
        }

        if (priv->country_table) {
                g_hash_table_destroy (priv->country_table);
                priv->country_table = NULL;
        }

        G_OBJECT_CLASS (clock_zonetable_parent_class)->finalize (g_obj);
}

static gboolean
clock_zonetable_parse_coord (const gchar *coord, gfloat *ret)
{
        gfloat deg = 0;
        gfloat min = 0;
        gfloat sec = 0;

        const gchar *num = coord + 1;
        int read;
        int len = strlen (num);

        if (len == 4) {
                /* DDMM */
                read = sscanf (num, "%2f%2f", &deg, &min);
                if (read != 2)
                        return FALSE;
        } else if (len == 5) {
                /* DDDMM */
                sscanf (num, "%3f%2f", &deg, &min);
                if (read != 2)
                        return FALSE;
        } else if (len == 6) {
                /* DDMMSS */
                sscanf (num, "%2f%2f%2f", &deg, &min, &sec);
                if (read != 3)
                        return FALSE;
        } else if (len == 7) {
                /* DDDMMSS */
                sscanf (num, "%3f%2f%2f", &deg, &min, &sec);
                if (read != 3)
                        return FALSE;
        }

        *ret = deg + min / 60 + sec / 3600;

        if (coord[0] == '-') {
                *ret = -*ret;
        }

        return TRUE;
}


static gboolean
clock_zonetable_parse_location (const gchar *location, gfloat *lat, gfloat *lon)
{
        int i;
        gboolean success;

        gchar *lat_str = NULL;
        const gchar *lon_str = NULL;

        for (i = 1; location[i] != '\0'; i++) {
                if (location[i] == '+' || location[i] == '-') {
                        lat_str = g_strndup (location, i);
                        lon_str = location + i;
                        break;
                }
        }

        if (!lat_str || !lon_str)
                return FALSE;

        success = clock_zonetable_parse_coord (lat_str, lat) &&
                  clock_zonetable_parse_coord (lon_str, lon);

        g_free (lat_str);
        /* lon_str wasn't copied */

        return success;
}

static ClockZoneInfo *
clock_zonetable_parse_info_line (const gchar *line)
{
        ClockZoneInfo *ret = NULL;

        gchar *country = NULL;
        gchar *location = NULL;
        gchar *zone = NULL;
        gchar *comment = NULL;

        gfloat lat;
        gfloat lon;

        gchar **split = g_strsplit (line, "\t", 0);

        if (split[0] && split[1] && split[2] && (!split[3] || !split[4])) {
                country = split[0];
                location = split[1];
                zone = split[2];

                if (split[3] != NULL)
                        comment = split[3];

                if (clock_zonetable_parse_location (location, &lat, &lon))
                        ret = clock_zoneinfo_new (zone, country,
                                                  comment, lat, lon);
        }

        g_strfreev (split);
        return ret;
}

static ClockCountry *
clock_zonetable_parse_iso3166_line (const gchar *line)
{
        ClockCountry *ret = NULL;

        gchar *code = NULL;
        gchar *name = NULL;

        gchar **split = g_strsplit (line, "\t", 0);

        if (split[0] && split[1] && !split[2]) {
                code = split[0];
                name = split[1];

                ret = clock_country_new (code, name);
        }

        g_strfreev (split);
        return ret;
}

static void
clock_zonetable_load_zonetab (ClockZoneTable *this)
{
        ClockZoneTablePrivate *priv = PRIVATE (this);
        GIOChannel *channel;
        gchar *line;
        gchar *old_line;
        ClockZoneInfo *info;

        priv->table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             NULL, g_object_unref);
        priv->l10n_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  NULL, g_object_unref);

        channel = g_io_channel_new_file (ZONETAB_FILE, "r", NULL);

        /* FIXME: be more solid than just crashing */
        g_assert (channel != NULL);

        old_line = NULL;
        while (g_io_channel_read_line (channel, &line, NULL,
                                       NULL, NULL) == G_IO_STATUS_NORMAL) {
                g_free (old_line);
                old_line = line;

                g_strstrip (line);

                if (line[0] == '#')
                        continue;

                info = clock_zonetable_parse_info_line (line);
                if (!info)
                        continue;

                priv->list = g_list_prepend (priv->list, info);
                g_hash_table_replace (priv->table,
                                      clock_zoneinfo_get_name (info),
                                      g_object_ref_sink (info));
                g_hash_table_replace (priv->l10n_table,
                                      clock_zoneinfo_get_l10n_name (info),
                                      g_object_ref_sink (info));
        }
        g_free (old_line);

        g_io_channel_unref (channel);
}

static void
clock_zonetable_load_iso3166 (ClockZoneTable *this)
{
        ClockZoneTablePrivate *priv = PRIVATE (this);
        GIOChannel *channel;
        gchar *line;
        gchar *old_line;
        ClockCountry *info;

        priv->country_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     NULL, g_object_unref);

        channel = g_io_channel_new_file (ISO3166_FILE, "r", NULL);

        /* FIXME: be more solid than just crashing */
        g_assert (channel != NULL);

        old_line = NULL;
        while (g_io_channel_read_line (channel, &line, NULL,
                                       NULL, NULL) == G_IO_STATUS_NORMAL) {
                g_free (old_line);
                old_line = line;

                g_strstrip (line);

                if (line[0] == '#')
                        continue;

                info = clock_zonetable_parse_iso3166_line (line);
                if (!info)
                        continue;

                priv->country_list = g_list_prepend (priv->country_list, info);
                g_hash_table_replace (priv->country_table,
                                      (char *) clock_country_get_code (info),
                                      g_object_ref_sink (info));
        }
        g_free (old_line);

        g_io_channel_unref (channel);
}

ClockZoneInfo *
clock_zonetable_get_zone (ClockZoneTable *this,
                          const gchar    *name)
{
        ClockZoneTablePrivate *priv = PRIVATE (this);

        return CLOCK_ZONEINFO (g_hash_table_lookup (priv->table, name));
}

ClockZoneInfo *
clock_zonetable_get_l10n_zone (ClockZoneTable *this,
                               const gchar    *l10n_name)
{
        ClockZoneTablePrivate *priv = PRIVATE (this);

        return CLOCK_ZONEINFO (g_hash_table_lookup (priv->l10n_table, l10n_name));
}

GList *
clock_zonetable_get_zones (ClockZoneTable *this)
{
        ClockZoneTablePrivate *priv = PRIVATE (this);

        return priv->list;
}

ClockCountry *
clock_zonetable_get_country (ClockZoneTable *this,
                             const gchar    *code)
{
        ClockZoneTablePrivate *priv = PRIVATE (this);

        return CLOCK_COUNTRY (g_hash_table_lookup (priv->country_table, code));
}
