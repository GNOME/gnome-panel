/*
 * panel-menu-bar.c: panel Applications/Places/Desktop menu bar
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Vincent Untz
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
 *	Mark McLoughlin <mark@skynet.ie>
 *	Vincent Untz <vincent@vuntz.net>
 */

#include <config.h>

#include "panel-menu-bar.h"

#include <string.h>
#include <glib/gi18n.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-url.h>

#include "panel-util.h"
#include "panel-background.h"
#include "panel-action-button.h"
#include "panel-stock-icons.h"
#include "panel-recent.h"
#include "applet.h"
#include "menu.h"
#include "panel-gconf.h"
#include "panel-globals.h"
#include "panel-profile.h"
#include "panel-lockdown.h"

#define BOOKMARKS_FILENAME      ".gtk-bookmarks"
#define DESKTOP_IS_HOME_DIR_DIR "/apps/nautilus/preferences"
#define DESKTOP_IS_HOME_DIR_KEY "/apps/nautilus/preferences/desktop_is_home_dir"
#define MAX_ITEMS_OR_SUBMENU    5

/*
 * TODO:
 *   + drag and drop loses icon for URIs
 *   + drag and drop of bookmarks/network places/removable media should create
 *     a menu button
 *   + if a menu is open and get updated, it should reappear and not just
 *     disappear
 *   + see FIXME
 */

#define PANEL_MENU_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_MENU_BAR, PanelMenuBarPrivate))

struct _PanelMenuBarPrivate {
	AppletInfo  *info;
	PanelWidget *panel;

	GtkWidget   *applications_menu;
	GtkWidget   *places_menu;
	GtkWidget   *desktop_menu;
	GtkWidget   *applications_item;
	GtkWidget   *places_item;
	GtkWidget   *desktop_item;

	GnomeVFSMonitorHandle *bookmarks_monitor;

	gulong       volume_mounted_id;
	gulong       volume_unmounted_id;
};

static GnomeVFSVolumeMonitor *volume_monitor = NULL;

static GObjectClass *parent_class;
static GtkWidget *panel_menu_bar_create_applications_menu (PanelMenuBar *menubar);
static GtkWidget *panel_menu_bar_create_places_menu (PanelMenuBar *menubar);
static GtkWidget *panel_menu_bar_create_desktop_menu (PanelMenuBar *menubar);

static void
activate_uri (GtkWidget *menuitem,
	      char      *path)
{
	GError    *error = NULL;
	GdkScreen *screen;
	char      *url;
	char      *escaped;

	screen = menuitem_to_screen (menuitem);

	url = gnome_vfs_make_uri_from_input_with_dirs (path,
						       GNOME_VFS_MAKE_URI_DIR_HOMEDIR);
	gnome_url_show_on_screen (url, screen, &error);

	if (error) {
		escaped = g_markup_escape_text (url, -1);
		panel_error_dialog (screen, "cannot_show_url", TRUE,
				    _("Cannot display location '%s'"),
				    "%s",
				    escaped,
				    error->message);

		g_error_free (error);
		g_free (escaped);
	}
	g_free (url);
}

static void
activate_app_def (GtkWidget *menuitem,
		  char      *path)
{
	GnomeDesktopItem *item;
	GError           *error;

	error = NULL;
	item = gnome_desktop_item_new_from_file (path, 0, &error);
	if (item) {
		g_assert (error == NULL);

		panel_ditem_launch (item, NULL, 0,
				    menuitem_to_screen (menuitem), &error);
		if (error) {
			panel_error_dialog (menuitem_to_screen (menuitem),
					    "cannot_launch_entry", TRUE,
					    _("Cannot launch entry"),
					    "%s",
					    error->message);

			g_error_free (error);
		}
		gnome_desktop_item_unref (item);
	} else {
		g_assert (error != NULL);

		panel_error_dialog (menuitem_to_screen (menuitem),
				    "cannot_load_entry", TRUE,
				    _("Cannot load entry"),
				    "%s",
				    error->message);
		g_error_free (error);
	}
}

