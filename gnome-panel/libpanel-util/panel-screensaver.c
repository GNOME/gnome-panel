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

#include <gio/gio.h>

#include "panel-cleanup.h"

#include "panel-screensaver.h"

struct _PanelScreensaverPrivate {
	GDBusProxy *proxy;
};

G_DEFINE_TYPE (PanelScreensaver, panel_screensaver, G_TYPE_OBJECT);

static void
panel_screensaver_class_init (PanelScreensaverClass *klass)
{
	g_type_class_add_private (klass,
				  sizeof (PanelScreensaverPrivate));
}

static void
panel_screensaver_init (PanelScreensaver *screensaver)
{
	GError *error;

	screensaver->priv = G_TYPE_INSTANCE_GET_PRIVATE (screensaver,
							 PANEL_TYPE_SCREENSAVER,
							 PanelScreensaverPrivate);

	error = NULL;
	screensaver->priv->proxy = g_dbus_proxy_new_for_bus_sync (
						G_BUS_TYPE_SESSION,
						G_DBUS_PROXY_FLAGS_NONE,
						NULL,
						"org.gnome.ScreenSaver",
						"/org/gnome/ScreenSaver",
						"org.gnome.ScreenSaver",
						NULL, &error);

	if (error) {
		g_warning ("Could not connect to screensaver: %s",
			   error->message);
		g_error_free (error);
	}
}

void
panel_screensaver_lock (PanelScreensaver *screensaver)
{
	GVariant *ret;
	GError   *error;

	g_return_if_fail (PANEL_IS_SCREENSAVER (screensaver));

	if (!screensaver->priv->proxy) {
		g_warning ("Screensaver service not available.");
		return;
	}

	error = NULL;
	ret = g_dbus_proxy_call_sync (screensaver->priv->proxy,
				      "Lock",
				      NULL,
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      &error);

	if (ret)
		g_variant_unref (ret);

	if (error) {
		g_warning ("Could not ask screensaver to lock: %s",
			   error->message);
		g_error_free (error);
	}
}

void
panel_screensaver_activate (PanelScreensaver *screensaver)
{
	GVariant *ret;
	GError   *error;

	g_return_if_fail (PANEL_IS_SCREENSAVER (screensaver));

	if (!screensaver->priv->proxy) {
		g_warning ("Screensaver service not available.");
		return;
	}

	error = NULL;
	ret = g_dbus_proxy_call_sync (screensaver->priv->proxy,
				      "SetActive",
				      g_variant_new ("(b)", TRUE),
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      &error);

	if (ret)
		g_variant_unref (ret);

	if (error) {
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
