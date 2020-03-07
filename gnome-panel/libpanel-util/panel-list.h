/*
 * panel-list.h: GList & GSList extensions
 *
 * Copyright (C) 2008 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef PANEL_LIST_H
#define PANEL_LIST_H

#include <glib.h>

G_BEGIN_DECLS

GList *panel_g_list_insert_before (GList        *list,
				   GList        *sibling,
				   GList        *link);
GList *panel_g_list_swap_next     (GList        *list,
				   GList        *dl);
GList *panel_g_list_swap_prev     (GList        *list,
				   GList        *dl);

G_END_DECLS

#endif /* PANEL_LIST_H */
