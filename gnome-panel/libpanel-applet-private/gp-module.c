/*
 * Copyright (C) 2016 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "gp-module-private.h"
#include "libgnome-panel/gp-applet-info-private.h"
#include "libgnome-panel/gp-module-info-private.h"

typedef guint32        (* GetAbiVersionFunc)   (void);
typedef GpModuleInfo * (* GetModuleInfoFunc)   (void);
typedef void           (* GetAppletVTableFunc) (GpAppletVTable *vtable);

struct _GpModule
{
  GObject         parent;

  gchar          *path;
  GModule        *library;

  GpModuleInfo   *info;

  GpAppletVTable  applet_vtable;
  GHashTable     *applets;
};

G_DEFINE_TYPE (GpModule, gp_module, G_TYPE_OBJECT)

static gboolean
match_backend (GpAppletInfo *info)
{
  GdkDisplay *display;
  gchar **backends;
  gboolean match;
  guint i;

  if (info->backends == NULL)
    return TRUE;

  display = gdk_display_get_default ();
  backends = g_strsplit (info->backends, ",", -1);
  match = FALSE;

  for (i = 0; backends[i] != NULL; i++)
    {
      if (g_strcmp0 (backends[i], "*") == 0)
        {
          match = TRUE;
          break;
        }

#ifdef GDK_WINDOWING_WAYLAND
      if (g_strcmp0 (backends[i], "wayland") == 0 &&
          GDK_IS_WAYLAND_DISPLAY (display))
        {
          match = TRUE;
          break;
        }
#endif

#ifdef GDK_WINDOWING_X11
      if (g_strcmp0 (backends[i], "x11") == 0 && GDK_IS_X11_DISPLAY (display))
        {
          match = TRUE;
          break;
        }
#endif
    }

  g_strfreev (backends);

  return match;
}

static const gchar *
get_current_backend (void)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    return "wayland";
#endif

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    return "x11";
#endif

  return "unknown";
}

static gboolean
is_valid_applet (GpModule     *module,
                 const gchar  *applet,
                 GError      **error)
{
  guint i;

  for (i = 0; module->info->applets[i] != NULL; i++)
    {
      if (g_strcmp0 (module->info->applets[i], applet) == 0)
        return TRUE;
    }

  g_set_error (error, GP_MODULE_ERROR, GP_MODULE_ERROR_APPLET_DOES_NOT_EXIST,
               "Module '%s' does not have applet '%s'",
               module->info->id, applet);

  return FALSE;
}

static GpAppletInfo *
get_applet_info (GpModule     *module,
                 const gchar  *applet,
                 GError      **error)
{
  GpAppletInfo *info;

  info = g_hash_table_lookup (module->applets, applet);

  if (info != NULL)
    return info;

  info = module->applet_vtable.get_applet_info (applet);

  if (info == NULL)
    {
      g_set_error (error, GP_MODULE_ERROR, GP_MODULE_ERROR_MISSING_APPLET_INFO,
                   "Module '%s' did not return required info about applet '%s'",
                   module->info->id, applet);

      return NULL;
    }

  g_hash_table_insert (module->applets, g_strdup (applet), info);

  return info;
}

static void
applet_info_free (gpointer data)
{
  GpAppletInfo *info;

  info = (GpAppletInfo *) data;

  gp_applet_info_free (info);
}

static void
gp_module_finalize (GObject *object)
{
  GpModule *module;

  module = GP_MODULE (object);

  g_clear_pointer (&module->path, g_free);

  if (module->library != NULL)
    {
      g_module_close (module->library);
      module->library = NULL;
    }

  g_clear_pointer (&module->info, gp_module_info_free);
  g_clear_pointer (&module->applets, g_hash_table_destroy);

  G_OBJECT_CLASS (gp_module_parent_class)->finalize (object);
}

static void
gp_module_class_init (GpModuleClass *module_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (module_class);

  object_class->finalize = gp_module_finalize;
}

static void
gp_module_init (GpModule *module)
{
  module->applets = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, applet_info_free);
}

GQuark
gp_module_error_quark (void)
{
  return g_quark_from_static_string ("gp-module-error-quark");
}

GpModule *
gp_module_new_from_path (const gchar *path)
{
  GpModule *module;
  GModuleFlags flags;
  const gchar *symbol;
  GetAbiVersionFunc abi_version_func;
  GetModuleInfoFunc module_info_func;
  GetAppletVTableFunc applet_vtable_func;

  g_return_val_if_fail (path != NULL && *path != '\0', NULL);

  module = g_object_new (GP_TYPE_MODULE, NULL);

  flags = G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL;

  module->path = g_strdup (path);
  module->library = g_module_open (path, flags);

  if (module->library == NULL)
    {
      g_warning ("Failed to load module '%s': %s", path, g_module_error ());

      g_object_unref (module);
      return NULL;
    }

  symbol = "gp_module_get_abi_version";
  if (!g_module_symbol (module->library, symbol, (gpointer) &abi_version_func))
    {
      g_warning ("Failed to get '%s' for module '%s': %s",
                 symbol, path, g_module_error ());

      g_object_unref (module);
      return NULL;
    }

  if (abi_version_func == NULL)
    {
      g_warning ("Invalid '%s' in module '%s'", symbol, path);

      g_object_unref (module);
      return NULL;
    }

  if (abi_version_func () != GP_MODULE_ABI_VERSION)
    {
      g_warning ("Module '%s' ABI version does not match", path);

      g_object_unref (module);
      return NULL;
    }

  symbol = "gp_module_get_module_info";
  if (!g_module_symbol (module->library, symbol, (gpointer) &module_info_func))
    {
      g_warning ("Failed to get '%s' for module '%s': %s",
                 symbol, path, g_module_error ());

      g_object_unref (module);
      return NULL;
    }

  if (module_info_func == NULL)
    {
      g_warning ("Invalid '%s' in module '%s'", symbol, path);

      g_object_unref (module);
      return NULL;
    }

  module->info = module_info_func ();
  if (module->info == NULL)
    {
      g_warning ("Failed to get 'GpModuleInfo' from module '%s'", module->path);
      return NULL;
    }

  if (module->info->id == NULL || *module->info->id == '\0')
    {
      g_warning ("Module '%s' does not have valid id", module->path);
      return NULL;
    }

  if (module->info->applets == NULL || module->info->applets[0] == NULL)
    {
      g_warning ("Module '%s' does not have valid applets", module->path);
      return NULL;
    }

  symbol = "gp_module_get_applet_vtable";
  if (!g_module_symbol (module->library, symbol, (gpointer) &applet_vtable_func))
    {
      g_warning ("Failed to get '%s' for module '%s': %s",
                 symbol, path, g_module_error ());

      g_object_unref (module);
      return NULL;
    }

  if (applet_vtable_func == NULL)
    {
      g_warning ("Invalid '%s' in module '%s'", symbol, path);

      g_object_unref (module);
      return NULL;
    }

  applet_vtable_func (&module->applet_vtable);

  return module;
}

const gchar *
gp_module_get_id (GpModule *module)
{
  return module->info->id;
}

const gchar * const *
gp_module_get_applets (GpModule *module)
{
  return (const gchar * const *) module->info->applets;
}

/**
 * gp_module_applet_new:
 * @module: a #GpModule
 * @applet: the applet id
 * @error: return location for a #GError, or %NULL
 *
 * Returns the #GpAppletInfo for @applet in @module if exists, %NULL
 * otherwise.
 *
 * Returns: (transfer none): the #GpAppletInfo, or %NULL.
 */
