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

#ifndef GP_USER_MENU_H
#define GP_USER_MENU_H

#include "gp-menu-utils.h"

G_BEGIN_DECLS

#define GP_TYPE_USER_MENU (gp_user_menu_get_type ())
G_DECLARE_FINAL_TYPE (GpUserMenu, gp_user_menu, GP, USER_MENU, GtkMenu)

GtkWidget *gp_user_menu_new             (void);

void       gp_user_menu_set_append_func (GpUserMenu            *user_menu,
                                         GpAppendMenuItemsFunc  append_func,
                                         gpointer               user_data);

void       gp_user_menu_reload          (GpUserMenu            *user_menu);

G_END_DECLS

#endif
