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
 *
 * |[<!-- language="C" -->
 * static GpAppletInfo *
 * example_get_applet_info (const gchar *applet)
 * {
 *   GpAppletInfo *info;
 *
 *   if (g_strcmp0 (applet, "example1") == 0)
 *     {
 *       info = gp_applet_info_new (_("Example 1 name"),
 *                                  _("Example 1 description"),
 *                                  "example1-icon");
 *     }
 *   else if (g_strcmp0 (applet, "example2") == 0)
 *     {
 *       info = gp_applet_info_new (_("Example 2 name"),
 *                                  _("Example 2 description"),
 *                                  "example2-icon");
 *
 *       gp_applet_info_set_backends (info, "x11");
 *     }
 *   else
 *     {
 *       info = NULL;
 *     }
 *
 *   return info;
 * }
 *
 * static GType
 * example_get_applet_type (const gchar *applet)
 * {
 *   if (g_strcmp0 (applet, "example1") == 0)
 *     {
 *       return EXAMPLE_TYPE_EXAMPLE1;
 *     }
 *   else if (g_strcmp0 (applet, "example2") == 0)
 *     {
 *       return EXAMPLE_TYPE_EXAMPLE2;
 *     }
 *
 *   return G_TYPE_NONE;
 * }
 *
 * static const gchar *
 * example_get_applet_from_iid (const gchar *iid)
 * {
 *   if (g_strcmp0 (iid, "ExampleAppletFactory::Example1Applet") == 0)
 *     {
 *       return "example1";
 *     }
 *   else if (g_strcmp0 (iid, "ExampleAppletFactory::Example2Applet") == 0)
 *     {
 *       return "example2";
 *     }
 *
 *   return NULL;
 * }
 *
 * static gboolean
 * example_setup_about (GtkAboutDialog *dialog,
 *                      const gchar    *applet)
 * {
 *   if (g_strcmp0 (applet, "example1") == 0)
 *     {
 *       gtk_about_dialog_set_comments (about, "...");
 *       gtk_about_dialog_set_copyright (about, "...");
 *       // ...
 *
 *       return TRUE;
 *     }
 *
 *   return FALSE;
 * }
 *
 * guint32
 * gp_module_get_abi_version (void)
 * {
 *   return GP_MODULE_ABI_VERSION;
 * }
 *
 * GpModuleInfo *
 * gp_module_get_module_info (void)
 * {
 *   GpModuleInfo *info;
 *
 *   bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
 *   bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
 *
 *   info = gp_module_info_new ("org.example.example",
 *                              PACKAGE_VERSION, GETTEXT_PACKAGE);
 *
 *   gp_module_info_set_applets (info, "example1", "example2", NULL);
 *
 *   return info;
 * }
 *
 * void
 * gp_module_get_applet_vtable (GpAppletVTable *vtable)
 * {
 *   *vtable = (GpAppletVTable) {
 *     example_get_applet_info,
 *     example_get_applet_type,
 *     example_get_applet_from_iid, // or NULL if not needed
 *     example_setup_about // or NULL if not needed
 *   };
 * }
 * ]|
 */

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
