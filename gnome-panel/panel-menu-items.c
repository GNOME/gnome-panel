/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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
#include <gio/gio.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-gtk.h>
#include <libpanel-util/panel-keyfile.h>
#include <libpanel-util/panel-launch.h>
#include <libpanel-util/panel-end-session-dialog.h>
#include <libpanel-util/panel-session-manager.h>
#include <libpanel-util/panel-show.h>
#include <gdm/gdm-user-switching.h>

#include "menu.h"
#include "panel-action-button.h"
#include "panel-globals.h"
#include "panel-icon-names.h"
#include "panel-lockdown.h"
#include "panel-menu-bar-object.h"
#include "panel-recent.h"
#include "panel-stock-icons.h"
#include "panel-util.h"
#include "panel-schemas.h"

#define MAX_ITEMS_OR_SUBMENU    8
#define MAX_BOOKMARK_ITEMS      100

G_DEFINE_TYPE (PanelPlaceMenuItem, panel_place_menu_item, PANEL_TYPE_IMAGE_MENU_ITEM)
G_DEFINE_TYPE (PanelDesktopMenuItem, panel_desktop_menu_item, PANEL_TYPE_IMAGE_MENU_ITEM)

#define PANEL_PLACE_MENU_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_PLACE_MENU_ITEM, PanelPlaceMenuItemPrivate))
#define PANEL_DESKTOP_MENU_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PANEL_TYPE_DESKTOP_MENU_ITEM, PanelDesktopMenuItemPrivate))

struct _PanelPlaceMenuItemPrivate {
	GtkWidget   *menu;
	PanelWidget *panel;
	GSettings   *settings;

	GtkRecentManager *recent_manager;

	GFileMonitor *bookmarks_monitor;

	GVolumeMonitor *volume_monitor;
	gulong       drive_changed_id;
	gulong       drive_connected_id;
	gulong       drive_disconnected_id;
	gulong       volume_added_id;
	gulong       volume_changed_id;
	gulong       volume_removed_id;
	gulong       mount_added_id;
	gulong       mount_changed_id;
	gulong       mount_removed_id;
};

struct _PanelDesktopMenuItemPrivate {
	GtkWidget   *menu;
	PanelWidget *panel;
	GtkIconSize  icon_size;
};

static void
activate_uri_on_screen (const char *uri,
			GdkScreen  *screen)
{
	panel_show_uri (screen, uri, gtk_get_current_event_time (), NULL);
}

static void
activate_uri (GtkWidget  *menuitem,
	      const char *uri)
{
	activate_uri_on_screen (uri, menuitem_to_screen (menuitem));
}

static void
activate_path (GtkWidget  *menuitem,
	       const char *path)
{
	char *uri;

	uri = g_filename_to_uri (path, NULL, NULL);
	activate_uri_on_screen (uri, menuitem_to_screen (menuitem));
	g_free (uri);
}

static void
activate_home_uri (GtkWidget *menuitem,
		   gpointer   data)
{
	activate_path (menuitem, g_get_home_dir ());
}

static void
activate_desktop_uri (GtkWidget *menuitem,
		      gpointer   data)
{
	activate_path (menuitem,
		       g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
}

static GtkWidget *
panel_menu_item_desktop_new (char      *path,
			     char      *force_name,
			     gboolean   use_icon)
{
	GKeyFile  *key_file;
	gboolean   loaded;
	GtkWidget *item;
	char      *path_freeme;
	char      *full_path;
	char      *uri;
	char      *type;
	gboolean   is_application;
	char      *tryexec;
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
		return NULL;
	}

	/* For Application desktop files, respect TryExec */
	type = panel_key_file_get_string (key_file, "Type");
	if (!type) {
		g_key_file_free (key_file);
		if (path_freeme)
			g_free (path_freeme);
		return NULL;
	}
	is_application = (strcmp (type, "Application") == 0);
	g_free (type);

	if (is_application) {
		tryexec = panel_key_file_get_string (key_file, "TryExec");
		if (tryexec) {
			char *prog;

			prog = g_find_program_in_path (tryexec);
			g_free (tryexec);

			if (!prog) {
				/* FIXME: we could add some file monitor magic,
				 * so that the menu items appears when the
				 * program appears, but that's really complex
				 * for not a huge benefit */
				g_key_file_free (key_file);
				if (path_freeme)
					g_free (path_freeme);
				return NULL;
			}

			g_free (prog);
		}
	}

	/* Now, simply build the menu item */
	icon    = panel_key_file_get_locale_string (key_file, "Icon");
	comment = panel_key_file_get_locale_string (key_file, "Comment");

	if (PANEL_GLIB_STR_EMPTY (force_name))
		name = panel_key_file_get_locale_string (key_file, "Name");
	else
		name = g_strdup (force_name);

	if (use_icon) {
		item = panel_image_menu_item_new2 ();
        } else {
		item = panel_image_menu_item_new ();
	}

	setup_menu_item_with_icon (item, panel_menu_icon_get_size (),
				   icon, NULL, name);

	panel_util_set_tooltip_text (item, comment);

	g_signal_connect_data (item, "activate",
			       G_CALLBACK (panel_menu_item_activate_desktop_file),
			       g_strdup (full_path),
			       (GClosureNotify) g_free, 0);
	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	uri = g_filename_to_uri (full_path, NULL, NULL);

	setup_uri_drag (item, uri, icon);
	g_free (uri);

	g_key_file_free (key_file);

	if (icon)
		g_free (icon);

	if (name)
		g_free (name);

	if (comment)
		g_free (comment);

	if (path_freeme)
		g_free (path_freeme);

	return item;
}

static GtkWidget *
panel_menu_item_uri_new (const char *uri,
			 const char *icon_name,
			 GIcon      *gicon,
			 const char *title,
			 const char *tooltip,
			 GCallback   callback)
{
	GtkWidget *item;
	char      *user_data;

	item = panel_image_menu_item_new2 ();
	setup_menu_item_with_icon (item,
				   panel_menu_icon_get_size (),
				   icon_name, gicon,
				   title);

	panel_util_set_tooltip_text (item, tooltip);

	user_data = g_strdup (uri);
	g_signal_connect_data (item, "activate", callback, user_data,
			       (GClosureNotify) g_free, 0);

	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	setup_uri_drag (item, uri, icon_name);

	return item;
}

