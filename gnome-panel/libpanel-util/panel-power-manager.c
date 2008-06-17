/* panel-power-manager.c - functions for powering down, restarting, and
 *                         suspending the computer
 *
 * Copyright (C) 2006 Ray Strode <rstrode@redhat.com>
 * Copyright (C) 2008 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Ray Strode <rstrode@redhat.com>
 *      Vincent Untz <vuntz@gnome.org>
 */

#include <dbus/dbus-glib.h>

#include "panel-cleanup.h"
#include "panel-dbus-service.h"

#include "panel-power-manager.h"

static void panel_power_manager_class_install_signals (PanelPowerManagerClass *manager_class);
static GObject *panel_power_manager_constructor (GType                  type,
						 guint                  n_construct_properties,
						 GObjectConstructParam *construct_properties);

enum {
  REQUEST_FAILED = 0,
  NUMBER_OF_SIGNALS
};

static guint panel_power_manager_signals[NUMBER_OF_SIGNALS];

G_DEFINE_TYPE (PanelPowerManager, panel_power_manager, PANEL_TYPE_DBUS_SERVICE);

static void
panel_power_manager_class_init (PanelPowerManagerClass *manager_class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (manager_class);

	object_class->constructor = panel_power_manager_constructor;

	panel_power_manager_class_install_signals (manager_class);
}

static void
panel_power_manager_class_install_signals (PanelPowerManagerClass *manager_class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (manager_class);

	panel_power_manager_signals[REQUEST_FAILED] =
		g_signal_new ("request-failed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (PanelPowerManagerClass, request_failed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	manager_class->request_failed = NULL;
}

static void
panel_power_manager_init (PanelPowerManager *manager)
{
}

static GObject *
panel_power_manager_constructor (GType                  type,
				 guint                  n_construct_properties,
				 GObjectConstructParam *construct_properties)
{
	GObject *obj;
	GError  *error;

	obj = G_OBJECT_CLASS (panel_power_manager_parent_class)->constructor (type,
									      n_construct_properties,
									      construct_properties);


	panel_dbus_service_define_service (PANEL_DBUS_SERVICE (obj),
					   "org.freedesktop.PowerManagement",
					   "/org/freedesktop/PowerManagement",
					   "org.freedesktop.PowerManagement");

	error = NULL;
	if (!panel_dbus_service_ensure_connection (PANEL_DBUS_SERVICE (obj),
						   &error)) {
		g_message ("Could not connect to power manager: %s",
			   error->message);
		g_error_free (error);
	}

	return obj;
}

GQuark
panel_power_manager_error_quark (void)
{
	static GQuark error_quark = 0;

	if (error_quark == 0)
		error_quark = g_quark_from_static_string ("panel-power-manager-error");

	return error_quark;
}

PanelPowerManager *
panel_power_manager_new (void)
{
	PanelPowerManager *manager;

	manager = g_object_new (PANEL_TYPE_POWER_MANAGER, NULL);

	return manager;
}

gboolean
panel_power_manager_can_suspend (PanelPowerManager *manager)
{
	GError *error;
	gboolean can_suspend;
	DBusGProxy *proxy;

	g_return_val_if_fail (PANEL_IS_POWER_MANAGER (manager), FALSE);

	error = NULL;

	if (!panel_dbus_service_ensure_connection (PANEL_DBUS_SERVICE (manager),
						   &error)) {
		g_message ("Could not connect to power manager: %s",
			   error->message);
		g_error_free (error);
		return FALSE;
	}

	proxy = panel_dbus_service_get_proxy (PANEL_DBUS_SERVICE (manager));

	can_suspend = FALSE;
	if (!dbus_g_proxy_call (proxy, "CanSuspend",
				&error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &can_suspend, G_TYPE_INVALID)) {
		if (error != NULL) {
			g_message ("Could not ask power manager if user can suspend: %s",
				   error->message);
			g_error_free (error);
		}
		can_suspend = FALSE;
	}

	return can_suspend;
}

gboolean
panel_power_manager_can_hibernate (PanelPowerManager *manager)
{
	GError *error;
	gboolean can_hibernate;
	DBusGProxy *proxy;

	g_return_val_if_fail (PANEL_IS_POWER_MANAGER (manager), FALSE);

	error = NULL;

	if (!panel_dbus_service_ensure_connection (PANEL_DBUS_SERVICE (manager),
						   &error)) {
		g_message ("Could not connect to power manager: %s",
			   error->message);
		g_error_free (error);
		return FALSE;
	}

	proxy = panel_dbus_service_get_proxy (PANEL_DBUS_SERVICE (manager));

	can_hibernate = FALSE;
	if (!dbus_g_proxy_call (proxy, "CanHibernate",
				&error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &can_hibernate, G_TYPE_INVALID)) {
		if (error != NULL) {
			g_message ("Could not ask power manager if user can suspend: %s",
				   error->message);
			g_error_free (error);
		}
		can_hibernate = FALSE;
	}

	return can_hibernate;
}

void
panel_power_manager_attempt_suspend (PanelPowerManager *manager)
{
	GError *error;
	DBusGProxy *proxy;

	g_return_if_fail (PANEL_IS_POWER_MANAGER (manager));

	error = NULL;

	if (!panel_dbus_service_ensure_connection (PANEL_DBUS_SERVICE (manager),
						   &error)) {
		g_warning ("Could not connect to power manager: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	proxy = panel_dbus_service_get_proxy (PANEL_DBUS_SERVICE (manager));

	if (!dbus_g_proxy_call (proxy, "Suspend",
				&error,
				G_TYPE_INVALID, G_TYPE_INVALID) &&
	    error != NULL) {
		GError *call_error;

		g_warning ("Could not ask power manager to suspend: %s",
			   error->message);

		call_error = g_error_new_literal (PANEL_POWER_MANAGER_ERROR,
						  PANEL_POWER_MANAGER_ERROR_SUSPENDING,
						  error->message);
		g_error_free (error);

		g_signal_emit (G_OBJECT (manager),
			       panel_power_manager_signals[REQUEST_FAILED],
			       0, call_error);
		g_error_free (call_error);
	}
}

void
panel_power_manager_attempt_hibernate (PanelPowerManager *manager)
{
	GError *error;
	DBusGProxy *proxy;

	g_return_if_fail (PANEL_IS_POWER_MANAGER (manager));

	error = NULL;

	if (!panel_dbus_service_ensure_connection (PANEL_DBUS_SERVICE (manager),
						   &error)) {
		g_warning ("Could not connect to power manager: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	proxy = panel_dbus_service_get_proxy (PANEL_DBUS_SERVICE (manager));

	if (!dbus_g_proxy_call (proxy, "Hibernate",
				&error,
				G_TYPE_INVALID, G_TYPE_INVALID) &&
	    error != NULL) {
		GError *call_error;

		g_warning ("Could not ask power manager to hibernate: %s",
			   error->message);

		call_error = g_error_new_literal (PANEL_POWER_MANAGER_ERROR,
						  PANEL_POWER_MANAGER_ERROR_HIBERNATING,
						  error->message);
		g_error_free (error);

		g_signal_emit (G_OBJECT (manager),
			       panel_power_manager_signals[REQUEST_FAILED],
			       0, call_error);
		g_error_free (call_error);
	}
}

PanelPowerManager *
panel_power_manager_get (void)
{
	static PanelPowerManager *manager = NULL;

	if (manager == NULL) {
		manager = panel_power_manager_new ();
		panel_cleanup_register (panel_cleanup_unref_and_nullify,
					&manager);
	}

	return g_object_ref (manager);
}
