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

GP_EXPORT
#define GP_TYPE_INITIAL_SETUP_DIALOG (gp_initial_setup_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GpInitialSetupDialog, gp_initial_setup_dialog,
                      GP, INITIAL_SETUP_DIALOG, GtkWindow)

GP_EXPORT
GpInitialSetupDialog *gp_initial_setup_dialog_new          (void);

GP_EXPORT
void                  gp_initial_setup_dialog_add_callback (GpInitialSetupDialog   *self,
                                                            GpInitialSetupCallback  callback,
                                                            gpointer                user_data,
                                                            GDestroyNotify          free_func);

GP_EXPORT
GVariant             *gp_initial_setup_dialog_get_settings (GpInitialSetupDialog   *self);

GP_EXPORT
void                  gp_initial_setup_dialog_set_settings (GpInitialSetupDialog   *self,
                                                            GVariant               *settings);

G_END_DECLS

#endif