static void
panel_menu_bar_append_from_desktop (GtkWidget *menu,
				    char      *path)
{
	GKeyFile  *key_file;
	gboolean   loaded;
	GtkWidget *item;
	char      *path_freeme;
	char      *full_path;
	char      *icon;
	char      *name;
	char      *comment;

	path_freeme = NULL;

	key_file = g_key_file_new ();

	if (g_path_is_absolute (path)) {
		loaded = g_key_file_load_from_file (key_file, path,
						    G_KEY_FILE_NONE, NULL);
		full_path = path;
	} else {
		char *lookup_file;
		char *desktop_path;

		if (!g_str_has_suffix (path, ".desktop")) {
			desktop_path = g_strconcat (path, ".desktop", NULL);
		} else {
			desktop_path = path;
		}

		lookup_file = g_strconcat ("applications", G_DIR_SEPARATOR_S,
					   desktop_path, NULL);
		loaded = g_key_file_load_from_data_dirs (key_file, lookup_file,
							 &path_freeme,
							 G_KEY_FILE_NONE,
							 NULL);
		full_path = path_freeme;
		g_free (lookup_file);

		if (desktop_path != path)
			g_free (desktop_path);
	}

	if (!loaded) {
		g_key_file_free (key_file);
		if (path_freeme)
			g_free (path_freeme);
		return;
	}

	icon    = g_key_file_get_locale_string (key_file, "Desktop Entry",
						"Icon", NULL, NULL);
	name    = g_key_file_get_locale_string (key_file, "Desktop Entry",
						"Name", NULL, NULL);
	comment = g_key_file_get_locale_string (key_file, "Desktop Entry",
						"Comment", NULL, NULL);

	item = gtk_image_menu_item_new ();
	setup_menu_item_with_icon (item, panel_menu_icon_get_size (),
				   icon, NULL, name, TRUE);

	if (comment != NULL)
		gtk_tooltips_set_tip (panel_tooltips, item, comment, NULL);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect_data (item, "activate",
			       G_CALLBACK (activate_app_def),
			       g_strdup (full_path),
			       (GClosureNotify) g_free, 0);
	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);
	setup_uri_drag (item, full_path, icon);

	g_key_file_free (key_file);

	if (path_freeme)
		g_free (path_freeme);
}

static void
panel_menu_bar_show_menu (PanelMenuBar *menubar,
			  GtkWidget    *menu)
{
	gtk_menu_set_screen (GTK_MENU (menu),
			     gtk_widget_get_screen (GTK_WIDGET (menubar)));
}

static void
panel_menu_bar_connect_menu_signals (PanelMenuBar  *menubar,
				     GtkWidget    **menu)
{
	/* intercept all right button clicks makes sure they don't
	   go to the object itself */
	g_signal_connect (*menu, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	g_signal_connect (*menu, "destroy", G_CALLBACK (gtk_widget_destroyed),
			  menu);

	g_signal_connect_swapped (*menu, "show",
				  G_CALLBACK (panel_menu_bar_show_menu),
				  menubar);

	g_signal_connect_swapped (*menu, "hide",
				  G_CALLBACK (gtk_menu_shell_deselect), 
				  menubar);
}

static void
panel_menu_bar_append_place_item (PanelMenuBar *menubar,
				  const char   *icon_name,
				  const char   *title,
				  const char   *tooltip,
				  GtkWidget    *menu,
				  GCallback     callback,
				  const char   *uri)
{
	GtkWidget *item;
	char      *user_data;

	item = gtk_image_menu_item_new ();
	setup_menu_item_with_icon (item,
				   panel_menu_icon_get_size (),
				   icon_name,
				   NULL,
				   title,
				   TRUE);

	gtk_tooltips_set_tip (panel_tooltips,
			      item,
			      tooltip,
			      NULL);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	user_data = g_strdup (uri);
	g_signal_connect_data (item, "activate", callback, user_data,
			       (GClosureNotify) g_free, 0);

	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	setup_uri_drag (item, uri, icon_name);
}

static void
panel_menu_bar_recreate_places_menu (PanelMenuBar *menubar)
{
	if (menubar->priv->places_menu) {
		gtk_widget_destroy (menubar->priv->places_menu);
		menubar->priv->places_menu = panel_menu_bar_create_places_menu (menubar);
		panel_menu_bar_connect_menu_signals (menubar,
						     &menubar->priv->places_menu);
		panel_applet_menu_set_recurse (GTK_MENU (menubar->priv->places_menu),
					       "menu_panel",
					       menubar->priv->panel);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar->priv->places_item),
					   menubar->priv->places_menu);
	}
}

