/*
 * panel-layout.c:
 * vim: set et:
 *
 * Copyright (C) 2011 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */


#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdk.h>

#include <libpanel-util/panel-cleanup.h>
#include <libpanel-util/panel-dconf.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-gsettings.h>

#include "panel.h"
#include "panel-gconf.h"
#include "panel-multiscreen.h"
#include "panel-object-loader.h"
#include "panel-schemas.h"
#include "panel-toplevel.h"

#include "panel-layout.h"

static GSettings *layout_settings = NULL;

#define PANEL_LAYOUT_ERROR panel_layout_error_quark ()
#define PANEL_LAYOUT_OBJECT_GCONF_PATH_TEMPLATE "/apps/panel3-applets/%s"

static void panel_layout_load_toplevel    (const char *toplevel_id);
static void panel_layout_load_object      (const char *object_id);
static void panel_layout_changed_toplevel (void);
static void panel_layout_changed_object   (void);

static GQuark
panel_layout_error_quark (void)
{
        static GQuark ret = 0;

        if (ret == 0)
                ret = g_quark_from_static_string ("panel_layout_error");

        return ret;
}

static void
panel_layout_init (void)
{
        if (layout_settings == NULL) {
                layout_settings = g_settings_new (PANEL_LAYOUT_SCHEMA);
                panel_cleanup_register (panel_cleanup_unref_and_nullify,
                                        &layout_settings);
        }
}


/************************************\
 * Adding to the layout from a file *
\************************************/


typedef struct {
        const char *name;
        GType       type;
} PanelLayoutKeyDefinition;

static PanelLayoutKeyDefinition panel_layout_toplevel_keys[] = {
        { PANEL_TOPLEVEL_NAME_KEY,            G_TYPE_STRING   },
        { PANEL_TOPLEVEL_SCREEN_KEY,          G_TYPE_INT      },
        { PANEL_TOPLEVEL_MONITOR_KEY,         G_TYPE_INT      },
        { PANEL_TOPLEVEL_EXPAND_KEY,          G_TYPE_BOOLEAN  },
        { PANEL_TOPLEVEL_ORIENTATION_KEY,     G_TYPE_STRING   },
        { PANEL_TOPLEVEL_SIZE_KEY,            G_TYPE_INT      },
        { PANEL_TOPLEVEL_X_KEY,               G_TYPE_INT      },
        { PANEL_TOPLEVEL_Y_KEY,               G_TYPE_INT      },
        { PANEL_TOPLEVEL_X_RIGHT_KEY,         G_TYPE_INT      },
        { PANEL_TOPLEVEL_Y_BOTTOM_KEY,        G_TYPE_INT      },
        { PANEL_TOPLEVEL_X_CENTERED_KEY,      G_TYPE_BOOLEAN  },
        { PANEL_TOPLEVEL_Y_CENTERED_KEY,      G_TYPE_BOOLEAN  },
        { PANEL_TOPLEVEL_AUTO_HIDE_KEY,       G_TYPE_BOOLEAN  },
        { PANEL_TOPLEVEL_ENABLE_BUTTONS_KEY,  G_TYPE_BOOLEAN  },
        { PANEL_TOPLEVEL_ENABLE_ARROWS_KEY,   G_TYPE_BOOLEAN  },
        { PANEL_TOPLEVEL_HIDE_DELAY_KEY,      G_TYPE_INT      },
        { PANEL_TOPLEVEL_UNHIDE_DELAY_KEY,    G_TYPE_INT      },
        { PANEL_TOPLEVEL_AUTO_HIDE_SIZE_KEY,  G_TYPE_INT      },
        { PANEL_TOPLEVEL_ANIMATION_SPEED_KEY, G_TYPE_STRING   }
};

static PanelLayoutKeyDefinition panel_layout_object_keys[] = {
        { PANEL_OBJECT_IID_KEY,         G_TYPE_STRING   },
        { PANEL_OBJECT_TOPLEVEL_ID_KEY, G_TYPE_STRING   },
        { PANEL_OBJECT_PACK_TYPE_KEY,   G_TYPE_STRING   },
        { PANEL_OBJECT_PACK_INDEX_KEY,  G_TYPE_INT      }
};

