/*
 * panel-recent.c
 *
 * Copyright (C) 2002 James Willcox <jwillcox@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 * 	James Willcox <jwillcox@gnome.org>
 */

#include <config.h>

#include <libgnome/libgnome.h>
#include <libgnomevfs/gnome-vfs-application-registry.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "egg-recent-model.h"
#include "egg-recent-view.h"
#include "egg-recent-view-gtk.h"
#include "egg-recent-item.h"
#include "egg-screen-exec.h"
#include "menu.h"
#include "menu-util.h"
#include "panel-util.h"
#include "panel-recent.h"
#include "panel-stock-icons.h"

/* FIXME: This code really should not be in the panel */
static gboolean
show_uri (const char *uri, const char *mime_type, GdkScreen *screen,
	  GError **error)
{
	GnomeVFSMimeApplication *app;
	GString                 *str;
	char                    *cmd   = NULL;
	char                    *path;
	gboolean                 ret   = TRUE;
	
	app = gnome_vfs_mime_get_default_application (mime_type);

	if (!app) {
		g_set_error (error, 0, 0,
			     _("Couldn't find a suitable application"));
		return FALSE;
	}

	str = g_string_new ("");

	if (app->requires_terminal) {
		/* FIXME: we should use their preferred terminal
		 * maybe a multiscreen variant of gnome_execcute_terminal_shell?
		 */
		g_string_append_printf (str, "gnome-terminal -x %s", app->command);
	} else
		str = g_string_append (str, app->command);

	switch (app->expects_uris) {
	case GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_PATHS:
		path = gnome_vfs_get_local_path_from_uri (uri);
		if (path != NULL) {
			g_string_append_printf (str, " %s", path);
			g_free (path);
		} else {
			gnome_vfs_mime_application_free (app);
			g_string_free (str, TRUE);
			g_set_error (error, 0, 0, _("The default application for"
				     " this type of file cannot handle remote "
				     "files"));
			return FALSE;
		}
		break;

	case GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS:
		g_string_append_printf (str, " %s", uri);
		break;

	case GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS_FOR_NON_FILES:
		path = gnome_vfs_get_local_path_from_uri (uri);

		if (path != NULL)
			g_string_append_printf (str, " %s", path);
		else
			g_string_append_printf (str, " %s", uri);

		g_free (path);
		break;
	}

	gnome_vfs_mime_application_free (app);

	cmd = g_string_free (str, FALSE);

	if (cmd != NULL &&
	    !egg_screen_execute_command_line_async (screen, cmd, error)) {
		ret = FALSE;
	}

	g_free (cmd);

	return ret;
}


static void
recent_documents_activate_cb (EggRecentViewGtk *view, EggRecentItem *item,
			      GdkScreen *screen)
{
	char   *uri, *uri_utf8, *mime_type;
	GError *error = NULL;
	
	uri = egg_recent_item_get_uri (item);
	uri_utf8 = egg_recent_item_get_uri_utf8 (item);
	mime_type = egg_recent_item_get_mime_type (item);

	if (show_uri (uri, mime_type, screen, &error) != TRUE) {
		if (error != NULL) {
			panel_error_dialog (screen, "cannot_open_recent_doc",
					    error->message);
			g_error_free (error);
		} else
			panel_error_dialog (screen,
					"cannot_open_recent_doc_unknown",
					_("An unknown error occurred while trying"
					  " to open %s"), uri_utf8);
	}

	g_free (uri);
	g_free (uri_utf8);
	g_free (mime_type);
}

void
panel_recent_append_documents_menu (GtkWidget *top_menu)
{
	GtkWidget        *menu;
	GtkWidget        *menu_item;
	EggRecentModel   *model;
	EggRecentViewGtk *view;
	GdkScreen        *screen;

	menu_item = stock_menu_item_new (_("Open Recent"),
					 GTK_STOCK_OPEN,
					 FALSE);
	menu = panel_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu);

	gtk_menu_shell_append (GTK_MENU_SHELL (top_menu), menu_item);
	gtk_widget_show_all (menu_item);
	
	/* a model that shows the global recent doc list */
	model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);

	view = egg_recent_view_gtk_new (menu, NULL);

	screen = gtk_widget_get_screen (menu);
	g_signal_connect (view, "activate",
			  G_CALLBACK (recent_documents_activate_cb),
			  screen);
	egg_recent_view_gtk_show_numbers (view, FALSE);
	egg_recent_view_set_model (EGG_RECENT_VIEW (view), model);
	egg_recent_view_gtk_set_icon_size (view, panel_menu_icon_get_size ());
	g_object_unref (G_OBJECT (model));

	g_object_set_data_full (G_OBJECT (menu), "recent-view",
				view, g_object_unref);
}