static void
panel_menu_bar_recreate_desktop_menu (PanelMenuBar *menubar)
{
	if (menubar->priv->desktop_menu) {
		gtk_widget_destroy (menubar->priv->desktop_menu);
		menubar->priv->desktop_menu = panel_menu_bar_create_desktop_menu (menubar);
		panel_menu_bar_connect_menu_signals (menubar,
						     &menubar->priv->desktop_menu);
		panel_applet_menu_set_recurse (GTK_MENU (menubar->priv->desktop_menu),
					       "menu_panel",
					       menubar->priv->panel);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar->priv->desktop_item),
					   menubar->priv->desktop_menu);
	}
}

static void
panel_menu_bar_append_applications_menu (GtkWidget    *menu,
					 PanelMenuBar *menubar)
{
	GtkWidget *item;

	if (!g_object_get_data (G_OBJECT (menu), "panel-menu-needs-appending"))
		return;

	g_object_set_data (G_OBJECT (menu), "panel-menu-needs-appending", NULL);

	item = menu_create_action_item (PANEL_ACTION_RUN);

	if (item != NULL &&
	    GTK_MENU_SHELL (menubar->priv->applications_menu)->children != NULL) {
		add_menu_separator (menubar->priv->applications_menu);
		gtk_menu_shell_append (GTK_MENU_SHELL (menubar->priv->applications_menu),
				       item);
	}
}

static GtkWidget *
panel_menu_bar_create_applications_menu (PanelMenuBar *menubar)
{
	GtkWidget *applications_menu;

	applications_menu = create_applications_menu ("applications.menu", NULL);

	g_signal_connect (applications_menu, "show",
			  G_CALLBACK (panel_menu_bar_append_applications_menu),
			  menubar);

	return applications_menu;
}

static void
panel_menu_bar_desktop_is_home_dir_changed (GConfClient  *client,
					    guint         cnxn_id,
					    GConfEntry   *entry,
					    PanelMenuBar *menubar)
{
	panel_menu_bar_recreate_places_menu (menubar);
}

static void
panel_menu_bar_gtk_bookmarks_changed (GnomeVFSMonitorHandle *handle,
				      const gchar *monitor_uri,
				      const gchar *info_uri,
				      GnomeVFSMonitorEventType event_type,
				      gpointer user_data)
{
	g_assert (PANEL_IS_MENU_BAR (user_data));

	panel_menu_bar_recreate_places_menu (PANEL_MENU_BAR (user_data));
}

static void
panel_menu_bar_volume_changed (GnomeVFSVolumeMonitor *monitor,
			       GnomeVFSVolume        *volume,
			       PanelMenuBar          *menubar)
{
	panel_menu_bar_recreate_places_menu (menubar);
}

