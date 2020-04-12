/*
 * panel-launch.h: some helpers to launch desktop files
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

#ifndef PANEL_LAUNCH_H
#define PANEL_LAUNCH_H

#include <gio/gio.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

gboolean panel_launch_desktop_file (const char  *desktop_file,
				    GdkScreen   *screen,
				    GError     **error);

G_END_DECLS

#endif /* PANEL_LAUNCH_H */
