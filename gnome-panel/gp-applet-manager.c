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

static GVariant *
get_initial_settings (PanelAppletFrameActivating *frame_act)
{
  gchar *path;
  GSettings *settings;
  GVariant *initial_settings;

  path = panel_applet_frame_activating_get_initial_settings_path (frame_act);
  settings = g_settings_new_with_path ("org.gnome.gnome-panel.applet.initial-settings", path);
  g_free (path);

  initial_settings = g_settings_get_user_value (settings, "settings");
  g_object_unref (settings);

  return initial_settings;
}

static void
remove_initial_settings (PanelAppletFrameActivating *frame_act)
{
  gchar *path;
  GSettings *settings;

  path = panel_applet_frame_activating_get_initial_settings_path (frame_act);
  settings = g_settings_new_with_path ("org.gnome.gnome-panel.applet.initial-settings", path);
  g_free (path);

  g_settings_reset (settings, "settings");
  g_object_unref (settings);
}

static void
gp_applet_manager_finalize (GObject *object)
{
  GpAppletManager *self;

  self = GP_APPLET_MANAGER (object);

  g_clear_pointer (&self->backend, g_free);
  g_clear_object (&self->manager);

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

  self->manager = gp_module_manager_new ();
}

GpAppletManager *
gp_applet_manager_new (void)
{
  return g_object_new (GP_TYPE_APPLET_MANAGER, NULL);
}

GpModuleManager *
gp_applet_manager_get_module_manager (GpAppletManager *self)
{
  return self->manager;
}

gboolean
gp_applet_manager_factory_activate (GpAppletManager *self,
                                    const char      *iid)
{
  GpAppletInfo *info;

  info = gp_applet_manager_get_applet_info (self, iid);

  if (info == NULL)
    return FALSE;

  return TRUE;
}

void
gp_applet_manager_factory_deactivate (GpAppletManager *self,
                                      const char      *iid)
{
}

GpAppletInfo *
gp_applet_manager_get_applet_info (GpAppletManager *self,
                                   const char      *iid)
{
  const char *applet_id;
  char *module_id;
  GpModule *module;

  applet_id = g_strrstr (iid, "::");
  if (!applet_id)
    return NULL;

  module_id = g_strndup (iid, strlen (iid) - strlen (applet_id));
  module = gp_module_manager_get_module (self->manager, module_id);
  g_free (module_id);

  if (module == NULL)
    return NULL;

  applet_id += 2;

  return gp_module_get_applet_info (module, applet_id, NULL);
}

