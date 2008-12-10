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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef PANEL_LAUNCH_H
#define PANEL_LAUNCH_H

#include <gio/gio.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

gboolean panel_app_info_launch_uris (GAppInfo   *appinfo,
				     GList      *uris,
				     GdkScreen  *screen,
				     guint32     timestamp,
				     GError    **error);

gboolean panel_app_info_launch_uri (GAppInfo     *appinfo,
				    const gchar  *uri,
				    GdkScreen    *screen,
				    guint32       timestamp,
				    GError      **error);

gboolean panel_launch_key_file (GKeyFile   *keyfile,
				GList      *uri_list,
				GdkScreen  *screen,
				GError    **error);

gboolean panel_launch_desktop_file (const char  *desktop_file,
				    GdkScreen   *screen,
				    GError     **error);

gboolean panel_launch_desktop_file_with_fallback (const char  *desktop_file,
						  const char  *fallback_exec,
						  GdkScreen   *screen,
						  GError     **error);

G_END_DECLS

#endif /* PANEL_LAUNCH_H */
