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
 * SECTION: gp-module
 * @title: Module
 * @short_description: a module with one or more applets
 * @include: libngome-panel/gp-module.h
 *
 * A module with one or more applets.
 */

/**
 * GP_MODULE_ABI_VERSION:
 *
 * The version of the module system's ABI.
 */
#define GP_MODULE_ABI_VERSION 0x0001

/**
 * GpModuleGetInfo:
 *
 * Each module must have a function gp_module_get_info() with this
 * prototype.
 *
 * Use gp_module_info_new() to create return value.
 *
 * Returns: (transfer full): a newly created #GpModuleInfo.
 */
typedef GpModuleInfo *(* GpModuleGetInfo)       (void);

/**
 * GpModuleLoad:
 * @module: a #GTypeModule
 *
 * Each module must have a function gp_module_load() with this prototype.
 *
 * This function is run after the module has been loaded. Typically, this
 * function will register applet types and setup gettext.
 */
typedef void          (* GpModuleLoad)          (GTypeModule    *module);

/**
 * GpModuleUnload:
 * @module: a #GTypeModule
 *
 * Each module must have a function gp_module_unload() with this prototype.
 *
 * This function is run when the module is being unloaded.
 */
typedef void          (* GpModuleUnload)        (GTypeModule    *module);

/**
 * GpModuleGetAppletInfo:
 * @applet: the applet id
 *
 * Each module must have a function gp_module_get_applet_info() with this
 * prototype.
 *
 * This function will be called to get #GpAppletInfo about all applets
 * that was returned from gp_module_get_info().
 *
 * Use gp_applet_info_new() to create return value.
 *
 * Returns: (transfer full): a newly created #GpAppletInfo.
 */
typedef GpAppletInfo *(* GpModuleGetAppletInfo) (const gchar    *applet);

/**
 * GpModuleGetAppletType:
 * @applet: the applet id
 *
 * Each module must have a function gp_module_get_applet_type() with this
 * prototype.
 *
 * This function will be called to get #GType for requested applet.
 *
 * Returns: a previously registered #GType.
 */
typedef GType         (* GpModuleGetAppletType) (const gchar    *applet);

/**
 * GpModuleSetupAbout:
 * @dialog: a #GtkAboutDialog
 * @applet: the applet id
 *
 * Each module might have a optional function gp_module_setup_about()
 * with this prototype.
 *
 * Returns: %TRUE if dialog is ready, %FALSE otherwise.
 */
typedef gboolean      (* GpModuleSetupAbout)    (GtkAboutDialog *dialog,
                                                 const gchar    *applet);

/**
 * gp_module_get_abi_version:
 *
 * Required API for GNOME Panel modules to implement. This function must
 * always return %GP_MODULE_ABI_VERSION.
 *
 * Returns: the module ABI version.
 */
guint32       gp_module_get_abi_version (void);

/**
 * gp_module_get_info:
 * @abi_version: (out): return location for module ABI version
 *
 * Required API for GNOME Panel modules to implement, see %GpModuleGetInfo.
 *
 * Returns: (transfer full): a newly created #GpModuleInfo.
 */
GpModuleInfo *gp_module_get_info        (guint32        *abi_version);

/**
 * gp_module_load:
 * @module: a #GTypeModule
 *
 * Required API for GNOME Panel modules to implement.
 */
void          gp_module_load            (GTypeModule    *module);

/**
 * gp_module_unload:
 * @module: a #GTypeModule
 *
 * Required API for GNOME Panel modules to implement.
 */
void          gp_module_unload          (GTypeModule    *module);

/**
 * gp_module_get_applet_info:
 * @applet: the applet id
 *
 * Required API for GNOME Panel modules to implement, see
 * %GpModuleGetAppletInfo.
 *
 * Returns: (transfer full): a newly created #GpAppletInfo.
 */
GpAppletInfo *gp_module_get_applet_info (const gchar    *applet);

/**
 * gp_module_get_applet_type:
 * @applet: the applet id
 *
 * Required API for GNOME Panel modules to implement, see
 * %GpModuleGetAppletType.
 *
 * Returns: a previously registered #GType.
 */
GType         gp_module_get_applet_type (const gchar    *applet);

/**
 * gp_module_setup_about:
 * @dialog: a #GtkAboutDialog
 * @applet: the applet id
 *
 * Optional API for GNOME Panel modules to implement, see
 * %GpModuleSetupAbout.
 *
 * Returns: %TRUE if dialog is ready, %FALSE otherwise.
 */
gboolean      gp_module_setup_about     (GtkAboutDialog *dialog,
                                         const gchar    *applet);

G_END_DECLS

#endif
