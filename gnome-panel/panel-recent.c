/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include <string.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "egg-recent-model.h"
#include "egg-recent-view.h"
#include "egg-recent-view-gtk.h"
#include "egg-recent-item.h"
#include "menu.h"
#include "panel-util.h"
#include "panel-globals.h"
#include "panel-recent.h"
#include "panel-stock-icons.h"
#include "panel-multiscreen.h"

static gboolean
show_uri (const char *uri, const char *mime_type, GdkScreen *screen,
	  GError **error)
{
	char **env;
	GnomeVFSResult result;
	GnomeVFSMimeApplication *app;
	GList *uris = NULL;

	app = gnome_vfs_mime_get_default_application_for_uri (uri, mime_type);
	if (app == NULL) {
		g_set_error (error, 0, 0, _("Couldn't find a suitable application"));
		return FALSE;
	}

	env = panel_make_environment_for_screen (screen, NULL);

	uris = g_list_append (uris, (gpointer)uri);
	result = gnome_vfs_mime_application_launch_with_env (app, uris, env);
	g_list_free (uris);

	g_strfreev (env);
	gnome_vfs_mime_application_free (app);

	if (result != GNOME_VFS_OK) {
		return FALSE;
	}

	return TRUE;
}


static void
recent_documents_activate_cb (EggRecentViewGtk *view, EggRecentItem *item,
			      GtkWidget *widget)
{
	char   *uri, *uri_utf8, *mime_type;
	GError *error = NULL;
	GdkScreen *screen;

	screen = gtk_widget_get_screen (widget);
	
	uri = egg_recent_item_get_uri (item);
	uri_utf8 = egg_recent_item_get_uri_utf8 (item);
	mime_type = egg_recent_item_get_mime_type (item);

	if (show_uri (uri, mime_type, screen, &error) != TRUE) {
		if (error != NULL) {
			panel_error_dialog (screen,
					    "cannot_open_recent_doc", TRUE,
					    _("Cannot open recently used document"),
					    "%s",
					    error->message);
			g_error_free (error);
		} else
			panel_error_dialog (screen,
					    "cannot_open_recent_doc_unknown",
					    TRUE,
					    _("Cannot open recently used document"),
					    _("An unknown error occurred while trying to open %s"),
					    uri_utf8);
	}

	g_free (uri);
	g_free (uri_utf8);
	g_free (mime_type);
}

static void
panel_recent_model_changed_cb (EggRecentModel *model,
                               GList          *list,
                               GtkWidget      *menu_item)
{
	GList *recent_list = NULL;

	recent_list = egg_recent_model_get_list (model);
	if (recent_list)
		gtk_widget_set_sensitive (menu_item, TRUE);
	else
		gtk_widget_set_sensitive (menu_item, FALSE);
}

static GtkWidget *clear_recent_dialog = NULL;

static void
clear_dialog_response (GtkWidget      *widget,
		       int             response,
		       EggRecentModel *model)
{
        if (response == GTK_RESPONSE_ACCEPT)
		egg_recent_model_clear (model);

	gtk_widget_destroy (widget);
}

static void
recent_documents_clear_cb (GtkMenuItem    *menuitem,
                           EggRecentModel *model)
{
	gpointer tmp;

	if (clear_recent_dialog != NULL) {
		gtk_window_set_screen (GTK_WINDOW (clear_recent_dialog),
				       gtk_widget_get_screen (GTK_WIDGET (menuitem)));
		gtk_window_present (GTK_WINDOW (clear_recent_dialog));
		return;
	}

	clear_recent_dialog = gtk_message_dialog_new (NULL,
						      0 /* flags */,
						      GTK_MESSAGE_QUESTION,
						      GTK_BUTTONS_NONE,
						      _("Clear the Recent Documents list?"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (clear_recent_dialog),
						  _("If you clear the Recent Documents list, you clear the following:\n\n"
						    "\342\200\242 All items from the Actions \342\206\222 Recent Documents menu item.\n"
						    "\342\200\242 All items from the recent documents list in all applications."));

	gtk_dialog_add_buttons (GTK_DIALOG (clear_recent_dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				PANEL_STOCK_CLEAR, GTK_RESPONSE_ACCEPT,
				NULL);

	gtk_container_set_border_width (GTK_CONTAINER (clear_recent_dialog), 6);

	gtk_dialog_set_default_response (GTK_DIALOG (clear_recent_dialog), GTK_RESPONSE_ACCEPT);

	g_signal_connect (clear_recent_dialog, "response",
			  G_CALLBACK (clear_dialog_response), model);

	tmp = &clear_recent_dialog;
	g_object_add_weak_pointer (G_OBJECT (clear_recent_dialog), tmp);

	gtk_window_set_screen (GTK_WINDOW (clear_recent_dialog),
			       gtk_widget_get_screen (GTK_WIDGET (menuitem)));
	gtk_widget_show (clear_recent_dialog);
}

static void
recent_documents_tooltip_func (GtkTooltips   *tooltips,
			       GtkWidget     *menu_item,
			       EggRecentItem *item,
			       gpointer       user_data)
{
	char *uri;
	char *tooltip;
	int   offset;

	uri = egg_recent_item_get_uri_for_display (item);
	g_return_if_fail (uri != NULL);

	if (g_str_has_prefix (uri, g_get_home_dir ()))
		offset = strlen (g_get_home_dir ()) + 1;
	else
		offset = 0;

	/* Translators: %s is a URI */
	tooltip = g_strdup_printf (_("Open '%s'"), uri + offset);

	g_free (uri);

	gtk_tooltips_set_tip (tooltips, menu_item, tooltip, NULL);

	g_free (tooltip);
}

void
panel_recent_append_documents_menu (GtkWidget *top_menu)
{
	GtkWidget        *menu;
	GtkWidget        *menu_item;
	EggRecentModel   *model;
	EggRecentViewGtk *view;

	menu_item = gtk_image_menu_item_new ();
	setup_stock_menu_item (menu_item,
			       panel_menu_icon_get_size (),
			       GTK_STOCK_OPEN,
			       _("Recent Documents"),
			       TRUE);
	menu = panel_create_menu ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu);

	g_signal_connect (G_OBJECT (menu), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	gtk_menu_shell_append (GTK_MENU_SHELL (top_menu), menu_item);
	gtk_widget_show_all (menu_item);
	
	/* a model that shows the global recent doc list */
	model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);
	g_signal_connect (model, "changed",
			  G_CALLBACK (panel_recent_model_changed_cb),
			  menu_item);

	view = egg_recent_view_gtk_new (menu, NULL);

	egg_recent_view_gtk_set_tooltip_func (view,
					      recent_documents_tooltip_func,
					      NULL);
	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	menu_item = gtk_image_menu_item_new ();
	setup_stock_menu_item (menu_item,
			       panel_menu_icon_get_size (),
			       GTK_STOCK_CLEAR,
			       _("Clear Recent Documents"),
			       TRUE);
	gtk_tooltips_set_tip (panel_tooltips,
			      menu_item,
			      _("Clear all items from the recent documents list"),
			      NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (recent_documents_clear_cb),
			  model);

	g_signal_connect (view, "activate",
			  G_CALLBACK (recent_documents_activate_cb),
			  menu);
	egg_recent_view_gtk_show_numbers (view, FALSE);
	egg_recent_view_set_model (EGG_RECENT_VIEW (view), model);
	egg_recent_view_gtk_set_icon_size (view, panel_menu_icon_get_size ());
	g_object_unref (G_OBJECT (model));

	g_object_set_data_full (G_OBJECT (menu), "recent-view",
				view, g_object_unref);
}
