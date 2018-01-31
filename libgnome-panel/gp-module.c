/*
 * Copyright (C) 2016-2018 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

/**
 * SECTION: gp-module
 * @title: Module
 * @short_description: a module with one or more applets
 * @include: libngome-panel/gp-module.h
 *
 * A module with one or more applets.
 *
 * |[<!-- language="C" -->
 * static gboolean
 * example1_setup_about (GtkAboutDialog *dialog)
 * {
 *   gtk_about_dialog_set_comments (about, "...");
 *   gtk_about_dialog_set_copyright (about, "...");
 *   // ...
 * }
 *
 * static GpAppletInfo *
 * example_get_applet_info (const gchar *id)
 * {
 *   GpAppletInfo *info;
 *
 *   if (g_strcmp0 (id, "example1") == 0)
 *     {
 *       info = gp_applet_info_new (example1_get_type,
 *                                  _("Example 1 name"),
 *                                  _("Example 1 description"),
 *                                  "example1-icon");
 *
 *       gp_applet_info_set_about_dialog (info, example1_setup_about);
 *       gp_applet_info_set_help_uri (info, "help:example/example1");
 *     }
 *   else if (g_strcmp0 (id, "example2") == 0)
 *     {
 *       info = gp_applet_info_new (example2_get_type,
 *                                  _("Example 2 name"),
 *                                  _("Example 2 description"),
 *                                  "example2-icon");
 *
 *       gp_applet_info_set_backends (info, "x11");
 *     }
 *   else
 *     {
 *       info = NULL;
 *     }
 *
 *   return info;
 * }
 *
 * static const gchar *
 * example_get_applet_id_from_iid (const gchar *iid)
 * {
 *   if (g_strcmp0 (iid, "ExampleAppletFactory::Example1Applet") == 0)
 *     {
 *       return "example1";
 *     }
 *   else if (g_strcmp0 (iid, "ExampleAppletFactory::Example2Applet") == 0)
 *     {
 *       return "example2";
 *     }
 *
 *   return NULL;
 * }
 *
 * void
 * gp_module_load (GpModule *module)
 * {
 *   bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
 *   bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
 *   gp_module_set_gettext_domain (module, GETTEXT_PACKAGE);
 *
 *   gp_module_set_abi_version (module, GP_MODULE_ABI_VERSION);
 *
 *   gp_module_set_id (module, "org.example.example");
 *   gp_module_set_version (module, PACKAGE_VERSION);
 *
 *   gp_module_set_applet_ids (module, "example1", "example2", NULL);
 *
 *   gp_module_set_get_applet_info (module, example_get_applet_info);
 *   gp_module_set_compatibility (module, example_get_applet_id_from_iid);
 * }
 * ]|
 */

#include "config.h"

#include <gtk/gtk.h>
#include <stdarg.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "gp-applet-info-private.h"
#include "gp-module-private.h"

typedef void (* LoadFunc) (GpModule *module);

struct _GpModule
{
  GObject                  parent;

  gchar                   *path;
  GModule                 *library;

  guint32                  abi_version;

  gchar                   *id;
  gchar                   *version;

  gchar                   *gettext_domain;

  gchar                  **applet_ids;

  GpGetAppletInfoFunc      get_applet_info_func;

  GetAppletIdFromIidFunc   compatibility_func;

  GHashTable              *applets;
};

G_DEFINE_TYPE (GpModule, gp_module, G_TYPE_OBJECT)

static gchar **
get_applets (va_list args)
{
  GPtrArray *array;
  const gchar *applet;

  array = g_ptr_array_new ();
  while ((applet = va_arg (args, const gchar *)) != NULL)
    g_ptr_array_add (array, g_strdup (applet));

  g_ptr_array_add (array, NULL);

  return (gchar **) g_ptr_array_free (array, FALSE);
}

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

  for (i = 0; module->applet_ids[i] != NULL; i++)
    {
      if (g_strcmp0 (module->applet_ids[i], applet) == 0)
        return TRUE;
    }

  g_set_error (error, GP_MODULE_ERROR, GP_MODULE_ERROR_APPLET_DOES_NOT_EXIST,
               "Module '%s' does not have applet '%s'",
               module->id, applet);

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

  info = module->get_applet_info_func (applet);

  if (info == NULL)
    {
      g_set_error (error, GP_MODULE_ERROR, GP_MODULE_ERROR_MISSING_APPLET_INFO,
                   "Module '%s' did not return required info about applet '%s'",
                   module->id, applet);

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

  g_clear_pointer (&module->id, g_free);
  g_clear_pointer (&module->version, g_free);
  g_clear_pointer (&module->gettext_domain, g_free);
  g_clear_pointer (&module->applet_ids, g_strfreev);
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
  LoadFunc load_func;

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

  symbol = "gp_module_load";
  if (!g_module_symbol (module->library, symbol, (gpointer) &load_func))
    {
      g_warning ("Failed to get '%s' for module '%s': %s",
                 symbol, path, g_module_error ());

      g_object_unref (module);
      return NULL;
    }

  if (load_func == NULL)
    {
      g_warning ("Invalid '%s' in module '%s'", symbol, path);

      g_object_unref (module);
      return NULL;
    }

  load_func (module);

  if (module->abi_version != GP_MODULE_ABI_VERSION)
    {
      g_warning ("Module '%s' ABI version does not match", path);

      g_object_unref (module);
      return NULL;
    }

  if (module->id == NULL || *module->id == '\0')
    {
      g_warning ("Module '%s' does not have valid id", module->path);
      return NULL;
    }

  if (module->applet_ids == NULL || module->applet_ids[0] == NULL)
    {
      g_warning ("Module '%s' does not have valid applets", module->path);
      return NULL;
    }

  return module;
}