static gboolean
panel_layout_append_self_check (GSettings                 *settings,
                                PanelLayoutKeyDefinition  *key_definitions,
                                int                        key_definitions_len,
                                GError                   **error)
{
        char **settings_keys = NULL;
        int    i, j;

        /* Don't do those checks twice; we use a static array with a boolean
         * for each set of key definitions we might want to check, and mark the
         * booleans to TRUE once we've entered this function once for a set. */
        static gboolean self_check_done[2] = {FALSE, FALSE};

        g_assert (key_definitions == panel_layout_toplevel_keys ||
                  key_definitions == panel_layout_object_keys);

        if (key_definitions == panel_layout_toplevel_keys) {
            if (self_check_done[0])
                    return TRUE;
            else
                    self_check_done[0] = TRUE;
        }
        if (key_definitions == panel_layout_object_keys) {
            if (self_check_done[1])
                    return TRUE;
            else
                    self_check_done[1] = TRUE;
        }
        /* End hacky way of avoiding double-checks */

        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        settings_keys = g_settings_list_keys (settings);

        for (i = 0; settings_keys[i] != NULL; i++) {
                gboolean found = FALSE;

                for (j = 0; j < key_definitions_len; j++) {
                        if (g_strcmp0 (settings_keys[i],
                                       key_definitions[j].name) == 0) {
                                found = TRUE;
                                break;
                        }
                }

                if (!found) {
                        g_set_error (error, PANEL_LAYOUT_ERROR, 0,
                                     "Mismatch between keys defined in schema and keys known to gnome-panel ('%s' is not known)",
                                     settings_keys[i]);
                        g_strfreev (settings_keys);
                        return FALSE;
                }
        }

        g_strfreev (settings_keys);

        if (i != key_definitions_len) {
                g_set_error (error, PANEL_LAYOUT_ERROR, 0,
                             "Mismatch between keys defined in schema and keys known to gnome-panel (%d keys in schema, %d keys known to gnome-panel)",
                             i, j);
                return FALSE;
        }

        return TRUE;
}

static char *
panel_layout_find_free_id (const char *id_list_key,
                           const char *schema,
                           const char *path_prefix,
                           const char *try_id,
                           int         screen_for_toplevels)
{
        char      *unique_id;
        char     **existing_ids;
        char     **existing_dirs;
        gboolean   existing;
        int        index;
        int        i;

        existing_ids = g_settings_get_strv (layout_settings,
                                            id_list_key);
        existing_dirs = panel_dconf_list_subdirs (path_prefix, TRUE);

        index = 0;
        existing = TRUE;

        /* If a specific id is specified, try to use it; it might be
         * free */
        if (try_id) {
                if (screen_for_toplevels != -1 &&
                    g_strcmp0 (schema, PANEL_TOPLEVEL_SCHEMA) == 0)
                        unique_id = g_strdup_printf ("%s-screen%d",
                                                     try_id,
                                                     screen_for_toplevels);
                else
                        unique_id = g_strdup (try_id);

                existing = FALSE;

                for (i = 0; !existing && existing_ids[i] != NULL; i++) {
                        if (g_strcmp0 (unique_id,
                                       existing_ids[i]) == 0)
                                existing = TRUE;
                }
                for (i = 0; !existing && existing_dirs[i] != NULL; i++) {
                        if (g_strcmp0 (unique_id,
                                       existing_dirs[i]) == 0)
                                existing = TRUE;
                                break;
                }

                if (existing)
                        g_free (unique_id);

        } else {
                if (g_strcmp0 (schema, PANEL_TOPLEVEL_SCHEMA) == 0)
                        try_id = "toplevel";
                else if (g_strcmp0 (schema, PANEL_OBJECT_SCHEMA) == 0)
                        try_id = "object";
                else
                        g_assert_not_reached ();
        }

        /* Append an index at the end of the id to find a unique
         * id, not used yet */
        while (existing) {
                if (screen_for_toplevels != -1 &&
                    g_strcmp0 (schema, PANEL_TOPLEVEL_SCHEMA) == 0)
                        unique_id = g_strdup_printf ("%s-screen%d-%d",
                                                     try_id,
                                                     screen_for_toplevels,
                                                     index);
                else
                        unique_id = g_strdup_printf ("%s-%d", try_id, index);

                existing = FALSE;

                for (i = 0; !existing && existing_ids[i] != NULL; i++) {
                        if (g_strcmp0 (unique_id,
                                       existing_ids[i]) == 0)
                                existing = TRUE;
                }
                for (i = 0; !existing && existing_dirs[i] != NULL; i++) {
                        if (g_strcmp0 (unique_id,
                                       existing_dirs[i]) == 0)
                                existing = TRUE;
                }

                if (existing)
                        g_free (unique_id);
                index++;
        }

        g_strfreev (existing_dirs);
        g_strfreev (existing_ids);

        return unique_id;
}

