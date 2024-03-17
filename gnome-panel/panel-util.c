/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * GNOME panel utils
 * (C) 1997, 1998, 1999, 2000 The Free Software Foundation
 * Copyright 2000 Helix Code, Inc.
 * Copyright 2000,2001 Eazel, Inc.
 * Copyright 2001 George Lebl
 * Copyright 2002 Sun Microsystems Inc.
 *
 * Authors: George Lebl
 *          Jacob Berkman
 *          Mark McLoughlin
 */

#include <config.h>

#include "panel-util.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-keyfile.h>
#include <libpanel-util/panel-xdg.h>

#include "applet.h"
#include "panel-bindings.h"
#include "panel-icon-names.h"
#include "panel-schemas.h"

void
panel_push_window_busy (GtkWidget *window)
{
	int busy = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window),
						       "Panel:WindowBusy"));

	busy ++;

	if (busy == 1) {
		GdkWindow *win;

		gtk_widget_set_sensitive (window, FALSE);

		win = gtk_widget_get_window (window);
		if (win != NULL) {
			GdkDisplay *display;
			GdkCursor *cursor;

			display = gdk_display_get_default ();
			cursor = gdk_cursor_new_for_display (display, GDK_WATCH);

			gdk_window_set_cursor (win, cursor);
			g_object_unref (cursor);
			gdk_display_flush (display);
		}
	}

	g_object_set_data (G_OBJECT (window), "Panel:WindowBusy",
			   GINT_TO_POINTER (busy));
}

void
panel_pop_window_busy (GtkWidget *window)
{
	int busy = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window),
						       "Panel:WindowBusy"));
	busy --;

	if (busy <= 0) {
		GdkWindow *win;

		gtk_widget_set_sensitive (window, TRUE);

		win = gtk_widget_get_window (window);
		if (win != NULL)
			gdk_window_set_cursor (win, NULL);

		g_object_set_data (G_OBJECT (window),
				   "Panel:WindowBusy", NULL);
	} else {
		g_object_set_data (G_OBJECT (window), "Panel:WindowBusy",
				   GINT_TO_POINTER (busy));
	}
}

static char *
panel_find_icon (GtkIconTheme  *icon_theme,
		 const char    *icon_name,
		 gint           size)
{
	GtkIconInfo *info;
	char        *retval;
	char        *icon_no_extension;

	if (icon_name == NULL || strcmp (icon_name, "") == 0)
		return NULL;

	if (g_path_is_absolute (icon_name)) {
		if (g_file_test (icon_name, G_FILE_TEST_EXISTS)) {
			return g_strdup (icon_name);
		} else {
			char *basename;

			basename = g_path_get_basename (icon_name);
			retval = panel_find_icon (icon_theme, basename,
						  size);
			g_free (basename);

			return retval;
		}
	}

	/* This is needed because some .desktop files have an icon name *and*
	 * an extension as icon */
	icon_no_extension = panel_xdg_icon_remove_extension (icon_name);

	info = gtk_icon_theme_lookup_icon (icon_theme, icon_no_extension,
					   size, 0);

	g_free (icon_no_extension);

	if (info) {
		retval = g_strdup (gtk_icon_info_get_filename (info));
		g_object_unref (info);
	} else
		retval = NULL;

	return retval;
}

static char *
panel_util_get_from_personal_path (const char *file)
{
	return g_build_filename (g_get_user_config_dir (),
				 "gnome-panel", file, NULL);
}

static char *
panel_launcher_get_personal_path (void)
{
	return panel_util_get_from_personal_path ("launchers");
}

static GFile *
panel_launcher_get_gfile (const char *location)
{
	char  *path;
	GFile *file;

	if (!g_ascii_strncasecmp (location, "file:", strlen ("file:")))
		return g_file_new_for_uri (location);

	if (g_path_is_absolute (location))
		return g_file_new_for_path (location);

	path = panel_make_full_path (NULL, location);
	file = g_file_new_for_path (path);
	g_free (path);

	return file;
}

char *
panel_launcher_get_filename (const char *location)
{
	GFile *file;
	GFile *launchers_dir;
	char  *launchers_path;
	char  *retval;

	if (!g_path_is_absolute (location) &&
	    g_ascii_strncasecmp (location, "file:", strlen ("file:")))
		/* this is not a local URI */
		return NULL;

	launchers_path = panel_launcher_get_personal_path ();
	launchers_dir = g_file_new_for_path (launchers_path);
	g_free (launchers_path);

	file = panel_launcher_get_gfile (location);

	retval = g_file_get_relative_path (launchers_dir, file);

	g_object_unref (file);
	g_object_unref (launchers_dir);

	return retval;
}

