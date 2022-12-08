/*
 * Copyright (C) 2016-2020 Alberts Muktupāvels
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

#ifndef GP_MODULE_PRIVATE_H
#define GP_MODULE_PRIVATE_H

#include <libgnome-panel/gp-applet.h>
#include <libgnome-panel/gp-macros.h>
#include <libgnome-panel/gp-module.h>

G_BEGIN_DECLS

typedef enum
{
  GP_MODULE_ERROR_ABI_DOES_NOT_MATCH,
  GP_MODULE_ERROR_APPLET_DOES_NOT_EXIST,
  GP_MODULE_ERROR_MISSING_APPLET_INFO,
  GP_MODULE_ERROR_MISSING_APPLET_TYPE,
} GpModuleError;

GP_EXPORT
#define GP_MODULE_ERROR gp_module_error_quark ()
GQuark gp_module_error_quark (void);

GP_EXPORT
GpModule            *gp_module_new_from_path          (const gchar      *path);

GP_EXPORT
const gchar         *gp_module_get_id                 (GpModule         *module);

GP_EXPORT
const gchar         *gp_module_get_version            (GpModule         *module);

GP_EXPORT
const gchar * const *gp_module_get_applets            (GpModule         *module);

GP_EXPORT
GpAppletInfo        *gp_module_get_applet_info        (GpModule         *module,
                                                       const gchar      *applet,
                                                       GError          **error);

GP_EXPORT
const gchar         *gp_module_get_applet_id_from_iid (GpModule         *module,
                                                       const gchar      *old_iid);

GP_EXPORT
GpActionFlags        gp_module_get_actions            (GpModule         *self);

GP_EXPORT
gboolean             gp_module_handle_action          (GpModule         *self,
                                                       GpActionFlags     action,
                                                       uint32_t          time);

GP_EXPORT
GpApplet            *gp_module_applet_new             (GpModule         *module,
                                                       const gchar      *applet,
                                                       const gchar      *settings_path,
                                                       GVariant         *initial_settings,
                                                       GError          **error);

GP_EXPORT
GtkWidget           *gp_module_create_about_dialog    (GpModule         *module,
                                                       GtkWindow        *parent,
                                                       const char       *applet);

GP_EXPORT
void                 gp_module_show_help              (GpModule         *module,
                                                       GtkWindow        *parent,
                                                       const char       *applet,
                                                       const char       *page);

GP_EXPORT
gboolean             gp_module_is_applet_disabled     (GpModule         *module,
                                                       const char       *applet,
                                                       const char       *backend,
                                                       GpLockdownFlags   lockdowns,
                                                       char            **reason);

G_END_DECLS

#endif
