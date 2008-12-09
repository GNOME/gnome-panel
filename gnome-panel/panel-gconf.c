/*
 * panel-gconf.c: panel gconf utility methods
 *
 * Copyright (C) 2001 - 2003 Sun Microsystems, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      Glynn Foster <glynn.foster@sun.com>
 */

#include <config.h>

#include "panel-gconf.h"

#include <string.h>
#include <glib.h>
#include <gconf/gconf-client.h>

#include <libpanel-util/panel-cleanup.h>

#undef PANEL_GCONF_DEBUG

GConfClient *
panel_gconf_get_client (void)
{
	static GConfClient *panel_gconf_client = NULL;

	if (!panel_gconf_client) {
		panel_gconf_client = gconf_client_get_default ();
		panel_cleanup_register (panel_cleanup_unref_and_nullify,
					&panel_gconf_client);
	}

	return panel_gconf_client;
}

/*
 * panel_gconf_sprintf:
 * @format: the format string. See sprintf() documentation.
 * @...: the arguments to be inserted.
 *
 * This is a version of sprintf using a static buffer which is
 * intended for use in generating the full gconf key for all panel
 * config keys.
 * Note, you should not free the return value from this function and
 * you should realize that the return value will get overwritten or
 * freed by a subsequent call to this function.
 *
 * Return Value: a pointer to the static string buffer.
 */
const char *
panel_gconf_sprintf (const char *format,
		     ...)
{
	static char *buffer = NULL;
	static int   buflen = 128;
	va_list      args;
	int          len;

	if (!buffer)
		buffer = g_new (char, buflen);

	va_start (args, format);
	len = g_vsnprintf (buffer, buflen, format, args);

	if (len >= buflen) {
		int i;

		/* Round up length to the nearest power of 2 */
		for (i = 0; len != 1; i++, len >>= 1);

		buflen = len << (i + 1);
		g_assert (buflen > 0);

		g_free (buffer);
		buffer = g_new (char, buflen);

		va_start (args, format);
		len = g_vsnprintf (buffer, buflen, format, args);

		g_assert (len < buflen);
	}

	va_end (args);

	return buffer;
}

const char *
panel_gconf_key_type_to_id_list (PanelGConfKeyType type)
{
	char *retval;

	switch (type) {
	case PANEL_GCONF_TOPLEVELS:
		retval = "toplevel_id_list";
		break;
	case PANEL_GCONF_APPLETS:
		retval = "applet_id_list";
		break;
	case PANEL_GCONF_OBJECTS:
		retval = "object_id_list";
		break;
	default:
		retval = NULL;
		g_assert_not_reached ();
		break;
	}

	return retval;
}

const char * 
panel_gconf_global_key (const char *key)
{
	return panel_gconf_sprintf ("/apps/panel/global/%s", key);
}

const char *
panel_gconf_general_key (const char *key)
{
	return panel_gconf_sprintf (PANEL_CONFIG_DIR "/general/%s", key);
}

