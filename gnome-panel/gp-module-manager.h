/*
 * Copyright (C) 2018-2020 Alberts Muktupāvels
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

#ifndef GP_MODULE_MANAGER_H
#define GP_MODULE_MANAGER_H

#include "libgnome-panel/gp-module-private.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define GP_TYPE_MODULE_MANAGER (gp_module_manager_get_type ())
G_DECLARE_FINAL_TYPE (GpModuleManager, gp_module_manager,
                      GP, MODULE_MANAGER, GObject)

GpModuleManager *gp_module_manager_new         (void);

GList           *gp_module_manager_get_modules (GpModuleManager *self);

GpModule        *gp_module_manager_get_module  (GpModuleManager *self,
                                                const char      *id);

G_END_DECLS

#endif