/**
 * gp_module_set_abi_version:
 * @module: a #GpModule
 * @abi_version: %GP_MODULE_ABI_VERSION
 *
 * This function must be called from gp_module_load().
 */
void
gp_module_set_abi_version (GpModule *module,
                           guint32   abi_version)
{
  module->abi_version = abi_version;
}

/**
 * gp_module_set_gettext_domain:
 * @module: a #GpModule
 * @gettext_domain: the gettext domain
 *
 * Sets the gettext domain for this module.
 */
void
gp_module_set_gettext_domain (GpModule    *module,
                              const gchar *gettext_domain)
{
  g_clear_pointer (&module->gettext_domain, g_free);
  module->gettext_domain = g_strdup (gettext_domain);
}

/**
 * gp_module_set_id:
 * @module: a #GpModule
 * @id: the id of this module
 *
 * The module @id must be globally unique. For this reason, it is very
 * strongly recommended to use reverse domain style identifier:
 * https://wiki.gnome.org/HowDoI/ChooseApplicationID
 */
void
gp_module_set_id (GpModule    *module,
                  const gchar *id)
{
  g_clear_pointer (&module->id, g_free);
  module->id = g_strdup (id);
}

const gchar *
gp_module_get_id (GpModule *module)
{
  return module->id;
}

/**
 * gp_module_set_version:
 * @module: a #GpModule
 * @version: the version of this module
 *
 * Sets the version of this module.
 */
void
gp_module_set_version (GpModule    *module,
                       const gchar *version)
{
  g_clear_pointer (&module->version, g_free);
  module->version = g_strdup (version);
}

/**
 * gp_module_info_set_applets:
 * @info: a #GpModuleInfo
 * @...: a %NULL-terminated list of applet ids in this module
 *
 * Sets the applets available in this module.
 */
void
gp_module_set_applet_ids (GpModule *module,
                          ...)
{
  va_list args;

  va_start (args, module);
  g_strfreev (module->applet_ids);
  module->applet_ids = get_applets (args);
  va_end (args);
}

const gchar *
gp_module_get_version (GpModule *module)
{
  return module->version;
}

const gchar * const *
gp_module_get_applets (GpModule *module)
{
  return (const gchar * const *) module->applet_ids;
}

/**
 * gp_module_set_get_applet_info:
 * @module: a #GpModule
 * @func: the function to call to get #GpAppletInfo
 *
 * Specifies a function to be used to get #GpAppletInfo.
 */
void
gp_module_set_get_applet_info (GpModule            *module,
                               GpGetAppletInfoFunc  func)
{
  module->get_applet_info_func = func;
}

/**
 * gp_module_get_applet_info:
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

/**
 * gp_module_set_compatibility:
 * @module: a #GpModule
 * @func: the function to call to convert applet iid to id
 *
 * Specifies a function to be used to convert old applet iid to id.
 *
 * The function must check if iid is known to module and only then return
 * new applet id.
 */
void
gp_module_set_compatibility (GpModule               *module,
                             GetAppletIdFromIidFunc  func)
{
  module->compatibility_func = func;
}

const gchar *
gp_module_get_applet_id_from_iid (GpModule    *module,
                                  const gchar *old_iid)
{
  if (module->compatibility_func == NULL)
    return NULL;

  return module->compatibility_func (old_iid);
}

/**
 * gp_module_applet_new:
 * @module: a #GpModule
 * @applet: the applet id
 * @settings_path: the #GSettings path to the per-instance settings
 * @error: return location for a #GError, or %NULL
 *
 * Returns a newly allocated applet.
 *
 * Returns: (transfer full): a newly created #GpApplet, or %NULL.
 */
GpApplet *
gp_module_applet_new (GpModule     *module,
                      const gchar  *applet,
                      const gchar  *settings_path,
                      GError      **error)
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
                   applet, module->id, get_current_backend ());

      return NULL;
    }

  type = info->get_applet_type_func ();
  if (type == G_TYPE_NONE)
    {
      g_set_error (error, GP_MODULE_ERROR, GP_MODULE_ERROR_MISSING_APPLET_INFO,
                   "Module '%s' did not return required info about applet '%s'",
                   module->id, applet);

      return NULL;
    }

  return g_object_new (type,
                       "id", applet,
                       "settings-path", settings_path,
                       "gettext-domain", module->gettext_domain,
                       NULL);
}
