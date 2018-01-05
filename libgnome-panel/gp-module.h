/*
 * Copyright (C) 2016-2018 Alberts Muktupāvels
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

G_BEGIN_DECLS

/**
 * GP_MODULE_ABI_VERSION:
 *
 * The version of the module system's ABI.
 */
#define GP_MODULE_ABI_VERSION 0x0001

/**
 * GetAppletIdFromIidFunc:
 * @iid: the applet iid
 *
 * Specifies the type of the module function called to convert old applet
 * iid to new id. See gp_module_set_compatibility().
 *
 * Returns: (transfer none): the applet id, or %NULL.
 */
typedef const gchar * (* GetAppletIdFromIidFunc) (const gchar *iid);

/**
 * GpAppletVTable:
 * @get_applet_info: (transfer full): returns a #GpAppletInfo.
 * @get_applet_type: returns a #GType.
 * @setup_about: Function for setting up about dialog.
 *
 * The #GpAppletVTable provides the functions required by the #GpModule.
 */
typedef struct _GpAppletVTable GpAppletVTable;
struct _GpAppletVTable
{
  GpAppletInfo * (* get_applet_info)     (const gchar    *applet);

  GType          (* get_applet_type)     (const gchar    *applet);

  gboolean       (* setup_about)         (GtkAboutDialog *dialog,
                                          const gchar    *applet);
};

#define GP_TYPE_MODULE (gp_module_get_type ())
G_DECLARE_FINAL_TYPE (GpModule, gp_module, GP, MODULE, GObject)

void          gp_module_set_abi_version    (GpModule               *module,
                                            guint32                 abi_version);

void          gp_module_set_gettext_domain (GpModule               *module,
                                            const gchar            *gettext_domain);

void          gp_module_set_id             (GpModule               *module,
                                            const gchar            *id);

void          gp_module_set_version        (GpModule               *module,
                                            const gchar            *version);

void          gp_module_set_applet_ids     (GpModule               *module,
                                            ...);

void          gp_module_set_compatibility  (GpModule               *module,
                                            GetAppletIdFromIidFunc  func);

/**
 * gp_module_load:
 * @module: a #GpModule
 *
 * Required API for GNOME Panel modules to implement.
 *
 * This function will be called after module has been loaded and should be
 * used to setup all required module info. As minimum gp_module_set_id() and
 * gp_module_set_abi_version() should be called.
 */
void          gp_module_load               (GpModule    *module);

/**
 * gp_module_get_applet_vtable:
 * @vtable: (out caller-allocates): return location for the #GpAppletVTable
 *
 * Required API for GNOME Panel modules to implement.
 */
void          gp_module_get_applet_vtable (GpAppletVTable *vtable);

G_END_DECLS

#endif