char *
guess_icon_from_exec (GtkIconTheme *icon_theme,
		      GKeyFile     *key_file)
{
	char *exec;
	char *icon_name;
	char *path;

	exec = panel_key_file_get_string (key_file, "Exec");
	if (!exec || !exec [0]) {
		g_free (exec);
		return NULL;
	}

	icon_name = g_path_get_basename (exec);
	g_free (exec);

	path = panel_find_icon (icon_theme, icon_name, 48);
	if (!path) {
		g_free (icon_name);
		return NULL;
	}

	g_free (path);

	return icon_name;
}

static GFile *
panel_util_get_gfile_root (GFile *file)
{
	GFile *parent;
	GFile *parent_old;

	/* search for the root on the URI */
	parent_old = g_object_ref (file);
	parent = g_file_get_parent (file);
	while (parent != NULL) {
		g_object_unref (parent_old);
		parent_old = parent;
		parent = g_file_get_parent (parent);
	}

	return parent_old;
}

static char *
panel_util_get_file_display_name_if_mount (GFile *file)
{
	GFile          *compare;
	GVolumeMonitor *monitor;
	GList          *mounts, *l;
	char           *ret;

	ret = NULL;

	/* compare with all mounts */
	monitor = g_volume_monitor_get ();
	mounts = g_volume_monitor_get_mounts (monitor);
	for (l = mounts; l != NULL; l = l->next) {
		GMount *mount;
		mount = G_MOUNT (l->data);
		compare = g_mount_get_root (mount);
		if (!ret && g_file_equal (file, compare))
			ret = g_mount_get_name (mount);
		g_object_unref (mount);
	}
	g_list_free (mounts);
	g_object_unref (monitor);

	return ret;
}

static char *
panel_util_get_file_display_for_common_files (GFile *file)
{
	GFile *compare;

	compare = g_file_new_for_path (g_get_home_dir ());
	if (g_file_equal (file, compare)) {
		g_object_unref (compare);
		return g_strdup (_("Home"));
	}
	g_object_unref (compare);

	compare = g_file_new_for_path ("/");
	if (g_file_equal (file, compare)) {
		g_object_unref (compare);
		/* Translators: this is the same string as the one found in
		 * nautilus */
		return g_strdup (_("File System"));
	}
	g_object_unref (compare);

	return NULL;
}

static char *
panel_util_get_file_description (GFile *file)
{
	GFileInfo *info;
	char      *ret;

	ret = NULL;

	info = g_file_query_info (file, "standard::description",
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL, NULL);

	if (info) {
		ret = g_strdup (g_file_info_get_attribute_string (info,
								  G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION));
		g_object_unref (info);
	}

	return ret;
}

static char *
panel_util_get_file_display_name (GFile    *file,
				  gboolean  use_fallback)
{
	GFileInfo *info;
	char      *ret;

	ret = NULL;

	info = g_file_query_info (file, "standard::display-name",
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL, NULL);

	if (info) {
		ret = g_strdup (g_file_info_get_display_name (info));
		g_object_unref (info);
	}

	if (!ret && use_fallback) {
		/* can happen with URI schemes non supported by gvfs */
		char *basename;

		basename = g_file_get_basename (file);
		ret = g_filename_display_name (basename);
		g_free (basename);
	}

	return ret;
}

static char *
panel_util_get_file_icon_name_if_mount (GFile *file)
{
	GFile          *compare;
	GVolumeMonitor *monitor;
	GList          *mounts, *l;
	char           *ret;

	ret = NULL;

	/* compare with all mounts */
	monitor = g_volume_monitor_get ();
	mounts = g_volume_monitor_get_mounts (monitor);
	for (l = mounts; l != NULL; l = l->next) {
		GMount *mount;
		mount = G_MOUNT (l->data);
		compare = g_mount_get_root (mount);
		if (!ret && g_file_equal (file, compare)) {
			GIcon *gicon;
			gicon = g_mount_get_icon (mount);
			ret = panel_util_get_icon_name_from_g_icon (gicon);
			g_object_unref (gicon);
		}
		g_object_unref (mount);
	}
	g_list_free (mounts);
	g_object_unref (monitor);

	return ret;
}