static GtkWidget *
panel_menu_items_create_action_item_full (PanelActionButtonType  action_type,
					  const char            *label,
					  const char            *tooltip,
					  gboolean               create_even_if_disabled)
{
	GtkWidget *item;

	if (!create_even_if_disabled && panel_action_get_is_disabled (action_type))
		return NULL;

	item = panel_image_menu_item_new ();
        setup_menu_item_with_icon (item,
				   panel_menu_icon_get_size (),
				   panel_action_get_icon_name (action_type),
				   NULL,
				   label ? label : panel_action_get_text (action_type));

	panel_util_set_tooltip_text (item,
				     tooltip ?
					tooltip :
					panel_action_get_tooltip (action_type));

	g_signal_connect (item, "activate",
			  panel_action_get_invoke (action_type), NULL);
	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);
	setup_internal_applet_drag (item, action_type);

	return item;
}

static GtkWidget *
panel_menu_items_create_action_item (PanelActionButtonType action_type)
{
	return panel_menu_items_create_action_item_full (action_type,
							 NULL, NULL, FALSE);
}

static void
panel_menu_item_activate_switch_user (GtkWidget *menuitem,
				      gpointer   user_data)
{
	GError *error;

	if (panel_lockdown_get_disable_switch_user_s ())
		return;

	error = NULL;

	/* If running under LightDM switch to the greeter using dbus */
	if (g_getenv("XDG_SEAT_PATH")) {
		GDBusConnection *bus;
		GVariant *result = NULL;

		bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
		if (error)
			g_warning ("Failed to get system bus: %s", error->message);
		g_clear_error (&error);

		if (bus)
			result = g_dbus_connection_call_sync (bus,
							      "org.freedesktop.DisplayManager",
							      g_getenv ("XDG_SEAT_PATH"),
							      "org.freedesktop.DisplayManager.Seat",
							      "SwitchToGreeter",
							      g_variant_new ("()"),
							      G_VARIANT_TYPE ("()"),
							      G_DBUS_CALL_FLAGS_NONE,
							      -1,
							      NULL,
							      &error);
                if (error)
                        g_warning ("Failed to switch to greeter: %s", error->message);
                g_clear_error (&error);

                if (result)
                        g_variant_unref (result);
        } else {
		gboolean ret;

		ret = gdm_goto_login_session_sync (NULL, &error);

		if (!ret) {
			g_warning ("Failed to switch to greeter: %s", error->message);
			g_error_free (error);
		}
	}
}

static GtkWidget *
panel_menu_items_create_switch_user (gboolean use_icon)
{
	GtkWidget *item;

	if (use_icon) {
		item = panel_image_menu_item_new2 ();
        } else {
		item = panel_image_menu_item_new ();
	}

	setup_menu_item_with_icon (item, panel_menu_icon_get_size (),
				   PANEL_ICON_USERS, NULL, _("Switch User"));

	g_signal_connect (item, "activate",
			  G_CALLBACK (panel_menu_item_activate_switch_user),
			  NULL);
	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	return item;
}

static void
panel_place_menu_item_append_gtk_bookmarks (GtkWidget *menu)
{
	typedef struct {
		char *full_uri;
		char *label;
	} PanelBookmark;

	GtkWidget   *add_menu;
	char        *filename;
	GIOChannel  *io_channel;
	GHashTable  *table;
	int          i;
	GSList      *lines = NULL;
	GSList      *add_bookmarks, *l;
	PanelBookmark *bookmark;

	filename = panel_gtk_get_bookmark_file ();

	io_channel = g_io_channel_new_file (filename, "r", NULL);
	g_free (filename);

	if (!io_channel)
		return;

	/* We use a hard limit to avoid having users shooting their
	 * own feet, and to avoid crashing the system if a misbehaving
	 * application creates a big bookmars file.
	 */
	for (i = 0; i < MAX_BOOKMARK_ITEMS; i++) {
		char      *contents;
		gsize      length;
		gsize      terminator_pos;
		GIOStatus  status;

		status = g_io_channel_read_line (io_channel, &contents, &length, &terminator_pos, NULL);

		if (status != G_IO_STATUS_NORMAL)
			break;

		if (length == 0)
			break;

		/* Clear the line terminator (\n), if any */
		if (terminator_pos > 0)
			contents[terminator_pos] = '\0';

		lines = g_slist_prepend (lines, contents);
	}

	g_io_channel_shutdown (io_channel, FALSE, NULL);
	g_io_channel_unref (io_channel);

	if (!lines)
		return;

	lines = g_slist_reverse (lines);

	table = g_hash_table_new (g_str_hash, g_str_equal);
	add_bookmarks = NULL;

	for (l = lines; l; l = l->next) {
		char *line = (char*) l->data;

		if (line[0] && !g_hash_table_lookup (table, line)) {
			GFile    *file;
			char     *space;
			char     *label;
			gboolean  keep;

			g_hash_table_insert (table, line, line);

			space = strchr (line, ' ');
			if (space) {
				*space = '\0';
				label = g_strdup (space + 1);
			} else {
				label = NULL;
			}

			keep = FALSE;

			if (g_str_has_prefix (line, "x-nautilus-search:"))
				keep = TRUE;

			if (!keep) {
				file = g_file_new_for_uri (line);
				keep = !g_file_is_native (file) ||
				       g_file_query_exists (file, NULL);
				g_object_unref (file);
			}

			if (!keep) {
				if (label)
					g_free (label);
				continue;
			}

			bookmark = g_malloc (sizeof (PanelBookmark));
			bookmark->full_uri = g_strdup (line);
			bookmark->label = label;
			add_bookmarks = g_slist_prepend (add_bookmarks, bookmark);
		}
	}

	g_hash_table_destroy (table);
	g_slist_foreach (lines, (GFunc) g_free, NULL);
	g_slist_free (lines);

	add_bookmarks = g_slist_reverse (add_bookmarks);

	if (g_slist_length (add_bookmarks) <= MAX_ITEMS_OR_SUBMENU) {
		add_menu = menu;
	} else {
		GtkWidget *item;

		item = panel_image_menu_item_new ();
		setup_menu_item_with_icon (item, panel_menu_icon_get_size (),
					   PANEL_ICON_BOOKMARKS, NULL,
					   _("Bookmarks"));

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		add_menu = create_empty_menu ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), add_menu);
	}

	for (l = add_bookmarks; l; l = l->next) {
		char *display_name;
		char *tooltip;
		char *label;
		char *icon;
		GFile *file;
		GIcon *gicon;
		GtkWidget *menu_item;

		bookmark = l->data;
		
		file = g_file_new_for_uri (bookmark->full_uri);
		display_name = g_file_get_parse_name (file);
		g_object_unref (file);
		/* Translators: %s is a URI */
		tooltip = g_strdup_printf (_("Open '%s'"), display_name);
		g_free (display_name);

		label = NULL;
		if (bookmark->label) {
			label = g_strdup (g_strstrip (bookmark->label));
			if (!label [0]) {
				g_free (label);
				label = NULL;
			}
		}

		if (!label) {
			label = panel_util_get_label_for_uri (bookmark->full_uri);

			if (!label) {
				g_free (tooltip);
				g_free (bookmark->full_uri);
				if (bookmark->label)
					g_free (bookmark->label);
				g_free (bookmark);
				continue;
			}
		}

		icon = panel_util_get_icon_for_uri (bookmark->full_uri);
		/*FIXME: we should probably get a GIcon if possible, so that we
		 * have customized icons for cd-rom, eg */
		if (!icon)
			icon = g_strdup (PANEL_ICON_FOLDER);

		gicon = g_themed_icon_new_with_default_fallbacks (icon);

		//FIXME: drag and drop will be broken for x-nautilus-search uris
		menu_item = panel_menu_item_uri_new (bookmark->full_uri,
						     icon, gicon,
						     label,
						     tooltip,
						     G_CALLBACK (activate_uri));
		gtk_menu_shell_append (GTK_MENU_SHELL (add_menu), menu_item);

		g_free (icon);
		g_object_unref (gicon);
		g_free (tooltip);
		g_free (label);
		g_free (bookmark->full_uri);
		if (bookmark->label)
			g_free (bookmark->label);
		g_free (bookmark);
	}

	g_slist_free (add_bookmarks);
}