static gboolean
panel_layout_append_group_helper (GKeyFile                  *keyfile,
                                  const char                *group,
                                  int                        set_screen_to,
                                  const char                *group_prefix,
                                  const char                *id_list_key,
                                  const char                *schema,
                                  const char                *path_prefix,
                                  const char                *default_prefix,
                                  PanelLayoutKeyDefinition  *key_definitions,
                                  int                        key_definitions_len,
                                  gboolean                   dry_run,
                                  GError                   **error,
                                  const char                *type_for_error_message)
{
        gboolean    retval = FALSE;
        const char *id;
        char       *unique_id = NULL;
        char       *path = NULL;
        GSettings  *settings = NULL;
        char      **keyfile_keys = NULL;
        char       *value_str;
        int         value_int;
        gboolean    value_boolean;
        int         i, j;

        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        /* Try to extract an id from the group, by stripping the prefix,
         * and create a unique id out of that */
        id = group + strlen (group_prefix);
        while (g_ascii_isspace (*id))
                id++;

        if (!*id)
                id = NULL;

        if (id && !panel_gsettings_is_valid_keyname (id, error))
                return FALSE;

        unique_id = panel_layout_find_free_id (id_list_key, schema, path_prefix,
                                               id, set_screen_to);

        path = g_strdup_printf ("%s%s/", path_prefix, unique_id);
        settings = g_settings_new_with_path (schema, path);
        g_free (path);

        /* Check that what the code knows matches what the schemas say */
        if (!panel_layout_append_self_check (settings,
                                             key_definitions,
                                             key_definitions_len,
                                             error))
                goto out;

        keyfile_keys = g_key_file_get_keys (keyfile, group, NULL, error);

        if (!keyfile_keys)
                goto out;

        /* Now do the real work: we validate/add keys from the keyfile */
        for (i = 0; keyfile_keys[i] != NULL; i++) {
                gboolean found = FALSE;

                for (j = 0; j < key_definitions_len; j++) {
                        if (g_strcmp0 (keyfile_keys[i],
                                       key_definitions[j].name) == 0) {
                                found = TRUE;
                                break;
                        }
                }

                if (!found) {
                        g_set_error (error, PANEL_LAYOUT_ERROR, 0,
                                     "Unknown key '%s' for %s",
                                     keyfile_keys[i],
                                     type_for_error_message);
                        return FALSE;
                }

                switch (key_definitions[j].type) {
                        case G_TYPE_STRING:
                                value_str = g_key_file_get_string (
                                                        keyfile,
                                                        group, keyfile_keys[i],
                                                        error);
                                if (!value_str)
                                        goto out;

                                if (!dry_run)
                                        g_settings_set_string (settings,
                                                               key_definitions[j].name,
                                                               value_str);
                                g_free (value_str);
                                break;

                        case G_TYPE_INT:
                                value_int = g_key_file_get_integer (
                                                        keyfile,
                                                        group, keyfile_keys[i],
                                                        error);
                                if (error && *error)
                                        goto out;

                                if (!dry_run)
                                        g_settings_set_int (settings,
                                                            key_definitions[j].name,
                                                            value_int);
                                break;

                        case G_TYPE_BOOLEAN:
                                value_boolean = g_key_file_get_boolean (
                                                        keyfile,
                                                        group, keyfile_keys[i],
                                                        error);
                                if (error && *error)
                                        goto out;

                                if (!dry_run)
                                        g_settings_set_boolean (settings,
                                                                key_definitions[j].name,
                                                                value_boolean);
                                break;
                        default:
                                g_assert_not_reached ();
                                break;
                }
        }

        if (!dry_run) {
                if (set_screen_to != -1 &&
                    g_strcmp0 (schema, PANEL_TOPLEVEL_SCHEMA) == 0)
                        g_settings_set_int (settings,
                                            PANEL_TOPLEVEL_SCREEN_KEY,
                                            set_screen_to);

                panel_gsettings_append_strv (layout_settings,
                                             id_list_key,
                                             unique_id);
        }

        retval = TRUE;

out:
        if (keyfile_keys)
                g_strfreev (keyfile_keys);
        if (settings)
                g_object_unref (settings);
        if (unique_id)
                g_free (unique_id);

        return retval;
}