const char *
panel_gconf_full_key (PanelGConfKeyType  type,
		      const char        *id,
		      const char        *key)
{
	char *subdir = NULL;

	switch (type) {
	case PANEL_GCONF_TOPLEVELS:
		subdir = "toplevels";
		break;
	case PANEL_GCONF_OBJECTS:
		subdir = "objects";
		break;
	case PANEL_GCONF_APPLETS:
		subdir = "applets";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return panel_gconf_sprintf (PANEL_CONFIG_DIR "/%s/%s/%s",
				    subdir, id, key);
}

const char *
panel_gconf_basename (const char *key)
{
	char *retval;

	g_return_val_if_fail (key != NULL, NULL);

	retval = strrchr (key, '/');

	return retval ? retval + 1 : NULL;
}

char *
panel_gconf_dirname (const char *key)
{
	char *retval;
	int   len;

	g_return_val_if_fail (key != NULL, NULL);

	retval = strrchr (key, '/');
	g_assert (retval != NULL);

	len = retval - key;
	g_assert (len > 0);

	retval = g_new0 (char, len + 1);
	memcpy (retval, key, len);

	return retval;
}

static void
panel_notify_object_dead (guint notify_id)
{
	GConfClient *client;

	client = panel_gconf_get_client ();

	gconf_client_notify_remove (client, notify_id);
}

guint
panel_gconf_notify_add_while_alive (const char            *key, 
				    GConfClientNotifyFunc  notify_func, 
				    GObject               *alive_object)
{
	GConfClient *client;
	guint        notify_id;

	g_return_val_if_fail (G_IS_OBJECT (alive_object), 0);

	client = panel_gconf_get_client ();

	notify_id = gconf_client_notify_add (client, key, notify_func,
					     alive_object, NULL, NULL);

	if (notify_id > 0)
		g_object_weak_ref (alive_object,
				   (GWeakNotify) panel_notify_object_dead,
				   GUINT_TO_POINTER (notify_id));

	return notify_id;
}

void
panel_gconf_copy_dir (GConfClient  *client,
		      const char   *src_dir,
		      const char   *dest_dir)
{
	GSList *list, *l;

	list = gconf_client_all_entries (client, src_dir, NULL);
	for (l = list; l; l = l->next) {
		GConfEntry *entry = l->data;
		const char *key;
		char       *tmp;

		tmp = g_path_get_basename (gconf_entry_get_key (entry));
		key = panel_gconf_sprintf ("%s/%s", dest_dir, tmp);
		g_free (tmp);

		gconf_engine_associate_schema (client->engine,
					       key,
					       gconf_entry_get_schema_name (entry),
					       NULL);

		if (!gconf_entry_get_is_default (entry) && entry->value)
			gconf_client_set (client, key, entry->value, NULL);

		gconf_entry_unref (entry);
	}
	g_slist_free (list);

	list = gconf_client_all_dirs (client, src_dir, NULL);
	for (l = list; l; l = l->next) {
		char *subdir = l->data;
		char *src_subdir;
		char *dest_subdir;
		char *tmp;

		tmp = g_path_get_basename (subdir);
		src_subdir  = gconf_concat_dir_and_key (src_dir,  tmp);
		dest_subdir = gconf_concat_dir_and_key (dest_dir, tmp);
		g_free (tmp);

		panel_gconf_copy_dir (client, src_subdir, dest_subdir);

		g_free (src_subdir);
		g_free (dest_subdir);
		g_free (subdir);
	}

	g_slist_free (list);
}

void
panel_gconf_associate_schemas_in_dir (GConfClient  *client,
				      const char   *profile_dir,
				      const char   *schema_dir)
{
	GSList *list, *l;

#ifdef PANEL_GCONF_DEBUG
	g_print ("associating schemas in %s to %s\n", schema_dir, profile_dir);
#endif

	list = gconf_client_all_entries (client, schema_dir, NULL);
	for (l = list; l; l = l->next) {
		GConfEntry *entry = l->data;
		const char *key;
		char       *tmp;

		tmp = g_path_get_basename (gconf_entry_get_key (entry));

		key = panel_gconf_sprintf ("%s/%s", profile_dir, tmp);

		g_free (tmp);

		gconf_engine_associate_schema (
			client->engine, key, gconf_entry_get_key (entry), NULL);

		gconf_entry_unref (entry);
	}

	g_slist_free (list);

	list = gconf_client_all_dirs (client, schema_dir, NULL);
	for (l = list; l; l = l->next) {
		char *subdir = l->data;
		char *prefs_subdir;
		char *schema_subdir;
		char *tmp;

		tmp = g_path_get_basename (subdir);

		prefs_subdir  = g_strdup_printf ("%s/%s", profile_dir, tmp);
		schema_subdir = g_strdup_printf ("%s/%s", schema_dir, tmp);

		panel_gconf_associate_schemas_in_dir (
			client, prefs_subdir, schema_subdir);

		g_free (prefs_subdir);
		g_free (schema_subdir);
		g_free (subdir);
		g_free (tmp);
	}

	g_slist_free (list);
}

gint
panel_gconf_value_strcmp (gconstpointer a,
			  gconstpointer b)
{
	const char *str_a;
	const char *str_b;

	if (a == b || !a || !b)
		return 0;

	str_a = gconf_value_get_string (a);
	str_b = gconf_value_get_string (b);

	return strcmp (str_a, str_b);
}
