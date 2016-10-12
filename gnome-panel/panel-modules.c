/*
 * panel-modules.c
 *
 * Copyright (C) 2010 Vincent Untz <vuntz@gnome.org>
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
 *      Vincent Untz <vuntz@gnome.org>
 */

#include <config.h>

#include <gio/gio.h>

#include <libpanel-applet-private/panel-applets-manager-dbus.h>

#include "panel-applets-manager.h"

#include "panel-modules.h"

static void
panel_modules_ensure_extension_points_registered (void)
{
	static gboolean registered_extensions = FALSE;
	GIOExtensionPoint *ep;

	if (!registered_extensions) {
		registered_extensions = TRUE;

		ep = g_io_extension_point_register (PANEL_APPLETS_MANAGER_EXTENSION_POINT_NAME);
		g_io_extension_point_set_required_type (ep, PANEL_TYPE_APPLETS_MANAGER);
	}
 }

void
panel_modules_ensure_loaded (void)
{
	static gboolean loaded_dirs = FALSE;

	panel_modules_ensure_extension_points_registered ();

	if (!loaded_dirs) {
		loaded_dirs = TRUE;

		panel_applets_manager_dbus_get_type ();
	}
}