static void
drive_poll_for_media_cb (GObject      *source_object,
			 GAsyncResult *res,
			 gpointer      user_data)
{
	GdkScreen *screen;
	GError    *error;
	char      *primary;
	char      *name;

	error = NULL;
	if (!g_drive_poll_for_media_finish (G_DRIVE (source_object),
					    res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			screen = GDK_SCREEN (user_data);

			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to scan %s for media changes"),
						   name);
			g_free (name);
			panel_error_dialog (NULL, screen,
					    "cannot_scan_drive", TRUE,
					    primary, error->message);
			g_free (primary);
		}
		g_error_free (error);
	}

	//FIXME: should we mount the volume and activate the root of the new
	//mount?
}

static void
panel_menu_item_rescan_drive (GtkWidget *menuitem,
			      GDrive    *drive)
{
	g_drive_poll_for_media (drive, NULL,
				drive_poll_for_media_cb,
				menuitem_to_screen (menuitem));
}

static GtkWidget *
panel_menu_item_drive_new (GDrive *drive)
{
	GtkWidget *item;
	GIcon     *icon;
	char      *title;
	char      *tooltip;

	icon = g_drive_get_icon (drive);
	title = g_drive_get_name (drive);

	item = panel_image_menu_item_new2 ();
	setup_menu_item_with_icon (item,
				   panel_menu_icon_get_size (),
				   NULL, icon,
				   title);
	g_object_unref (icon);

	tooltip = g_strdup_printf (_("Rescan %s"), title);
	panel_util_set_tooltip_text (item, tooltip);
	g_free (tooltip);

	g_free (title);

	g_signal_connect_data (item, "activate",
			       G_CALLBACK (panel_menu_item_rescan_drive),
			       g_object_ref (drive),
			       (GClosureNotify) g_object_unref, 0);

	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	return item;
}

typedef struct {
	GdkScreen       *screen;
	GMountOperation *mount_op;
} PanelVolumeMountData;

static void
volume_mount_cb (GObject      *source_object,
		 GAsyncResult *res,
		 gpointer      user_data)
{
	PanelVolumeMountData *mount_data = user_data;
	GError *error;

	error = NULL;
	if (!g_volume_mount_finish (G_VOLUME (source_object), res, &error)) {
		char *primary;
		char *name;

		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_volume_get_name (G_VOLUME (source_object));
			primary = g_strdup_printf (_("Unable to mount %s"),
						   name);
			g_free (name);

			panel_error_dialog (NULL, mount_data->screen,
					    "cannot_mount_volume", TRUE,
					    primary, error->message);
			g_free (primary);
		}
		g_error_free (error);
	} else {
		GMount *mount;
		GFile  *root;
		char   *rooturi;

		mount = g_volume_get_mount (G_VOLUME (source_object));
		root = g_mount_get_root (mount);
		rooturi = g_file_get_uri (root);
		activate_uri_on_screen (rooturi, mount_data->screen);
		g_object_unref (mount);
		g_object_unref (root);
		g_free (rooturi);
	}

	g_object_unref (mount_data->mount_op);
	g_slice_free (PanelVolumeMountData, mount_data);
}

static void
panel_menu_item_mount_volume (GtkWidget *menuitem,
			      GVolume   *volume)
{
	PanelVolumeMountData *mount_data;

	mount_data = g_slice_new (PanelVolumeMountData);
	mount_data->screen = menuitem_to_screen (menuitem);
	mount_data->mount_op = gtk_mount_operation_new (NULL);
	gtk_mount_operation_set_screen (GTK_MOUNT_OPERATION (mount_data->mount_op),
					mount_data->screen);

	g_volume_mount (volume, G_MOUNT_MOUNT_NONE, mount_data->mount_op, NULL,
			volume_mount_cb, mount_data);
}

