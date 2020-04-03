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

#ifndef GP_APPLET_INFO_H
#define GP_APPLET_INFO_H

#include "gp-initial-setup-dialog.h"

#include <glib.h>
#include <gtk/gtk.h>

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

GpAppletInfo *gp_applet_info_new                      (GpGetAppletTypeFunc       func,
                                                       const gchar              *name,
                                                       const gchar              *description,
                                                       const gchar              *icon_name);

void          gp_applet_info_set_initial_setup_dialog (GpAppletInfo             *info,
                                                       GpInitialSetupDialogFunc  func);

void          gp_applet_info_set_help_uri             (GpAppletInfo             *info,
                                                       const gchar              *help_uri);

void          gp_applet_info_set_about_dialog         (GpAppletInfo             *info,
                                                       GpAboutDialogFunc         func);

void          gp_applet_info_set_backends             (GpAppletInfo             *info,
                                                       const gchar              *backends);

G_END_DECLS

#endif