static gboolean
panel_layout_append_group (GKeyFile    *keyfile,
                           const char  *group,
                           int          screen_for_toplevels,
                           gboolean     dry_run,
                           GError     **error)
{
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        if (g_strcmp0 (group, "Toplevel") == 0 ||
            g_str_has_prefix (group, "Toplevel "))
                return panel_layout_append_group_helper (
                                        keyfile, group,
                                        screen_for_toplevels,
                                        "Toplevel",
                                        PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY,
                                        PANEL_TOPLEVEL_SCHEMA,
                                        PANEL_LAYOUT_TOPLEVEL_PATH,
                                        PANEL_LAYOUT_TOPLEVEL_DEFAULT_PREFIX,
                                        panel_layout_toplevel_keys,
                                        G_N_ELEMENTS (panel_layout_toplevel_keys),
                                        dry_run, error, "toplevel");
        else if (g_strcmp0 (group, "Object") == 0 ||
                 g_str_has_prefix (group, "Object "))
                return panel_layout_append_group_helper (
                                        keyfile, group,
                                        -1,
                                        "Object",
                                        PANEL_LAYOUT_OBJECT_ID_LIST_KEY,
                                        PANEL_OBJECT_SCHEMA,
                                        PANEL_LAYOUT_OBJECT_PATH,
                                        PANEL_LAYOUT_OBJECT_DEFAULT_PREFIX,
                                        panel_layout_object_keys,
                                        G_N_ELEMENTS (panel_layout_object_keys),
                                        dry_run, error, "object");

        g_set_error (error, PANEL_LAYOUT_ERROR, 0,
                     "Unknown group '%s'", group);

        return FALSE;
}

static void
panel_layout_append_from_file_real (const char *layout_file,
                                    int         screen_for_toplevels)
{
        GError    *error = NULL;
        GKeyFile  *keyfile = NULL;
        char     **groups = NULL;
        gboolean   found_one = FALSE;
        int        i;

        panel_layout_init ();

        keyfile = g_key_file_new ();

        error = NULL;
        if (!g_key_file_load_from_file (keyfile, layout_file,
                                        G_KEY_FILE_NONE, &error))
                goto out;

        groups = g_key_file_get_groups (keyfile, NULL);

        /* First pass to validate: we don't want to add only a subset of the
         * layout; the whole layout has to be valid */
        for (i = 0; groups[i] != NULL; i++) {
                if (!panel_layout_append_group (keyfile, groups[i],
                                                screen_for_toplevels,
                                                TRUE, &error))
                        goto out;
                else
                        found_one = TRUE;
        }

        if (!found_one) {
                error = g_error_new (PANEL_LAYOUT_ERROR, 0,
                                     "No defined toplevel or object");
                goto out;
        }

        /* Second pass to really add the layout. We know there'll be no error
         * since the first pass worked. */
        for (i = 0; groups[i] != NULL; i++)
                panel_layout_append_group (keyfile, groups[i],
                                           screen_for_toplevels,
                                           FALSE, NULL);

out:
        if (error) {
                g_printerr ("Error while parsing default layout from '%s': %s\n",
                            layout_file, error->message);
                g_error_free (error);
        }

        if (groups)
                g_strfreev (groups);

        if (keyfile)
                g_key_file_free (keyfile);
}