static GtkWidget *
panel_menu_item_volume_new (GVolume *volume)
{
	GtkWidget *item;
	GIcon     *icon;
	char      *title;
	char      *tooltip;

	icon = g_volume_get_icon (volume);
	title = g_volume_get_name (volume);

	item = panel_image_menu_item_new2 ();
	setup_menu_item_with_icon (item,
				   panel_menu_icon_get_size (),
				   NULL, icon,
				   title);
	g_object_unref (icon);

	tooltip = g_strdup_printf (_("Mount %s"), title);
	panel_util_set_tooltip_text (item, tooltip);
	g_free (tooltip);

	g_free (title);

	g_signal_connect_data (item, "activate",
			       G_CALLBACK (panel_menu_item_mount_volume),
			       g_object_ref (volume),
			       (GClosureNotify) g_object_unref, 0);

	g_signal_connect (G_OBJECT (item), "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	return item;
}

static GtkWidget *
panel_menu_item_mount_new (GMount *mount)
{
	GtkWidget *item;
	GFile     *root;
	GIcon     *icon;
	char      *display_name;
	char      *activation_uri;

	icon = g_mount_get_icon (mount);
	display_name = g_mount_get_name (mount);

	root = g_mount_get_root (mount);
	activation_uri = g_file_get_uri (root);
	g_object_unref (root);

	item = panel_menu_item_uri_new (activation_uri, NULL, icon,
					display_name,
					display_name, //FIXME tooltip
					G_CALLBACK (activate_uri));

	g_object_unref (icon);
	g_free (display_name);
	g_free (activation_uri);

	return item;
}

typedef enum {
	PANEL_GIO_DRIVE,
	PANEL_GIO_VOLUME,
	PANEL_GIO_MOUNT
} PanelGioItemType;

typedef struct {
	PanelGioItemType type;
	union {
		GDrive *drive;
		GVolume *volume;
		GMount *mount;
	} u;
} PanelGioItem;

/* this is loosely based on update_places() from nautilus-places-sidebar.c */
static void
panel_place_menu_item_append_local_gio (PanelPlaceMenuItem *place_item,
					GtkWidget          *menu)
{
	GList   *l;
	GList   *ll;
	GList   *drives;
	GDrive  *drive;
	GList   *volumes;
	GVolume *volume;
	GList   *mounts;
	GMount  *mount;
	GSList       *items;
	GSList       *sl;
	PanelGioItem *item;
	GtkWidget *add_menu;

	items = NULL;

	/* first go through all connected drives */
	drives = g_volume_monitor_get_connected_drives (place_item->priv->volume_monitor);
	for (l = drives; l != NULL; l = l->next) {
		drive = l->data;

		volumes = g_drive_get_volumes (drive);
		if (volumes != NULL) {
			for (ll = volumes; ll != NULL; ll = ll->next) {
				volume = ll->data;
				mount = g_volume_get_mount (volume);
				item = g_slice_new (PanelGioItem);
				if (mount != NULL) {
					item->type = PANEL_GIO_MOUNT;
					item->u.mount = mount;
				} else {
					/* Do show the unmounted volumes; this
					 * is so the user can mount it (in case
					 * automounting is off).
					 *
					 * Also, even if automounting is
					 * enabled, this gives a visual cue
					 * that the user should remember to
					 * yank out the media if he just
					 * unmounted it.
					 */
					item->type = PANEL_GIO_VOLUME;
					item->u.volume = g_object_ref (volume);
				}
				items = g_slist_prepend (items, item);
				g_object_unref (volume);
			}
			g_list_free (volumes);
		} else {
			if (g_drive_is_media_removable (drive) &&
			    !g_drive_is_media_check_automatic (drive)) {
				/* If the drive has no mountable volumes and we
				 * cannot detect media change.. we display the
				 * drive so the user can manually poll the
				 * drive by clicking on it..."
				 *
				 * This is mainly for drives like floppies
				 * where media detection doesn't work.. but
				 * it's also for human beings who like to turn
				 * off media detection in the OS to save
				 * battery juice.
				 */
				item = g_slice_new (PanelGioItem);
				item->type = PANEL_GIO_DRIVE;
				item->u.drive = g_object_ref (drive);
				items = g_slist_prepend (items, item);
			}
		}
		g_object_unref (drive);
	}
	g_list_free (drives);

	/* add all volumes that is not associated with a drive */
	volumes = g_volume_monitor_get_volumes (place_item->priv->volume_monitor);
	for (l = volumes; l != NULL; l = l->next) {
		volume = l->data;
		drive = g_volume_get_drive (volume);
		if (drive != NULL) {
		    	g_object_unref (volume);
			g_object_unref (drive);
			continue;
		}
		mount = g_volume_get_mount (volume);
		item = g_slice_new (PanelGioItem);
		if (mount != NULL) {
			item->type = PANEL_GIO_MOUNT;
			item->u.mount = mount;
		} else {
			/* see comment above in why we add an icon for an
			 * unmounted mountable volume */
			item->type = PANEL_GIO_VOLUME;
			item->u.volume = g_object_ref (volume);
		}
		items = g_slist_prepend (items, item);
		g_object_unref (volume);
	}
	g_list_free (volumes);

	/* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
	mounts = g_volume_monitor_get_mounts (place_item->priv->volume_monitor);
	for (l = mounts; l != NULL; l = l->next) {
		GFile *root;

		mount = l->data;

		if (g_mount_is_shadowed (mount)) {
			g_object_unref (mount);
			continue;
		}

		volume = g_mount_get_volume (mount);
		if (volume != NULL) {
			g_object_unref (volume);
			g_object_unref (mount);
			continue;
		}

		root = g_mount_get_root (mount);
		if (!g_file_is_native (root)) {
			g_object_unref (root);
			g_object_unref (mount);
			continue;
		}
		g_object_unref (root);

		item = g_slice_new (PanelGioItem);
		item->type = PANEL_GIO_MOUNT;
		item->u.mount = mount;
		items = g_slist_prepend (items, item);
	}
	g_list_free (mounts);

	/* now that we have everything, add the items inline or in a submenu */
	items = g_slist_reverse (items);

	if (g_slist_length (items) <= MAX_ITEMS_OR_SUBMENU) {
		add_menu = menu;
	} else {
		GtkWidget  *item;

		item = panel_image_menu_item_new ();
		setup_menu_item_with_icon (item, panel_menu_icon_get_size (),
					   PANEL_ICON_REMOVABLE_MEDIA,
					   NULL,
					   _("Removable Media"));

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		add_menu = create_empty_menu ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), add_menu);
	}

	for (sl = items; sl; sl = sl->next) {
		GtkWidget *menu_item;
		item = sl->data;

		switch (item->type) {
		case PANEL_GIO_DRIVE:
			menu_item = panel_menu_item_drive_new (item->u.drive);
			g_object_unref (item->u.drive);
			break;
		case PANEL_GIO_VOLUME:
			menu_item = panel_menu_item_volume_new (item->u.volume);
			g_object_unref (item->u.volume);
			break;
		case PANEL_GIO_MOUNT:
			menu_item = panel_menu_item_mount_new (item->u.mount);
			g_object_unref (item->u.mount);
			break;
		default:
			g_assert_not_reached ();
		}
		g_slice_free (PanelGioItem, item);

		gtk_menu_shell_append (GTK_MENU_SHELL (add_menu), menu_item);
	}

	g_slist_free (items);
}

