/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

#ifndef GP_MODULE_H
#define GP_MODULE_H

#include <gtk/gtk.h>
#include <libgnome-panel/gp-applet-info.h>
#include <libgnome-panel/gp-module-info.h>

G_BEGIN_DECLS

/**
 * GP_MODULE_ABI_VERSION:
 *
 * The version of the module system's ABI.
 */
#define GP_MODULE_ABI_VERSION 0x0001

/**
 * GpAppletVTable:
 * @get_applet_info: (transfer full): returns a #GpAppletInfo.
 * @get_applet_type: returns a #GType.
 * @get_applet_from_iid: Compatibility function.
 * @setup_about: Function for setting up about dialog.
 *
 * The #GpAppletVTable provides the functions required by the #GpModule.
 */
typedef struct _GpAppletVTable GpAppletVTable;
struct _GpAppletVTable
{
  GpAppletInfo * (* get_applet_info)     (const gchar    *applet);

  GType          (* get_applet_type)     (const gchar    *applet);

  const gchar  * (* get_applet_from_iid) (const gchar    *iid);

  gboolean       (* setup_about)         (GtkAboutDialog *dialog,
                                          const gchar    *applet);
};

/**
 * gp_module_get_abi_version:
 *
 * Required API for GNOME Panel modules to implement. This function must
 * always return %GP_MODULE_ABI_VERSION.
 *
 * Returns: the module ABI version.
 */
guint32       gp_module_get_abi_version   (void);

/**
 * gp_module_get_module_info:
 *
 * Required API for GNOME Panel modules to implement. This function must
 * return a newly created #GpModuleInfo.
 *
 * Returns: a #GpModuleInfo.
 */
GpModuleInfo *gp_module_get_module_info   (void);

/**
 * gp_module_get_applet_vtable:
 * @vtable: (out caller-allocates): return location for the #GpAppletVTable
 *
 * Required API for GNOME Panel modules to implement.
 */
void          gp_module_get_applet_vtable (GpAppletVTable *vtable);

G_END_DECLS

#endif
