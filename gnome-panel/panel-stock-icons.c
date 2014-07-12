/*
 * panel-stock-icons.c panel stock icons registration
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
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
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include "config.h"

#include "panel-stock-icons.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "panel-icon-names.h"

static GtkIconSize panel_menu_icon_size = 0;

GtkIconSize
panel_menu_icon_get_size (void)
{
	if (panel_menu_icon_size == 0) {
		GSettings *settings = g_settings_new ("org.gnome.gnome-panel.general");
		panel_menu_icon_size = (GtkIconSize) g_settings_get_enum (settings, "panel-menu");
		g_object_unref (settings);
	}

	return panel_menu_icon_size;
}
