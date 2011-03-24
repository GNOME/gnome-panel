/*
 * panel-screensaver.c:
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#include <dbus/dbus-glib.h>

#include "panel-cleanup.h"
#include "panel-dbus-service.h"

#include "panel-screensaver.h"

static GObject *panel_screensaver_constructor (GType                  type,
					       guint                  n_construct_properties,
					       GObjectConstructParam *construct_properties);

G_DEFINE_TYPE (PanelScreensaver, panel_screensaver, PANEL_TYPE_DBUS_SERVICE);

static void
panel_screensaver_class_init (PanelScreensaverClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->constructor = panel_screensaver_constructor;
}

static void
panel_screensaver_init (PanelScreensaver *manager)
{
}

static GObject *
panel_screensaver_constructor (GType                  type,
				   guint                  n_construct_properties,
				   GObjectConstructParam *construct_properties)
{
	GObject *obj;
	GError  *error;

	obj = G_OBJECT_CLASS (panel_screensaver_parent_class)->constructor (
							type,
							n_construct_properties,
							construct_properties);


	panel_dbus_service_define_service (PANEL_DBUS_SERVICE (obj),
					   "org.gnome.ScreenSaver",
					   "/org/gnome/ScreenSaver",
					   "org.gnome.ScreenSaver");

	error = NULL;
	if (!panel_dbus_service_ensure_connection (PANEL_DBUS_SERVICE (obj),
						   &error)) {
		g_message ("Could not connect to screensaver: %s",
			   error->message);
		g_error_free (error);
	}

	return obj;
}

void
panel_screensaver_lock (PanelScreensaver *screensaver)
{
	GError *error;
	DBusGProxy *proxy;

	g_return_if_fail (PANEL_IS_SCREENSAVER (screensaver));

	error = NULL;

	if (!panel_dbus_service_ensure_connection (PANEL_DBUS_SERVICE (screensaver),
						   &error)) {
		g_warning ("Could not connect to screensaver: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	proxy = panel_dbus_service_get_proxy (PANEL_DBUS_SERVICE (screensaver));

	if (!dbus_g_proxy_call (proxy, "Lock", &error,
				G_TYPE_INVALID,
				G_TYPE_INVALID) &&
	    error != NULL) {
		g_warning ("Could not ask screensaver to lock: %s",
			   error->message);
		g_error_free (error);
	}
}

void
panel_screensaver_activate (PanelScreensaver *screensaver)
{
	GError *error;
	DBusGProxy *proxy;

	g_return_if_fail (PANEL_IS_SCREENSAVER (screensaver));

	error = NULL;

	if (!panel_dbus_service_ensure_connection (PANEL_DBUS_SERVICE (screensaver),
						   &error)) {
		g_warning ("Could not connect to screensaver: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	proxy = panel_dbus_service_get_proxy (PANEL_DBUS_SERVICE (screensaver));

	if (!dbus_g_proxy_call (proxy, "SetActive", &error,
				G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID,
				G_TYPE_INVALID) &&
	    error != NULL) {
		g_warning ("Could not ask screensaver to activate: %s",
			   error->message);
		g_error_free (error);
	}
}

PanelScreensaver *
panel_screensaver_get (void)
{
	static PanelScreensaver *screensaver = NULL;

	if (screensaver == NULL) {
		screensaver = g_object_new (PANEL_TYPE_SCREENSAVER, NULL);
		panel_cleanup_register (panel_cleanup_unref_and_nullify,
					&screensaver);
	}

	return screensaver;
}