/* this is loosely based on update_places() from nautilus-places-sidebar.c */
static void
panel_place_menu_item_append_remote_gio (PanelPlaceMenuItem *place_item,
					 GtkWidget          *menu)
{
	GtkWidget *add_menu;
	GList     *mounts, *l;
	GMount    *mount;
	GSList    *add_mounts, *sl;

	/* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
	mounts = g_volume_monitor_get_mounts (place_item->priv->volume_monitor);
	add_mounts = NULL;

	for (l = mounts; l; l = l->next) {
		GVolume *volume;
		GFile   *root;

		mount = l->data;

		if (g_mount_is_shadowed (mount)) {
			g_object_unref (mount);
			continue;
		}

		volume = g_mount_get_volume (mount);
		if (volume != NULL) {
			g_object_unref (volume);
			g_object_unref (mount);
			continue;
		}

		root = g_mount_get_root (mount);
		if (g_file_is_native (root)) {
			g_object_unref (root);
			g_object_unref (mount);
			continue;
		}
		g_object_unref (root);


		add_mounts = g_slist_prepend (add_mounts, mount);
	}
	add_mounts = g_slist_reverse (add_mounts);

	if (g_slist_length (add_mounts) <= MAX_ITEMS_OR_SUBMENU) {
		add_menu = menu;
	} else {
		GtkWidget  *item;

		item = panel_image_menu_item_new2 ();
		setup_menu_item_with_icon (item, panel_menu_icon_get_size (),
					   PANEL_ICON_NETWORK_SERVER,
					   NULL,
					   _("Network Places"));

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		add_menu = create_empty_menu ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), add_menu);
	}

	for (sl = add_mounts; sl; sl = sl->next) {
		GtkWidget *item;

		mount = sl->data;

		item = panel_menu_item_mount_new (mount);
		gtk_menu_shell_append (GTK_MENU_SHELL (add_menu), item);

		g_object_unref (mount);
	}

	g_slist_free (add_mounts);
	g_list_free (mounts);
}


static GtkWidget *
panel_place_menu_item_create_menu (PanelPlaceMenuItem *place_item)
{
	GtkWidget *places_menu;
	GtkWidget *item;
	char      *name;
	char      *uri;
	GFile     *file;

	places_menu = panel_create_menu ();

	file = g_file_new_for_path (g_get_home_dir ());
	uri = g_file_get_uri (file);
	name = panel_util_get_label_for_uri (uri);
	g_object_unref (file);
	
	item = panel_menu_item_uri_new (uri, PANEL_ICON_HOME, NULL,
					name,
					_("Open your personal folder"),
					G_CALLBACK (activate_home_uri));
	gtk_menu_shell_append (GTK_MENU_SHELL (places_menu), item);

	g_free (name);
	g_free (uri);

	file = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
	uri = g_file_get_uri (file);
	g_object_unref (file);

	item = panel_menu_item_uri_new (
			/* FIXME: if the dir changes, we'd need to update the drag data since the uri is not the same */
			uri, PANEL_ICON_DESKTOP, NULL,
			/* Translators: Desktop is used here as in
			 * "Desktop Folder" (this is not the Desktop
			 * environment). */
			C_("Desktop Folder", "Desktop"),
			_("Open the contents of your desktop in a folder"),
			G_CALLBACK (activate_desktop_uri));
	gtk_menu_shell_append (GTK_MENU_SHELL (places_menu), item);

	g_free (uri);

	panel_place_menu_item_append_gtk_bookmarks (places_menu);
	add_menu_separator (places_menu);

	item = panel_menu_item_uri_new ("computer://",
					PANEL_ICON_COMPUTER, NULL,
					_("Computer"),
					_("Browse all local and remote disks and folders accessible from this computer"),
					G_CALLBACK (activate_uri));
	gtk_menu_shell_append (GTK_MENU_SHELL (places_menu), item);

	panel_place_menu_item_append_local_gio (place_item, places_menu);
	add_menu_separator (places_menu);

	item = panel_menu_item_uri_new ("network://",
					PANEL_ICON_NETWORK, NULL,
					_("Network"),
					_("Browse bookmarked and local network locations"),
					G_CALLBACK (activate_uri));
	gtk_menu_shell_append (GTK_MENU_SHELL (places_menu), item);

	panel_place_menu_item_append_remote_gio (place_item, places_menu);

	if (panel_is_program_in_path ("nautilus-connect-server")) {
		item = panel_menu_items_create_action_item (PANEL_ACTION_CONNECT_SERVER);
		if (item != NULL)
			gtk_menu_shell_append (GTK_MENU_SHELL (places_menu),
					       item);
	}

	add_menu_separator (places_menu);

	item = panel_menu_item_desktop_new ("gnome-search-tool.desktop",
					    NULL,
					    FALSE);
	if (item)
		gtk_menu_shell_append (GTK_MENU_SHELL (places_menu), item);

	panel_recent_append_documents_menu (places_menu,
					    place_item->priv->recent_manager);

	return places_menu;
}

