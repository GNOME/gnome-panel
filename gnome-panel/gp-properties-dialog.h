/*
 * Copyright (C) 2016 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GP_PROPERTIES_DIALOG_H
#define GP_PROPERTIES_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GP_TYPE_PROPERTIES_DIALOG gp_properties_dialog_get_type ()
G_DECLARE_FINAL_TYPE (GpPropertiesDialog, gp_properties_dialog,
                      GP, PROPERTIES_DIALOG, GtkWindow)

GtkWidget *gp_properties_dialog_new (const gchar *toplevel_id);

G_END_DECLS

#endif
