/*
 * panel-glib.c: various small extensions to glib
 *
 * Copyright (C) 2008 Novell, Inc.
 *
 * Originally based on code from panel-util.c (there was no relevant copyright
 * header at the time), but the code was:
 * Copyright (C) Novell, Inc. (for the panel_g_utf8_strstrcase() code)
 * Copyright (C) Dennis Cranston (for the panel_g_lookup_in_data_dirs() code)
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

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "panel-glib.h"

typedef char * (*LookupInDir) (const char *basename, const char *dir);

static char *
_lookup_in_dir (const char *basename,
		const char *dir)
{
	char *path;

	path = g_build_filename (dir, basename, NULL);
	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		g_free (path);
		return NULL;
	}

	return path;
}

static char *
_panel_g_lookup_in_data_dirs_internal (const char *basename,
				       LookupInDir lookup)
{
	const char * const *system_data_dirs;
	const char          *user_data_dir;
	char                *retval;
	int                  i;

	user_data_dir    = g_get_user_data_dir ();
	system_data_dirs = g_get_system_data_dirs ();

	if ((retval = lookup (basename, user_data_dir)))
		return retval;

	for (i = 0; system_data_dirs[i]; i++)
		if ((retval = lookup (basename, system_data_dirs[i])))
			return retval;

	return NULL;
}

char *
panel_g_lookup_in_data_dirs (const char *basename)
{
	return _panel_g_lookup_in_data_dirs_internal (basename,
						      _lookup_in_dir);
}

/* Copied from evolution-data-server/libedataserver/e-util.c:
 * e_util_unicode_get_utf8() */
static const char *
_unicode_get_utf8 (const char *text, gunichar *out)
{
	*out = g_utf8_get_char (text);
	return (*out == (gunichar)-1) ? NULL : g_utf8_next_char (text);
}

/* Copied from evolution-data-server/libedataserver/e-util.c:
 * e_util_utf8_strstrcase() */
const char *
panel_g_utf8_strstrcase (const char *haystack, const char *needle)
{
	gunichar *nuni;
	gunichar unival;
	gint nlen;
	const char *o, *p;

	if (haystack == NULL) return NULL;
	if (needle == NULL) return NULL;
	if (strlen (needle) == 0) return haystack;
	if (strlen (haystack) == 0) return NULL;

	nuni = g_alloca (sizeof (gunichar) * strlen (needle));

	nlen = 0;
	for (p = _unicode_get_utf8 (needle, &unival);
	     p && unival;
	     p = _unicode_get_utf8 (p, &unival)) {
		nuni[nlen++] = g_unichar_tolower (unival);
	}
	/* NULL means there was illegal utf-8 sequence */
	if (!p) return NULL;

	o = haystack;
	for (p = _unicode_get_utf8 (o, &unival);
	     p && unival;
	     p = _unicode_get_utf8 (p, &unival)) {
		gunichar sc;
		sc = g_unichar_tolower (unival);
		/* We have valid stripped char */
		if (sc == nuni[0]) {
			const char *q = p;
			gint npos = 1;
			while (npos < nlen) {
				q = _unicode_get_utf8 (q, &unival);
				if (!q || !unival) return NULL;
				sc = g_unichar_tolower (unival);
				if (sc != nuni[npos]) break;
				npos++;
			}
			if (npos == nlen) {
				return o;
			}
		}
		o = p;
	}

	return NULL;
}

static gboolean
panel_ensure_dir (const char *dirname)
{
	char *parsed, *p;

	if (dirname == NULL)
		return FALSE;

	parsed = g_strdup (dirname);

	if (g_file_test (parsed, G_FILE_TEST_IS_DIR)) {
		g_free (parsed);
		return TRUE;
	}

	p = strchr (parsed, '/');
	if (p == parsed)
		p = strchr (p+1, '/');

	while (p != NULL) {
		*p = '\0';
		if (g_mkdir (parsed, 0700) != 0 &&
		    errno != EEXIST && errno != ENOSYS) {
			g_free (parsed);
			return FALSE;
		}
		*p = '/';
		p = strchr (p+1, '/');
	}

	if (g_mkdir (parsed, 0700) != 0 &&
	    errno != EEXIST && errno != ENOSYS) {
		g_free (parsed);
		return FALSE;
	}

	g_free (parsed);
	return TRUE;
}

static char *
panel_util_get_from_personal_path (const char *file)
{
	return g_build_filename (g_get_user_config_dir (),
				 "gnome-panel", file, NULL);
}

static char *
panel_launcher_get_personal_path (void)
{
	return panel_util_get_from_personal_path ("launchers");
}

char *
panel_make_full_path (const char *dir,
		      const char *filename)
{
	char *retval;
	char *freeme = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	if (!dir) {
		freeme = panel_launcher_get_personal_path ();
		dir = freeme;
	}

	/* Make sure the launcher directory exists */
	if (!g_file_test (dir, G_FILE_TEST_EXISTS))
		panel_ensure_dir (dir);

	retval = g_build_filename (dir, filename, NULL);

	g_free (freeme);

	return retval;
}

char *
panel_util_get_icon_name_from_g_icon (GIcon *gicon)
{
	const char * const *names;
	GtkIconTheme *icon_theme;
	int i;

	if (!G_IS_THEMED_ICON (gicon))
		return NULL;

	names = g_themed_icon_get_names (G_THEMED_ICON (gicon));
	icon_theme = gtk_icon_theme_get_default ();

	for (i = 0; names[i] != NULL; i++) {
		if (gtk_icon_theme_has_icon (icon_theme, names[i]))
			return g_strdup (names[i]);
	}

	return NULL;
}