GpAppletInfo *
gp_module_get_applet_info (GpModule     *module,
                           const gchar  *applet,
                           GError      **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!is_valid_applet (module, applet, error))
    return NULL;

  return get_applet_info (module, applet, error);
}

const gchar *
gp_module_get_applet_from_iid (GpModule    *module,
                               const gchar *old_iid)
{
  if (module->applet_vtable.get_applet_from_iid == NULL)
    return NULL;

  return module->applet_vtable.get_applet_from_iid (old_iid);
}

/**
 * gp_module_applet_new:
 * @module: a #GpModule
 * @applet: the applet id
 * @settings_path: the #GSettings path to the per-instance settings
 * @locked_down: whether applet is on locked down panel
 * @orientation: the orientation of the panel
 * @position: the position of the panel
 * @error: return location for a #GError, or %NULL
 *
 * Returns a newly allocated applet.
 *
 * Returns: (transfer full): a newly created #GpApplet, or %NULL.
 */
GpApplet *
gp_module_applet_new (GpModule         *module,
                      const gchar      *applet,
                      const gchar      *settings_path,
                      gboolean          locked_down,
                      GtkOrientation    orientation,
                      GtkPositionType   position,
                      GError          **error)
{
  GpAppletInfo *info;
  GType type;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!is_valid_applet (module, applet, error))
    return NULL;

  info = get_applet_info (module, applet, error);
  if (info == NULL)
    return NULL;

  if (!match_backend (info))
    {
      g_set_error (error, GP_MODULE_ERROR, GP_MODULE_ERROR_MISSING_APPLET_TYPE,
                   "Applet '%s' from module '%s' does not work with current backend '%s'",
                   applet, module->info->id, get_current_backend ());

      return NULL;
    }

  type = module->applet_vtable.get_applet_type (applet);
  if (type == G_TYPE_NONE)
    {
      g_set_error (error, GP_MODULE_ERROR, GP_MODULE_ERROR_MISSING_APPLET_INFO,
                   "Module '%s' did not return required info about applet '%s'",
                   module->info->id, applet);

      return NULL;
    }

  return g_object_new (type,
                       "id", applet,
                       "settings-path", settings_path,
                       "translation-domain", module->info->translation_domain,
                       "locked-down", locked_down,
                       "orientation", orientation,
                       "position", position,
                       NULL);
}
