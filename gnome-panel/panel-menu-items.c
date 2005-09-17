/*
 * Copyright (C) 2005 Vincent Untz
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
 *	Vincent Untz <vincent@vuntz.net>
 *
 * Based on code from panel-menu-bar.c
 */

/*
 * TODO:
 *   + drag and drop loses icon for URIs
 *   + drag and drop of bookmarks/network places/removable media should create
 *     a menu button
 *   + if a menu is open and gets updated, it should reappear and not just
 *     disappear
 */

#include <config.h>

#include "panel-menu-items.h"

#include <string.h>
#include <glib/gi18n.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-url.h>

#include "menu.h"
#include "panel-action-button.h"
#include "panel-globals.h"
#include "panel-lockdown.h"
#include "panel-recent.h"
#include "panel-stock-icons.h"
#include "panel-util.h"

#define BOOKMARKS_FILENAME      ".gtk-bookmarks"
#define DESKTOP_IS_HOME_DIR_DIR "/apps/nautilus/preferences"
#define DESKTOP_IS_HOME_DIR_KEY "/apps/nautilus/preferences/desktop_is_home_dir"
#define NAMES_DIR               "/apps/nautilus/desktop"
#define HOME_NAME_KEY           "/apps/nautilus/desktop/home_icon_name"
#define COMPUTER_NAME_KEY       "/apps/nautilus/desktop/computer_icon_name"
#define MAX_ITEMS_OR_SUBMENU    5

#define PANEL_PLACE_MENU_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_PLACE_MENU_ITEM, PanelPlaceMenuItemPrivate))
#define PANEL_DESKTOP_MENU_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_DESKTOP_MENU_ITEM, PanelDesktopMenuItemPrivate))

struct _PanelPlaceMenuItemPrivate {
	GtkWidget   *menu;
	PanelWidget *panel;

	EggRecentViewGtk *recent_view;

	GnomeVFSMonitorHandle *bookmarks_monitor;

	gulong       volume_mounted_id;
	gulong       volume_unmounted_id;

	guint        use_image : 1;
};

struct _PanelDesktopMenuItemPrivate {
	GtkWidget   *menu;
	PanelWidget *panel;

	guint        use_image : 1;
	guint        append_lock_logout : 1;
};

static GObjectClass *place_parent_class;
static GObjectClass *desktop_parent_class;

static GnomeVFSVolumeMonitor *volume_monitor = NULL;

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
panel_menu_items_append_from_desktop (GtkWidget *menu,
				      char      *path,
				      char      *force_name)
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
	comment = g_key_file_get_locale_string (key_file, "Desktop Entry",
						"Comment", NULL, NULL);

	if (string_empty (force_name))
		name = g_key_file_get_locale_string (key_file, "Desktop Entry",
						     "Name", NULL, NULL);
	else
		name = g_strdup (force_name);

	item = gtk_image_menu_item_new ();
	setup_menu_item_with_icon (item, panel_menu_icon_get_size (),
				   icon, NULL, name, TRUE);

	if (comment != NULL)
		gtk_tooltips_set_tip (panel_tooltips, item, comment, NULL);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect_data (item, "activate",
			       G_CALLBACK (panel_menu_item_activate_desktop_file),
			       g_strdup (full_path),
			       (GClosureNotify) g_free, 0);
	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);
	setup_uri_drag (item, full_path, icon);

	g_key_file_free (key_file);

	if (icon)
		g_free (icon);

	if (name)
		g_free (name);

	if (comment)
		g_free (comment);

	if (path_freeme)
		g_free (path_freeme);
}

