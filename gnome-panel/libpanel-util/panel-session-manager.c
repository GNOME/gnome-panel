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
#include "panel-util-types.h"

#include "panel-session-manager.h"

struct _PanelSessionManagerPrivate {
	GDBusProxy *session_proxy;
	GDBusProxy *presence_proxy;
};

enum {
        PRESENCE_CHANGED,
        LAST_SIGNAL
};

static guint panel_session_manager_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (PanelSessionManager, panel_session_manager, G_TYPE_OBJECT);

static void
panel_session_manager_presence_on_signal (GDBusProxy          *proxy,
                                          gchar               *sender_name,
                                          gchar               *signal_name,
                                          GVariant            *parameters,
                                          PanelSessionManager *manager)
{
        if (g_strcmp0 (signal_name, "StatusChanged") == 0) {
                unsigned int status;
                g_variant_get (parameters, "(u)", &status);

                g_signal_emit (G_OBJECT (manager),
                               panel_session_manager_signals[PRESENCE_CHANGED],
                               0, status);
        }
}

static void
panel_session_manager_class_init (PanelSessionManagerClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass,
				  sizeof (PanelSessionManagerPrivate));

        panel_session_manager_signals[PRESENCE_CHANGED] =
                g_signal_new ("presence-changed",
                              G_TYPE_FROM_CLASS (gobject_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PanelSessionManagerClass,
                                               presence_changed),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__ENUM,
                              G_TYPE_NONE, 1,
                              PANEL_TYPE_SESSION_MANAGER_PRESENCE_TYPE);
}

static void
panel_session_manager_init (PanelSessionManager *manager)
{
	GError *error;

	manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
						     PANEL_TYPE_SESSION_MANAGER,
						     PanelSessionManagerPrivate);

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
                return;
	}

	error = NULL;
	manager->priv->presence_proxy = g_dbus_proxy_new_for_bus_sync (
						G_BUS_TYPE_SESSION,
						G_DBUS_PROXY_FLAGS_NONE,
						NULL,
						"org.gnome.SessionManager",
						"/org/gnome/SessionManager/Presence",
						"org.gnome.SessionManager.Presence",
						NULL, &error);

	if (error) {
		g_warning ("Could not connect to session manager: %s",
			   error->message);
		g_error_free (error);
                return;
	}

        g_signal_connect (manager->priv->presence_proxy,
                          "g-signal",
                          G_CALLBACK (panel_session_manager_presence_on_signal),
                          manager);
}

void
panel_session_manager_set_presence (PanelSessionManager             *manager,
                                    PanelSessionManagerPresenceType  presence)
{
	GVariant *ret;
	GError   *error;

	g_return_if_fail (PANEL_IS_SESSION_MANAGER (manager));

	if (!manager->priv->presence_proxy) {
		g_warning ("Session manager service not available.");
		return;
	}

	error = NULL;
	ret = g_dbus_proxy_call_sync (manager->priv->presence_proxy,
				      "SetStatus",
				      g_variant_new ("(u)", presence),
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      &error);

	if (ret)
		g_variant_unref (ret);

	if (error) {
		g_warning ("Could not ask session manager to change presence: %s",
			   error->message);
		g_error_free (error);
	}
}

PanelSessionManagerPresenceType
panel_session_manager_get_presence (PanelSessionManager *manager)
{
        GVariant *variant;
        PanelSessionManagerPresenceType ret;

	g_return_val_if_fail (PANEL_IS_SESSION_MANAGER (manager),
                              PANEL_SESSION_MANAGER_PRESENCE_AVAILABLE);

	if (!manager->priv->presence_proxy) {
		g_warning ("Session manager service not available.");
		return PANEL_SESSION_MANAGER_PRESENCE_AVAILABLE;
	}

        variant = g_dbus_proxy_get_cached_property (manager->priv->presence_proxy,
                                                    "status");

        if (!variant) {
                g_warning ("Could not get presence from session manager.");
                return PANEL_SESSION_MANAGER_PRESENCE_AVAILABLE;
        }

        g_variant_get (variant, "u", &ret);
        g_variant_unref (variant);

        return ret;
}

void
panel_session_manager_request_logout (PanelSessionManager           *manager,
				      PanelSessionManagerLogoutType  mode)
{
	GVariant *ret;
	GError   *error;

	g_return_if_fail (PANEL_IS_SESSION_MANAGER (manager));

	if (!manager->priv->session_proxy) {
		g_warning ("Session manager service not available.");
		return;
	}

	error = NULL;
	ret = g_dbus_proxy_call_sync (manager->priv->session_proxy,
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

	if (!manager->priv->session_proxy) {
		g_warning ("Session manager service not available.");
		return;
	}

	error = NULL;
	ret = g_dbus_proxy_call_sync (manager->priv->session_proxy,
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
