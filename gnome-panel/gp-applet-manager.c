/*
 * Copyright (C) 2016-2020 Alberts Muktupāvels
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "gp-applet-manager.h"
#include "gp-module-manager.h"
#include "libgnome-panel/gp-applet-info-private.h"
#include "libgnome-panel/gp-initial-setup-dialog-private.h"
#include "libgnome-panel/gp-module-private.h"
#include "panel-applet-frame.h"
#include "panel-lockdown.h"

struct _GpAppletManager
{
  GObject          parent;

  char            *backend;

  GpApplication   *application;
  GpModuleManager *manager;
};

G_DEFINE_TYPE (GpAppletManager, gp_applet_manager, G_TYPE_OBJECT)

static char *
get_current_backend (void)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    return g_strdup ("wayland");
#endif

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    return g_strdup ("x11");
#endif

  return g_strdup ("unknown");
}

static void
gp_applet_manager_finalize (GObject *object)
{
  GpAppletManager *self;

  self = GP_APPLET_MANAGER (object);

  g_clear_pointer (&self->backend, g_free);

  G_OBJECT_CLASS (gp_applet_manager_parent_class)->finalize (object);
}

static void
gp_applet_manager_class_init (GpAppletManagerClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->finalize = gp_applet_manager_finalize;
}

static void
gp_applet_manager_init (GpAppletManager *self)
{
  self->backend = get_current_backend ();
}

GpAppletManager *
gp_applet_manager_new (GpApplication *application)
{
  GpAppletManager *self;

  self = g_object_new (GP_TYPE_APPLET_MANAGER, NULL);
  self->application = application;
  self->manager = gp_application_get_module_manager (application);

  return self;
}

GpAppletInfo *
gp_applet_manager_get_applet_info (GpAppletManager  *self,
                                   const char       *module_id,
                                   const char       *applet_id,
                                   GError          **error)
{
  GpModule *module;

  module = gp_module_manager_get_module (self->manager, module_id, error);

  if (module == NULL)
    return NULL;

  return gp_module_get_applet_info (module, applet_id, error);
}

GpApplet *
gp_applet_manager_load_applet (GpAppletManager  *self,
                               const char       *module_id,
                               const char       *applet_id,
                               const char       *settings_path,
                               GVariant         *initial_settings,
                               GError          **error)
{
  GpModule *module;

  module = gp_module_manager_get_module (self->manager, module_id, error);

  if (!module)
    return NULL;

  return gp_module_applet_new (module,
                               applet_id,
                               settings_path,
                               initial_settings,
                               error);
}

char *
gp_applet_manager_get_new_iid (GpAppletManager *self,
                               const char      *old_iid)
{
  GList *modules;
  GList *l;
  gchar *new_iid;

  modules = gp_module_manager_get_modules (self->manager);
  new_iid = NULL;

  for (l = modules; l != NULL; l = l->next)
    {
      GpModule *module;
      const gchar *applet;

      module = GP_MODULE (l->data);
      applet = gp_module_get_applet_id_from_iid (module, old_iid);

      if (applet != NULL)
        {
          new_iid = g_strdup_printf ("%s::%s", gp_module_get_id (module), applet);
          break;
        }
    }

  g_list_free (modules);

  return new_iid;
}

gboolean
gp_applet_manager_open_initial_setup_dialog (GpAppletManager        *self,
                                             const char             *module_id,
                                             const char             *applet_id,
                                             GVariant               *settings,
                                             GtkWindow              *parent,
                                             GpInitialSetupCallback  callback,
                                             gpointer                user_data,
                                             GDestroyNotify          free_func)
{
  GpModule *module;
  GpAppletInfo *info;
  GpInitialSetupDialog *dialog;

  module = gp_module_manager_get_module (self->manager, module_id, NULL);

  if (!module)
    {
      if (free_func != NULL)
        free_func (user_data);

      return FALSE;
    }

  info = gp_module_get_applet_info (module, applet_id, NULL);

  if (!info || !info->initial_setup_dialog_func)
    {
      if (free_func != NULL)
        free_func (user_data);

      return FALSE;
    }

  dialog = gp_initial_setup_dialog_new ();

  gp_initial_setup_dialog_add_callback (dialog, callback, user_data, free_func);
  gp_initial_setup_dialog_set_settings (dialog, settings);

  info->initial_setup_dialog_func (dialog);

  if (parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
      gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    }
  else
    {
      gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    }


  gtk_widget_show (GTK_WIDGET (dialog));

  return TRUE;
}

gboolean
gp_applet_manager_is_applet_disabled (GpAppletManager  *self,
                                      const char       *module_id,
                                      const char       *applet_id,
                                      char            **reason)
{
  PanelLockdown *lockdown;
  GpModule *module;
  GpLockdownFlags lockdowns;

  g_return_val_if_fail (reason == NULL || *reason == NULL, FALSE);

  lockdown = gp_application_get_lockdown (self->application);

  if (panel_lockdown_is_applet_disabled (lockdown, module_id, applet_id))
    {
      if (reason != NULL)
        *reason = g_strdup (_("Disabled because this applet is listed in "
                              "“disabled-applets” setting in “org.gnome.gnome-"
                              "panel.lockdown” GSettings schema."));

      return TRUE;
    }

  module = gp_module_manager_get_module (self->manager, module_id, NULL);

  if (!module)
    return FALSE;

  lockdowns = panel_lockdown_get_flags (lockdown, module_id, applet_id);

  return gp_module_is_applet_disabled (module,
                                       applet_id,
                                       self->backend,
                                       lockdowns,
                                       reason);
}