/* TODO: convert this to a simple call to g_file_query_info? */
static const char *
panel_util_get_icon_for_uri_known_folders (const char *uri)
{
	const char *icon;
	char       *path;
	int         len;

	icon = NULL;
	
	if (!g_str_has_prefix (uri, "file:"))
		return NULL;

	path = g_filename_from_uri (uri, NULL, NULL);

	/* Sanity check. We may be given a broken URI. */
	if (path == NULL)
		return NULL;

	len = strlen (path);
	if (path[len] == '/')
		path[len] = '\0';

	if (strcmp (path, "/") == 0)
		icon = PANEL_ICON_FILESYSTEM;
	else if (strcmp (path, g_get_home_dir ()) == 0)
		icon = PANEL_ICON_HOME;
	else if (strcmp (path,
			 g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP)) == 0)
		icon = PANEL_ICON_DESKTOP;
	else if (g_strcmp0 (path,
			    g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS)) == 0)
		icon = PANEL_ICON_FOLDER_DOCUMENTS;
	else if (g_strcmp0 (path,
			    g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD)) == 0)
		icon = PANEL_ICON_FOLDER_DOWNLOAD;
	else if (g_strcmp0 (path,
			    g_get_user_special_dir (G_USER_DIRECTORY_MUSIC)) == 0)
		icon = PANEL_ICON_FOLDER_MUSIC;
	else if (g_strcmp0 (path,
			    g_get_user_special_dir (G_USER_DIRECTORY_PICTURES)) == 0)
		icon = PANEL_ICON_FOLDER_PICTURES;
	else if (g_strcmp0 (path,
			    g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE)) == 0)
		icon = PANEL_ICON_FOLDER_PUBLIC_SHARE;
	else if (g_strcmp0 (path,
			    g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES)) == 0)
		icon = PANEL_ICON_FOLDER_TEMPLATES;
	else if (g_strcmp0 (path,
			    g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS)) == 0)
		icon = PANEL_ICON_FOLDER_VIDEOS;

	g_free (path);

	return icon;
}

/* This is based on nautilus_compute_title_for_uri() and
 * nautilus_file_get_display_name_nocopy() */
char *
panel_util_get_label_for_uri (const char *text_uri)
{
	GFile *file;
	char  *label;
	GFile *root;
	char  *root_display;

	/* Here's what we do:
	 *  + check if the URI is a mount
	 *  + if file: URI:
	 *   - check for known file: URI
	 *   - check for description of the GFile
	 *   - use display name of the GFile
	 *  + else:
	 *   - check for description of the GFile
	 *   - if the URI is a root: "root displayname"
	 *   - else: "root displayname: displayname"
	 */

	label = NULL;

	file = g_file_new_for_uri (text_uri);

	label = panel_util_get_file_display_name_if_mount (file);
	if (label) {
		g_object_unref (file);
		return label;
	}

	if (g_str_has_prefix (text_uri, "file:")) {
		label = panel_util_get_file_display_for_common_files (file);
		if (!label)
			label = panel_util_get_file_description (file);
		if (!label)
			label = panel_util_get_file_display_name (file, TRUE);
		g_object_unref (file);

		return label;
	}

	label = panel_util_get_file_description (file);
	if (label) {
		g_object_unref (file);
		return label;
	}

	root = panel_util_get_gfile_root (file);
	root_display = panel_util_get_file_description (root);
	if (!root_display)
		root_display = panel_util_get_file_display_name (root, FALSE);
	if (!root_display)
		/* can happen with URI schemes non supported by gvfs */
		root_display = g_file_get_uri_scheme (root);

	if (g_file_equal (file, root))
		label = root_display;
	else {
		char *displayname;

		displayname = panel_util_get_file_display_name (file, TRUE);
		/* Translators: the first string is the name of a gvfs
		 * method, and the second string is a path. For
		 * example, "Trash: some-directory". It means that the
		 * directory called "some-directory" is in the trash.
		 */
		label = g_strdup_printf (_("%1$s: %2$s"),
					 root_display, displayname);
		g_free (root_display);
		g_free (displayname);
	}

	g_object_unref (root);
	g_object_unref (file);

	return label;
}

/* FIXME: we probably want to return a GIcon, that would be built with
 * g_themed_icon_new_with_default_fallbacks() since we can get an icon like
 * "folder-music", where "folder" is the safe fallback. */