static void
panel_place_menu_item_recreate_menu (GtkWidget *widget)
{
	PanelPlaceMenuItem *place_item;

	place_item = PANEL_PLACE_MENU_ITEM (widget);

	if (place_item->priv->menu) {
		gtk_widget_destroy (place_item->priv->menu);
		place_item->priv->menu = panel_place_menu_item_create_menu (place_item);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (place_item),
					   place_item->priv->menu);
		panel_applet_menu_set_recurse (GTK_MENU (place_item->priv->menu),
					       "menu_panel",
					       place_item->priv->panel);
	}
}

static void
panel_place_menu_item_key_changed (GSettings *settings, const gchar *key, GtkWidget *place_item)
{
	panel_place_menu_item_recreate_menu (place_item);
}

static void
panel_place_menu_item_gtk_bookmarks_changed (GFileMonitor *handle,
					     GFile        *file,
					     GFile        *other_file,
					     GFileMonitorEvent event,
					     gpointer      user_data)
{
	panel_place_menu_item_recreate_menu (GTK_WIDGET (user_data));
}

static void
panel_place_menu_item_drives_changed (GVolumeMonitor *monitor,
				      GDrive         *drive,
				      GtkWidget      *place_menu)
{
	panel_place_menu_item_recreate_menu (place_menu);
}

static void
panel_place_menu_item_volumes_changed (GVolumeMonitor *monitor,
				       GVolume        *volume,
				       GtkWidget      *place_menu)
{
	panel_place_menu_item_recreate_menu (place_menu);
}

static void
panel_place_menu_item_mounts_changed (GVolumeMonitor *monitor,
				      GMount         *mount,
				      GtkWidget      *place_menu)
{
	panel_place_menu_item_recreate_menu (place_menu);
}

static GtkWidget *
panel_desktop_menu_item_create_menu (PanelDesktopMenuItem *desktop_item,
				     gboolean append_lock_logout)
{
	GtkWidget *desktop_menu;
	GtkWidget *item;

	desktop_menu = panel_create_menu ();

	/* Do not force the string like in gnome-shell, but just use the one
	 * from the .desktop file */
	item = panel_menu_item_desktop_new ("gnome-control-center.desktop",
					    NULL, FALSE);
	if (item)
		gtk_menu_shell_append (GTK_MENU_SHELL (desktop_menu), item);

	if (append_lock_logout)
		panel_menu_items_append_lock_logout (desktop_menu);

	return desktop_menu;
}

static void
panel_place_menu_item_dispose (GObject *object)
{
  PanelPlaceMenuItem *menuitem = (PanelPlaceMenuItem *) object;

  g_clear_object (&menuitem->priv->settings);

  if (menuitem->priv->bookmarks_monitor != NULL)
    {
      g_file_monitor_cancel (menuitem->priv->bookmarks_monitor);

      g_clear_object (&menuitem->priv->bookmarks_monitor);
    }

  if (menuitem->priv->drive_changed_id)
    g_signal_handler_disconnect (menuitem->priv->volume_monitor,
                                 menuitem->priv->drive_changed_id);
  menuitem->priv->drive_changed_id = 0;

  if (menuitem->priv->drive_connected_id)
    g_signal_handler_disconnect (menuitem->priv->volume_monitor,
                                 menuitem->priv->drive_connected_id);
  menuitem->priv->drive_connected_id = 0;

  if (menuitem->priv->drive_disconnected_id)
    g_signal_handler_disconnect (menuitem->priv->volume_monitor,
                                 menuitem->priv->drive_disconnected_id);
  menuitem->priv->drive_disconnected_id = 0;

  if (menuitem->priv->volume_added_id)
    g_signal_handler_disconnect (menuitem->priv->volume_monitor,
                                 menuitem->priv->volume_added_id);
  menuitem->priv->volume_added_id = 0;

  if (menuitem->priv->volume_changed_id)
    g_signal_handler_disconnect (menuitem->priv->volume_monitor,
                                 menuitem->priv->volume_changed_id);
  menuitem->priv->volume_changed_id = 0;

  if (menuitem->priv->volume_removed_id)
    g_signal_handler_disconnect (menuitem->priv->volume_monitor,
                                 menuitem->priv->volume_removed_id);
  menuitem->priv->volume_removed_id = 0;

  if (menuitem->priv->mount_added_id)
    g_signal_handler_disconnect (menuitem->priv->volume_monitor,
                                 menuitem->priv->mount_added_id);
  menuitem->priv->mount_added_id = 0;

  if (menuitem->priv->mount_changed_id)
    g_signal_handler_disconnect (menuitem->priv->volume_monitor,
                                 menuitem->priv->mount_changed_id);
  menuitem->priv->mount_changed_id = 0;

  if (menuitem->priv->mount_removed_id)
    g_signal_handler_disconnect (menuitem->priv->volume_monitor,
                                 menuitem->priv->mount_removed_id);
  menuitem->priv->mount_removed_id = 0;

  g_clear_object (&menuitem->priv->volume_monitor);

  G_OBJECT_CLASS (panel_place_menu_item_parent_class)->dispose (object);
}

