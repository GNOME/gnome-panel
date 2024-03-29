/*
 * Copyright (C) 2019 Alberts Muktupāvels
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

#ifndef GP_APPLICATION_H
#define GP_APPLICATION_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _GpAppletManager GpAppletManager;
typedef struct _GpModuleManager GpModuleManager;
typedef struct _PanelLayout PanelLayout;
typedef struct _PanelLockdown PanelLockdown;
typedef struct _PanelToplevel PanelToplevel;

#define GP_TYPE_APPLICATION (gp_application_get_type ())
G_DECLARE_FINAL_TYPE (GpApplication, gp_application, GP, APPLICATION, GObject)

GpApplication   *gp_application_new                  (GError        **error);

GSettings       *gp_application_get_general_settings (GpApplication  *self);

GpModuleManager *gp_application_get_module_manager   (GpApplication  *self);

GpAppletManager *gp_application_get_applet_manager   (GpApplication  *self);

PanelLockdown   *gp_application_get_lockdown         (GpApplication  *self);

PanelLayout     *gp_application_get_layout           (GpApplication  *self);

void             gp_application_add_toplevel         (GpApplication  *self,
                                                      PanelToplevel  *toplevel);

void             gp_application_remove_toplevel      (GpApplication  *self,
                                                      PanelToplevel  *toplevel);

GList           *gp_application_get_toplevels        (GpApplication  *self);

PanelToplevel   *gp_application_get_toplevel_by_id   (GpApplication  *self,
                                                      const char     *id);

G_END_DECLS

#endif