static void
panel_layout_append_from_file_for_screen (const char *layout_file,
                                          GdkScreen  *screen)
{
        int screen_n = gdk_screen_get_number (screen);

        panel_layout_append_from_file_real (layout_file, screen_n);
}

void
panel_layout_append_from_file (const char *layout_file)
{
        panel_layout_append_from_file_real (layout_file, -1);
}


/***********************\
 * New toplevel/object *
\***********************/


void
panel_layout_toplevel_create (GdkScreen *screen)
{
        char             *unique_id;
        char             *path;
        GSettings        *settings;
	PanelOrientation  orientation;
	int               monitor;

        unique_id = panel_layout_find_free_id (PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY,
                                               PANEL_TOPLEVEL_SCHEMA,
                                               PANEL_LAYOUT_TOPLEVEL_PATH,
                                               NULL, -1);

        path = g_strdup_printf ("%s%s/", PANEL_LAYOUT_TOPLEVEL_PATH, unique_id);
        settings = g_settings_new_with_path (PANEL_TOPLEVEL_SCHEMA, path);
        g_free (path);

        g_settings_set_int (settings,
                            PANEL_TOPLEVEL_SCREEN_KEY,
                            gdk_screen_get_number (screen));

        if (panel_toplevel_find_empty_spot (screen, &orientation, &monitor)) {
                g_settings_set_enum (settings,
                                     PANEL_TOPLEVEL_ORIENTATION_KEY,
                                     orientation);
                g_settings_set_int (settings,
                                    PANEL_TOPLEVEL_MONITOR_KEY,
                                    monitor);
        }

        g_object_unref (settings);

        panel_gsettings_append_strv (layout_settings,
                                     PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY,
                                     unique_id);

        g_free (unique_id);
}

void
panel_layout_object_create (PanelObjectType      type,
                            const char          *type_detail,
                            const char          *toplevel_id,
                            PanelObjectPackType  pack_type,
                            int                  pack_index)
{
        char *id;

        id = panel_layout_object_create_start (type, type_detail,
                                               toplevel_id, pack_type, pack_index,
                                               NULL);

        if (!id)
                return;

        panel_layout_object_create_finish (id);

        g_free (id);
}

char *
panel_layout_object_get_gconf_path (const char *object_id)
{
        char *ret;
        char *gconfied_id;

        /* gconf uses '_' and not '-' */
        gconfied_id = g_strdup (object_id);
        g_strdelimit (gconfied_id, "-", '_');

        ret = g_strdup_printf (PANEL_LAYOUT_OBJECT_GCONF_PATH_TEMPLATE,
                               gconfied_id);

        g_free (gconfied_id);

        return ret;
}

GSettings *
panel_layout_get_instance_settings (GSettings  *settings_object,
                                    const char *schema)
{
        char      *path;
        char      *path_instance;
        GSettings *settings_instance;

        g_return_val_if_fail (G_IS_SETTINGS (settings_object), NULL);

        g_object_get (settings_object, "path", &path, NULL);
        path_instance = g_strdup_printf ("%s%s", path,
                                         PANEL_LAYOUT_OBJECT_CONFIG_SUFFIX);
        g_free (path);

        settings_instance = g_settings_new_with_path (schema, path_instance);
        g_free (path_instance);

        return settings_instance;
}

