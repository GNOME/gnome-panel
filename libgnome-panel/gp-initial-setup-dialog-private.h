/*
 * Copyright (C) 2018 Alberts Muktupāvels
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

#ifndef GP_INITIAL_SETUP_DIALOG_PRIVATE_H
#define GP_INITIAL_SETUP_DIALOG_PRIVATE_H

#include "gp-initial-setup-dialog.h"

G_BEGIN_DECLS

typedef void  (* GpInitialSetupCallback) (GpInitialSetupDialog *dialog,
                                          gboolean              canceled,
                                          gpointer              user_data);

#define GP_TYPE_INITIAL_SETUP_DIALOG (gp_initital_setup_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GpInitialSetupDialog, gp_initital_setup_dialog,
                      GP, INITIAL_SETUP_DIALOG, GtkWindow)

GpInitialSetupDialog *gp_initital_setup_dialog_new          (void);

void                  gp_initital_setup_dialog_add_callback (GpInitialSetupDialog   *dialog,
                                                             GpInitialSetupCallback  callback,
                                                             gpointer                user_data,
                                                             GDestroyNotify          free_func);

GVariant             *gp_initital_setup_dialog_get_settings (GpInitialSetupDialog   *dialog);

void                  gp_initital_setup_dialog_set_settings (GpInitialSetupDialog   *dialog,
                                                             GVariant               *settings);

G_END_DECLS

#endif