static void
panel_menu_items_append_place_item (const char *icon_name,
				    const char *title,
				    const char *tooltip,
				    GtkWidget  *menu,
				    GCallback   callback,
				    const char *uri)
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
panel_place_menu_item_append_gtk_bookmarks (GtkWidget *menu)
{
	typedef struct {
		GnomeVFSURI *uri;
		char        *label;
	} PanelBookmark;

	GtkWidget   *add_menu;
	char        *filename;
	char        *contents;
	gchar      **lines;
	GHashTable  *table;
	int          i;
	GSList      *add_bookmarks, *l;
	PanelBookmark *bookmark;

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
	add_bookmarks = NULL;

	for (i = 0; lines[i]; i++) {
		if (lines[i][0] && !g_hash_table_lookup (table, lines[i])) {
			GnomeVFSURI *uri;
			char        *space;
			char        *label;
			char        *unescaped_uri;

			space = strchr (lines[i], ' ');
			if (space) {
				*space = '\0';
				label = g_strdup (space + 1);
			} else {
				label = NULL;
			}

			unescaped_uri = gnome_vfs_unescape_string (lines[i],
								   "");
			uri = gnome_vfs_uri_new (unescaped_uri);
			g_free (unescaped_uri);

			if (!uri ||
			     (gnome_vfs_uri_is_local (uri) &&
			     !gnome_vfs_uri_exists (uri))) {
				if (label)
					g_free (label);
				gnome_vfs_uri_unref (uri);
				continue;
			}

			bookmark = g_malloc (sizeof (PanelBookmark));
			bookmark->uri = uri;
			bookmark->label = label;
			add_bookmarks = g_slist_prepend (add_bookmarks, bookmark);
			g_hash_table_insert (table, lines[i], lines[i]);
		}
	}

	g_hash_table_destroy (table);
	g_strfreev (lines);

	add_bookmarks = g_slist_reverse (add_bookmarks);

	if (g_slist_length (add_bookmarks) <= MAX_ITEMS_OR_SUBMENU) {
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

	for (l = add_bookmarks; l; l = l->next) {
		char *full_uri;
		char *display_uri;
		char *tooltip;
		char *label;

		bookmark = l->data;
		full_uri = gnome_vfs_uri_to_string (bookmark->uri,
						    GNOME_VFS_URI_HIDE_NONE);

		display_uri = gnome_vfs_format_uri_for_display (full_uri);
		/* Translators: %s is a URI */
		tooltip = g_strdup_printf (_("Open '%s'"), display_uri);
		g_free (display_uri);

		label = NULL;
		if (bookmark->label) {
			label = g_strdup (g_strstrip (bookmark->label));
			if (!label [0]) {
				g_free (label);
				label = NULL;
			}
		}

		if (!label) {
			if (gnome_vfs_uri_is_local (bookmark->uri)) {
				const char *buffer;

				buffer = gnome_vfs_uri_get_path (bookmark->uri);
				label = g_filename_display_basename (buffer);
			} else {
				char *buffer;

				buffer = gnome_vfs_uri_extract_short_name (bookmark->uri);
				label = g_filename_display_name (buffer);
				g_free (buffer);
			}
		}

		panel_menu_items_append_place_item ("gnome-fs-directory",
						    label,
						    tooltip,
						    add_menu,
						    G_CALLBACK (activate_uri),
						    full_uri);

		g_free (tooltip);
		g_free (full_uri);
		g_free (label);
		if (bookmark->label)
			g_free (bookmark->label);
		gnome_vfs_uri_unref (bookmark->uri);
		g_free (bookmark);
	}

	g_slist_free (add_bookmarks);
}

static gint
panel_place_menu_item_sort_volume (gconstpointer a,
				   gconstpointer b) {
	const GnomeVFSVolume *volume_a = a;
	const GnomeVFSVolume *volume_b = b;
	char                 *display_a;
	char                 *display_b;
	int                   retval;

	g_return_val_if_fail (a && b, 0);

	display_a = gnome_vfs_volume_get_display_name (volume_a);
	display_b = gnome_vfs_volume_get_display_name (volume_b);

	retval = g_utf8_collate (display_a, display_b);

	g_free (display_a);
	g_free (display_b);

	return retval;
}

static void
panel_place_menu_item_append_volumes (GtkWidget *menu,
				      gboolean   connected_volumes)
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

	if (connected_volumes)
		add_volumes = g_slist_sort (add_volumes,
					    panel_place_menu_item_sort_volume);
	else
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

		panel_menu_items_append_place_item (icon,
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
panel_place_menu_item_create_menu (EggRecentViewGtk **recent_view)
{
	GtkWidget *places_menu;
	GtkWidget *item;
	char      *gconf_name;

	places_menu = panel_create_menu ();

	gconf_name = gconf_client_get_string (panel_gconf_get_client (),
					      HOME_NAME_KEY,
					      NULL);
	panel_menu_items_append_from_desktop (places_menu,
					      "nautilus-home.desktop",
					      gconf_name);
	if (gconf_name)
		g_free (gconf_name);

	if (!gconf_client_get_bool (panel_gconf_get_client (),
				    DESKTOP_IS_HOME_DIR_KEY,
				    NULL))
		panel_menu_items_append_place_item (
				"gnome-fs-desktop",
				/* Translators: Desktop is used here as in
				 * "Desktop Folder" (this is not the Desktop
				 * environment). Do not keep "Desktop Folder|"
				 * in the translation */
				Q_("Desktop Folder|Desktop"),
				_("Open the desktop as a folder"),
				places_menu,
				G_CALLBACK (activate_uri),
				"Desktop");

	panel_place_menu_item_append_gtk_bookmarks (places_menu);
	add_menu_separator (places_menu);

	gconf_name = gconf_client_get_string (panel_gconf_get_client (),
					      COMPUTER_NAME_KEY,
					      NULL);
	panel_menu_items_append_from_desktop (places_menu,
					      "nautilus-computer.desktop",
					      gconf_name);
	if (gconf_name)
		g_free (gconf_name);

	panel_place_menu_item_append_volumes (places_menu, FALSE);
	add_menu_separator (places_menu);

	panel_menu_items_append_from_desktop (places_menu,
					      "network-scheme.desktop",
					      NULL);
	panel_place_menu_item_append_volumes (places_menu, TRUE);

	if (panel_is_program_in_path ("nautilus-connect-server")) {
		item = panel_menu_items_create_action_item (PANEL_ACTION_CONNECT_SERVER);
		if (item != NULL)
			gtk_menu_shell_append (GTK_MENU_SHELL (places_menu),
					       item);
	}

	add_menu_separator (places_menu);

	panel_menu_items_append_from_desktop (places_menu,
					      "gnome-search-tool.desktop",
					      NULL);

	*recent_view = panel_recent_append_documents_menu (places_menu,
							   *recent_view);

	return places_menu;
}

static void
panel_place_menu_item_recreate_menu (GtkWidget *widget)
{
	PanelPlaceMenuItem *place_item;

	place_item = PANEL_PLACE_MENU_ITEM (widget);

	if (place_item->priv->menu) {
		gtk_widget_destroy (place_item->priv->menu);
		place_item->priv->menu = panel_place_menu_item_create_menu (&place_item->priv->recent_view);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (place_item),
					   place_item->priv->menu);
		panel_applet_menu_set_recurse (GTK_MENU (place_item->priv->menu),
					       "menu_panel",
					       place_item->priv->panel);
	}
}

static void
panel_place_menu_item_key_changed (GConfClient *client,
				   guint        cnxn_id,
				   GConfEntry  *entry,
				   GtkWidget   *place_item)
{
	panel_place_menu_item_recreate_menu (place_item);
}

static void
panel_place_menu_item_gtk_bookmarks_changed (GnomeVFSMonitorHandle *handle,
					     const gchar *monitor_uri,
					     const gchar *info_uri,
					     GnomeVFSMonitorEventType event_type,
					     gpointer user_data)
{
	panel_place_menu_item_recreate_menu (GTK_WIDGET (user_data));
}

static void
panel_place_menu_item_volume_changed (GnomeVFSVolumeMonitor *monitor,
				      GnomeVFSVolume        *volume,
				      GtkWidget             *place_menu)
{
	panel_place_menu_item_recreate_menu (place_menu);
}

static void
panel_desktop_menu_item_append_menu (GtkWidget            *menu,
				     PanelDesktopMenuItem *parent)
{
	gboolean   add_separator;
	GList     *children;
	GtkWidget *item;

	if (!g_object_get_data (G_OBJECT (menu), "panel-menu-needs-appending"))
		return;

	g_object_set_data (G_OBJECT (menu), "panel-menu-needs-appending", NULL);

	add_separator = FALSE;
	children = gtk_container_get_children (GTK_CONTAINER (menu));

	if (children != NULL) {
		while (children->next != NULL)
			children = children->next;
		add_separator = !GTK_IS_SEPARATOR (GTK_WIDGET (children->data));
	}

	if (add_separator)
		add_menu_separator (menu);

	if (panel_is_program_in_path ("gnome-screenshot")) {
		item = panel_menu_items_create_action_item (PANEL_ACTION_SCREENSHOT);
		if (item != NULL)
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	panel_menu_items_append_from_desktop (menu, "yelp.desktop", NULL);
	panel_menu_items_append_from_desktop (menu, "gnome-about.desktop", NULL);

	if (parent->priv->append_lock_logout)
		panel_menu_items_append_lock_logout (menu);
}

static GtkWidget *
panel_desktop_menu_item_create_menu (PanelDesktopMenuItem *desktop_item)
{
	GtkWidget *desktop_menu;

	desktop_menu = create_applications_menu ("settings.menu", NULL);

	g_signal_connect (desktop_menu, "show",
			  G_CALLBACK (panel_desktop_menu_item_append_menu),
			  desktop_item);

	return desktop_menu;
}

static void
panel_desktop_menu_item_recreate_menu (PanelDesktopMenuItem *desktop_item)
{
	if (desktop_item->priv->menu) {
		gtk_widget_destroy (desktop_item->priv->menu);
		desktop_item->priv->menu = panel_desktop_menu_item_create_menu (desktop_item);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (desktop_item),
					   desktop_item->priv->menu);
		panel_applet_menu_set_recurse (GTK_MENU (desktop_item->priv->menu),
					       "menu_panel",
					       desktop_item->priv->panel);
	}
}

static void
panel_place_menu_item_finalize (GObject *object)
{
	PanelPlaceMenuItem *menuitem = (PanelPlaceMenuItem *) object;

	gconf_client_remove_dir (panel_gconf_get_client (),
				 DESKTOP_IS_HOME_DIR_DIR,
				 NULL);
	gconf_client_remove_dir (panel_gconf_get_client (),
				 NAMES_DIR,
				 NULL);

	if (menuitem->priv->recent_view != NULL)
		g_object_unref (menuitem->priv->recent_view);
	menuitem->priv->recent_view = NULL;

	if (menuitem->priv->bookmarks_monitor != NULL)
		gnome_vfs_monitor_cancel (menuitem->priv->bookmarks_monitor);
	menuitem->priv->bookmarks_monitor = NULL;

	if (menuitem->priv->volume_mounted_id)
		g_signal_handler_disconnect (volume_monitor,
					     menuitem->priv->volume_mounted_id);
	menuitem->priv->volume_mounted_id = 0;

	if (menuitem->priv->volume_unmounted_id)
		g_signal_handler_disconnect (volume_monitor,
					     menuitem->priv->volume_unmounted_id);
	menuitem->priv->volume_unmounted_id = 0;

	place_parent_class->finalize (object);
}

static void
panel_desktop_menu_item_finalize (GObject *object)
{
	PanelDesktopMenuItem *menuitem = (PanelDesktopMenuItem *) object;

	if (menuitem->priv->append_lock_logout)
		panel_lockdown_notify_remove (G_CALLBACK (panel_desktop_menu_item_recreate_menu),
					      menuitem);
	desktop_parent_class->finalize (object);
}

static void
panel_place_menu_item_instance_init (PanelPlaceMenuItem      *menuitem,
				     PanelPlaceMenuItemClass *klass)
{
	char *bookmarks_filename;
	char *bookmarks_uri;

	menuitem->priv = PANEL_PLACE_MENU_ITEM_GET_PRIVATE (menuitem);

	gconf_client_add_dir (panel_gconf_get_client (),
			      DESKTOP_IS_HOME_DIR_DIR,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);
	gconf_client_add_dir (panel_gconf_get_client (),
			      NAMES_DIR,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);

	panel_gconf_notify_add_while_alive (HOME_NAME_KEY,
					    (GConfClientNotifyFunc) panel_place_menu_item_key_changed,
					    G_OBJECT (menuitem));
	panel_gconf_notify_add_while_alive (DESKTOP_IS_HOME_DIR_KEY,
					    (GConfClientNotifyFunc) panel_place_menu_item_key_changed,
					    G_OBJECT (menuitem));
	panel_gconf_notify_add_while_alive (COMPUTER_NAME_KEY,
					    (GConfClientNotifyFunc) panel_place_menu_item_key_changed,
					    G_OBJECT (menuitem));

	bookmarks_filename = g_build_filename (g_get_home_dir (),
					       BOOKMARKS_FILENAME, NULL);
	bookmarks_uri = gnome_vfs_get_uri_from_local_path (bookmarks_filename);

	if (bookmarks_uri) {
		GnomeVFSResult result;
		result = gnome_vfs_monitor_add (&menuitem->priv->bookmarks_monitor,
						bookmarks_uri,
						GNOME_VFS_MONITOR_FILE,
						panel_place_menu_item_gtk_bookmarks_changed,
						menuitem);
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

	menuitem->priv->volume_mounted_id = g_signal_connect (volume_monitor,
							      "volume_mounted",
							      G_CALLBACK (panel_place_menu_item_volume_changed),
							      menuitem);
	menuitem->priv->volume_unmounted_id = g_signal_connect (volume_monitor,
							        "volume_unmounted",
							        G_CALLBACK (panel_place_menu_item_volume_changed),
							        menuitem);

}

static void
panel_desktop_menu_item_instance_init (PanelDesktopMenuItem      *menuitem,
				       PanelDesktopMenuItemClass *klass)
{
	menuitem->priv = PANEL_DESKTOP_MENU_ITEM_GET_PRIVATE (menuitem);
}

static void
panel_place_menu_item_class_init (PanelPlaceMenuItemClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass   *) klass;

	place_parent_class = g_type_class_peek_parent (klass);
	gobject_class->finalize  = panel_place_menu_item_finalize;

	g_type_class_add_private (klass, sizeof (PanelPlaceMenuItemPrivate));
}

static void
panel_desktop_menu_item_class_init (PanelDesktopMenuItemClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass   *) klass;

	desktop_parent_class = g_type_class_peek_parent (klass);
	gobject_class->finalize  = panel_desktop_menu_item_finalize;

	g_type_class_add_private (klass, sizeof (PanelDesktopMenuItemPrivate));
}

