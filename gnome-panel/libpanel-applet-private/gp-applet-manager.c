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

#include "gp-applet-manager.h"

struct _GpAppletManager
{
  PanelAppletsManager  parent;
};

G_DEFINE_TYPE (GpAppletManager, gp_applet_manager, PANEL_TYPE_APPLETS_MANAGER)

static GList *
gp_applet_manager_get_applets (PanelAppletsManager *amanager)
{
  return NULL;
}

static gboolean
gp_applet_manager_factory_activate (PanelAppletsManager *manager,
                                    const gchar         *iid)
{
  return FALSE;
}

static gboolean
gp_applet_manager_factory_deactivate (PanelAppletsManager *manager,
                                      const gchar         *iid)
{
  return FALSE;
}

static PanelAppletInfo *
gp_applet_manager_get_applet_info (PanelAppletsManager *manager,
                                   const gchar         *iid)
{
  return NULL;
}

static gboolean
gp_applet_manager_load_applet (PanelAppletsManager        *manager,
                               const gchar                *iid,
                               PanelAppletFrameActivating *frame_act)
{
  return FALSE;
}

static GtkWidget *
gp_applet_manager_get_applet_widget (PanelAppletsManager *manager,
                                     const gchar         *iid,
                                     guint                uid)
{
  return NULL;
}

static void
gp_applet_manager_class_init (GpAppletManagerClass *manager_class)
{
  PanelAppletsManagerClass *applets_manager_class;

  applets_manager_class = PANEL_APPLETS_MANAGER_CLASS (manager_class);

  applets_manager_class->get_applets = gp_applet_manager_get_applets;
  applets_manager_class->factory_activate = gp_applet_manager_factory_activate;
  applets_manager_class->factory_deactivate = gp_applet_manager_factory_deactivate;
  applets_manager_class->get_applet_info = gp_applet_manager_get_applet_info;
  applets_manager_class->load_applet = gp_applet_manager_load_applet;
  applets_manager_class->get_applet_widget = gp_applet_manager_get_applet_widget;
}

static void
gp_applet_manager_init (GpAppletManager *manager)
{
}
