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

#ifndef GP_APPLET_INFO_H
#define GP_APPLET_INFO_H

#include "gp-initial-setup-dialog.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome-panel/gp-lockdown.h>
#include <libgnome-panel/gp-macros.h>

G_BEGIN_DECLS

/**
 * GpAppletInfo:
 *
 * The #GpAppletInfo struct is an opaque data structure.
 */
typedef struct _GpAppletInfo GpAppletInfo;

/**
 * GpGetAppletTypeFunc:
 *
 * Function for getting #GType of applet.
 *
 * Returns: the #GType of applet.
 */
typedef GType (* GpGetAppletTypeFunc)    (void);

/**
 * GpInitialSetupDialogFunc:
 * @dialog: a #GtkAboutDialog
 *
 * Function for setting up initial setup dialog.
 */
typedef void  (* GpInitialSetupDialogFunc) (GpInitialSetupDialog *dialog);

/**
 * GpAboutDialogFunc:
 * @dialog: a #GtkAboutDialog
 *
 * Function for setting up about dialog.
 *
 * The dialog will be already filled in with following information - version
 * program name, comments and logo icon name.
 *
 * Version will be same that was set with gp_module_set_version(). Other
 * three fields are information from gp_applet_info_new().
 */
typedef void  (* GpAboutDialogFunc) (GtkAboutDialog *dialog);

/**
 * GpIsDisabledFunc:
 * @flags: a #GpLockdownFlags with active lockdowns
 * @reason: (out) (transfer full): return location for reason
 *
 * This function must return %TRUE if applet must be fully disabled (applet
 * will not be loaded nor user will be able to add it to panel). Function also
 * should return reason why applet is disabled.
 *
 * If applet is usable with some active lockdowns it should return %FALSE and
 * use #GpApplet:lockdows property to adjust behaviour/functionality.
 *
 * Returns: %TRUE if applet should be disabled.
 */
typedef gboolean (* GpIsDisabledFunc) (GpLockdownFlags   flags,
                                       char            **reason);


GP_EXPORT
GpAppletInfo *gp_applet_info_new                      (GpGetAppletTypeFunc       func,
                                                       const gchar              *name,
                                                       const gchar              *description,
                                                       const gchar              *icon_name);

GP_EXPORT
void          gp_applet_info_set_initial_setup_dialog (GpAppletInfo             *info,
                                                       GpInitialSetupDialogFunc  func);

GP_EXPORT
void          gp_applet_info_set_help_uri             (GpAppletInfo             *info,
                                                       const gchar              *help_uri);

GP_EXPORT
void          gp_applet_info_set_about_dialog         (GpAppletInfo             *info,
                                                       GpAboutDialogFunc         func);

GP_EXPORT
void          gp_applet_info_set_backends             (GpAppletInfo             *info,
                                                       const gchar              *backends);

GP_EXPORT
void          gp_applet_info_set_is_disabled          (GpAppletInfo             *info,
                                                       GpIsDisabledFunc          func);

G_END_DECLS

#endif
