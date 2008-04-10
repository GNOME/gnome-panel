/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <polkit/polkit.h>
#include <polkit-dbus/polkit-dbus.h>

#include "set-timezone.h"

#define CACHE_VALIDITY_SEC 2

static DBusGConnection *
get_session_bus (void)
{
        GError          *error;
        static DBusGConnection *bus = NULL;

	if (bus == NULL) {
        	error = NULL;
        	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        	if (bus == NULL) {
               		g_warning ("Couldn't connect to session bus: %s", 
				   error->message);
                	g_error_free (error);
        	}
	}

        return bus;
}

static DBusGConnection *
get_system_bus (void)
{
        GError          *error;
        static DBusGConnection *bus = NULL;

	if (bus == NULL) {
        	error = NULL;
        	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        	if (bus == NULL) {
                	g_warning ("Couldn't connect to system bus: %s", 
				   error->message);
                	g_error_free (error);
		}
        }

        return bus;
}

static gboolean
pk_io_watch_have_data (GIOChannel *channel, GIOCondition condition, gpointer user_data)
{
        int fd;
        PolKitContext *pk_context = user_data;
        fd = g_io_channel_unix_get_fd (channel);
        polkit_context_io_func (pk_context, fd);
        return TRUE;
}

static int 
pk_io_add_watch_fn (PolKitContext *pk_context, int fd)
{
        guint id = 0;
        GIOChannel *channel;
        channel = g_io_channel_unix_new (fd);
        if (channel == NULL)
                goto out;
        id = g_io_add_watch (channel, G_IO_IN, pk_io_watch_have_data, pk_context);
        if (id == 0) {
                g_io_channel_unref (channel);
                goto out;
        }
        g_io_channel_unref (channel);
out:
        return id;
}

static void 
pk_io_remove_watch_fn (PolKitContext *pk_context, int watch_id)
{
        g_source_remove (watch_id);
}

static PolKitContext *
get_pk_context (void)
{
	static PolKitContext *pk_context = NULL;

	if (pk_context == NULL) {
		pk_context = polkit_context_new ();
                polkit_context_set_io_watch_functions (pk_context,
                                                       pk_io_add_watch_fn,
                                                       pk_io_remove_watch_fn);
		if (!polkit_context_init (pk_context, NULL)) {
			polkit_context_unref (pk_context);
			pk_context = NULL;
		}
	}

	return pk_context;
}

static gint
can_do (const gchar *pk_action_id)
{
	DBusConnection *system_bus;
	PolKitCaller *pk_caller;
        PolKitAction *pk_action;
        PolKitResult pk_result;
	PolKitContext *pk_context;
        DBusError dbus_error;
	gint res = 0;

        pk_caller = NULL;
        pk_action = NULL;

	system_bus = dbus_g_connection_get_connection (get_system_bus ());
	if (system_bus == NULL)
		goto out;
	
	pk_context = get_pk_context ();
	if (pk_context == NULL)
		goto out;
	
        pk_action = polkit_action_new ();
        polkit_action_set_action_id (pk_action, pk_action_id);

        dbus_error_init (&dbus_error);
        pk_caller = polkit_caller_new_from_pid (system_bus, getpid (), &dbus_error);
        if (pk_caller == NULL) {
                fprintf (stderr, "cannot get caller from dbus name\n");
                goto out;
        }

        pk_result = polkit_context_can_caller_do_action (pk_context, pk_action, pk_caller);

	switch (pk_result) {
        case POLKIT_RESULT_UNKNOWN:
        case POLKIT_RESULT_NO:
 		res = 0;
		break;
        case POLKIT_RESULT_YES:
		res = 2;
		break;
        default:
                /* This covers all the POLKIT_RESULT_ONLY_VIA_[SELF|ADMIN]_AUTH_* cases as more of these
                 * may be added in the future.
                 */
		res = 1;
		break;
	}
	
out:
        if (pk_action != NULL)
                polkit_action_unref (pk_action);
        if (pk_caller != NULL)
                polkit_caller_unref (pk_caller);

	return res;
}

gint
can_set_system_timezone (void)
{
	static gboolean cache = FALSE;
	static time_t   last_refreshed = 0;
	time_t          now;

	time (&now);
	if (ABS (now - last_refreshed) > CACHE_VALIDITY_SEC) {
		cache = can_do ("org.gnome.clockapplet.mechanism.settimezone");
		last_refreshed = now;
	}

	return cache;
}

gint
can_set_system_time (void)
{
	static gboolean cache = FALSE;
	static time_t   last_refreshed = 0;
	time_t          now;

	time (&now);
	if (ABS (now - last_refreshed) > CACHE_VALIDITY_SEC) {
		cache = can_do ("org.gnome.clockapplet.mechanism.settime");
		last_refreshed = now;
	}

	return cache;
}

typedef struct {
	gint ref_count;
        gchar *call;
	gint64 time;
	gchar *filename;
	GFunc callback;
	gpointer data;
	GDestroyNotify notify;
} SetTimeCallbackData;

