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

#ifndef GP_MENU_H
#define GP_MENU_H

#include "gp-menu-utils.h"

G_BEGIN_DECLS

#define GP_TYPE_MENU (gp_menu_get_type ())
G_DECLARE_FINAL_TYPE (GpMenu, gp_menu, GP, MENU, GtkMenu)

GtkWidget *gp_menu_new             (const gchar           *name,
                                    gboolean               required);

void       gp_menu_set_path        (GpMenu                *menu,
                                    const gchar           *path);

GIcon     *gp_menu_get_icon        (GpMenu                *menu);

void       gp_menu_set_append_func (GpMenu                *menu,
                                    GpAppendMenuItemsFunc  append_func,
                                    gpointer               user_data);

void       gp_menu_reload          (GpMenu                *menu);

G_END_DECLS

#endif
