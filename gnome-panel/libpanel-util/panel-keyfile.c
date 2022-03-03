/*
 * panel-keyfile.c: GKeyFile extensions
 *
 * Copyright (C) 2008 Novell, Inc.
 *
 * Based on code from panel-util.c (there was no copyright header at the time)
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

#include <string.h>
#include <sys/stat.h>

#include <glib.h>
#include <gio/gio.h>

#include "panel-keyfile.h"

#define KEYFILE_TRUSTED_SHEBANG "#!/usr/bin/env xdg-open\n"

GKeyFile *
panel_key_file_new_desktop (void)
{
	GKeyFile *retval;

	retval = g_key_file_new ();

	//FIXME? g_key_file_set_string (retval, G_KEY_FILE_DESKTOP_GROUP, "Name", _("No Name"));
	g_key_file_set_string (retval, G_KEY_FILE_DESKTOP_GROUP, "Version", "1.0");

	return retval;
}

static void
_panel_key_file_make_executable (const gchar *path)
{
	GFile     *file;
	GFileInfo *info;
	guint32    current_perms;
	guint32    new_perms;

	file = g_file_new_for_path (path);

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_TYPE","
				  G_FILE_ATTRIBUTE_UNIX_MODE,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL,
				  NULL);

	if (info == NULL) {
		g_warning ("Cannot mark %s executable", path);
		g_object_unref (file);
		return;
	}

	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE)) {
		current_perms = g_file_info_get_attribute_uint32 (info,
								  G_FILE_ATTRIBUTE_UNIX_MODE);
		new_perms = current_perms | S_IXGRP | S_IXUSR | S_IXOTH;
		if ((current_perms != new_perms) &&
		    !g_file_set_attribute_uint32 (file,
			    			  G_FILE_ATTRIBUTE_UNIX_MODE,
						  new_perms,
						  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						  NULL, NULL))
			g_warning ("Cannot mark %s executable", path);
	}

	g_object_unref (info);
	g_object_unref (file);
}

//FIXME: kill this when bug #309224 is fixed
gboolean
panel_key_file_to_file (GKeyFile     *keyfile,
			const gchar  *file,
			GError      **error)
{
	gchar   *filename;
	GError  *write_error;
	gchar   *data;
	gsize    length;
	gboolean res;

	g_return_val_if_fail (keyfile != NULL, FALSE);
	g_return_val_if_fail (file != NULL, FALSE);

	write_error = NULL;
	data = g_key_file_to_data (keyfile, &length, &write_error);
	if (write_error) {
		g_propagate_error (error, write_error);
		return FALSE;
	}

	if (!g_path_is_absolute (file))
		filename = g_filename_from_uri (file, NULL, &write_error);
	else
		filename = g_filename_from_utf8 (file, -1, NULL, NULL,
						 &write_error);

	if (write_error) {
		g_propagate_error (error, write_error);
		g_free (filename);
		g_free (data);
		return FALSE;
	}

	if (!g_str_has_prefix (data, "#!")) {
		gchar *new_data;
		gsize  new_length;

		new_length = length + strlen (KEYFILE_TRUSTED_SHEBANG);
		new_data = g_malloc (new_length);

		strcpy (new_data, KEYFILE_TRUSTED_SHEBANG);
		memcpy (new_data + strlen (KEYFILE_TRUSTED_SHEBANG),
			data, length);

		g_free (data);
		data = new_data;
		length = new_length;
	}

	res = g_file_set_contents (filename, data, length, &write_error);

	if (write_error) {
		g_propagate_error (error, write_error);
		g_free (data);
		g_free (filename);
		return FALSE;
	}

	g_free (data);

	_panel_key_file_make_executable (filename);
	g_free (filename);

	return res;
}

gboolean
panel_key_file_get_boolean (GKeyFile    *keyfile,
			    const gchar *key,
			    gboolean     default_value)
{
	GError   *error;
	gboolean  retval;

	error = NULL;
	retval = g_key_file_get_boolean (keyfile, G_KEY_FILE_DESKTOP_GROUP, key, &error);
	if (error != NULL) {
		retval = default_value;
		g_error_free (error);
	}

	return retval;
}

void
panel_key_file_set_locale_string (GKeyFile    *keyfile,
				  const gchar *key,
				  const gchar *value)
{
	const char         *locale;
	const char * const *langs_pointer;
	int                 i;

	locale = NULL;
	langs_pointer = g_get_language_names ();
	for (i = 0; langs_pointer[i] != NULL; i++) {
		/* find first without encoding  */
		if (strchr (langs_pointer[i], '.') == NULL) {
			locale = langs_pointer[i]; 
			break;
		}
	}

	if (locale)
		g_key_file_set_locale_string (keyfile, G_KEY_FILE_DESKTOP_GROUP,
					      key, locale, value);
	else
		g_key_file_set_string (keyfile, G_KEY_FILE_DESKTOP_GROUP,
				       key, value);
}
