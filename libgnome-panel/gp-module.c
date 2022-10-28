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
 *       gp_applet_info_set_help_uri (info, "help:example");
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

/**
 * GpModule:
 *
 * #GpModule is an opaque data structure and can only be accessed using
 * the following functions.
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <stdarg.h>

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

  GpActionFlags            actions;
  GpActionFunc             action_func;

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
check_abi (GpModule  *module,
           GError   **error)
{
  if (module->abi_version != GP_MODULE_ABI_VERSION)
    {
      g_set_error (error,
                   GP_MODULE_ERROR,
                   GP_MODULE_ERROR_ABI_DOES_NOT_MATCH,
                   "Module “%s” ABI version does not match",
                   module->id);

      return FALSE;
    }

  return TRUE;
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

  if (module->id == NULL || *module->id == '\0')
    {
      g_warning ("Module '%s' does not have valid id", module->path);
      return NULL;
    }

  if (module->applet_ids == NULL)
    module->applet_ids = g_new0 (char *, 1);

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
 * gp_module_set_applet_ids:
 * @module: a #GpModuleInfo
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

  if (!check_abi (module, error))
    return NULL;

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
  if (!check_abi (module, NULL))
    return NULL;

  if (module->compatibility_func == NULL)
    return NULL;

  return module->compatibility_func (old_iid);
}

void
gp_module_set_actions (GpModule      *self,
                       GpActionFlags  actions,
                       GpActionFunc   func)
{
  self->actions = actions;
  self->action_func = func;
}

GpActionFlags
gp_module_get_actions (GpModule *self)
{
  if (!check_abi (self, NULL))
    return GP_ACTION_NONE;

  return self->actions;
}

gboolean
gp_module_handle_action (GpModule      *self,
                         GpActionFlags  action,
                         uint32_t       time)
{
  g_return_val_if_fail ((self->actions & action) == action, FALSE);
  g_return_val_if_fail (self->action_func != NULL, FALSE);

  if (!check_abi (self, NULL))
    return FALSE;

  return self->action_func (self, action, time);
}

/**
 * gp_module_applet_new:
 * @module: a #GpModule
 * @applet_id: the applet id
 * @settings_path: the #GSettings path to the per-instance settings
 * @initial_settings: initial settings
 * @error: return location for a #GError, or %NULL
 *
 * Returns a newly allocated applet.
 *
 * Returns: (transfer full): a newly created #GpApplet, or %NULL.
 */
GpApplet *
gp_module_applet_new (GpModule     *module,
                      const gchar  *applet_id,
                      const gchar  *settings_path,
                      GVariant     *initial_settings,
                      GError      **error)
{
  GpAppletInfo *info;
  GType type;
  GpApplet *applet;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!check_abi (module, error))
    return NULL;

  if (!is_valid_applet (module, applet_id, error))
    return NULL;

  info = get_applet_info (module, applet_id, error);
  if (info == NULL)
    return NULL;

  type = info->get_applet_type_func ();
  if (type == G_TYPE_NONE)
    {
      g_set_error (error, GP_MODULE_ERROR, GP_MODULE_ERROR_MISSING_APPLET_INFO,
                   "Module '%s' did not return required info about applet '%s'",
                   module->id, applet_id);

      return NULL;
    }

  applet = g_object_new (type,
                         "module", module,
                         "id", applet_id,
                         "settings-path", settings_path,
                         "gettext-domain", module->gettext_domain,
                         NULL);

  if (initial_settings != NULL)
    {
      if (!GP_APPLET_GET_CLASS (applet)->initial_setup (applet,
                                                        initial_settings,
                                                        error))
        {
          g_object_ref_sink (applet);
          g_object_unref (applet);
          return NULL;
        }
    }

  if (!g_initable_init (G_INITABLE (applet), NULL, error))
    {
      g_object_ref_sink (applet);
      g_object_unref (applet);
      return NULL;
    }

  return applet;
}

GtkWidget *
gp_module_create_about_dialog (GpModule   *module,
                               GtkWindow  *parent,
                               const char *applet)
{
  GpAppletInfo *info;
  GtkAboutDialog *dialog;

  if (!check_abi (module, NULL))
    return NULL;

  info = get_applet_info (module, applet, NULL);
  g_assert (info != NULL);

  if (info->about_dialog_func == NULL)
    return NULL;

  dialog = GTK_ABOUT_DIALOG (gtk_about_dialog_new ());

  gtk_about_dialog_set_program_name (dialog, info->name);
  gtk_about_dialog_set_comments (dialog, info->description);
  gtk_about_dialog_set_logo_icon_name (dialog, info->icon_name);
  gtk_about_dialog_set_version (dialog, module->version);
  info->about_dialog_func (dialog);

  return GTK_WIDGET (dialog);
}

void
gp_module_show_help (GpModule   *module,
                     GtkWindow  *parent,
                     const char *applet,
                     const char *page)
{
  GpAppletInfo *info;
  char *help_uri;
  guint32 timestamp;
  GError *error;
  char *message;
  GtkWidget *dialog;

  if (!check_abi (module, NULL))
    return;

  info = get_applet_info (module, applet, NULL);
  g_assert (info != NULL);

  if (info->help_uri == NULL || *info->help_uri == '\0')
    return;

  if (page != NULL && *page != '\0')
    help_uri = g_strdup_printf ("%s/%s", info->help_uri, page);
  else
    help_uri = g_strdup (info->help_uri);

  timestamp = gtk_get_current_event_time ();

  error = NULL;
  gtk_show_uri_on_window (parent, help_uri, timestamp, &error);

  if (error == NULL)
    {
      g_free (help_uri);
      return;
    }

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      g_free (help_uri);
      return;
    }

  message = g_markup_printf_escaped (_("Could not display help document '%s'"),
                                     help_uri);

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s",
                                   message);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "%s",
                                            error->message);

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_window_set_title (GTK_WINDOW (dialog),
                        _("Error displaying help document"));

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_widget_show (dialog);

  g_error_free (error);

  g_free (help_uri);
  g_free (message);
}

gboolean
gp_module_is_applet_disabled (GpModule         *module,
                              const char       *applet,
                              const char       *backend,
                              GpLockdownFlags   lockdowns,
                              char            **reason)
{
  GpAppletInfo *info;
  char *local_reason;
  gboolean is_disabled;

  g_return_val_if_fail (reason == NULL || *reason == NULL, FALSE);

  if (!check_abi (module, NULL))
    return FALSE;

  info = get_applet_info (module, applet, NULL);
  g_assert (info != NULL);

  if (info->is_disabled_func == NULL)
    return FALSE;

  if (info->backends != NULL)
    {
      char **backends;

      backends = g_strsplit (info->backends, ",", -1);

      if (!g_strv_contains ((const char * const *) backends, backend))
        {
          if (reason != NULL)
            {
              *reason = g_strdup_printf (_("Backend “%s” is not supported."),
                                         backend);
            }

          g_strfreev (backends);
          return TRUE;
        }

      g_strfreev (backends);
    }

  local_reason = NULL;
  is_disabled = info->is_disabled_func (lockdowns, &local_reason);

  if (reason != NULL)
    *reason = local_reason;
  else
    g_free (local_reason);

  return is_disabled;
}