static void
panel_menu_bar_append_gtk_bookmarks (PanelMenuBar *menubar,
				     GtkWidget    *menu)
{
	GtkWidget   *add_menu;
	char        *filename;
	char        *contents;
	gchar      **lines;
	GHashTable  *table;
	int          i;
	GSList      *add_files, *l;
	GnomeVFSURI *uri;
	char        *basename;
	char        *display_uri;
	char        *tooltip;

	filename = g_build_filename (g_get_home_dir (),
				     BOOKMARKS_FILENAME, NULL);

	if (!g_file_get_contents (filename, &contents, NULL, NULL)) {
		g_free (filename);
		return;
	}

	g_free (filename);

	lines = g_strsplit (contents, "\n", -1);
	g_free (contents);

	table = g_hash_table_new (g_str_hash, g_str_equal);
	add_files = NULL;

	//FIXME gnome_vfs_uri_exists: could there be an authentication
	//dialog if there's a bookmarks to sftp://something?
	for (i = 0; lines[i]; i++) {
		if (lines[i][0] && !g_hash_table_lookup (table, lines[i])) {
			char *unescaped_uri;

			unescaped_uri = gnome_vfs_unescape_string (lines[i],
								   "");
			uri = gnome_vfs_uri_new (unescaped_uri);
			g_free (unescaped_uri);

			if (!gnome_vfs_uri_exists (uri)) {
				gnome_vfs_uri_unref (uri);
				continue;
			}

			add_files = g_slist_prepend (add_files, uri);
			g_hash_table_insert (table, lines[i], lines[i]);
		}
	}

	g_hash_table_destroy (table);
	g_strfreev (lines);

	if (g_slist_length (add_files) <= MAX_ITEMS_OR_SUBMENU) {
		add_menu = menu;
	} else {
		GtkWidget *item;

		item = gtk_image_menu_item_new ();
		setup_menu_item_with_icon (item, panel_menu_icon_get_size (),
					   "stock_bookmark", NULL,
					   _("Bookmarks"), TRUE);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		add_menu = create_empty_menu ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), add_menu);
	}

	for (l = add_files; l; l = l->next) {
		const char *full_uri;

		uri = l->data;
		full_uri = gnome_vfs_uri_get_path (uri);

		display_uri = gnome_vfs_format_uri_for_display (full_uri);
		/* Translators: %s is a URI */
		tooltip = g_strdup_printf (_("Open '%s'"), display_uri);
		g_free (display_uri);

		basename = gnome_vfs_uri_extract_short_name (uri);
		panel_menu_bar_append_place_item (menubar, "gnome-fs-directory",
						  basename, tooltip, add_menu,
						  G_CALLBACK (activate_uri),
						  full_uri);

		g_free (basename);
		g_free (tooltip);
		gnome_vfs_uri_unref (uri);
	}

	g_slist_free (add_files);
}

static void
panel_menu_bar_append_volumes (PanelMenuBar *menubar,
			       GtkWidget    *menu,
			       gboolean      connected_volumes)
{
	GtkWidget *add_menu;
	GList     *volumes, *l;
	GSList    *add_volumes, *sl;

	volumes = gnome_vfs_volume_monitor_get_mounted_volumes (volume_monitor);
	add_volumes = NULL;

	for (l = volumes; l; l = l->next) {
		GnomeVFSVolume *volume = l->data;

		if (!gnome_vfs_volume_is_user_visible (volume) ||
		    !gnome_vfs_volume_is_mounted (volume))
			continue;

		switch (gnome_vfs_volume_get_volume_type (volume)) {
		case GNOME_VFS_VOLUME_TYPE_CONNECTED_SERVER:
			if (connected_volumes)
				add_volumes = g_slist_prepend (add_volumes,
							       volume);
			break;
		default:
			if (!connected_volumes)
				add_volumes = g_slist_prepend (add_volumes,
							       volume);
			break;
		}
	}

	add_volumes = g_slist_reverse (add_volumes);

	if (g_slist_length (add_volumes) <= MAX_ITEMS_OR_SUBMENU) {
		add_menu = menu;
	} else {
		GtkWidget *item;

		if (connected_volumes)
			item = gtk_menu_item_new_with_label (_("Network Places"));
		else
			item = gtk_menu_item_new_with_label (_("Removable Media"));

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		add_menu = create_empty_menu ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), add_menu);
	}

	for (sl = add_volumes; sl; sl = sl->next) {
		GnomeVFSVolume *volume = sl->data;
		char           *icon;
		char           *display_name;
		char           *activation_uri;

		icon           = gnome_vfs_volume_get_icon (volume);
		display_name   = gnome_vfs_volume_get_display_name (volume);
		activation_uri = gnome_vfs_volume_get_activation_uri (volume);

		panel_menu_bar_append_place_item (menubar,
						  icon,
						  display_name,
						  display_name, //FIXME tooltip
						  add_menu,
						  G_CALLBACK (activate_uri),
						  activation_uri);

		g_free (icon);
		g_free (display_name);
		g_free (activation_uri);
	}

	g_slist_free (add_volumes);

	g_list_foreach (volumes, (GFunc) gnome_vfs_volume_unref, NULL);
	g_list_free (volumes);
}