gboolean
gp_applet_manager_load_applet (GpAppletManager            *self,
                               const char                 *iid,
                               PanelAppletFrameActivating *frame_act)
{
  const gchar *applet_id;
  gchar *module_id;
  GpModule *module;
  gchar *settings_path;
  gboolean locked_down;
  PanelOrientation panel_orientation;
  GtkOrientation orientation;
  GtkPositionType position;
  GVariant *initial_settings;
  GError *error;
  GpApplet *applet;
  PanelAppletFrame *frame;

  g_return_val_if_fail (iid != NULL, FALSE);
  g_return_val_if_fail (frame_act != NULL, FALSE);

  applet_id = g_strrstr (iid, "::");
  if (!applet_id)
    return FALSE;

  module_id = g_strndup (iid, strlen (iid) - strlen (applet_id));
  module = gp_module_manager_get_module (self->manager, module_id);
  g_free (module_id);

  if (!module)
    return FALSE;

  applet_id += 2;
  settings_path = panel_applet_frame_activating_get_settings_path (frame_act);
  locked_down = panel_applet_frame_activating_get_locked_down (frame_act);
  panel_orientation = panel_applet_frame_activating_get_orientation (frame_act);

  switch (panel_orientation)
    {
      case PANEL_ORIENTATION_BOTTOM:
        orientation = GTK_ORIENTATION_HORIZONTAL;
        position = GTK_POS_BOTTOM;
        break;
      case PANEL_ORIENTATION_LEFT:
        orientation = GTK_ORIENTATION_VERTICAL;
        position = GTK_POS_LEFT;
        break;
      case PANEL_ORIENTATION_RIGHT:
        orientation = GTK_ORIENTATION_VERTICAL;
        position = GTK_POS_RIGHT;
        break;
      case PANEL_ORIENTATION_TOP:
      default:
        orientation = GTK_ORIENTATION_HORIZONTAL;
        position = GTK_POS_TOP;
        break;
    }

  initial_settings = get_initial_settings (frame_act);

  error = NULL;
  applet = gp_module_applet_new (module, applet_id, settings_path,
                                 initial_settings, &error);

  g_clear_pointer (&initial_settings, g_variant_unref);
  g_free (settings_path);

  if (!applet)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return FALSE;
    }

  remove_initial_settings (frame_act);

  gp_applet_set_locked_down (applet, locked_down);
  gp_applet_set_orientation (applet, orientation);
  gp_applet_set_position (applet, position);

  frame = g_object_new (PANEL_TYPE_APPLET_FRAME, NULL);

  _panel_applet_frame_set_applet (frame, applet);
  _panel_applet_frame_set_iid (frame, iid);

  _panel_applet_frame_activated (frame, frame_act, NULL);

  return TRUE;
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
                                             const char             *iid,
                                             GVariant               *settings,
                                             GtkWindow              *parent,
                                             GpInitialSetupCallback  callback,
                                             gpointer                user_data,
                                             GDestroyNotify          free_func)
{
  const gchar *applet_id;
  gchar *module_id;
  GpModule *module;
  GpAppletInfo *info;
  GpInitialSetupDialog *dialog;

  g_return_val_if_fail (iid != NULL, FALSE);

  applet_id = g_strrstr (iid, "::");
  if (!applet_id)
    return FALSE;

  module_id = g_strndup (iid, strlen (iid) - strlen (applet_id));
  module = gp_module_manager_get_module (self->manager, module_id);
  g_free (module_id);

  if (!module)
    return FALSE;

  applet_id += 2;
  info = gp_module_get_applet_info (module, applet_id, NULL);

  if (!info || !info->initial_setup_dialog_func)
    return FALSE;

  dialog = gp_initital_setup_dialog_new ();

  gp_initital_setup_dialog_add_callback (dialog, callback, user_data, free_func);
  gp_initital_setup_dialog_set_settings (dialog, settings);

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

GtkWidget *
gp_applet_manager_get_standalone_menu (GpAppletManager *self)
{
  GSettings *general_settings;
  gboolean enable_tooltips;
  gboolean locked_down;
  guint menu_icon_size;
  GList *modules;
  GList *l;
  GtkWidget *menu;

  general_settings = g_settings_new ("org.gnome.gnome-panel.general");

  enable_tooltips = g_settings_get_boolean (general_settings, "enable-tooltips");
  locked_down = panel_lockdown_get_panels_locked_down_s ();
  menu_icon_size = g_settings_get_enum (general_settings, "menu-icon-size");

  g_object_unref (general_settings);

  modules = gp_module_manager_get_modules (self->manager);
  menu = NULL;

  for (l = modules; l != NULL; l = l->next)
    {
      GpModule *module;

      module = GP_MODULE (l->data);

      menu = gp_module_get_standalone_menu (module, enable_tooltips,
                                            locked_down, menu_icon_size);

      if (menu != NULL)
        break;
    }

  g_list_free (modules);

  return menu;
}

gboolean
gp_applet_manager_is_applet_disabled (GpAppletManager  *self,
                                      const char       *iid,
                                      char            **reason)
{
  const char *applet_id;
  char *module_id;
  GpModule *module;
  GpLockdownFlags lockdowns;

  g_return_val_if_fail (iid != NULL, FALSE);
  g_return_val_if_fail (reason == NULL || *reason == NULL, FALSE);

  if (panel_lockdown_is_applet_disabled (panel_lockdown_get (), iid))
    {
      if (reason != NULL)
        *reason = g_strdup (_("Disabled because this applet is listed in "
                              "“disabled-applets” setting in “org.gnome.gnome-"
                              "panel.lockdown” GSettings schema."));

      return TRUE;
    }

  applet_id = g_strrstr (iid, "::");
  if (!applet_id)
    {
      g_assert_not_reached ();
      return TRUE;
    }

  module_id = g_strndup (iid, strlen (iid) - strlen (applet_id));
  module = gp_module_manager_get_module (self->manager, module_id);
  g_free (module_id);

  if (!module)
    return FALSE;

  applet_id += 2;
  lockdowns = panel_lockdown_get_flags_s (iid);

  return gp_module_is_applet_disabled (module,
                                       applet_id,
                                       self->backend,
                                       lockdowns,
                                       reason);
}