static void
panel_place_menu_item_init (PanelPlaceMenuItem *menuitem)
{
	GFile *bookmark;
	char  *bookmarks_filename;
	GError *error;
	GSettings *settings;

	menuitem->priv = PANEL_PLACE_MENU_ITEM_GET_PRIVATE (menuitem);

	settings = g_settings_new (GNOME_NAUTILUS_DESKTOP_SCHEMA);

	menuitem->priv->settings = settings;

	g_signal_connect (settings, "changed::" GNOME_NAUTILUS_DESKTOP_HOME_ICON_NAME_KEY,
	                  G_CALLBACK (panel_place_menu_item_key_changed), menuitem);

	menuitem->priv->recent_manager = gtk_recent_manager_get_default ();

	bookmarks_filename = panel_gtk_get_bookmark_file ();
	bookmark = g_file_new_for_path (bookmarks_filename);

	error = NULL;
	menuitem->priv->bookmarks_monitor = g_file_monitor_file 
        						(bookmark,
        						G_FILE_MONITOR_NONE,
        						NULL,
        						&error);
	if (error) {
		g_warning ("Failed to add file monitor for %s: %s\n",
			   bookmarks_filename, error->message);
		g_error_free (error);
	} else {
		g_signal_connect (G_OBJECT (menuitem->priv->bookmarks_monitor), 
				  "changed", 
				  (GCallback) panel_place_menu_item_gtk_bookmarks_changed,
				  menuitem);
	}

	g_object_unref (bookmark);
	g_free (bookmarks_filename);

	menuitem->priv->volume_monitor = g_volume_monitor_get ();

	menuitem->priv->drive_changed_id = g_signal_connect (menuitem->priv->volume_monitor,
							   "drive-changed",
							   G_CALLBACK (panel_place_menu_item_drives_changed),
							   menuitem);
	menuitem->priv->drive_connected_id = g_signal_connect (menuitem->priv->volume_monitor,
							     "drive-connected",
							     G_CALLBACK (panel_place_menu_item_drives_changed),
							     menuitem);
	menuitem->priv->drive_disconnected_id = g_signal_connect (menuitem->priv->volume_monitor,
							     "drive-disconnected",
							     G_CALLBACK (panel_place_menu_item_drives_changed),
							     menuitem);
	menuitem->priv->volume_added_id = g_signal_connect (menuitem->priv->volume_monitor,
							   "volume-added",
							   G_CALLBACK (panel_place_menu_item_volumes_changed),
							   menuitem);
	menuitem->priv->volume_changed_id = g_signal_connect (menuitem->priv->volume_monitor,
							     "volume-changed",
							     G_CALLBACK (panel_place_menu_item_volumes_changed),
							     menuitem);
	menuitem->priv->volume_removed_id = g_signal_connect (menuitem->priv->volume_monitor,
							     "volume-removed",
							     G_CALLBACK (panel_place_menu_item_volumes_changed),
							     menuitem);
	menuitem->priv->mount_added_id = g_signal_connect (menuitem->priv->volume_monitor,
							   "mount-added",
							   G_CALLBACK (panel_place_menu_item_mounts_changed),
							   menuitem);
	menuitem->priv->mount_changed_id = g_signal_connect (menuitem->priv->volume_monitor,
							     "mount-changed",
							     G_CALLBACK (panel_place_menu_item_mounts_changed),
							     menuitem);
	menuitem->priv->mount_removed_id = g_signal_connect (menuitem->priv->volume_monitor,
							     "mount-removed",
							     G_CALLBACK (panel_place_menu_item_mounts_changed),
							     menuitem);

}

static void
panel_desktop_menu_item_init (PanelDesktopMenuItem *menuitem)
{
	menuitem->priv = PANEL_DESKTOP_MENU_ITEM_GET_PRIVATE (menuitem);
}

static void
panel_place_menu_item_class_init (PanelPlaceMenuItemClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass *) klass;

	gobject_class->dispose = panel_place_menu_item_dispose;

	g_type_class_add_private (klass, sizeof (PanelPlaceMenuItemPrivate));
}

static void
panel_desktop_menu_item_class_init (PanelDesktopMenuItemClass *klass)
{
	g_type_class_add_private (klass, sizeof (PanelDesktopMenuItemPrivate));
}

GtkWidget *
panel_place_menu_item_new (gboolean use_image,
			   gboolean in_menubar)
{
	PanelPlaceMenuItem *menuitem;
	const char          *name;
	const char          *icon_name;

	menuitem = g_object_new (PANEL_TYPE_PLACE_MENU_ITEM, NULL);

	name = _("Places");
	icon_name = PANEL_ICON_FOLDER;

	if (in_menubar) {
		gtk_menu_item_set_label (GTK_MENU_ITEM (menuitem), name);
		if (use_image) {
			GtkIconSize icon_size;
			GtkWidget *image;

			icon_size = panel_menu_bar_object_icon_get_size ();
			image = gtk_image_new_from_icon_name (icon_name, icon_size);

			gtk_image_set_pixel_size (GTK_IMAGE (image),
			                          panel_menu_bar_object_icon_get_pixel_size (icon_size));
			panel_image_menu_item_set_image (PANEL_IMAGE_MENU_ITEM (menuitem), image);
		}
	} else {
		if (use_image)
			setup_menu_item_with_icon (GTK_WIDGET (menuitem),
						   panel_menu_icon_get_size (),
						   icon_name, NULL,
						   name);
		else
			setup_menuitem (GTK_WIDGET (menuitem),
					GTK_ICON_SIZE_INVALID, NULL,
					name);
	}

	menuitem->priv->menu = panel_place_menu_item_create_menu (menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   menuitem->priv->menu);

	return GTK_WIDGET (menuitem);
}

