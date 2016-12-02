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

#include <string.h>

#include "gp-applet-frame.h"
#include "gp-applet-manager.h"
#include "gp-module-private.h"
#include "libgnome-panel/gp-applet-info-private.h"
#include "modules/builtin/gp-builtin.h"

struct _GpAppletManager
{
  PanelAppletsManager  parent;

  GHashTable          *modules;
  GHashTable          *infos;
};

G_DEFINE_TYPE (GpAppletManager, gp_applet_manager, PANEL_TYPE_APPLETS_MANAGER)

static void
get_applet_infos (GpAppletManager *manager,
                  const gchar     *id,
                  GpModule        *module)
{
  const gchar *const *applets;
  guint i;

  applets = gp_module_get_applets (module);

  for (i = 0; applets[i] != NULL; i++)
    {
      GError *error;
      GpAppletInfo *info;
      gchar *iid;
      PanelAppletInfo *applet_info;

      error = NULL;
      info = gp_module_get_applet_info (module, applets[i], &error);

      if (info == NULL)
        {
          g_warning ("%s", error->message);
          g_error_free (error);

          continue;
        }

      iid = g_strdup_printf ("%s::%s", id, applets[i]);
      applet_info = panel_applet_info_new (iid, info->name,
                                           info->description,
                                           info->icon);

      g_hash_table_insert (manager->infos, g_strdup (iid), applet_info);
      g_free (iid);
    }
}

static void
load_builtin_modules (GpAppletManager *manager)
{
  guint i;

  for (i = 0; builtin[i] != NULL; i++)
    {
      GpModule *module;
      const gchar *id;

      module = gp_module_new_from_vtable (builtin[i]);

      if (module == NULL)
        continue;

      id = gp_module_get_id (module);

      g_hash_table_insert (manager->modules, g_strdup (id), module);
      get_applet_infos (manager, id, module);
    }
}

static void
load_external_modules (GpAppletManager *manager)
{
  GDir *dir;
  const gchar *name;

  dir = g_dir_open (MODULESDIR, 0, NULL);
  if (!dir)
    return;

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      gchar *path;
      GpModule *module;
      const gchar *id;

      path = g_build_filename (MODULESDIR, name, NULL);
      module = gp_module_new_from_path (path);
      g_free (path);

      if (module == NULL)
        continue;

      id = gp_module_get_id (module);

      g_hash_table_insert (manager->modules, g_strdup (id), module);
      get_applet_infos (manager, id, module);
    }

  g_dir_close (dir);
}

static void
applet_info_free (gpointer data)
{
  PanelAppletInfo *info;

  info = (PanelAppletInfo *) data;

  panel_applet_info_free (info);
}

static void
gp_applet_manager_finalize (GObject *object)
{
  GpAppletManager *manager;

  manager = GP_APPLET_MANAGER (object);

  g_clear_pointer (&manager->modules, g_hash_table_destroy);
  g_clear_pointer (&manager->infos, g_hash_table_destroy);

  G_OBJECT_CLASS (gp_applet_manager_parent_class)->finalize (object);
}

static GList *
gp_applet_manager_get_applets (PanelAppletsManager *manager)
{
  GpAppletManager *applet_manager;

  applet_manager = GP_APPLET_MANAGER (manager);

  return g_hash_table_get_values (applet_manager->infos);
}

static gboolean
gp_applet_manager_factory_activate (PanelAppletsManager *manager,
                                    const gchar         *iid)
{
  GpAppletManager *applet_manager;

  applet_manager = GP_APPLET_MANAGER (manager);

  if (!g_hash_table_lookup (applet_manager->infos, iid))
    return FALSE;

  return TRUE;
}

static gboolean
gp_applet_manager_factory_deactivate (PanelAppletsManager *manager,
                                      const gchar         *iid)
{
  GpAppletManager *applet_manager;

  applet_manager = GP_APPLET_MANAGER (manager);

  if (!g_hash_table_lookup (applet_manager->infos, iid))
    return FALSE;

  return TRUE;
}

