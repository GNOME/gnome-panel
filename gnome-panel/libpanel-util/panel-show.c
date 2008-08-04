/*
 * panel-show.c: a helper around gtk_show_uri
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

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include <gtk/gtk.h>

#include "panel-error.h"
#include "panel-glib.h"

#include "panel-show.h"

static void
_panel_show_error_dialog (const gchar *uri,
			  GdkScreen   *screen,
			  const gchar *message)
{
	char *escaped;
	char *primary;

	escaped = g_markup_escape_text (uri, -1);
	primary = g_strdup_printf (_("Could not open location '%s'"),
				   escaped);
	g_free (escaped);

	panel_error_dialog (NULL, screen, "cannot_show_url", TRUE,
			    primary, message);
	g_free (primary);
}

static void
_panel_show_handle_error (const gchar  *uri,
			  GdkScreen    *screen,
			  GError       *local_error,
			  GError      **error)
{
	g_return_if_fail (local_error != NULL);

	if (error != NULL)
		g_propagate_error (error, local_error);

	else if (local_error != NULL) {
		if (local_error->code != G_IO_ERROR_CANCELLED)
			_panel_show_error_dialog (uri, screen,
						  local_error->message);
		g_error_free (local_error);
	}
}

static gboolean
_panel_app_info_launch_uri (GAppInfo     *appinfo,
			    const gchar  *uri,
			    GdkScreen    *screen,
			    guint32       timestamp,
			    GError      **error)
{
	GList               *uris;
	GdkAppLaunchContext *context;
	GError              *local_error;

	uris = NULL;
	uris = g_list_prepend (uris, (gpointer) uri);

	context = gdk_app_launch_context_new ();
	gdk_app_launch_context_set_screen (context, screen);
	gdk_app_launch_context_set_timestamp (context, timestamp);

	local_error = NULL;
	g_app_info_launch_uris (appinfo, uris,
				(GAppLaunchContext *) context,
				&local_error);
	g_list_free (uris);
	g_object_unref (context);

	if (local_error == NULL)
		return TRUE;

	_panel_show_handle_error (uri, screen, local_error, error);

	return FALSE;
}

static gboolean
panel_show_nautilus_search_uri (GdkScreen    *screen,
				const gchar  *uri,
				guint32       timestamp,
				GError      **error)
{
	char            *desktopfile;
	GDesktopAppInfo *appinfo;
	gboolean         ret;

	desktopfile = panel_g_lookup_in_applications_dirs ("nautilus-folder-handler.desktop");
	if (desktopfile) {
		appinfo = g_desktop_app_info_new_from_filename (desktopfile);
		g_free (desktopfile);
	}

	if (!appinfo) {
		_panel_show_error_dialog (uri, screen,
					  _("No application to handle search folders is installed."));
		return FALSE;
	}

	ret = _panel_app_info_launch_uri ((GAppInfo *) appinfo,
					  uri, screen, timestamp, error);
	g_object_unref (appinfo);

	return ret;
}

gboolean
panel_show_uri (GdkScreen    *screen,
		const gchar  *uri,
		guint32       timestamp,
		GError      **error)
{
	GError *local_error = NULL;

	g_return_val_if_fail (screen != NULL, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (g_str_has_prefix (uri, "x-nautilus-search:")) {
		return panel_show_nautilus_search_uri (screen, uri,
						       timestamp, error);
	}

	gtk_show_uri (screen, uri, timestamp, &local_error);

	if (local_error == NULL)
		return TRUE;

	_panel_show_handle_error (uri, screen, local_error, error);

	return FALSE;
}

gboolean
panel_show_uri_force_mime_type (GdkScreen    *screen,
				const gchar  *uri,
				const gchar  *mime_type,
				guint32       timestamp,
				GError      **error)
{
	GFile    *file;
	GAppInfo *app;
	gboolean  ret;

	g_return_val_if_fail (screen != NULL, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (mime_type != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	file = g_file_new_for_uri (uri);
	app = g_app_info_get_default_for_type (mime_type,
					       !g_file_is_native (file));
	g_object_unref (file);

	if (app == NULL) {
		/* no application for the mime type, so let's fallback on
		 * automatic detection */
		return panel_show_uri (screen, uri, timestamp, error);
	}

	ret = _panel_app_info_launch_uri (app, uri, screen, timestamp, error);
	g_object_unref (app);

	return ret;
}