static void
free_data (gpointer d)
{
	SetTimeCallbackData *data = d;

	data->ref_count--;
	if (data->ref_count == 0) {
		if (data->notify)
			data->notify (data->data);
		g_free (data->filename);
		g_free (data);
	}
}

static void set_time_async (SetTimeCallbackData *data);

static void 
auth_notify (DBusGProxy     *proxy,
             DBusGProxyCall *call,
             void           *user_data)
{
	SetTimeCallbackData *data = user_data;
	GError *error = NULL;
	gboolean gained_privilege;

	if (dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_BOOLEAN, &gained_privilege, G_TYPE_INVALID)) {
		if (gained_privilege)
			set_time_async (data);
	}
	else {
		if (data->callback) 
			data->callback (data->data, error);
		else
			g_error_free (error);
	}
}

static void
do_auth_async (const gchar         *action, 
               const gchar         *result, 
               SetTimeCallbackData *data)
{
        DBusGConnection *bus;
	DBusGProxy *proxy;

	g_debug ("helper refused; returned polkit_result='%s' and polkit_action='%s'",
		 result, action);

	/* Now ask the user for auth... */
        bus = get_session_bus ();
	if (bus == NULL)
		return;

	proxy = dbus_g_proxy_new_for_name (bus,
					"org.gnome.PolicyKit",
					"/org/gnome/PolicyKit/Manager",
					"org.gnome.PolicyKit.Manager");
	
	data->ref_count++;
	dbus_g_proxy_begin_call_with_timeout (proxy,
					      "ShowDialog",
					      auth_notify,
					      data, free_data,
					      INT_MAX,
					      G_TYPE_STRING, action,
					      G_TYPE_UINT, 0,
					      G_TYPE_INVALID);
}

static void
set_time_notify (DBusGProxy     *proxy,
		 DBusGProxyCall *call,
		 void           *user_data)
{
	SetTimeCallbackData *data = user_data;
	GError *error = NULL;

	if (dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID)) {
		if (data->callback) 
			data->callback (data->data, NULL);
	}
	else {
		if (error->domain == DBUS_GERROR &&
		    error->code == DBUS_GERROR_NO_REPLY) {
			/* these errors happen because dbus doesn't
			 * use monotonic clocks
			 */	
			g_warning ("ignoring no-reply error when setting time");
			g_error_free (error);
			if (data->callback)
				data->callback (data->data, NULL);
		}
		else if (dbus_g_error_has_name (error, "org.gnome.ClockApplet.Mechanism.NotPrivileged")) {
			gchar **tokens;

			tokens = g_strsplit (error->message, " ", 2);
			g_error_free (error);                            
			if (g_strv_length (tokens) == 2) 
				do_auth_async (tokens[0], tokens[1], data);
			else
				g_warning ("helper return string malformed");
			g_strfreev (tokens);
		}
		else {
			if (data->callback)
				data->callback (data->data, error);
			else
				g_error_free (error);
		}		
	}
}

static void
set_time_async (SetTimeCallbackData *data)
{
        DBusGConnection *bus;
        DBusGProxy      *proxy;

        bus = get_system_bus ();
        if (bus == NULL)
                return;

	proxy = dbus_g_proxy_new_for_name (bus,
					   "org.gnome.ClockApplet.Mechanism",
					   "/",
					   "org.gnome.ClockApplet.Mechanism");

	data->ref_count++;
	if (strcmp (data->call, "SetTime") == 0)
		dbus_g_proxy_begin_call_with_timeout (proxy, 
						      "SetTime",
						      set_time_notify,
						      data, free_data,
						      INT_MAX,
						      /* parameters: */
						      G_TYPE_INT64, data->time,
						      G_TYPE_INVALID,
						      /* return values: */
						      G_TYPE_INVALID);
	else 
		dbus_g_proxy_begin_call_with_timeout (proxy, 
						      "SetTimezone",
						      set_time_notify,
						      data, free_data,
						      INT_MAX,
						      /* parameters: */
						      G_TYPE_STRING, data->filename,
						      G_TYPE_INVALID,
						      /* return values: */
						      G_TYPE_INVALID);
}

void
set_system_time_async (gint64         time, 
		       GFunc          callback, 
		       gpointer       d, 
		       GDestroyNotify notify)
{
	SetTimeCallbackData *data;

	if (time == -1)
		return;

	data = g_new (SetTimeCallbackData, 1);
	data->ref_count = 1;
	data->call = "SetTime";
	data->time = time;
	data->filename = NULL;
	data->callback = callback;
	data->data = d;
	data->notify = notify;

	set_time_async (data);
	free_data (data);
}

void
set_system_timezone_async (const gchar    *filename,
	             	   GFunc           callback, 
		           gpointer        d, 
		           GDestroyNotify  notify)
{
	SetTimeCallbackData *data;

	if (filename == NULL)
		return;

	data = g_new (SetTimeCallbackData, 1);
	data->ref_count = 1;
	data->call = "SetTimezone";
	data->time = -1;
	data->filename = g_strdup (filename);
	data->callback = callback;
	data->data = d;
	data->notify = notify;

	set_time_async (data);
	free_data (data);
}