GType
panel_place_menu_item_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelPlaceMenuItemClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_place_menu_item_class_init,
			NULL,
			NULL,
			sizeof (PanelPlaceMenuItem),
			0,
			(GInstanceInitFunc) panel_place_menu_item_instance_init,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_IMAGE_MENU_ITEM,
					       "PanelPlaceMenuItem", &info, 0);
	}

	return type;
}

GType
panel_desktop_menu_item_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PanelDesktopMenuItemClass),
			NULL,
			NULL,
			(GClassInitFunc) panel_desktop_menu_item_class_init,
			NULL,
			NULL,
			sizeof (PanelDesktopMenuItem),
			0,
			(GInstanceInitFunc) panel_desktop_menu_item_instance_init,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_IMAGE_MENU_ITEM,
					       "PanelDesktopMenuItem", &info, 0);
	}

	return type;
}


GtkWidget *
panel_place_menu_item_new (gboolean use_image)
{
	PanelPlaceMenuItem *menuitem;
	GtkWidget          *accel_label;

	menuitem = g_object_new (PANEL_TYPE_PLACE_MENU_ITEM, NULL);

	accel_label = gtk_accel_label_new (_("Places"));
	gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

	gtk_container_add (GTK_CONTAINER (menuitem), accel_label);
	gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label),
					  GTK_WIDGET (menuitem));
	gtk_widget_show (accel_label);

	menuitem->priv->use_image = use_image;
	if (use_image) {
		GtkWidget *image;

		image = gtk_image_new_from_icon_name ("gnome-fs-directory",
						      panel_menu_icon_get_size ());

		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem),
					       image);
	}

	menuitem->priv->menu = panel_place_menu_item_create_menu (&menuitem->priv->recent_view);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   menuitem->priv->menu);

	return GTK_WIDGET (menuitem);
}

