/*
 * panel-shell.c: panel shell interface implementation
 *
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2008 Red Hat, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Jacob Berkman <jacob@ximian.com>
 *      Colin Walters <walters@verbum.org>
 *      Vincent Untz <vuntz@gnome.org>
 */

#include <config.h>
#include <glib/gi18n.h>

#include <dbus/dbus-glib.h>

#include <libpanel-util/panel-cleanup.h>

#include "panel-profile.h"
#include "panel-session.h"

#include "panel-shell.h"

#define PANEL_DBUS_SERVICE "org.gnome.Panel"

static DBusGConnection *dbus_connection = NULL;
static DBusGProxy      *session_bus = NULL;

static void
panel_shell_on_name_lost (DBusGProxy *proxy,
			  const char *name,
			  gpointer   user_data)
{
	if (strcmp (name, PANEL_DBUS_SERVICE) != 0)
		return;

	/* We lost our DBus name, and there is something replacing us.
	 * Tell the SM not to restart us automatically, then exit. */
	g_printerr ("Panel leaving: a new panel shell is starting.\n");

	panel_session_do_not_restart ();
	panel_shell_quit ();
}

static void
panel_shell_cleanup (gpointer data)
{
	if (dbus_connection != NULL) {
		dbus_g_connection_unref (dbus_connection);
		dbus_connection = NULL;
	}

	if (session_bus != NULL) {
		g_object_unref (session_bus);
		session_bus = NULL;
	}
}

gboolean
panel_shell_register (gboolean replace)
{
	GError   *error;
	guint     request_name_reply;
	guint32   flags;
	gboolean  retval;

	if (session_bus != NULL)
		return TRUE;

	retval = FALSE;

	panel_cleanup_register (PANEL_CLEAN_FUNC (panel_shell_cleanup), NULL);

	dbus_connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
	if (dbus_connection == NULL) {
		g_warning ("Cannot register the panel shell: cannot connect "
			   "to the session bus.");
		goto register_out;
	}

	session_bus = dbus_g_proxy_new_for_name (dbus_connection,
						 "org.freedesktop.DBus",
						 "/org/freedesktop/DBus",
						 "org.freedesktop.DBus");
	if (session_bus == NULL) {
		g_warning ("Cannot register the panel shell: cannot connect "
			   "to the session bus.");
		goto register_out;
	}

	dbus_g_proxy_add_signal (session_bus,
				 "NameLost",
				 G_TYPE_STRING,
				 G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (session_bus,
				     "NameLost",
				     G_CALLBACK (panel_shell_on_name_lost),
				     NULL,
				     NULL);

	flags = DBUS_NAME_FLAG_DO_NOT_QUEUE|DBUS_NAME_FLAG_ALLOW_REPLACEMENT;
	if (replace)
		flags |= DBUS_NAME_FLAG_REPLACE_EXISTING;
	error = NULL;

	if (!dbus_g_proxy_call (session_bus,
				"RequestName",
				&error,
				G_TYPE_STRING, PANEL_DBUS_SERVICE,
				G_TYPE_UINT, flags,
				G_TYPE_INVALID,
				G_TYPE_UINT, &request_name_reply,
				G_TYPE_INVALID)) {
		g_warning ("Cannot register the panel shell: %s",
			   error->message);
		g_error_free (error);

		goto register_out;
	}

	if (request_name_reply == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
	    request_name_reply == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER)
		retval = TRUE;
	else if (request_name_reply == DBUS_REQUEST_NAME_REPLY_EXISTS)
		g_printerr ("Cannot register the panel shell: there is "
			    "already one running.\n");
	else if (request_name_reply == DBUS_REQUEST_NAME_REPLY_IN_QUEUE)
		/* This should never happen since we don't want to be queued. */
		g_warning ("Cannot register the panel shell: it was queued "
			   "after the running one, but this should not "
			   "happen.");
	else
		g_warning ("Cannot register the panel shell: unhandled "
			   "reply %u from RequestName", request_name_reply);

register_out:

	if (!retval) {
		panel_session_do_not_restart ();
		panel_shell_cleanup (NULL);
	}

	return retval;
}

void
panel_shell_quit (void)
{
	GSList *toplevels_to_destroy, *l;

        toplevels_to_destroy = g_slist_copy (panel_toplevel_list_toplevels ());
        for (l = toplevels_to_destroy; l; l = l->next)
		gtk_widget_destroy (l->data);
        g_slist_free (toplevels_to_destroy);

	gtk_main_quit ();
}
