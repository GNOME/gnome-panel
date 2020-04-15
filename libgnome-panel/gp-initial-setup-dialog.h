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

#ifndef GP_INITIAL_SETUP_DIALOG_H
#define GP_INITIAL_SETUP_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * GpInitialSetupDialog:
 *
 * The #GpInitialSetupDialog struct is an opaque data structure.
 */
typedef struct _GpInitialSetupDialog GpInitialSetupDialog;

void      gp_initital_setup_dialog_add_content_widget (GpInitialSetupDialog *dialog,
                                                       GtkWidget            *content,
                                                       gpointer              user_data,
                                                       GDestroyNotify        free_func);

GVariant *gp_initital_setup_dialog_get_setting        (GpInitialSetupDialog *dialog,
                                                       const char           *key);

void      gp_initital_setup_dialog_set_setting        (GpInitialSetupDialog *dialog,
                                                       const gchar          *key,
                                                       GVariant             *value);

void      gp_initital_setup_dialog_set_done           (GpInitialSetupDialog *dialog,
                                                       gboolean              done);

G_END_DECLS

#endif