GtkWidget *
panel_desktop_menu_item_new (gboolean use_image,
			     gboolean append_lock_logout)
{
	PanelDesktopMenuItem *menuitem;
	GtkWidget            *accel_label;

	menuitem = g_object_new (PANEL_TYPE_DESKTOP_MENU_ITEM, NULL);

	/* Translators: Desktop is used here as in "Desktop Environment" (this
	 * is not the Desktop folder). Do not keep "Desktop Environment|" in
	 * the translation. */
	accel_label = gtk_accel_label_new (Q_("Desktop Environment|Desktop"));
	gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

	gtk_container_add (GTK_CONTAINER (menuitem), accel_label);
	gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label),
					  GTK_WIDGET (menuitem));
	gtk_widget_show (accel_label);

	menuitem->priv->use_image = use_image;
	if (use_image) {
		GtkWidget *image;

		image = gtk_image_new_from_icon_name ("gnome-fs-desktop",
						      panel_menu_icon_get_size ());

		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem),
					       image);
	}

	menuitem->priv->append_lock_logout = append_lock_logout;
	if (append_lock_logout)
		panel_lockdown_notify_add (G_CALLBACK (panel_desktop_menu_item_recreate_menu),
					   menuitem);

	menuitem->priv->menu = panel_desktop_menu_item_create_menu (menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   menuitem->priv->menu);

	return GTK_WIDGET (menuitem);
}

