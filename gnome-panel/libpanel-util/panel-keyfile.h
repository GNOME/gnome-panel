/*
 * panel-keyfile.h: GKeyFile extensions
 *
 * Copyright (C) 2008 Novell, Inc.
 *
 * Based on code from panel-util.h (there was no copyright header at the time)
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

#ifndef PANEL_KEYFILE_H
#define PANEL_KEYFILE_H

#include "glib.h"

G_BEGIN_DECLS

GKeyFile *panel_key_file_new_desktop  (void);
gboolean  panel_key_file_to_file      (GKeyFile       *keyfile,
				       const gchar    *file,
				       GError        **error);

gboolean panel_key_file_get_boolean   (GKeyFile       *keyfile,
				       const gchar    *key,
				       gboolean        default_value);
#define panel_key_file_get_string(key_file, key) \
	 g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP, key, NULL)
#define panel_key_file_get_locale_string(key_file, key) \
	 g_key_file_get_locale_string(key_file, G_KEY_FILE_DESKTOP_GROUP, key, NULL, NULL)

#define panel_key_file_set_boolean(key_file, key, value) \
	 g_key_file_set_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, key, value)
#define panel_key_file_set_string(key_file, key, value) \
	 g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, key, value)
void    panel_key_file_set_locale_string (GKeyFile    *keyfile,
					  const gchar *key,
					  const gchar *value);

G_END_DECLS

#endif /* PANEL_KEYFILE_H */