static GtkWidget *
panel_menu_bar_create_places_menu (PanelMenuBar *menubar)
{
	GtkWidget *places_menu;

	places_menu = panel_create_menu ();

	panel_menu_bar_append_from_desktop (places_menu,
					    "nautilus-home.desktop");

	if (!gconf_client_get_bool (panel_gconf_get_client (),
				    DESKTOP_IS_HOME_DIR_KEY,
				    NULL))
		panel_menu_bar_append_place_item (menubar, "gnome-fs-desktop",
						  _("Desktop"),
						  _("Desktop directory"),
						  places_menu,
						  G_CALLBACK (activate_uri),
						  "Desktop");

	panel_menu_bar_append_gtk_bookmarks (menubar, places_menu);
	add_menu_separator (places_menu);

	panel_menu_bar_append_from_desktop (places_menu,
					    "nautilus-computer.desktop");
	panel_menu_bar_append_volumes (menubar, places_menu, FALSE);
	add_menu_separator (places_menu);

	panel_menu_bar_append_from_desktop (places_menu,
					    "network-scheme.desktop");
	panel_menu_bar_append_volumes (menubar, places_menu, TRUE);
#if 0
	item = menu_create_action_item (PANEL_ACTION_CONNECT_SERVER);
	if (item != NULL)
		gtk_menu_shell_append (GTK_MENU_SHELL (places_menu), item);
#endif

	add_menu_separator (places_menu);

	panel_menu_bar_append_from_desktop (places_menu,
					    "gnome-search-tool.desktop");

	panel_recent_append_documents_menu (places_menu);

	return places_menu;
}

static void
panel_menu_bar_append_desktop_menu (GtkWidget    *menu,
				    PanelMenuBar *menubar)
{
	GtkWidget *desktop_menu;
	GtkWidget *item;
	gboolean   separator_inserted;

	if (!g_object_get_data (G_OBJECT (menu), "panel-menu-needs-appending"))
		return;

	g_object_set_data (G_OBJECT (menu), "panel-menu-needs-appending", NULL);

	desktop_menu = menubar->priv->desktop_menu;

	if (GTK_MENU_SHELL (desktop_menu)->children != NULL)
		add_menu_separator (desktop_menu);

	if (panel_is_program_in_path ("gnome-screenshot")) {
		item = menu_create_action_item (PANEL_ACTION_SCREENSHOT);
		if (item != NULL)
			gtk_menu_shell_append (GTK_MENU_SHELL (desktop_menu),
					       item);
	}

	panel_menu_bar_append_from_desktop (desktop_menu,
					    "yelp.desktop");

	panel_menu_bar_append_from_desktop (desktop_menu,
					    "gnome-about.desktop");

	separator_inserted = FALSE;

	if (panel_is_program_in_path ("xscreensaver")) {
		item = menu_create_action_item (PANEL_ACTION_LOCK);
		if (item != NULL) {
			add_menu_separator (desktop_menu);
			gtk_menu_shell_append (GTK_MENU_SHELL (desktop_menu), item);
			separator_inserted = TRUE;
		}
	}

	item = menu_create_action_item (PANEL_ACTION_LOGOUT);
	if (item != NULL) {
		if (!separator_inserted) {
			add_menu_separator (desktop_menu);
			separator_inserted = TRUE;
		}
		gtk_menu_shell_append (GTK_MENU_SHELL (desktop_menu), item);
	}
}

static GtkWidget *
panel_menu_bar_create_desktop_menu (PanelMenuBar *menubar)
{
	GtkWidget *desktop_menu;

	desktop_menu = create_applications_menu ("settings.menu", NULL);

	g_signal_connect (desktop_menu, "show",
			  G_CALLBACK (panel_menu_bar_append_desktop_menu),
			  menubar);

	return desktop_menu;
}

