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

gboolean
panel_gconf_recursive_unset (const gchar  *dir,
                             GError     **error)
{
        return gconf_client_recursive_unset (panel_gconf_get_client (), dir,
					     GCONF_UNSET_INCLUDING_SCHEMA_NAMES,
					     NULL);
}
