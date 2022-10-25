/*
 * panel-gsettings.h: various small extensions to gsettings
 *
 * Copyright (C) 2011 Novell, Inc.
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

#ifndef __PANEL_GSETTINGS_H__
#define __PANEL_GSETTINGS_H__

#include <gio/gio.h>

G_BEGIN_DECLS

gboolean panel_gsettings_is_valid_keyname (const gchar  *key,
                                           GError      **error);

gboolean panel_gsettings_append_strv      (GSettings   *settings,
                                           const gchar *key,
                                           const gchar *value);

gboolean panel_gsettings_remove_all_from_strv (GSettings   *settings,
                                               const gchar *key,
                                               const gchar *value);

G_END_DECLS

#endif /* __PANEL_GSETTINGS_H__ */