char *
panel_layout_object_create_start (PanelObjectType       type,
                                  const char           *type_detail,
                                  const char           *toplevel_id,
                                  PanelObjectPackType   pack_type,
                                  int                   pack_index,
                                  GSettings           **settings)
{
        char      *unique_id;
        char      *path;
        GSettings *settings_object;
        char      *iid;

        if (settings)
                *settings = NULL;

        iid = panel_object_type_to_iid (type, type_detail);
        if (!iid)
                return NULL;

        unique_id = panel_layout_find_free_id (PANEL_LAYOUT_OBJECT_ID_LIST_KEY,
                                               PANEL_OBJECT_SCHEMA,
                                               PANEL_LAYOUT_OBJECT_PATH,
                                               NULL, -1);

        path = g_strdup_printf ("%s%s/", PANEL_LAYOUT_OBJECT_PATH, unique_id);
        settings_object = g_settings_new_with_path (PANEL_OBJECT_SCHEMA, path);
        g_free (path);

        g_settings_set_string (settings_object,
                               PANEL_OBJECT_IID_KEY,
                               iid);
        g_settings_set_string (settings_object,
                               PANEL_OBJECT_TOPLEVEL_ID_KEY,
                               toplevel_id);
        g_settings_set_enum (settings_object,
                             PANEL_OBJECT_PACK_TYPE_KEY,
                             pack_type);
        g_settings_set_int (settings_object,
                            PANEL_OBJECT_PACK_INDEX_KEY,
                            pack_index);

        g_free (iid);

        if (settings)
                *settings = settings_object;
        else
                g_object_unref (settings_object);

        return unique_id;
}

void
panel_layout_object_create_finish (const char *object_id)
{
        panel_gsettings_append_strv (layout_settings,
                                     PANEL_LAYOUT_OBJECT_ID_LIST_KEY,
                                     object_id);
}


/*******************\
 * Changing layout *
\*******************/


gboolean
panel_layout_is_writable (void)
{
        return (g_settings_is_writable (layout_settings,
                                        PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY) &&
                g_settings_is_writable (layout_settings,
                                        PANEL_LAYOUT_OBJECT_ID_LIST_KEY));
}

void
panel_layout_delete_toplevel (const char *toplevel_id)
{
        char  *path;
        char  *id_copy;
        char **objects;
        int    i;

        if (PANEL_GLIB_STR_EMPTY (toplevel_id))
                return;

        /* The original will be freed if removal succeeds */
        id_copy = g_strdup (toplevel_id);

        if (!panel_gsettings_remove_all_from_strv (layout_settings,
                                                   PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY,
                                                   id_copy)) {
                g_free (id_copy);
                return;
        }

        path = g_strdup_printf ("%s%s/",
                                PANEL_LAYOUT_TOPLEVEL_PATH, id_copy);
        panel_dconf_recursive_reset (path, NULL);

        /* remove all applets that were on this toplevel */

        objects = g_settings_get_strv (layout_settings,
                                       PANEL_LAYOUT_OBJECT_ID_LIST_KEY);

        for (i = 0; objects[i] != NULL; i++) {
                GSettings *settings;
                char       *object_toplevel_id;

                path = g_strdup_printf ("%s%s/",
                                        PANEL_LAYOUT_OBJECT_PATH, objects[i]);

                settings = g_settings_new_with_path (PANEL_OBJECT_SCHEMA, path);
                object_toplevel_id = g_settings_get_string (settings,
                                                            PANEL_OBJECT_TOPLEVEL_ID_KEY);

                g_object_unref (settings);
                g_free (path);

                if (g_strcmp0 (id_copy, object_toplevel_id) == 0)
                        panel_layout_delete_object (objects[i]);

                g_free (object_toplevel_id);
        }

        g_strfreev (objects);

        g_free (id_copy);
}

void
panel_layout_delete_object (const char *object_id)
{
        char *path;
        char *id_copy;

        if (PANEL_GLIB_STR_EMPTY (object_id))
                return;

        /* The original will be freed if removal succeeds */
        id_copy = g_strdup (object_id);

        if (!panel_gsettings_remove_all_from_strv (layout_settings,
                                                   PANEL_LAYOUT_OBJECT_ID_LIST_KEY,
                                                   id_copy)) {
                g_free (id_copy);
                return;
        }

        path = g_strdup_printf ("%s%s/",
                                PANEL_LAYOUT_OBJECT_PATH, id_copy);
        panel_dconf_recursive_reset (path, NULL);
        g_free (path);

        path = panel_layout_object_get_gconf_path (id_copy);
        panel_gconf_recursive_unset (path, NULL);
        g_free (path);

        g_free (id_copy);
}