static PanelAppletInfo *
gp_applet_manager_get_applet_info (PanelAppletsManager *manager,
                                   const gchar         *iid)
{
  GpAppletManager *applet_manager;

  applet_manager = GP_APPLET_MANAGER (manager);

  return g_hash_table_lookup (applet_manager->infos, iid);
}

static gboolean
gp_applet_manager_load_applet (PanelAppletsManager        *manager,
                               const gchar                *iid,
                               PanelAppletFrameActivating *frame_act)
{
  GpAppletManager *applet_manager;
  const gchar *applet_id;
  gchar *module_id;
  GpModule *module;
  gchar *settings_path;
  gboolean locked_down;
  PanelOrientation panel_orientation;
  GtkOrientation orientation;
  GtkPositionType position;
  GError *error;
  GpApplet *applet;
  GpAppletFrame *frame;
  PanelAppletFrame *applet_frame;
  GtkWidget *widget;

  g_return_val_if_fail (iid != NULL, FALSE);
  g_return_val_if_fail (frame_act != NULL, FALSE);

  applet_manager = GP_APPLET_MANAGER (manager);

  applet_id = g_strrstr (iid, "::");
  if (!applet_id)
    return FALSE;

  module_id = g_strndup (iid, strlen (iid) - strlen (applet_id));
  module = g_hash_table_lookup (applet_manager->modules, module_id);
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

  error = NULL;
  applet = gp_module_applet_new (module, applet_id, settings_path, locked_down,
                                 orientation, position, &error);

  g_free (settings_path);

  if (!applet)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return FALSE;
    }

  frame = g_object_new (GP_TYPE_APPLET_FRAME, NULL);
  gp_applet_frame_set_applet (frame, applet);

  applet_frame = PANEL_APPLET_FRAME (frame);
  _panel_applet_frame_set_iid (applet_frame, iid);

  widget = GTK_WIDGET (applet);

  gtk_container_add (GTK_CONTAINER (frame), widget);
  gtk_widget_show (widget);

  _panel_applet_frame_activated (applet_frame, frame_act, NULL);

  return TRUE;
}

static GtkWidget *
gp_applet_manager_get_applet_widget (PanelAppletsManager *manager,
                                     const gchar         *iid,
                                     guint                uid)
{
  return NULL;
}

static gchar *
gp_applet_manager_get_new_iid (PanelAppletsManager *manager,
                               const gchar         *old_iid)
{
  GpAppletManager *applet_manager;
  GList *modules;
  GList *l;
  gchar *new_iid;

  applet_manager = GP_APPLET_MANAGER (manager);

  modules = g_hash_table_get_values (applet_manager->modules);
  new_iid = NULL;

  for (l = modules; l != NULL; l = l->next)
    {
      GpModule *module;
      const gchar *applet;

      module = GP_MODULE (l->data);
      applet = gp_module_get_applet_from_iid (module, old_iid);

      if (applet != NULL)
        {
          new_iid = g_strdup_printf ("%s::%s", gp_module_get_id (module), applet);
          break;
        }
    }

  g_list_free (modules);

  return new_iid;
}

static void
gp_applet_manager_class_init (GpAppletManagerClass *manager_class)
{
  GObjectClass *object_class;
  PanelAppletsManagerClass *applets_manager_class;

  object_class = G_OBJECT_CLASS (manager_class);
  applets_manager_class = PANEL_APPLETS_MANAGER_CLASS (manager_class);

  object_class->finalize = gp_applet_manager_finalize;

  applets_manager_class->get_applets = gp_applet_manager_get_applets;
  applets_manager_class->factory_activate = gp_applet_manager_factory_activate;
  applets_manager_class->factory_deactivate = gp_applet_manager_factory_deactivate;
  applets_manager_class->get_applet_info = gp_applet_manager_get_applet_info;
  applets_manager_class->load_applet = gp_applet_manager_load_applet;
  applets_manager_class->get_applet_widget = gp_applet_manager_get_applet_widget;
  applets_manager_class->get_new_iid = gp_applet_manager_get_new_iid;
}

static void
gp_applet_manager_init (GpAppletManager *manager)
{
  manager->modules = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, g_object_unref);

  manager->infos = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free, applet_info_free);

  load_builtin_modules (manager);
  load_external_modules (manager);
}
