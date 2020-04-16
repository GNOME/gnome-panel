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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#include <gio/gio.h>

#include "panel-cleanup.h"

#include "panel-session-manager.h"

struct _PanelSessionManagerPrivate {
	GDBusProxy *session_proxy;
};

G_DEFINE_TYPE_WITH_PRIVATE (PanelSessionManager, panel_session_manager, G_TYPE_OBJECT)

static void
panel_session_manager_class_init (PanelSessionManagerClass *klass)
{
}

static void
panel_session_manager_init (PanelSessionManager *manager)
{
	GError *error;

	manager->priv = panel_session_manager_get_instance_private (manager);

	error = NULL;
	manager->priv->session_proxy = g_dbus_proxy_new_for_bus_sync (
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

static void
shutdown_ready_callback (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
	PanelSessionManager *manager = (PanelSessionManager *) user_data;
	GError *error = NULL;
	GVariant *ret;

	ret = g_dbus_proxy_call_finish (manager->priv->session_proxy, res, &error);
	if (ret) {
		g_variant_unref (ret);
	}

	if (error) {
		g_warning ("Could not ask session manager to shut down: %s", error->message);
		g_error_free (error);
	}
}

void
panel_session_manager_request_shutdown (PanelSessionManager *manager)
{
	g_return_if_fail (PANEL_IS_SESSION_MANAGER (manager));

	if (!manager->priv->session_proxy) {
		g_warning ("Session manager service not available.");
		return;
	}

	g_dbus_proxy_call (manager->priv->session_proxy,
	                   "Shutdown",
	                   NULL,
	                   G_DBUS_CALL_FLAGS_NONE,
	                   -1,
	                   NULL,
	                   (GAsyncReadyCallback) shutdown_ready_callback,
	                   manager);
}

static void
reboot_ready_callback (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
	PanelSessionManager *manager = (PanelSessionManager *) user_data;
	GError *error = NULL;
	GVariant *ret;

	ret = g_dbus_proxy_call_finish (manager->priv->session_proxy, res, &error);
	if (ret) {
		g_variant_unref (ret);
	}

	if (error) {
		g_warning ("Could not ask session manager to reboot: %s", error->message);
		g_error_free (error);
	}
}

void
panel_session_manager_request_reboot (PanelSessionManager *manager)
{
	g_return_if_fail (PANEL_IS_SESSION_MANAGER (manager));

	if (!manager->priv->session_proxy) {
		g_warning ("Session manager service not available.");
		return;
	}

	g_dbus_proxy_call (manager->priv->session_proxy,
	                   "Reboot",
	                   NULL,
	                   G_DBUS_CALL_FLAGS_NONE,
	                   -1,
	                   NULL,
	                   (GAsyncReadyCallback) reboot_ready_callback,
	                   manager);
}

gboolean
panel_session_manager_is_shutdown_available (PanelSessionManager *manager)
{
	GVariant *ret;
	GError   *error;
	gboolean  is_shutdown_available = FALSE;

	g_return_val_if_fail (PANEL_IS_SESSION_MANAGER (manager), FALSE);

	if (!manager->priv->session_proxy) {
		g_warning ("Session manager service not available.");
		return FALSE;
	}

	error = NULL;
	ret = g_dbus_proxy_call_sync (manager->priv->session_proxy,
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