static void
panel_menu_bar_instance_init (PanelMenuBar      *menubar,
			      PanelMenuBarClass *klass)
{
	GtkWidget *image;
	char      *bookmarks_filename;
	char      *bookmarks_uri;

	menubar->priv = PANEL_MENU_BAR_GET_PRIVATE (menubar);

	menubar->priv->info = NULL;

	gconf_client_add_dir (panel_gconf_get_client (),
			      DESKTOP_IS_HOME_DIR_DIR,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);

	panel_gconf_notify_add_while_alive (DESKTOP_IS_HOME_DIR_KEY,
					    (GConfClientNotifyFunc) panel_menu_bar_desktop_is_home_dir_changed,
					    G_OBJECT (menubar));

	bookmarks_filename = g_build_filename (g_get_home_dir (),
					       BOOKMARKS_FILENAME, NULL);
	bookmarks_uri = gnome_vfs_get_uri_from_local_path (bookmarks_filename);

	if (bookmarks_uri) {
		GnomeVFSResult result;
		result = gnome_vfs_monitor_add (&menubar->priv->bookmarks_monitor,
						bookmarks_uri,
						GNOME_VFS_MONITOR_FILE,
						panel_menu_bar_gtk_bookmarks_changed,
						menubar);
		if (result != GNOME_VFS_OK)
			g_warning ("Failed to add file monitor for %s: %s\n",
				   bookmarks_uri,
				   gnome_vfs_result_to_string (result));

		g_free (bookmarks_uri);
	} else {
		g_warning ("Could not make URI of ~/"BOOKMARKS_FILENAME);
	}

	g_free (bookmarks_filename);

	if (!volume_monitor)
		volume_monitor = gnome_vfs_get_volume_monitor ();

	menubar->priv->volume_mounted_id = g_signal_connect (volume_monitor,
							     "volume_mounted",
							     G_CALLBACK (panel_menu_bar_volume_changed),
							     menubar);
	menubar->priv->volume_unmounted_id = g_signal_connect (volume_monitor,
							       "volume_unmounted",
							       G_CALLBACK (panel_menu_bar_volume_changed),
							       menubar);

	menubar->priv->applications_menu = panel_menu_bar_create_applications_menu (menubar);
	panel_menu_bar_connect_menu_signals (menubar,
					     &menubar->priv->applications_menu);

	menubar->priv->places_menu = panel_menu_bar_create_places_menu (menubar);
	panel_menu_bar_connect_menu_signals (menubar,
					     &menubar->priv->places_menu);

	menubar->priv->desktop_menu = panel_menu_bar_create_desktop_menu (menubar);
	panel_menu_bar_connect_menu_signals (menubar,
					     &menubar->priv->desktop_menu);

	menubar->priv->applications_item = gtk_image_menu_item_new_with_label (_("Applications"));
	image = gtk_image_new_from_stock (PANEL_STOCK_GNOME_LOGO,
					  panel_menu_bar_icon_get_size ());
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menubar->priv->applications_item),
				       image);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar->priv->applications_item),
				   menubar->priv->applications_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar),
			       menubar->priv->applications_item);

	menubar->priv->places_item = gtk_menu_item_new_with_label (_("Places"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar->priv->places_item),
				   menubar->priv->places_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar),
			       menubar->priv->places_item);

	menubar->priv->desktop_item = gtk_menu_item_new_with_label (_("Desktop"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar->priv->desktop_item),
				   menubar->priv->desktop_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar),
			       menubar->priv->desktop_item);

	panel_lockdown_notify_add (G_CALLBACK (panel_menu_bar_recreate_desktop_menu),
				   menubar);
}

static void
panel_menu_bar_parent_set (GtkWidget *widget,
			   GtkWidget *previous_parent)
{
	PanelMenuBar *menubar = PANEL_MENU_BAR (widget);

	g_assert (!widget->parent || PANEL_IS_WIDGET (widget->parent));

	menubar->priv->panel = (PanelWidget *) widget->parent;

	if (menubar->priv->applications_menu)
		panel_applet_menu_set_recurse (GTK_MENU (menubar->priv->applications_menu),
					       "menu_panel",
					       menubar->priv->panel);
	if (menubar->priv->places_menu)
		panel_applet_menu_set_recurse (GTK_MENU (menubar->priv->places_menu),
					       "menu_panel",
					       menubar->priv->panel);
	if (menubar->priv->desktop_menu)
		panel_applet_menu_set_recurse (GTK_MENU (menubar->priv->desktop_menu),
					       "menu_panel",
					       menubar->priv->panel);
}

static void
panel_menu_bar_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
	GtkAllocation old_allocation;

	old_allocation.x      = widget->allocation.x;
	old_allocation.y      = widget->allocation.y;
	old_allocation.width  = widget->allocation.width;
	old_allocation.height = widget->allocation.height;

	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

	if (old_allocation.x      == allocation->x &&
	    old_allocation.y      == allocation->y &&
	    old_allocation.width  == allocation->width &&
	    old_allocation.height == allocation->height)
		return;

	panel_menu_bar_change_background (PANEL_MENU_BAR (widget));
}

