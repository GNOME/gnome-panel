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
 * GpGetAppletInfoFunc:
 * @id: the applet id
 *
 * Returns a #GpAppletInfo.
 *
 * Returns: (transfer full): returns a #GpAppletInfo.
 */
typedef GpAppletInfo * (* GpGetAppletInfoFunc)    (const gchar *id);

/**
 * GetAppletIdFromIidFunc:
 * @iid: the applet iid
 *
 * Specifies the type of the module function called to convert old applet
 * iid to new id. See gp_module_set_compatibility().
 *
 * Returns: (transfer none): the applet id, or %NULL.
 */
typedef const gchar  * (* GetAppletIdFromIidFunc) (const gchar *iid);

/**
 * GetStandaloneMenuFunc:
 * @enable_tooltips: Whether the applet should show tooltips
 * @locked_down: Whether the applet is on locked down panel
 * @menu_icon_size: The size of icons in menus
 *
 * Specifies the type of the module function called to create a
 * standalone menu.
 *
 * Returns: (transfer full): returns a #GtkMenu.
 */
typedef GtkWidget    * (* GetStandaloneMenuFunc)  (gboolean     enable_tooltips,
                                                   gboolean     locked_down,
                                                   guint        menu_icon_size);

/**
 * GP_TYPE_MODULE:
 *
 * The type for GpModule.
 */
#define GP_TYPE_MODULE (gp_module_get_type ())
G_DECLARE_FINAL_TYPE (GpModule, gp_module, GP, MODULE, GObject)

void          gp_module_set_abi_version     (GpModule               *module,
                                             guint32                 abi_version);

void          gp_module_set_gettext_domain  (GpModule               *module,
                                             const gchar            *gettext_domain);

void          gp_module_set_id              (GpModule               *module,
                                             const gchar            *id);

void          gp_module_set_version         (GpModule               *module,
                                             const gchar            *version);

void          gp_module_set_applet_ids      (GpModule               *module,
                                             ...);

void          gp_module_set_get_applet_info (GpModule               *module,
                                             GpGetAppletInfoFunc     func);

void          gp_module_set_compatibility   (GpModule               *module,
                                             GetAppletIdFromIidFunc  func);

void          gp_module_set_standalone_menu (GpModule               *module,
                                             GetStandaloneMenuFunc   func);

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
void          gp_module_load                (GpModule               *module);

G_END_DECLS

#endif