void
panel_place_menu_item_set_panel (GtkWidget   *item,
				 PanelWidget *panel)
{
	PanelPlaceMenuItem *place_item;

	place_item = PANEL_PLACE_MENU_ITEM (item);

	place_item->priv->panel = panel;
	panel_applet_menu_set_recurse (GTK_MENU (place_item->priv->menu),
				       "menu_panel", panel);
}

void
panel_desktop_menu_item_set_panel (GtkWidget   *item,
				   PanelWidget *panel)
{
	PanelDesktopMenuItem *desktop_item;

	desktop_item = PANEL_DESKTOP_MENU_ITEM (item);

	desktop_item->priv->panel = panel;
	panel_applet_menu_set_recurse (GTK_MENU (desktop_item->priv->menu),
				       "menu_panel", panel);
}

GtkWidget *
panel_menu_items_create_action_item (PanelActionButtonType action_type)
{
	GtkWidget *item;

	if (panel_action_get_is_disabled (action_type))
		return NULL;

	item = gtk_image_menu_item_new ();
        setup_menu_item_with_icon (item,
				   panel_menu_icon_get_size (),
				   panel_action_get_icon_name (action_type),
				   NULL,
				   panel_action_get_text (action_type),
				   TRUE);

	gtk_tooltips_set_tip (panel_tooltips,
			      item,
			      panel_action_get_tooltip (action_type),
			      NULL);

	g_signal_connect (item, "activate",
			  panel_action_get_invoke (action_type), NULL);
	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);
	setup_internal_applet_drag (item, action_type);

	return item;
}

void
panel_menu_items_append_lock_logout (GtkWidget *menu)
{
	gboolean   separator_inserted;
	GList     *children;
	GtkWidget *item;

	separator_inserted = FALSE;
	children = gtk_container_get_children (GTK_CONTAINER (menu));

	if (children != NULL) {
		while (children->next != NULL)
			children = children->next;
		separator_inserted = GTK_IS_SEPARATOR (GTK_WIDGET (children->data));
	}

	if (panel_lock_screen_action_available ("lock")) {
		item = panel_menu_items_create_action_item (PANEL_ACTION_LOCK);
		if (item != NULL) {
			if (!separator_inserted) {
				add_menu_separator (menu);
				separator_inserted = TRUE;
			}

			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		}
	}

	item = panel_menu_items_create_action_item (PANEL_ACTION_LOGOUT);
	if (item != NULL) {
		if (!separator_inserted)
			add_menu_separator (menu);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}
}

void
panel_menu_item_activate_desktop_file (GtkWidget  *menuitem,
				       const char *path)
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