static void
panel_menu_bar_finalize (GObject *object)
{
	PanelMenuBar *menubar = (PanelMenuBar *) object;

	panel_lockdown_notify_remove (G_CALLBACK (panel_menu_bar_recreate_desktop_menu),
				      menubar);

	gconf_client_remove_dir (panel_gconf_get_client (),
				 DESKTOP_IS_HOME_DIR_DIR,
				 NULL);

	if (menubar->priv->bookmarks_monitor != NULL)
		gnome_vfs_monitor_cancel (menubar->priv->bookmarks_monitor);
	menubar->priv->bookmarks_monitor = NULL;

	if (menubar->priv->volume_mounted_id)
		g_signal_handler_disconnect (volume_monitor,
					     menubar->priv->volume_mounted_id);
	menubar->priv->volume_mounted_id = 0;

	if (menubar->priv->volume_unmounted_id)
		g_signal_handler_disconnect (volume_monitor,
					     menubar->priv->volume_unmounted_id);
	menubar->priv->volume_unmounted_id = 0;

	parent_class->finalize (object);
}

static void
panel_menu_bar_class_init (PanelMenuBarClass *klass)
{
	GObjectClass   *gobject_class = (GObjectClass   *) klass;
	GtkWidgetClass *widget_class  = (GtkWidgetClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize  = panel_menu_bar_finalize;
	widget_class->parent_set = panel_menu_bar_parent_set;
	widget_class->size_allocate = panel_menu_bar_size_allocate;

	g_type_class_add_private (klass, sizeof (PanelMenuBarPrivate));

	gtk_rc_parse_string (
		"style \"panel-menubar-style\"\n"
		"{\n"
		"  GtkMenuBar::shadow-type = none\n"
		"  GtkMenuBar::internal-padding = 0\n"
		"}\n"
		"class \"PanelMenuBar\" style \"panel-menubar-style\"");
}

GType
panel_menu_bar_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelMenuBarClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_menu_bar_class_init,
			NULL,
			NULL,
			sizeof (PanelMenuBar),
			0,
			(GInstanceInitFunc) panel_menu_bar_instance_init,
			NULL
		};

		type = g_type_register_static (
				GTK_TYPE_MENU_BAR, "PanelMenuBar", &info, 0);
	}

	return type;
}

static gboolean
panel_menu_bar_on_expose (GtkWidget      *widget,
			  GdkEventExpose *event,
			  gpointer        data)
{
	PanelMenuBar *menubar = data;

	if (GTK_WIDGET_HAS_FOCUS (menubar))
		gtk_paint_focus (widget->style,
				 widget->window, 
				 GTK_WIDGET_STATE (menubar),
				 NULL,
				 widget,
				 "menubar-applet",
				 0, 0, -1, -1);
	return FALSE;
}

static void
panel_menu_bar_load (PanelWidget *panel,
		     gboolean     locked,
		     int          position,
		     gboolean     exactpos,
		     const char  *id)
{
	PanelMenuBar *menubar;

	g_return_if_fail (panel != NULL);

	menubar = g_object_new (PANEL_TYPE_MENU_BAR, NULL);

	menubar->priv->info = panel_applet_register (
					GTK_WIDGET (menubar), NULL, NULL,
					panel, locked, position, exactpos,
					PANEL_OBJECT_MENU_BAR, id);
	if (!menubar->priv->info) {
		gtk_widget_destroy (GTK_WIDGET (menubar));
		return;
	}

	panel_applet_add_callback (menubar->priv->info,
				   "help",
				   GTK_STOCK_HELP,
				   _("_Help"),
				   NULL);
	g_signal_connect_after (G_OBJECT (menubar), "focus-in-event",
				G_CALLBACK (gtk_widget_queue_draw), menubar);
	g_signal_connect_after (G_OBJECT (menubar), "focus-out-event",
				G_CALLBACK (gtk_widget_queue_draw), menubar);
	g_signal_connect_after (G_OBJECT (menubar), "expose-event",
				G_CALLBACK (panel_menu_bar_on_expose), menubar);
	panel_widget_set_applet_expandable (panel, GTK_WIDGET (menubar), FALSE, TRUE);
	GTK_WIDGET_SET_FLAGS (menubar, GTK_CAN_FOCUS);
}

