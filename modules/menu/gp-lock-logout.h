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

#ifndef GP_LOCK_LOGOUT_H
#define GP_LOCK_LOGOUT_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GP_TYPE_LOCK_LOGOUT (gp_lock_logout_get_type ())
G_DECLARE_FINAL_TYPE (GpLockLogout, gp_lock_logout, GP, LOCK_LOGOUT, GObject)

GpLockLogout *gp_lock_logout_new            (void);

void          gp_lock_logout_append_to_menu (GpLockLogout *lock_logout,
                                             GtkMenu      *menu);

G_END_DECLS

#endif