char *
panel_util_get_icon_for_uri (const char *text_uri)
{
	const char *icon;
	GFile      *file;
	GFileInfo  *info;
	GIcon      *gicon;
	char       *retval;

	/* Here's what we do:
	 *  + check for known file: URI
	 *  + override burn: URI icon
	 *  + check if the URI is a mount
	 *  + override trash: URI icon for subfolders
	 *  + check for application/x-gnome-saved-search mime type and override
	 *    icon of the GFile
	 *  + use icon of the GFile
	 */

	/* this only checks file: URI */
	icon = panel_util_get_icon_for_uri_known_folders (text_uri);
	if (icon)
		return g_strdup (icon);

	/* gvfs doesn't give us a nice icon, so overriding */
	if (g_str_has_prefix (text_uri, "burn:"))
		return g_strdup (PANEL_ICON_BURNER);

	file = g_file_new_for_uri (text_uri);

	retval = panel_util_get_file_icon_name_if_mount (file);
	if (retval)
		return retval;

	/* gvfs doesn't give us a nice icon for subfolders of the trash, so
	 * overriding */
	if (g_str_has_prefix (text_uri, "trash:")) {
		GFile *root;

		root = panel_util_get_gfile_root (file);
		g_object_unref (file);
		file = root;
	}

	info = g_file_query_info (file, "standard::icon",
				  G_FILE_QUERY_INFO_NONE, NULL, NULL);
	g_object_unref (file);

	if (!info)
		return NULL;

	gicon = g_file_info_get_icon (info);
	retval = panel_util_get_icon_name_from_g_icon (gicon);
	g_object_unref (info);

	return retval;
}

/*
 * Keep code in panel-applet-bindings.c in sync!
 */
static void
panel_util_key_event_is_binding (GdkEventKey *event,
				 GType        type,
				 const char  *signal_name,
				 gboolean    *is_popup,
				 gboolean    *is_popup_modifier)
{
	GtkBindingSet   *binding_set;
	GtkBindingEntry *binding_entry;
	gboolean         popup = FALSE;
	gboolean         popup_modifier = FALSE;
	guint            modifiers;
	char            *signal_dash;
	char            *signal_underscore;

	modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

	signal_dash = g_strdup (signal_name);
	g_strdelimit (signal_dash, "_", '-');
	signal_underscore = g_strdup (signal_name);
	g_strdelimit (signal_underscore, "-", '_');

	binding_set = gtk_binding_set_by_class (g_type_class_peek (type));

	for (binding_entry = binding_set->entries;
	     binding_entry != NULL;
	     binding_entry = binding_entry->set_next) {
		GtkBindingSignal *binding_signal;

		for (binding_signal = binding_entry->signals;
		     binding_signal != NULL;
		     binding_signal = binding_signal->next) {
			if (g_strcmp0 (binding_signal->signal_name, signal_dash) == 0 ||
			    g_strcmp0 (binding_signal->signal_name, signal_underscore) == 0) {
				if (binding_entry->keyval != event->keyval)
					break;

				popup = modifiers == binding_entry->modifiers;
				popup_modifier = modifiers == (panel_bindings_get_mouse_button_modifier_keymask ()|binding_entry->modifiers);
				break;
			}
		}

		if (popup || popup_modifier)
			break;
	}

	if (is_popup)
		*is_popup = popup;
	if (is_popup_modifier)
		*is_popup_modifier = popup_modifier;

	g_free (signal_dash);
	g_free (signal_underscore);
}

void
panel_util_key_event_is_popup (GdkEventKey *event,
			       gboolean    *is_popup,
			       gboolean    *is_popup_modifier)
{
	panel_util_key_event_is_binding (event, GTK_TYPE_WIDGET, "popup-menu",
					 is_popup, is_popup_modifier);
}

void
panel_util_key_event_is_popup_panel (GdkEventKey *event,
				     gboolean    *is_popup,
				     gboolean    *is_popup_modifier)
{
	panel_util_key_event_is_binding (event, PANEL_TYPE_TOPLEVEL, "popup-panel-menu",
					 is_popup, is_popup_modifier);
}

int
panel_util_get_window_scaling_factor (void)
{
	GValue value = G_VALUE_INIT;

	g_value_init (&value, G_TYPE_INT);

	if (gdk_screen_get_setting (gdk_screen_get_default (),
	                            "gdk-window-scaling-factor",
	                            &value))
		return g_value_get_int (&value);

	return 1;
}
