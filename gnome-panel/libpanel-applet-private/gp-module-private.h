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

#ifndef GP_MODULE_PRIVATE_H
#define GP_MODULE_PRIVATE_H

#include "libgnome-panel/gp-applet.h"
#include "libgnome-panel/gp-module.h"

G_BEGIN_DECLS

#define GP_TYPE_MODULE gp_module_get_type ()
G_DECLARE_FINAL_TYPE (GpModule, gp_module, GP, MODULE, GObject)

typedef enum
{
  GP_MODULE_ERROR_APPLET_DOES_NOT_EXIST,
  GP_MODULE_ERROR_MISSING_APPLET_INFO,
  GP_MODULE_ERROR_MISSING_APPLET_TYPE,
} GpModuleError;

#define GP_MODULE_ERROR gp_module_error_quark ()
GQuark gp_module_error_quark (void);

GpModule            *gp_module_new_from_path       (const gchar           *path);

const gchar         *gp_module_get_id              (GpModule              *module);

const gchar * const *gp_module_get_applets         (GpModule              *module);

GpAppletInfo        *gp_module_get_applet_info     (GpModule              *module,
                                                    const gchar           *applet,
                                                    GError               **error);

const gchar         *gp_module_get_applet_from_iid (GpModule              *module,
                                                    const gchar           *old_iid);

GpApplet            *gp_module_applet_new          (GpModule              *module,
                                                    const gchar           *applet,
                                                    const gchar           *settings_path,
                                                    gboolean               locked_down,
                                                    GtkOrientation         orientation,
                                                    GtkPositionType        position,
                                                    GError               **error);

G_END_DECLS

#endif