void
panel_menu_bar_load_from_gconf (PanelWidget *panel,
				gboolean     locked,
				int          position,
				gboolean     exactpos,
				const char  *id)
{
	panel_menu_bar_load (panel, locked, position, exactpos, id);
}

void
panel_menu_bar_create (PanelToplevel *toplevel,
		       int            position)
{
	char *id;

	id = panel_profile_prepare_object (PANEL_OBJECT_MENU_BAR, toplevel, position, FALSE);
	panel_profile_add_to_list (PANEL_GCONF_OBJECTS, id);
	g_free (id);
}

void
panel_menu_bar_invoke_menu (PanelMenuBar *menubar,
			    const char   *callback_name)
{
	GdkScreen *screen;

	g_return_if_fail (PANEL_IS_MENU_BAR (menubar));
	g_return_if_fail (callback_name != NULL);

	if (strcmp (callback_name, "help"))
		return;

	screen = gtk_widget_get_screen (GTK_WIDGET (menubar));

	panel_show_help (screen, "user-guide.xml", "gospanel-37");
}

void
panel_menu_bar_popup_menu (PanelMenuBar *menubar,
			   guint32       activate_time)
{
	GtkMenu *menu;
	GtkMenuShell *menu_shell;
	
	g_return_if_fail (PANEL_IS_MENU_BAR (menubar));

	menu = GTK_MENU (menubar->priv->applications_menu);

	/* 
	 * We need to call _gtk_menu_shell_activate() here as is done in 
	 * window_key_press_handler in gtkmenubar.c which pops up menu
	 * when F10 is pressed.
	 *
	 * As that function is private its code is replicated here.
	 */
	menu_shell = GTK_MENU_SHELL (menubar);
	if (!menu_shell->active) {
		gtk_grab_add (GTK_WIDGET (menu_shell));
		menu_shell->have_grab = TRUE;
		menu_shell->active = TRUE;
	}
	gtk_menu_shell_select_item (menu_shell,
				    gtk_menu_get_attach_widget (menu));
}

void panel_menu_bar_change_background (PanelMenuBar *menubar)
{
	GtkRcStyle       *rc_style;
	GtkStyle         *style;
	const PanelColor *color;
	GdkGC            *gc;
	GdkPixmap        *pixmap;
	const GdkPixmap  *bg_pixmap;

	/* reset style */
	gtk_widget_set_style (GTK_WIDGET (menubar), NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (GTK_WIDGET (menubar), rc_style);
	g_object_unref (rc_style);

	switch (panel_background_get_type (&menubar->priv->panel->background)) {
	case PANEL_BACK_NONE:
		break;
	case PANEL_BACK_COLOR:
		color = panel_background_get_color (&menubar->priv->panel->background);
		if (color->alpha == 0xffff) {
			gtk_widget_modify_bg (GTK_WIDGET (menubar),
					      GTK_STATE_NORMAL, &(color->gdk));
			break;
		}
		/* else, we have an image, so don't break */
	case PANEL_BACK_IMAGE:
		bg_pixmap = panel_background_get_pixmap (&menubar->priv->panel->background);
		if (!bg_pixmap)
			return;

		gc = gdk_gc_new (GTK_WIDGET (menubar)->window);
		g_return_if_fail (GDK_IS_GC (gc));

		pixmap = gdk_pixmap_new (GTK_WIDGET (menubar)->window,
					 GTK_WIDGET (menubar)->allocation.width,
					 GTK_WIDGET (menubar)->allocation.height,
					 -1);

		gdk_draw_drawable (GDK_DRAWABLE (pixmap),
				   gc, 
				   GDK_DRAWABLE (bg_pixmap),
				   GTK_WIDGET (menubar)->allocation.x,
				   GTK_WIDGET (menubar)->allocation.y,
				   0, 0,
				   GTK_WIDGET (menubar)->allocation.width,
				   GTK_WIDGET (menubar)->allocation.height);

		g_object_unref (gc);

		style = gtk_style_copy (GTK_WIDGET (menubar)->style);
		if (style->bg_pixmap[GTK_STATE_NORMAL])
			g_object_unref (style->bg_pixmap[GTK_STATE_NORMAL]);
		style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref (pixmap);
		gtk_widget_set_style (GTK_WIDGET (menubar), style);

		g_object_unref (pixmap);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}
