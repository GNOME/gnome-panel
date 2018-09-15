/*
 * Copyright (C) 2018 Alberts Muktupāvels
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

#ifndef GP_MENU_UTILS_H
#define GP_MENU_UTILS_H

#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void (* GpAppendMenuItemsFunc) (GtkMenu  *menu,
                                        gpointer  user_data);

void   gp_menu_utils_app_info_launch       (GDesktopAppInfo *app_info);

void   gp_menu_utils_launch_uri            (const gchar     *uri);

GIcon *gp_menu_utils_get_icon_for_file     (GFile           *file);

gchar *gp_menu_utils_get_label_for_file    (GFile           *file);

void   gp_menu_utils_show_error_dialog     (const gchar     *message,
                                            GError          *error);

gchar *gp_menu_utils_get_user_name         (void);

gchar *gp_menu_utils_get_applications_menu (void);

void   append_separator_if_needed          (GtkMenu         *menu);

G_END_DECLS

#endif
