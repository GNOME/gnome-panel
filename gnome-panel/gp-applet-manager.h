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

#ifndef GP_APPLET_MANAGER_H
#define GP_APPLET_MANAGER_H

#include "panel-applets-manager.h"

G_BEGIN_DECLS

#define GP_TYPE_APPLET_MANAGER gp_applet_manager_get_type ()
G_DECLARE_FINAL_TYPE (GpAppletManager, gp_applet_manager,
                      GP, APPLET_MANAGER, GObject)

GpAppletManager *gp_applet_manager_new                       (void);

GpModuleManager *gp_applet_manager_get_module_manager        (GpAppletManager             *self);

gboolean         gp_applet_manager_factory_activate          (GpAppletManager             *self,
                                                              const char                  *iid);

void             gp_applet_manager_factory_deactivate        (GpAppletManager             *self,
                                                              const char                  *iid);

GpAppletInfo    *gp_applet_manager_get_applet_info           (GpAppletManager             *self,
                                                              const char                  *iid);

gboolean         gp_applet_manager_load_applet               (GpAppletManager             *self,
                                                              const char                  *iid,
                                                              PanelAppletFrameActivating  *frame_act);

char            *gp_applet_manager_get_new_iid               (GpAppletManager             *self,
                                                              const char                  *old_iid);

gboolean         gp_applet_manager_open_initial_setup_dialog (GpAppletManager             *self,
                                                              const char                  *iid,
                                                              GVariant                    *settings,
                                                              GtkWindow                   *parent,
                                                              GpInitialSetupCallback       callback,
                                                              gpointer                     user_data,
                                                              GDestroyNotify               free_func);

GtkWidget       *gp_applet_manager_get_standalone_menu       (GpAppletManager             *self);

gboolean         gp_applet_manager_is_applet_disabled        (GpAppletManager             *self,
                                                              const char                  *iid,
                                                              char                       **reason);

G_END_DECLS

#endif