GtkWidget *
panel_desktop_menu_item_new (gboolean use_image,
			     gboolean in_menubar,
			     gboolean append_lock_logout)
{
	PanelDesktopMenuItem *menuitem;
	char                 *name;
	const char           *icon_name;

	menuitem = g_object_new (PANEL_TYPE_DESKTOP_MENU_ITEM, NULL);

	name = panel_util_get_user_name ();
	icon_name = PANEL_ICON_COMPUTER;

	/* if we're in a menubar, we don't want to use setup_* as it changes
	 * the size requests and can make the panels bigger than we'd like */
	if (in_menubar) {
		gtk_menu_item_set_label (GTK_MENU_ITEM (menuitem), name);
		menuitem->priv->icon_size = panel_menu_bar_object_icon_get_size ();

		if (use_image) {
			GtkWidget *image;
			image = gtk_image_new_from_icon_name (icon_name,
							      menuitem->priv->icon_size);
			gtk_image_set_pixel_size (GTK_IMAGE (image),
			                          panel_menu_bar_object_icon_get_pixel_size (menuitem->priv->icon_size));
			panel_image_menu_item_set_image (PANEL_IMAGE_MENU_ITEM (menuitem), image);
		}
	} else {
		menuitem->priv->icon_size = panel_menu_icon_get_size ();

		if (use_image)
			setup_menu_item_with_icon (GTK_WIDGET (menuitem),
						   menuitem->priv->icon_size,
						   icon_name, NULL,
						   name);
		else
			setup_menuitem (GTK_WIDGET (menuitem),
					GTK_ICON_SIZE_INVALID, NULL,
					name);
	}

	g_free (name);

	menuitem->priv->menu = panel_desktop_menu_item_create_menu (menuitem,
								    append_lock_logout);
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

static void
panel_menu_items_lock_logout_separator_notified (PanelLockdown *lockdown,
						 gpointer       user_data)
{
	GtkWidget *separator = user_data;

	if (!panel_lockdown_get_disable_lock_screen (lockdown) ||
	    !panel_lockdown_get_disable_switch_user (lockdown) ||
	    !panel_lockdown_get_disable_log_out (lockdown))
		gtk_widget_show (separator);
	else
		gtk_widget_hide (separator);
}

void
panel_menu_items_append_lock_logout (GtkWidget *menu)
{
	GList      *children;
	GList      *last;
	GtkWidget  *item;
	GtkWidget *separator;
	gboolean   hide_separator;

	children = gtk_container_get_children (GTK_CONTAINER (menu));
	last = g_list_last (children);
	if (last != NULL &&
	    GTK_IS_SEPARATOR (last->data))
		item = GTK_WIDGET (last->data);
	else
		item = add_menu_separator (menu);
	g_list_free (children);

	panel_lockdown_on_notify (panel_lockdown_get (),
				  NULL,
				  G_OBJECT (item),
				  panel_menu_items_lock_logout_separator_notified,
				  item);
	panel_menu_items_lock_logout_separator_notified (panel_lockdown_get (),
							 item);

	item = panel_menu_items_create_switch_user (FALSE);

	if (item != NULL) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_object_bind_property (panel_lockdown_get (),
					"disable-switch-user",
					item,
					"visible",
					G_BINDING_SYNC_CREATE|G_BINDING_INVERT_BOOLEAN);
	}

	item = panel_menu_items_create_action_item_full (PANEL_ACTION_LOGOUT,
							 NULL, NULL, TRUE);

	if (item != NULL) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_object_bind_property (panel_lockdown_get (),
					"disable-log-out",
					item,
					"visible",
					G_BINDING_SYNC_CREATE|G_BINDING_INVERT_BOOLEAN);
	}

	item = panel_menu_items_create_action_item_full (PANEL_ACTION_LOCK,
							 NULL, NULL, TRUE);
	if (item != NULL) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_object_bind_property (panel_lockdown_get (),
					"disable-lock-screen",
					item,
					"visible",
					G_BINDING_SYNC_CREATE|G_BINDING_INVERT_BOOLEAN);
	}

	/* Separator */
	separator = add_menu_separator (menu);
	hide_separator = TRUE;

	g_object_bind_property (panel_lockdown_get (), "disable-log-out",
	                        separator, "visible",
	                        G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	/* Hibernate */
	if (panel_end_session_dialog_is_hibernate_available (panel_end_session_dialog_get ())) {
		item = panel_menu_items_create_action_item_full (PANEL_ACTION_HIBERNATE,
								 NULL, NULL, TRUE);
		if (item != NULL) {
			hide_separator = FALSE;

			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

			g_object_bind_property (panel_lockdown_get (), "disable-log-out",
			                        item, "visible",
			                        G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
		}
	}

	/* Suspend */
	if (panel_end_session_dialog_is_suspend_available (panel_end_session_dialog_get ())) {
		item = panel_menu_items_create_action_item_full (PANEL_ACTION_SUSPEND,
								 NULL, NULL, TRUE);
		if (item != NULL) {
			hide_separator = FALSE;

			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

			g_object_bind_property (panel_lockdown_get (), "disable-log-out",
			                        item, "visible",
			                        G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
		}
	}

	/* Hubrid Sleep */
	if (panel_end_session_dialog_is_hybrid_sleep_available (panel_end_session_dialog_get ())) {
		item = panel_menu_items_create_action_item_full (PANEL_ACTION_HYBRID_SLEEP,
								 NULL, NULL, TRUE);
		if (item != NULL) {
			hide_separator = FALSE;

			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

			g_object_bind_property (panel_lockdown_get (), "disable-log-out",
			                        item, "visible",
			                        G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
		}
	}

	/* FIXME: should be dynamic */
	if (panel_session_manager_is_shutdown_available (panel_session_manager_get ())) {
		item = panel_menu_items_create_action_item_full (PANEL_ACTION_REBOOT,
	                                                 NULL, NULL, TRUE);
		if (item != NULL) {
			hide_separator = FALSE;

			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

			g_object_bind_property (panel_lockdown_get (), "disable-log-out",
				                item, "visible",
				                G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
		}

		item = panel_menu_items_create_action_item_full (PANEL_ACTION_SHUTDOWN,
								 NULL, NULL, TRUE);
		if (item != NULL) {
			hide_separator = FALSE;

			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

			g_object_bind_property (panel_lockdown_get (), "disable-log-out",
				                item, "visible",
				                G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
		}
	}

	if (hide_separator)
		g_object_set (separator, "visible", FALSE, NULL);
}

void
panel_menu_item_activate_desktop_file (GtkWidget  *menuitem,
				       const char *path)
{
	panel_launch_desktop_file (path, menuitem_to_screen (menuitem), NULL);
}
