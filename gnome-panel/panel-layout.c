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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */


#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdk.h>

#include <libpanel-util/panel-dconf.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-gsettings.h>

#include "panel.h"
#include "panel-multiscreen.h"
#include "panel-object-loader.h"
#include "panel-schemas.h"
#include "panel-toplevel.h"

#include "panel-layout.h"

struct _PanelLayout
{
  GObject        parent;

  GSettings     *settings;

  GpApplication *application;
};

G_DEFINE_TYPE (PanelLayout, panel_layout, G_TYPE_OBJECT)

#define PANEL_LAYOUT_ERROR panel_layout_error_quark ()

#define DEFAULT_LAYOUT_FILE "default.layout"
#define PANEL_LAYOUT_INSTANCE_CONFIG_SUBPATH "@instance-config/"

static void panel_layout_load_toplevel    (PanelLayout *self,
                                           const char  *toplevel_id);
static void panel_layout_load_object      (PanelLayout *self,
                                           const char  *object_id);
static void panel_layout_changed_toplevel (PanelLayout *self);
static void panel_layout_changed_object   (PanelLayout *self);

static GQuark
panel_layout_error_quark (void)
{
        static GQuark ret = 0;

        if (ret == 0)
                ret = g_quark_from_static_string ("panel_layout_error");

        return ret;
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
        { PANEL_TOPLEVEL_MONITOR_KEY,         G_TYPE_INT      },
        { PANEL_TOPLEVEL_EXPAND_KEY,          G_TYPE_BOOLEAN  },
        { PANEL_TOPLEVEL_ORIENTATION_KEY,     G_TYPE_STRING   },
        { PANEL_TOPLEVEL_ALIGNMENT_KEY,       G_TYPE_INT      },
        { PANEL_TOPLEVEL_SIZE_KEY,            G_TYPE_INT      },
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
        { PANEL_OBJECT_MODULE_ID_KEY,   G_TYPE_STRING   },
        { PANEL_OBJECT_APPLET_ID_KEY,   G_TYPE_STRING   },
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
        GSettingsSchema *schema;
        char **settings_keys = NULL;
        int    i, j=0;

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

        g_object_get (settings, "settings-schema", &schema, NULL);
        settings_keys =  g_settings_schema_list_keys (schema);
        g_settings_schema_unref (schema);

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
panel_layout_find_free_id (PanelLayout *self,
                           const char  *id_list_key,
                           const char  *schema,
                           const char  *path_prefix,
                           const char  *try_id)
{
        char      *unique_id;
        char     **existing_ids;
        char     **existing_dirs;
        gboolean   existing;
        int        index;
        int        i;

        existing_ids = g_settings_get_strv (self->settings,
                                            id_list_key);
        existing_dirs = panel_dconf_list_subdirs (path_prefix, TRUE);

        index = 0;
        existing = TRUE;

        /* If a specific id is specified, try to use it; it might be
         * free */
        if (try_id) {
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
panel_layout_maybe_append_object_instance_config (GKeyFile    *keyfile,
                                                  const char  *group,
                                                  const char  *key,
                                                  const char  *path_prefix,
                                                  const char  *unique_id,
                                                  gboolean     dry_run,
                                                  gboolean    *key_handled,
                                                  GError     **error)
{
        char       *value_str;
        const char *keyname;
        GVariant   *variant;

        *key_handled = FALSE;

        if (!g_str_has_prefix(key, PANEL_LAYOUT_INSTANCE_CONFIG_SUBPATH))
                return TRUE;

        *key_handled = TRUE;

        value_str = g_key_file_get_string (
                                keyfile,
                                group, key,
                                error);
        if (!value_str)
                return FALSE;

        variant = g_variant_parse (NULL, value_str,
                                   NULL, NULL, error);

        if (!variant) {
                g_free (value_str);
                return FALSE;
        }

        keyname = key + strlen (PANEL_LAYOUT_INSTANCE_CONFIG_SUBPATH);

        if (dry_run) {
                /* the key can actually be in a subdirectory
                 * like instance-config/foo/key, so we split
                 * the tokens to validate all of them */
                char **tokens;
                char **token;

                tokens = g_strsplit (keyname, "/", -1);

                for (token = tokens; *token; token++) {
                        if (!panel_gsettings_is_valid_keyname (*token,
                                                               error)) {
                                g_strfreev (tokens);
                                g_variant_unref (variant);
                                g_free (value_str);
                                return FALSE;
                        }
                }

                g_strfreev (tokens);
        } else {
                char *tmp;

                tmp = g_strdup_printf ("%s%s/%s%s",
                                       path_prefix, unique_id,
                                       PANEL_LAYOUT_OBJECT_CONFIG_SUFFIX,
                                       keyname);
                panel_dconf_write_sync (tmp, variant, NULL);
                g_free (tmp);
        }

        g_variant_unref (variant);
        g_free (value_str);

        return TRUE;
}

static gboolean
panel_layout_append_group_helper (PanelLayout               *self,
                                  GKeyFile                  *keyfile,
                                  const char                *group,
                                  const char                *group_prefix,
                                  const char                *id_list_key,
                                  const char                *schema,
                                  const char                *path_prefix,
                                  PanelLayoutKeyDefinition  *key_definitions,
                                  int                        key_definitions_len,
                                  gboolean                   dry_run,
                                  GError                   **error,
                                  const char                *type_for_error_message)
{
        gboolean    retval = FALSE;
        gboolean    appending_object;
        char       *unique_id = NULL;
        char       *path = NULL;
        GSettings  *settings = NULL;
        char      **keyfile_keys = NULL;
        char       *value_str;
        int         value_int;
        gboolean    value_boolean;
        int         i, j;

        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        appending_object = (g_strcmp0 (schema, PANEL_OBJECT_SCHEMA) == 0);

        unique_id = g_key_file_get_string (keyfile, group, "unique-id", NULL);

        if (!panel_gsettings_is_valid_keyname (unique_id, error)) {
                g_free (unique_id);
                return FALSE;
        }

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

                if (g_strcmp0 (keyfile_keys[i], "unique-id") == 0)
                        continue;

                /* special case keys of the instance config of an object */
                if (appending_object) {
                        gboolean handled;

                        if (!panel_layout_maybe_append_object_instance_config (
                                                        keyfile,
                                                        group,
                                                        keyfile_keys[i],
                                                        path_prefix,
                                                        unique_id,
                                                        dry_run,
                                                        &handled,
                                                        error))
                                goto out;

                        if (handled)
                                continue;
                }

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
                        g_strfreev (keyfile_keys);
                        g_object_unref (settings);
                        g_free (unique_id);
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
                panel_gsettings_append_strv (self->settings,
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
panel_layout_append_group (PanelLayout  *self,
                           GKeyFile     *keyfile,
                           const char   *group,
                           gboolean      dry_run,
                           GError      **error)
{
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        if (g_strcmp0 (group, "Toplevel") == 0 ||
            g_str_has_prefix (group, "Toplevel "))
                return panel_layout_append_group_helper (
                                        self,
                                        keyfile, group,
                                        "Toplevel",
                                        PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY,
                                        PANEL_TOPLEVEL_SCHEMA,
                                        PANEL_LAYOUT_TOPLEVEL_PATH,
                                        panel_layout_toplevel_keys,
                                        G_N_ELEMENTS (panel_layout_toplevel_keys),
                                        dry_run, error, "toplevel");
        else if (g_strcmp0 (group, "Object") == 0 ||
                 g_str_has_prefix (group, "Object "))
                return panel_layout_append_group_helper (
                                        self,
                                        keyfile, group,
                                        "Object",
                                        PANEL_LAYOUT_OBJECT_ID_LIST_KEY,
                                        PANEL_OBJECT_SCHEMA,
                                        PANEL_LAYOUT_OBJECT_PATH,
                                        panel_layout_object_keys,
                                        G_N_ELEMENTS (panel_layout_object_keys),
                                        dry_run, error, "object");

        g_set_error (error, PANEL_LAYOUT_ERROR, 0,
                     "Unknown group '%s'", group);

        return FALSE;
}

static const char *
get_id_from_group (const char *group,
                   const char *prefix)
{
  const char *id;

  /* Try to extract an id from the group, by stripping the prefix,
   * and create a unique id out of that
   */

  id = group + strlen (prefix);

  while (g_ascii_isspace (*id))
    id++;

  if (*id == '\0')
    return NULL;

  return id;
}

static void
update_object_toplevel_ids (GKeyFile   *keyfile,
                            const char *old_id,
                            const char *new_id)
{
  char **groups;
  int i;

  if (old_id == NULL)
    old_id = "toplevel";

  groups = g_key_file_get_groups (keyfile, NULL);

  for (i = 0; groups[i] != NULL; i++)
    {
      char *str;

      if (g_strcmp0 (groups[i], "Object") != 0 &&
          !g_str_has_prefix (groups[i], "Object "))
        continue;

      str = g_key_file_get_string (keyfile, groups[i], "toplevel-id", NULL);

      if (g_strcmp0 (str, old_id) == 0)
        g_key_file_set_string (keyfile, groups[i], "toplevel-id", new_id);

      g_free (str);
    }

  g_strfreev (groups);
}

static void
ensure_unique_ids (PanelLayout *self,
                   GKeyFile    *keyfile)
{
  char **groups;
  int i;

  groups = g_key_file_get_groups (keyfile, NULL);

  for (i = 0; groups[i] != NULL; i++)
    {
      const char *group;
      char *unique_id;
      const char *id;

      group = groups[i];
      unique_id = NULL;
      id = NULL;

      if (g_strcmp0 (group, "Toplevel") == 0 ||
          g_str_has_prefix (group, "Toplevel "))
        {
          id = get_id_from_group (group, "Toplevel");
          unique_id = panel_layout_find_free_id (self,
                                                 PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY,
                                                 PANEL_TOPLEVEL_SCHEMA,
                                                 PANEL_LAYOUT_TOPLEVEL_PATH,
                                                 id);

          update_object_toplevel_ids (keyfile, id, unique_id);
        }
      else if (g_strcmp0 (group, "Object") == 0 ||
               g_str_has_prefix (group, "Object "))
        {
          id = get_id_from_group (group, "Object");
          unique_id = panel_layout_find_free_id (self,
                                                 PANEL_LAYOUT_OBJECT_ID_LIST_KEY,
                                                 PANEL_OBJECT_SCHEMA,
                                                 PANEL_LAYOUT_OBJECT_PATH,
                                                 id);
        }

      if (unique_id != NULL)
        {
          g_key_file_set_string (keyfile, group, "unique-id", unique_id);
          g_free (unique_id);
        }
    }

  g_strfreev (groups);
}

static gboolean
panel_layout_append_from_file (PanelLayout  *self,
                               const char   *layout_file,
                               GError      **error)
{
        GError    *local_error = NULL;
        GKeyFile  *keyfile = NULL;
        char     **groups = NULL;
        gboolean   found_one = FALSE;
        int        i;
        gboolean ret;

        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        keyfile = g_key_file_new ();

        local_error = NULL;
        if (!g_key_file_load_from_file (keyfile, layout_file,
                                        G_KEY_FILE_NONE, &local_error))
                goto out;

        ensure_unique_ids (self, keyfile);

        groups = g_key_file_get_groups (keyfile, NULL);

        /* First pass to validate: we don't want to add only a subset of the
         * layout; the whole layout has to be valid */
        for (i = 0; groups[i] != NULL; i++) {
                if (!panel_layout_append_group (self,
                                                keyfile,
                                                groups[i],
                                                TRUE,
                                                &local_error))
                        goto out;
                else
                        found_one = TRUE;
        }

        if (!found_one) {
                local_error = g_error_new (PANEL_LAYOUT_ERROR, 0,
                                           "No defined toplevel or object");
                goto out;
        }

        /* Second pass to really add the layout. We know there'll be no error
         * since the first pass worked. */
        for (i = 0; groups[i] != NULL; i++)
                panel_layout_append_group (self,
                                           keyfile,
                                           groups[i],
                                           FALSE,
                                           NULL);

out:
        ret = TRUE;

        if (local_error != NULL) {
                g_set_error (error,
                             PANEL_LAYOUT_ERROR,
                             0,
                             "Error while parsing default layout from '%s': %s\n",
                             layout_file,
                             local_error->message);

                g_error_free (local_error);

                ret = FALSE;
        }

        if (groups)
                g_strfreev (groups);

        g_key_file_free (keyfile);

        return ret;
}


/***********************\
 * New toplevel/object *
\***********************/


void
panel_layout_toplevel_create (PanelLayout *self,
                              GdkScreen   *screen)
{
        char             *unique_id;
        char             *path;
        GSettings        *settings;
	PanelOrientation  orientation;
	int               monitor;

        unique_id = panel_layout_find_free_id (self,
                                               PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY,
                                               PANEL_TOPLEVEL_SCHEMA,
                                               PANEL_LAYOUT_TOPLEVEL_PATH,
                                               NULL);

        path = g_strdup_printf ("%s%s/", PANEL_LAYOUT_TOPLEVEL_PATH, unique_id);
        settings = g_settings_new_with_path (PANEL_TOPLEVEL_SCHEMA, path);
        g_free (path);

        if (panel_toplevel_find_empty_spot (self->application,
                                            screen,
                                            &orientation,
                                            &monitor)) {
                g_settings_set_enum (settings,
                                     PANEL_TOPLEVEL_ORIENTATION_KEY,
                                     orientation);
                g_settings_set_int (settings,
                                    PANEL_TOPLEVEL_MONITOR_KEY,
                                    monitor);
        }

        g_object_unref (settings);

        panel_gsettings_append_strv (self->settings,
                                     PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY,
                                     unique_id);

        g_free (unique_id);
}

void
panel_layout_object_create (PanelLayout         *self,
                            const char          *module_id,
                            const char          *applet_id,
                            const char          *toplevel_id,
                            PanelObjectPackType  pack_type,
                            int                  pack_index,
                            GVariant            *initial_settings)
{
        char *id;

        id = panel_layout_object_create_start (self,
                                               module_id,
                                               applet_id,
                                               toplevel_id,
                                               pack_type,
                                               pack_index,
                                               initial_settings);

        if (!id)
                return;

        panel_layout_object_create_finish (self, id);

        g_free (id);
}



char *
panel_layout_object_create_start (PanelLayout          *self,
                                  const char           *module_id,
                                  const char           *applet_id,
                                  const char           *toplevel_id,
                                  PanelObjectPackType   pack_type,
                                  int                   pack_index,
                                  GVariant             *initial_settings)
{
        char      *unique_id;
        char      *path;
        GSettings *settings_object;

        if (module_id == NULL || applet_id == NULL)
                return NULL;

        unique_id = panel_layout_find_free_id (self,
                                               PANEL_LAYOUT_OBJECT_ID_LIST_KEY,
                                               PANEL_OBJECT_SCHEMA,
                                               PANEL_LAYOUT_OBJECT_PATH,
                                               applet_id);

        path = g_strdup_printf ("%s%s/", PANEL_LAYOUT_OBJECT_PATH, unique_id);
        settings_object = g_settings_new_with_path (PANEL_OBJECT_SCHEMA, path);
        g_free (path);

        g_settings_set_string (settings_object,
                               PANEL_OBJECT_MODULE_ID_KEY,
                               module_id);
        g_settings_set_string (settings_object,
                               PANEL_OBJECT_APPLET_ID_KEY,
                               applet_id);
        g_settings_set_string (settings_object,
                               PANEL_OBJECT_TOPLEVEL_ID_KEY,
                               toplevel_id);
        g_settings_set_enum (settings_object,
                             PANEL_OBJECT_PACK_TYPE_KEY,
                             pack_type);
        g_settings_set_int (settings_object,
                            PANEL_OBJECT_PACK_INDEX_KEY,
                            pack_index);

        if (initial_settings != NULL) {
                GSettings *tmp;

                path = g_strdup_printf ("%s%s/initial-settings/", PANEL_LAYOUT_OBJECT_PATH, unique_id);
                tmp = g_settings_new_with_path ("org.gnome.gnome-panel.applet.initial-settings", path);
                g_free (path);

                g_settings_set_value (tmp, "settings", initial_settings);
                g_object_unref (tmp);
        }

        g_object_unref (settings_object);

        return unique_id;
}

void
panel_layout_object_create_finish (PanelLayout *self,
                                   const char  *object_id)
{
        panel_gsettings_append_strv (self->settings,
                                     PANEL_LAYOUT_OBJECT_ID_LIST_KEY,
                                     object_id);
}


/*******************\
 * Changing layout *
\*******************/


gboolean
panel_layout_is_writable (PanelLayout *self)
{
        return (g_settings_is_writable (self->settings,
                                        PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY) &&
                g_settings_is_writable (self->settings,
                                        PANEL_LAYOUT_OBJECT_ID_LIST_KEY));
}

void
panel_layout_delete_toplevel (PanelLayout *self,
                              const char  *toplevel_id)
{
        char  *path;
        char  *id_copy;
        char **objects;
        int    i;

        if (PANEL_GLIB_STR_EMPTY (toplevel_id))
                return;

        /* The original will be freed if removal succeeds */
        id_copy = g_strdup (toplevel_id);

        if (!panel_gsettings_remove_all_from_strv (self->settings,
                                                   PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY,
                                                   id_copy)) {
                g_free (id_copy);
                return;
        }

        path = g_strdup_printf ("%s%s/",
                                PANEL_LAYOUT_TOPLEVEL_PATH, id_copy);
        panel_dconf_recursive_reset (path, NULL);

        /* remove all applets that were on this toplevel */

        objects = g_settings_get_strv (self->settings,
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
                        panel_layout_delete_object (self, objects[i]);

                g_free (object_toplevel_id);
        }

        g_strfreev (objects);

        g_free (id_copy);
}

void
panel_layout_delete_object (PanelLayout *self,
                            const char  *object_id)
{
        char *path;
        char *id_copy;

        if (PANEL_GLIB_STR_EMPTY (object_id))
                return;

        /* The original will be freed if removal succeeds */
        id_copy = g_strdup (object_id);

        if (!panel_gsettings_remove_all_from_strv (self->settings,
                                                   PANEL_LAYOUT_OBJECT_ID_LIST_KEY,
                                                   id_copy)) {
                g_free (id_copy);
                return;
        }

        path = g_strdup_printf ("%s%s/",
                                PANEL_LAYOUT_OBJECT_PATH, id_copy);
        panel_dconf_recursive_reset (path, NULL);
        g_free (path);

        g_free (id_copy);
}

static void
panel_layout_changed_toplevel (PanelLayout *self)
{
        char **ids;
        GList *toplevels;
        GList *to_remove;
        GList *l;
        gboolean loading;
        int i;

        ids = g_settings_get_strv (self->settings,
                                   PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY);

        /* Remove what is not in the layout anymore */

        toplevels = gp_application_get_toplevels (self->application);
        to_remove = NULL;

        for (l = toplevels; l != NULL; l = l->next) {
                gboolean found;
                const char *id;

                found = FALSE;
                id = panel_toplevel_get_id (l->data);

                for (i = 0; ids[i] != NULL; i++) {
                        if (g_strcmp0 (ids[i], id) == 0) {
                                found = TRUE;
                                break;
                        }
                }

                if (!found)
                        to_remove = g_list_prepend (to_remove, l->data);
        }

        g_list_free (toplevels);

        for (l = to_remove; l != NULL; l = l->next) {
                gp_application_remove_toplevel (self->application,
                                                PANEL_TOPLEVEL (l->data));
        }

        g_list_free (to_remove);

        /* Add what appeared in the layout */

        loading = FALSE;

        for (i = 0; ids[i] != NULL; i++) {
                gboolean found;

                toplevels = gp_application_get_toplevels (self->application);
                found = FALSE;

                for (l = toplevels; l != NULL; l = l->next) {
                        const char *id;

                        id = panel_toplevel_get_id (l->data);
                        if (g_strcmp0 (ids[i], id) == 0) {
                                found = TRUE;
                                break;
                        }
                }

                g_list_free (toplevels);

                if (!found) {
                        panel_layout_load_toplevel (self, ids[i]);
                        loading = TRUE;
                }
        }

        g_strfreev (ids);

        /* Reload list of objects to get those that might be on the new
         * toplevels */
        if (loading)
                panel_layout_changed_object (self);
}

static void
panel_layout_changed_object (PanelLayout *self)
{
        char       **ids;
        GSList      *to_remove;
        gboolean     found;
        const char  *id;
        GSList      *l;
        int          i;

        ids = g_settings_get_strv (self->settings,
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
                        panel_layout_load_object (self, ids[i]);
        }

        g_strfreev (ids);

        /* Always do this, even if there is no object that got loaded: if a
         * panel has been created, we want a do_load() to unhide it, even if
         * there is no object to load */
        panel_object_loader_do_load (self->application, FALSE);
}

static void
panel_layout_changed (GSettings *settings,
                      char      *key,
                      gpointer   user_data)
{
        PanelLayout *self;

        self = PANEL_LAYOUT (user_data);

        if (g_strcmp0 (key, PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY) == 0)
                panel_layout_changed_toplevel (self);
        else if (g_strcmp0 (key, PANEL_LAYOUT_OBJECT_ID_LIST_KEY) == 0)
                panel_layout_changed_object (self);
}

/******************\
 * Loading layout *
\******************/


static void
panel_layout_load_toplevel (PanelLayout *self,
                            const char  *toplevel_id)
{
        PanelToplevel *toplevel;
        char          *path;

        if (PANEL_GLIB_STR_EMPTY (toplevel_id))
                return;

        path = g_strdup_printf ("%s%s/",
                                PANEL_LAYOUT_TOPLEVEL_PATH, toplevel_id);

        toplevel = g_object_new (PANEL_TYPE_TOPLEVEL,
                                 "app", self->application,
                                 "decorated", FALSE,
                                 "settings-path", path,
                                 "toplevel-id", toplevel_id,
                                 "type-hint", GDK_WINDOW_TYPE_HINT_DOCK,
                                 NULL);

        gp_application_add_toplevel (self->application, toplevel);

        g_free (path);

        /* FIXME: we shouldn't have to do this manually */
        panel_setup (toplevel);

        gtk_widget_show (GTK_WIDGET (toplevel));
}

static void
panel_layout_load_object (PanelLayout *self,
                          const char  *object_id)
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
panel_layout_get_default_layout_file (PanelLayout *self)
{
        GSettings *settings;
        char *default_layout;
        char *layout_name;
        char *filename;

        settings = gp_application_get_general_settings (self->application);
        default_layout = g_settings_get_string (settings, "default-layout");

        layout_name = g_strdup_printf ("%s.layout", default_layout);
        g_free (default_layout);

        filename = g_build_filename (g_get_user_config_dir (),
                                     "gnome-panel",
                                     "layouts",
                                     layout_name,
                                     NULL);

        if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                g_free (layout_name);
                return filename;
        }

        g_free (filename);
        filename = g_build_filename (LAYOUTSDIR,
                                     layout_name,
                                     NULL);

        if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                g_free (layout_name);
                return filename;
        }

        g_free (layout_name);
        g_free (filename);

        return g_build_filename (LAYOUTSDIR,
                                 DEFAULT_LAYOUT_FILE,
                                 NULL);
}

static char **
panel_layout_load_default (PanelLayout  *self,
                           GError      **error)
{
  char *default_layout_file;
  char **toplevels;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!g_settings_is_writable (self->settings,
                               PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY) ||
      !g_settings_is_writable (self->settings,
                               PANEL_LAYOUT_OBJECT_ID_LIST_KEY))
    {
      g_set_error_literal (error,
                           PANEL_LAYOUT_ERROR,
                           0,
                           _("Cannot create initial panel layout"));

      return NULL;
    }

  default_layout_file = panel_layout_get_default_layout_file (self);

  if (!panel_layout_append_from_file (self, default_layout_file, error))
    {
      g_free (default_layout_file);
      return NULL;
    }

  g_free (default_layout_file);

  toplevels = g_settings_get_strv (self->settings,
                                   PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY);

  if (!toplevels[0])
    {
      g_strfreev (toplevels);

      g_set_error_literal (error,
                           PANEL_LAYOUT_ERROR,
                           0,
                           _("Cannot create initial panel layout"));

      return NULL;
    }

  return toplevels;
}

static void
panel_layout_class_init (PanelLayoutClass *self_class)
{
}

static void
panel_layout_init (PanelLayout *self)
{
  self->settings = g_settings_new (PANEL_LAYOUT_SCHEMA);
}

PanelLayout *
panel_layout_new (GpApplication *application)
{
  PanelLayout *layout;

  layout = g_object_new (PANEL_TYPE_LAYOUT, NULL);
  layout->application = application;

  return layout;
}

gboolean
panel_layout_load (PanelLayout  *self,
                   GError      **error)
{
        char **toplevels;
        char **objects;
        int    i;

        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        toplevels = g_settings_get_strv (self->settings,
                                         PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY);

        if (!toplevels[0])
          {
            g_strfreev (toplevels);

            toplevels = panel_layout_load_default (self, error);

            if (!toplevels)
              return FALSE;
          }

        for (i = 0; toplevels[i] != NULL; i++)
                panel_layout_load_toplevel (self, toplevels[i]);

        g_strfreev (toplevels);

        objects = g_settings_get_strv (self->settings,
                                       PANEL_LAYOUT_OBJECT_ID_LIST_KEY);

        for (i = 0; objects[i] != NULL; i++)
                panel_layout_load_object (self, objects[i]);

        g_strfreev (objects);

        g_signal_connect (self->settings,
                          "changed",
                          G_CALLBACK (panel_layout_changed),
                          self);

        panel_object_loader_do_load (self->application, TRUE);

        return TRUE;
}
