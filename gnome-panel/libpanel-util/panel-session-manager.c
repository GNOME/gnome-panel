/*
 * panel-session.c:
 *
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
 *	Vincent Untz <vuntz@gnome.org>
 */

#include <gio/gio.h>

#include "panel-cleanup.h"

#include "panel-session-manager.h"

struct _PanelSessionManagerPrivate {
	GDBusProxy *proxy;
};

G_DEFINE_TYPE (PanelSessionManager, panel_session_manager, G_TYPE_OBJECT);

static void
panel_session_manager_class_init (PanelSessionManagerClass *klass)
{
	g_type_class_add_private (klass,
				  sizeof (PanelSessionManagerPrivate));
}

static void
panel_session_manager_init (PanelSessionManager *manager)
{
	GError *error;

	manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
						     PANEL_TYPE_SESSION_MANAGER,
						     PanelSessionManagerPrivate);

	error = NULL;
	manager->priv->proxy = g_dbus_proxy_new_for_bus_sync (
						G_BUS_TYPE_SESSION,
						G_DBUS_PROXY_FLAGS_NONE,
						NULL,
						"org.gnome.SessionManager",
						"/org/gnome/SessionManager",
						"org.gnome.SessionManager",
						NULL, &error);

	if (error) {
		g_warning ("Could not connect to session manager: %s",
			   error->message);
		g_error_free (error);
	}
}

void
panel_session_manager_request_logout (PanelSessionManager           *manager,
				      PanelSessionManagerLogoutType  mode)
{
	GVariant *ret;
	GError   *error;

	g_return_if_fail (PANEL_IS_SESSION_MANAGER (manager));

	if (!manager->priv->proxy) {
		g_warning ("Session manager service not available.");
		return;
	}

	error = NULL;
	ret = g_dbus_proxy_call_sync (manager->priv->proxy,
				      "Logout",
				      g_variant_new ("(u)", mode),
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      &error);

	if (ret)
		g_variant_unref (ret);

	if (error) {
		g_warning ("Could not ask session manager to log out: %s",
			   error->message);
		g_error_free (error);
	}
}

void
panel_session_manager_request_shutdown (PanelSessionManager *manager)
{
	GVariant *ret;
	GError   *error;

	g_return_if_fail (PANEL_IS_SESSION_MANAGER (manager));

	if (!manager->priv->proxy) {
		g_warning ("Session manager service not available.");
		return;
	}

	error = NULL;
	ret = g_dbus_proxy_call_sync (manager->priv->proxy,
				      "Shutdown",
				      NULL,
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      &error);

	if (ret)
		g_variant_unref (ret);

	if (error) {
		g_warning ("Could not ask session manager to shut down: %s",
			   error->message);
		g_error_free (error);
	}
}

gboolean
panel_session_manager_is_shutdown_available (PanelSessionManager *manager)
{
	GVariant *ret;
	GError   *error;
	gboolean  is_shutdown_available = FALSE;

	g_return_val_if_fail (PANEL_IS_SESSION_MANAGER (manager), FALSE);

	if (!manager->priv->proxy) {
		g_warning ("Session manager service not available.");
		return FALSE;
	}

	error = NULL;
	ret = g_dbus_proxy_call_sync (manager->priv->proxy,
				      "CanShutdown",
				      NULL,
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      &error);

	if (error) {
		g_warning ("Could not ask session manager if shut down is available: %s",
			   error->message);
		g_error_free (error);

		return FALSE;
	} else {
		g_variant_get (ret, "(b)", &is_shutdown_available);
		g_variant_unref (ret);
	}

	return is_shutdown_available;
}

PanelSessionManager *
panel_session_manager_get (void)
{
	static PanelSessionManager *manager = NULL;

	if (manager == NULL) {
		manager = g_object_new (PANEL_TYPE_SESSION_MANAGER, NULL);
		panel_cleanup_register (panel_cleanup_unref_and_nullify,
					&manager);
	}

	return manager;
}