static void
panel_layout_changed_toplevel (void)
{
        char       **ids;
        GSList      *to_remove;
        gboolean     loading;
        gboolean     found;
        const char  *id;
        GSList      *l;
        int          i;

        ids = g_settings_get_strv (layout_settings,
                                   PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY);

        /* Remove what is not in the layout anymore */

        to_remove = NULL;

        for (l = panel_toplevel_list_toplevels (); l != NULL; l = l->next) {
                id = panel_toplevel_get_id (l->data);
                found = FALSE;

                for (i = 0; ids[i] != NULL; i++) {
                        if (g_strcmp0 (ids[i], id) == 0) {
                                found = TRUE;
                                break;
                        }
                }

                if (!found)
                        to_remove = g_slist_prepend (to_remove, l->data);
        }

        for (l = to_remove; l != NULL; l = l->next)
                gtk_widget_destroy (GTK_WIDGET (l->data));

        g_slist_free (to_remove);

        /* Add what appeared in the layout */

        loading = FALSE;

        for (i = 0; ids[i] != NULL; i++) {
                found = FALSE;

                for (l = panel_toplevel_list_toplevels (); l != NULL; l = l->next) {
                        id = panel_toplevel_get_id (l->data);
                        if (g_strcmp0 (ids[i], id) == 0) {
                                found = TRUE;
                                break;
                        }
                }

                if (!found) {
                        panel_layout_load_toplevel (ids[i]);
                        loading = TRUE;
                }
        }

        g_strfreev (ids);

        /* Reload list of objects to get those that might be on the new
         * toplevels */
        if (loading)
                panel_layout_changed_object ();
}

static void
panel_layout_changed_object (void)
{
        char       **ids;
        GSList      *to_remove;
        gboolean     found;
        const char  *id;
        GSList      *l;
        int          i;

        ids = g_settings_get_strv (layout_settings,
                                   PANEL_LAYOUT_OBJECT_ID_LIST_KEY);

        /* Remove what is not in the layout anymore */

        to_remove = NULL;

        for (l = panel_applet_list_applets (); l != NULL; l = l->next) {
                id = panel_applet_get_id (l->data);
                found = FALSE;

                for (i = 0; ids[i] != NULL; i++) {
                        if (g_strcmp0 (ids[i], id) == 0) {
                                found = TRUE;
                                break;
                        }
                }

                if (!found)
                        to_remove = g_slist_prepend (to_remove, l->data);
        }

        for (l = to_remove; l != NULL; l = l->next)
                panel_applet_clean (l->data);

        g_slist_free (to_remove);

        /* Add what appeared in the layout */

        for (i = 0; ids[i] != NULL; i++) {
                found = FALSE;

                if (panel_object_loader_is_queued (ids[i]))
                        continue;

                for (l = panel_applet_list_applets (); l != NULL; l = l->next) {
                        id = panel_applet_get_id (l->data);
                        if (g_strcmp0 (ids[i], id) == 0) {
                                found = TRUE;
                                break;
                        }
                }

                if (!found)
                        panel_layout_load_object (ids[i]);
        }

        g_strfreev (ids);

        /* Always do this, even if there is no object that got loaded: if a
         * panel has been created, we want a do_load() to unhide it, even if
         * there is no object to load */
        panel_object_loader_do_load (FALSE);
}

static void
panel_layout_changed (GSettings *settings,
                      char      *key,
                      gpointer   user_data)
{
        if (g_strcmp0 (key, PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY) == 0)
                panel_layout_changed_toplevel ();
        else if (g_strcmp0 (key, PANEL_LAYOUT_OBJECT_ID_LIST_KEY) == 0)
                panel_layout_changed_object ();
}

/******************\
 * Loading layout *
\******************/


static void
panel_layout_load_toplevel (const char *toplevel_id)
{
        PanelToplevel *toplevel;
        char          *path;
        GSettings     *settings;
        int            screen;

        if (PANEL_GLIB_STR_EMPTY (toplevel_id))
                return;

        path = g_strdup_printf ("%s%s/",
                                PANEL_LAYOUT_TOPLEVEL_PATH, toplevel_id);

        /* Check that the screen is valid */
        settings = g_settings_new_with_path (PANEL_TOPLEVEL_SCHEMA, path);
        screen = g_settings_get_int (settings, PANEL_TOPLEVEL_SCREEN_KEY);
        g_object_unref (settings);

        if (screen < 0 || screen >= panel_multiscreen_screens ()) {
                g_free (path);
                return;
        }

        toplevel = g_object_new (PANEL_TYPE_TOPLEVEL,
                                 "toplevel-id", toplevel_id,
                                 "settings-path", path,
                                 NULL);

        g_free (path);

        /* FIXME: we shouldn't have to do this manually */
        panel_setup (toplevel);

        gtk_widget_show (GTK_WIDGET (toplevel));
}

static void
panel_layout_load_object (const char *object_id)
{
        char *path;

        if (PANEL_GLIB_STR_EMPTY (object_id))
                return;

        path = g_strdup_printf ("%s%s/",
                                PANEL_LAYOUT_OBJECT_PATH, object_id);

        panel_object_loader_queue (object_id, path);

        g_free (path);
}

static char *
panel_layout_get_default_layout_file (void)
{
        return g_build_filename (PANELDATADIR,
                                 "panel-default-layout.layout",
                                 NULL);
}

static void
panel_layout_ensure_toplevel_per_screen (void)
{
        GSList     *toplevels;
        GSList     *empty_screens = NULL;
        GSList     *l;
        GdkDisplay *display;
        int         n_screens, i;
        char       *default_layout_file;

        toplevels = panel_toplevel_list_toplevels ();

        display = gdk_display_get_default ();

        n_screens = gdk_display_get_n_screens (display);
        for (i = 0; i < n_screens; i++) {
                GdkScreen *screen;

                screen = gdk_display_get_screen (display, i);

                for (l = toplevels; l; l = l->next)
                        if (gtk_window_get_screen (l->data) == screen)
                                break;

                if (!l)
                        empty_screens = g_slist_prepend (empty_screens, screen);
        }

        if (empty_screens == NULL)
                return;

        default_layout_file = panel_layout_get_default_layout_file ();

        for (l = empty_screens; l; l = l->next)
                panel_layout_append_from_file_for_screen (default_layout_file,
                                                          l->data);

        g_free (default_layout_file);

        g_slist_free (empty_screens);
}

gboolean
panel_layout_load (void)
{
        char **toplevels;
        char **objects;
        int    i;

        panel_layout_init ();

        toplevels = g_settings_get_strv (layout_settings,
                                         PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY);

        if (!toplevels[0]) {
                char *default_layout_file;

                g_strfreev (toplevels);

                if (!g_settings_is_writable (layout_settings,
                                             PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY) ||
                    !g_settings_is_writable (layout_settings,
                                             PANEL_LAYOUT_OBJECT_ID_LIST_KEY)) {
                        g_printerr (_("Cannot create initial panel layout.\n"));

                        return FALSE;
                }

                default_layout_file = panel_layout_get_default_layout_file ();
                panel_layout_append_from_file (default_layout_file);
                g_free (default_layout_file);

                toplevels = g_settings_get_strv (layout_settings,
                                                 PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY);

                if (!toplevels[0]) {
                        g_strfreev (toplevels);
                        g_printerr (_("Cannot create initial panel layout.\n"));

                        return FALSE;
                }
        }

        for (i = 0; toplevels[i] != NULL; i++)
                panel_layout_load_toplevel (toplevels[i]);

        g_strfreev (toplevels);

        objects = g_settings_get_strv (layout_settings,
                                       PANEL_LAYOUT_OBJECT_ID_LIST_KEY);

        for (i = 0; objects[i] != NULL; i++)
                panel_layout_load_object (objects[i]);

        g_strfreev (objects);

        g_signal_connect (layout_settings, "changed",
                          G_CALLBACK (panel_layout_changed), NULL);

        /* This needs to happen after we've loaded the current toplevels (to
         * know if we have toplevels on all screens), and after we've connected
         * to the settings changed notifications (to automatically load created
         * toplevels) */
        panel_layout_ensure_toplevel_per_screen ();

        panel_object_loader_do_load (TRUE);

        return TRUE;
}
