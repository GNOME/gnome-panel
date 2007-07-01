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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <glib/gi18n.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-icon-lookup.h>
#include <libgnomeui/gnome-help.h>

#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>

#include "applet.h"
#include "nothing.h"
#include "xstuff.h"
#include "panel-globals.h"
#include "launcher.h"
#include "panel-icon-names.h"

static int
panel_ditem_launch (GnomeDesktopItem  *item,
		    GList             *file_list,
		    GdkScreen         *screen,
		    GError           **error)
{
	int workspace;

	workspace = xstuff_get_current_workspace (screen);

	gnome_desktop_item_set_launch_time (item,
					    gtk_get_current_event_time ());

	return gnome_desktop_item_launch_on_screen (item, file_list, 0,
						    screen, workspace, error);
}

void
panel_util_launch_from_key_file (GKeyFile   *keyfile,
				 GList      *file_list,
				 GdkScreen  *screen,
				 GError    **error)
{
	GnomeDesktopItem  *ditem;
	int                i;
	static const char *keys [] = { "Type",
				       "Exec",
				       "URL",
				       "Dev",
				       "Path",
				       "Terminal",
				       "TerminalOptions",
				       "StartupNotify",
				       "StartupWMClass",
				     };
	static const char *locale_keys [] = { "Name",
					      "GenericName",
					      "Icon",
					      "MiniIcon",
					    };

	g_return_if_fail (keyfile != NULL);

	ditem = gnome_desktop_item_new ();
	if (ditem == NULL)
		return;

	for (i = 0; i < G_N_ELEMENTS (keys); i++) {
		char *value;

		value = panel_util_key_file_get_string (keyfile,
							keys [i]);
		if (value != NULL) {
			gnome_desktop_item_set_string (ditem, keys [i], value);
			g_free (value);
		}
	}

	for (i = 0; i < G_N_ELEMENTS (locale_keys); i++) {
		char *value;

		value = panel_util_key_file_get_locale_string (keyfile,
							       locale_keys [i]);
		if (value != NULL) {
			gnome_desktop_item_set_string (ditem, locale_keys [i], value);
			g_free (value);
		}
	}

	panel_ditem_launch (ditem, file_list, screen, error);
	gnome_desktop_item_unref (ditem);
}

void
panel_launch_desktop_file (const char  *desktop_file,
			   const char  *fallback_exec,
			   GdkScreen   *screen,
			   GError     **error)
{
	GnomeDesktopItem *ditem;

	if (g_path_is_absolute (desktop_file))
		ditem = gnome_desktop_item_new_from_file (desktop_file, 0,
							  error);
	else
		ditem = gnome_desktop_item_new_from_basename (desktop_file, 0,
							      error);

	if (ditem != NULL) {
		panel_ditem_launch (ditem, NULL, screen, error);
		gnome_desktop_item_unref (ditem);
	} else if (fallback_exec != NULL) {
		char *argv [2] = {(char *)fallback_exec, NULL};

		if (*error) {
			g_error_free (*error);
			*error = NULL;
		}

		gdk_spawn_on_screen (screen, NULL, argv, NULL,
				     G_SPAWN_SEARCH_PATH,
				     NULL, NULL, NULL, error);
	}
}

char *
panel_util_make_exec_uri_for_desktop (const char *exec)
{
	GString    *str;
	const char *c;

	if (!exec)
		return g_strdup ("");

	if (!strchr (exec, ' '))
		return g_strdup (exec);

	str = g_string_new_len (NULL, strlen (exec));

	str = g_string_append_c (str, '"');
	for (c = exec; *c != '\0'; c++) {
		/* FIXME: GKeyFile will add an additional backslach so we'll
		 * end up with toto\\" instead of toto\"
		 * We could use g_key_file_set_value(), but then we don't
		 * benefit from the other escaping that glib is doing...
		 */
		if (*c == '"')
			str = g_string_append (str, "\\\"");
		else
			str = g_string_append_c (str, *c);
	}
	str = g_string_append_c (str, '"');

	return g_string_free (str, FALSE);
}

void
panel_show_help (GdkScreen  *screen,
		 const char *doc_name,
		 const char *linkid)
{
	GError *error = NULL;

	if (!gnome_help_display_desktop_on_screen (NULL, "user-guide", doc_name, linkid, screen, &error)) {
		panel_error_dialog (NULL, screen, "cannot_show_help", TRUE,
				    _("Could not display help document"),
				    error != NULL ? error->message : NULL);

		g_clear_error (&error);
	}
}

GList *
panel_g_list_insert_before (GList *list,
			    GList *sibling,
			    GList *link)
{
	if (!list) {
		g_return_val_if_fail (sibling == NULL, list);
		return link;
	} else if (sibling) {
		if (sibling->prev) {
			link->prev = sibling->prev;
			link->prev->next = link;
			link->next = sibling;
			sibling->prev = link;
			return list;
		} else {
			link->next = sibling;
			sibling->prev = link;
			g_return_val_if_fail (sibling == list, link);
			return link;
		}
	} else {
		GList *last;

		last = list;
		while (last->next)
			last = last->next;

		last->next = link;
		link->prev = last;
		return list;
	}
}

GList *
panel_g_list_insert_after (GList *list,
			   GList *sibling,
			   GList *link)
{
	if (!list) {
		g_return_val_if_fail (sibling == NULL, link);
		return link;
	} else if (sibling) {
		if (sibling->next) {
			link->next = sibling->next;
			link->next->prev = link;
			link->prev = sibling;
			sibling->next = link;
			return list;
		} else {
			sibling->next = link;
			link->prev = sibling;
			return list;
		}
			
	} else {
		link->next = list;
		list->prev = link;
		return link;
	}
}

GList *
panel_g_list_swap_next (GList *list, GList *dl)
{
	GList *t;

	if(!dl->next)
		return list;
	if(dl->prev)
		dl->prev->next = dl->next;
	t = dl->prev;
	dl->prev = dl->next;
	dl->next->prev = t;
	if(dl->next->next)
		dl->next->next->prev = dl;
	t = dl->next->next;
	dl->next->next = dl;
	dl->next = t;

	if(list == dl)
		return dl->prev;
	return list;
}

GList *
panel_g_list_swap_prev (GList *list, GList *dl)
{
	GList *t;

	if(!dl->prev)
		return list;
	if(dl->next)
		dl->next->prev = dl->prev;
	t = dl->next;
	dl->next = dl->prev;
	dl->prev->next = t;
	if(dl->prev->prev)
		dl->prev->prev->next = dl;
	t = dl->prev->prev;
	dl->prev->prev = dl;
	dl->prev = t;

	if(list == dl->next)
		return dl;
	return list;
}

/*maybe this should be a glib function?
 it resorts a single item in the list*/
GList *
panel_g_list_resort_item(GList *list, gpointer data, GCompareFunc func)
{
	GList *dl;

	if(!list)
		return NULL;

	dl = g_list_find(list,data);

	g_return_val_if_fail(dl!=NULL,list);

	while(dl->next &&
	      (*func)(dl->data,dl->next->data)>0)
		list = panel_g_list_swap_next (list, dl);
	while(dl->prev &&
	      (*func)(dl->data,dl->prev->data)<0)
		list = panel_g_list_swap_prev (list, dl);
	return list;
}

GSList *
panel_g_slist_make_unique (GSList       *list,
			   GCompareFunc  compare,
			   gboolean      free_data)
{
	GSList *sorted, *l;

	g_assert (compare != NULL);

	if (!list)
		return NULL;

	sorted = g_slist_copy (list);
	sorted = g_slist_sort (sorted, compare);

	for (l = sorted; l; l = l->next) {
		GSList *next;

		next = l->next;
		if (l->data && next && next->data)
			if (!compare (l->data, next->data)) {
				list = g_slist_remove (list, l->data);
				if (free_data)
					g_free (l->data);
			}
	}

	g_slist_free (sorted);

	return list;
}

GtkWidget *
panel_error_dialog (GtkWindow  *parent,
		    GdkScreen  *screen,
		    const char *class,
		    gboolean    auto_destroy,
		    const char *primary_text,
		    const char *secondary_text)
{
	GtkWidget *dialog;
	char      *freeme;

	freeme = NULL;

	if (primary_text == NULL) {
		g_warning ("NULL dialog");
		 /* No need to translate this, this should NEVER happen */
		freeme = g_strdup_printf ("Error with displaying error "
					  "for dialog of class %s", class);
		primary_text = freeme;
	}

	dialog = gtk_message_dialog_new (parent, 0, GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK, "%s", primary_text);
	if (secondary_text != NULL)
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  "%s", secondary_text);

	gtk_widget_add_events (dialog, GDK_KEY_PRESS_MASK);
	g_signal_connect (dialog, "event",
			  G_CALLBACK (panel_dialog_window_event), NULL);

	if (screen)
		gtk_window_set_screen (GTK_WINDOW (dialog), screen);

	if (!parent) {
		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
		/* FIXME: We need a title in this case, but we don't know what
		 * the format should be. Let's put something simple until
		 * the following bug gets fixed:
		 * http://bugzilla.gnome.org/show_bug.cgi?id=165132 */
		gtk_window_set_title (GTK_WINDOW (dialog), _("Error"));
	}

	gtk_widget_show_all (dialog);

	if (auto_destroy)
		g_signal_connect_swapped (G_OBJECT (dialog), "response",
					  G_CALLBACK (gtk_widget_destroy),
					  G_OBJECT (dialog));

	if (freeme)
		g_free (freeme);

	return dialog;
}

int
panel_find_applet_index (GtkWidget *widget)
{
	GSList *applet_list, *l;
	int     i;

	applet_list = panel_applet_list_applets ();

	for (i = 0, l = applet_list; l; i++, l = l->next) {
		AppletInfo *info = l->data;

		if (info->widget == widget)
			return i;
	}

	return i;
}

void
panel_push_window_busy (GtkWidget *window)
{
	int busy = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window),
						       "Panel:WindowBusy"));

	busy ++;

	if (busy == 1) {
		gtk_widget_set_sensitive (window, FALSE);
		if (window->window != NULL) {
			GdkCursor *cursor = gdk_cursor_new (GDK_WATCH);
			gdk_window_set_cursor (window->window, cursor);
			gdk_cursor_unref (cursor);
			gdk_flush ();
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
		gtk_widget_set_sensitive (window, TRUE);
		if (window->window != NULL)
			gdk_window_set_cursor (window->window, NULL);
		g_object_set_data (G_OBJECT (window),
				   "Panel:WindowBusy", NULL);
	} else {
		g_object_set_data (G_OBJECT (window), "Panel:WindowBusy",
				   GINT_TO_POINTER (busy));
	}
}

gboolean
panel_is_program_in_path (const char *program)
{
	char *tmp = g_find_program_in_path (program);
	if (tmp != NULL) {
		g_free (tmp);
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean
panel_ensure_dir (const char *dirname)
{
	char *parsed, *p;

	if (dirname == NULL)
		return FALSE;

	parsed = g_strdup (dirname);

	if (g_file_test (parsed, G_FILE_TEST_IS_DIR)) {
		g_free (parsed);
		return TRUE;
	}

	p = strchr (parsed, '/');
	if (p == parsed)
		p = strchr (p+1, '/');

	while (p != NULL) {
		*p = '\0';
		if (mkdir (parsed, 0700) != 0 &&
		    errno != EEXIST && errno != ENOSYS) {
			g_free (parsed);
			return FALSE;
		}
		*p = '/';
		p = strchr (p+1, '/');
	}

	if (mkdir (parsed, 0700) != 0 &&
	    errno != EEXIST && errno != ENOSYS) {
		g_free (parsed);
		return FALSE;
	}

	g_free (parsed);
	return TRUE;
}

static gboolean
internal_panel_is_uri_writable (const char *uri, gboolean recurse)
{
	GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();

	if (gnome_vfs_get_file_info
	    (uri, info, GNOME_VFS_FILE_INFO_DEFAULT) != GNOME_VFS_OK) {
		char *dir;
		gboolean ret;

		gnome_vfs_file_info_unref (info);

		if ( ! recurse)
			return FALSE;

		dir = g_path_get_dirname (uri);
		ret = internal_panel_is_uri_writable (dir, FALSE);
		g_free (dir);

		return ret;
	}

	if ( ! (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS)) {
		gnome_vfs_file_info_unref (info);
		/* assume writable, if no permissions */
		return TRUE;
	} 

	if (info->permissions & GNOME_VFS_PERM_OTHER_WRITE) {
		gnome_vfs_file_info_unref (info);
		return TRUE;
	}

	if (info->gid == getgid () &&
	    info->permissions & GNOME_VFS_PERM_GROUP_WRITE) {
		gnome_vfs_file_info_unref (info);
		return TRUE;
	}

	if (info->uid == getuid () &&
	    info->permissions & GNOME_VFS_PERM_USER_WRITE) {
		gnome_vfs_file_info_unref (info);
		return TRUE;
	}

	if (info->gid == getgid () &&
	    info->permissions & GNOME_VFS_PERM_GROUP_WRITE) {
		gnome_vfs_file_info_unref (info);
		return TRUE;
	}

	if (info->permissions & GNOME_VFS_PERM_GROUP_WRITE) {
		gid_t *groups;
		int i, n;

		/* get size */
		n = getgroups (0, NULL);

		if (n == 0) {
			/* no more groups */
			gnome_vfs_file_info_unref (info);
			return FALSE;
		}

		groups = g_new0 (gid_t, n);

		n = getgroups (n, groups);
		for (i = 0; i < n; i++) {
			if (info->gid == groups[i]) {
				/* ok */
				gnome_vfs_file_info_unref (info);
				return TRUE;
			}
		}
	}

	/* no more perimission stuff to try */
	gnome_vfs_file_info_unref (info);
	return FALSE;
}

gboolean
panel_is_uri_writable (const char *uri)
{
	g_return_val_if_fail (uri != NULL, FALSE);

	return internal_panel_is_uri_writable (uri, TRUE /* recurse */);
}

gboolean
panel_uri_exists (const char *uri)
{
	GnomeVFSURI *suri;
	gboolean ret;

	g_return_val_if_fail (uri != NULL, FALSE);

	suri = gnome_vfs_uri_new (uri);

	if (!suri) {
		return FALSE;
	}

	ret = gnome_vfs_uri_exists (suri);

	gnome_vfs_uri_unref (suri);

	return ret;
}

char *
panel_util_icon_remove_extension (const char *icon)
{
	char *icon_no_extension;
	char *p;

	icon_no_extension = g_strdup (icon);
	p = strrchr (icon_no_extension, '.');
	if (p &&
	    (strcmp (p, ".png") == 0 ||
	     strcmp (p, ".xpm") == 0 ||
	     strcmp (p, ".svg") == 0)) {
	    *p = 0;
	}

	return icon_no_extension;
}

char *
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
	icon_no_extension = panel_util_icon_remove_extension (icon_name);

	info = gtk_icon_theme_lookup_icon (icon_theme, icon_no_extension,
					   size, 0);

	g_free (icon_no_extension);

	if (info) {
		retval = g_strdup (gtk_icon_info_get_filename (info));
		gtk_icon_info_free (info);
	} else
		retval = NULL;

	return retval;
}

GdkPixbuf *
panel_load_icon (GtkIconTheme  *icon_theme,
		 const char    *icon_name,
		 int            size,
		 int            desired_width,
		 int            desired_height,
		 char         **error_msg)
{
	GdkPixbuf *retval;
	char      *file;
	GError    *error;

	g_return_val_if_fail (error_msg == NULL || *error_msg == NULL, NULL);

	file = panel_find_icon (icon_theme, icon_name, size);
	if (!file) {
		if (error_msg)
			*error_msg = g_strdup_printf (_("Icon '%s' not found"),
						      icon_name);

		return NULL;
	}

	error = NULL;
	retval = gdk_pixbuf_new_from_file_at_size (file,
						   desired_width,
						   desired_height,
						   &error);
	if (error) {
		if (error_msg)
			*error_msg = g_strdup (error->message);
		g_error_free (error);
	}

	g_free (file);

	return retval;
}

static char *
panel_lock_screen_action_get_command (const char *action)
{
	char    *command          = NULL;
	gboolean use_gscreensaver = FALSE;

	if (panel_is_program_in_path ("gnome-screensaver-command")
	    && panel_is_program_in_path ("gnome-screensaver-preferences"))
		use_gscreensaver = TRUE;
	else if (!panel_is_program_in_path ("xscreensaver-command"))
		return NULL;

	if (strcmp (action, "prefs") == 0) {
		if (use_gscreensaver) {
			command = g_strdup ("gnome-screensaver-preferences");
		} else if (panel_is_program_in_path ("xscreensaver-demo")) {
			command = g_strdup ("xscreensaver-demo");
		} else {
			command = NULL;
		}
	} else if (strcmp (action, "activate") == 0
		   || strcmp (action, "lock") == 0) {
		/* Neither gnome-screensaver or xscreensaver allow root
		 * to lock the screen */
		if (geteuid () == 0) {
			command = NULL;
		} else {
			if (use_gscreensaver) {
				command = g_strdup_printf ("gnome-screensaver-command --%s", action);
			} else {
				command = g_strdup_printf ("xscreensaver-command -%s", action);
			}
		}
	}

	return command;
}

gboolean
panel_lock_screen_action_available (const char *action)
{
	char    *command;
	gboolean enabled = FALSE;

	g_return_val_if_fail (action != NULL, FALSE);

	command = panel_lock_screen_action_get_command (action);
	if (command)
		enabled = TRUE;

	g_free (command);

	return enabled;
}

void
panel_lock_screen_action (GdkScreen  *screen,
			  const char *action)
{
	GError  *error            = NULL;
	char    *command          = NULL;

	g_return_if_fail (GDK_IS_SCREEN (screen));
	g_return_if_fail (action != NULL);

	command = panel_lock_screen_action_get_command (action);

	if (!command)
		return;

	if (!gdk_spawn_command_line_on_screen (screen, command, &error)) {
		char *primary;

		primary = g_strdup_printf (_("Could not execute '%s'"),
					   command);
		panel_error_dialog (NULL, screen,
				    "cannot_exec_screensaver", TRUE,
				    primary, error->message);
		g_free (primary);
		g_error_free (error);
	}

	g_free (command);
}

void
panel_lock_screen (GdkScreen *screen)
{
	panel_lock_screen_action (screen, "lock");
}


char *
panel_make_full_path (const char *dir,
		      const char *filename)
{
	char *retval;
	char *freeme = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	if (!dir) {
		freeme = gnome_util_home_file (PANEL_LAUNCHERS_PATH);
		dir = freeme;
	}

	/* Make sure the launcher directory exists */
	if (!g_file_test (dir, G_FILE_TEST_EXISTS))
		panel_ensure_dir (dir);

	retval = g_build_filename (dir, filename, NULL);

	g_free (freeme);

	return retval;
}

char *
panel_make_unique_desktop_path_from_name (const char *dir,
					  const char *name)
{
	int   num = 1;
	char *path = NULL;
	char  filename[MAXNAMLEN];

/* g_file_set_contents() use "%s.XXXXXX"
 * FIXME: waiting for http://bugzilla.gnome.org/show_bug.cgi?id=437977 */
#define LENGTH_FOR_TMPFILE_EXT 7

	g_snprintf (filename,
		    sizeof (filename) - strlen (".desktop") - LENGTH_FOR_TMPFILE_EXT,
		    "%s", name);
	g_strlcat (filename, ".desktop", sizeof (filename));
	path = panel_make_full_path (dir, filename);
	if (!g_file_test (path, G_FILE_TEST_EXISTS))
		return path;
	g_free (path);

	while (TRUE) {
		char *buf;

		buf = g_strdup_printf ("-%d.desktop", num);
		g_snprintf (filename,
			    sizeof (filename) - strlen (buf) - LENGTH_FOR_TMPFILE_EXT,
			    "%s", name);
		g_strlcat (filename, buf, sizeof (filename));
		g_free (buf);

		path = panel_make_full_path (dir, filename);
		if (!g_file_test (path, G_FILE_TEST_EXISTS))
			return path;
		g_free (path);

		num++;
	}

	return NULL;
}

char *
panel_make_unique_desktop_uri (const char *dir,
			       const char *source)
{
	char     *name, *p;
	char     *uri;
	char     *path = NULL;

	/* Accept NULL source. Using an emptry string makes our life easier
	 * than keeping NULL. */
	if (!source)
		source = "";

	/* source may be an exec string, a path, or a URI. We truncate
	 * it at the first space (to get just the command name if it's
	 * an exec string), strip the path/URI, and remove the suffix
	 * if it's ".desktop".
	 */
	name = g_strndup (source, strcspn (source, " "));
	p = strrchr (name, '/');
	while (p && !*(p + 1)) {
		*p = '\0';
		p = strrchr (name, '/');
	}
	if (p)
		memmove (name, p + 1, strlen (p + 1) + 1);
	p = strrchr (name, '.');
	if (p && !strcmp (p, ".desktop")) {
		*p = '\0';

		/* also remove the -%d that might be at the end of the name */
		p = strrchr (name, '-');
		if (p) {
			char *end;
			strtol ((p + 1), &end, 10);
			if (!*end)
				*p = '\0';
		}
	}

	if (name[0] == '\0') {
		g_free (name);
		name = g_strdup (_("file"));
	}

	path = panel_make_unique_desktop_path_from_name (dir, name);
	g_free (name);

	uri = gnome_vfs_get_uri_from_local_path (path);
	g_free (path);

	return uri;
}

static char *
lookup_in_data_dir (const char *basename,
                    const char *data_dir)
{
	char *path;

	path = g_build_filename (data_dir, basename, NULL);
	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		g_free (path);
		return NULL;
	}

	return path;
}

char *
panel_lookup_in_data_dirs (const char *basename)
{
	const char * const *system_data_dirs;
	const char          *user_data_dir;
	char                *retval;
	int                  i;

	user_data_dir    = g_get_user_data_dir ();
	system_data_dirs = g_get_system_data_dirs ();

	if ((retval = lookup_in_data_dir (basename, user_data_dir)))
		return retval;

	for (i = 0; system_data_dirs[i]; i++)
		if ((retval = lookup_in_data_dir (basename, system_data_dirs[i])))
			return retval;

	return NULL;
}

/* Stolen from evolution-data-server/libedataserver/e-util.c:
 * e_util_unicode_get_utf8()
 * e_util_utf8_strstrcase() */
static char *
panel_util_unicode_get_utf8 (const char *text, gunichar *out)
{
	*out = g_utf8_get_char (text);
	return (*out == (gunichar)-1) ? NULL : g_utf8_next_char (text);
}

const char *
panel_util_utf8_strstrcase (const char *haystack, const char *needle)
{
	gunichar *nuni;
	gunichar unival;
	gint nlen;
	const char *o, *p;

	if (haystack == NULL) return NULL;
	if (needle == NULL) return NULL;
	if (strlen (needle) == 0) return haystack;
	if (strlen (haystack) == 0) return NULL;

	nuni = g_alloca (sizeof (gunichar) * strlen (needle));

	nlen = 0;
	for (p = panel_util_unicode_get_utf8 (needle, &unival); p && unival; p = panel_util_unicode_get_utf8 (p, &unival)) {
		nuni[nlen++] = g_unichar_tolower (unival);
	}
	/* NULL means there was illegal utf-8 sequence */
	if (!p) return NULL;

	o = haystack;
	for (p = panel_util_unicode_get_utf8 (o, &unival); p && unival; p = panel_util_unicode_get_utf8 (p, &unival)) {
		gint sc;
		sc = g_unichar_tolower (unival);
		/* We have valid stripped char */
		if (sc == nuni[0]) {
			const char *q = p;
			gint npos = 1;
			while (npos < nlen) {
				q = panel_util_unicode_get_utf8 (q, &unival);
				if (!q || !unival) return NULL;
				sc = g_unichar_tolower (unival);
				if (sc != nuni[npos]) break;
				npos++;
			}
			if (npos == nlen) {
				return o;
			}
		}
		o = p;
	}

	return NULL;
}

GdkPixbuf *
panel_util_cairo_rgbdata_to_pixbuf (unsigned char *data,
				    int            width,
				    int            height)
{
	GdkPixbuf     *retval;
	unsigned char *dstptr;
	unsigned char *srcptr;
	int            align;

	g_assert (width > 0 && height > 0);

	if (!data)
		return NULL;

	retval = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
	if (!retval)
		return NULL;

	dstptr = gdk_pixbuf_get_pixels (retval);
	srcptr = data;
	align  = gdk_pixbuf_get_rowstride (retval) - (width * 3);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
/* cairo == 00RRGGBB */
#define CAIRO_RED 2
#define CAIRO_GREEN 1
#define CAIRO_BLUE 0
#else
/* cairo == BBGGRR00 */
#define CAIRO_RED 1
#define CAIRO_GREEN 2
#define CAIRO_BLUE 3
#endif

	while (height--) {
		int x = width;
		while (x--) {
			/* pixbuf == BBGGRR */
			dstptr[0] = srcptr[CAIRO_RED];
			dstptr[1] = srcptr[CAIRO_GREEN];
			dstptr[2] = srcptr[CAIRO_BLUE];

			dstptr += 3;
			srcptr += 4;
		}

		dstptr += align;
	}
#undef CAIRO_RED
#undef CAIRO_GREEN
#undef CAIRO_BLUE

	return retval;
}

GKeyFile *
panel_util_key_file_new_desktop (void)
{
	GKeyFile *retval;

	retval = g_key_file_new ();

	//FIXME? g_key_file_set_string (retval, "Desktop Entry", "Name", _("No Name"));
	g_key_file_set_string (retval, "Desktop Entry", "Encoding", "UTF-8");
	g_key_file_set_string (retval, "Desktop Entry", "Version", "1.0");

	return retval;
}

//FIXME: kill this when bug #309224 is fixed
gboolean
panel_util_key_file_to_file (GKeyFile     *keyfile,
			     const gchar  *file,
			     GError      **error)
{
	gchar   *filename;
	GError  *write_error;
	gchar   *data;
	gsize    length;
	gboolean res;

	g_return_val_if_fail (keyfile != NULL, FALSE);
	g_return_val_if_fail (file != NULL, FALSE);

	write_error = NULL;
	data = g_key_file_to_data (keyfile, &length, &write_error);
	if (write_error) {
		g_propagate_error (error, write_error);
		return FALSE;
	}

	if (!g_path_is_absolute (file))
		filename = g_filename_from_uri (file, NULL, &write_error);
	else
		filename = g_filename_from_utf8 (file, -1, NULL, NULL,
						 &write_error);

	if (write_error) {
		g_propagate_error (error, write_error);
		g_free (data);
		return FALSE;
	}

	res = g_file_set_contents (filename, data, length, &write_error);
	g_free (filename);

	if (write_error) {
		g_propagate_error (error, write_error);
		g_free (data);
		return FALSE;
	}

	g_free (data);
	return res;
}

gboolean
panel_util_key_file_load_from_uri (GKeyFile       *keyfile,
                                   const gchar    *uri,
				   GKeyFileFlags   flags,
				   GError        **error)
{
	char     *scheme;
	gboolean  is_local;
	gboolean  result;

	g_return_val_if_fail (keyfile != NULL, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	scheme = gnome_vfs_get_uri_scheme (uri);
	is_local = (scheme == NULL) || !g_ascii_strcasecmp (scheme, "file");
	g_free (scheme);

	if (is_local) {
		char *path;

		if (g_path_is_absolute (uri))
			path = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);
		else
			path = g_filename_from_uri (uri, NULL, NULL);
		result = g_key_file_load_from_file (keyfile, path,
						    flags, error);
		g_free (path);
	} else {
		GnomeVFSResult  vfs_result;
		int             size;
		char           *contents;

		vfs_result = gnome_vfs_read_entire_file (uri, &size, &contents);
		if (vfs_result != GNOME_VFS_OK)
			return FALSE;

		result = g_key_file_load_from_data (keyfile, contents, size,
						    flags, error);

		g_free (contents);
	}

	return result;
}

gboolean
panel_util_key_file_get_boolean (GKeyFile       *keyfile,
				 const gchar    *key,
				 gboolean        default_value)
{
	GError   *error;
	gboolean  retval;

	error = NULL;
	retval = g_key_file_get_boolean (keyfile, "Desktop Entry", key, &error);
	if (error != NULL) {
		retval = default_value;
		g_error_free (error);
	}

	return retval;
}

void
panel_util_key_file_set_locale_string (GKeyFile    *keyfile,
				       const gchar *key,
				       const gchar *value)
{
	const char         *locale;
	const char * const *langs_pointer;
	int                 i;

	locale = NULL;
	langs_pointer = g_get_language_names ();
	for (i = 0; langs_pointer[i] != NULL; i++) {
		/* find first without encoding  */
		if (strchr (langs_pointer[i], '.') == NULL) {
			locale = langs_pointer[i]; 
			break;
		}
	}

	if (locale)
		g_key_file_set_locale_string (keyfile, "Desktop Entry",
					      key, locale, value);
	else
		g_key_file_set_string (keyfile, "Desktop Entry",
				       key, value);
}

void
panel_util_key_file_remove_locale_key (GKeyFile    *keyfile,
				       const gchar *key)
{
	const char * const *langs_pointer;
	int                 i;
	char               *locale_key;

	locale_key = NULL;
	langs_pointer = g_get_language_names ();
	for (i = 0; langs_pointer[i] != NULL; i++) {
		/* find first without encoding  */
		if (strchr (langs_pointer[i], '.') == NULL) {
			locale_key = g_strdup_printf ("%s[%s]",
						      key, langs_pointer[i]);
			if (g_key_file_has_key (keyfile, "Desktop Entry",
						locale_key, NULL))
				break;

			g_free (locale_key);
			locale_key = NULL;
		}
	}

	if (locale_key) {
		g_key_file_remove_key (keyfile, "Desktop Entry",
				       locale_key, NULL);
		g_free (locale_key);
	} else
		g_key_file_remove_key (keyfile, "Desktop Entry",
				       key, NULL);
}

void
panel_util_key_file_remove_all_locale_key (GKeyFile    *keyfile,
					   const gchar *key)
{
	char **keys;
	int    key_len;
	int    i;

	if (!key)
		return;

	keys = g_key_file_get_keys (keyfile, "Desktop Entry", NULL, NULL);
	if (!keys)
		return;

	key_len = strlen (key);

	for (i = 0; keys[i] != NULL; i++) {
		int len;

		if (strncmp (keys[i], key, key_len))
			continue;

		len = strlen (keys[i]);
		if (len == key_len ||
		    (len > key_len && keys[i][key_len] == '['))
			g_key_file_remove_key (keyfile, "Desktop Entry",
					       keys[i], NULL);
	}

	g_strfreev (keys);
}

void
panel_util_key_file_ensure_C_key (GKeyFile   *keyfile,
				  const char *key)
{
	char *C_value;
	char *buffer;

	/* Make sure we set the "C" locale strings to the terms we set here.
	 * This is so that if the user logs into another locale they get their
	 * own description there rather then empty. It is not the C locale
	 * however, but the user created this entry herself so it's OK */
	C_value = panel_util_key_file_get_string (keyfile, key);
	if (C_value == NULL || C_value [0] == '\0') {
		buffer = panel_util_key_file_get_locale_string (keyfile, key);
		if (buffer) {
			panel_util_key_file_set_string (keyfile, key, buffer);
			g_free (buffer);
		}
	}
	g_free (C_value);
}

char *
guess_icon_from_exec (GtkIconTheme *icon_theme,
		      GKeyFile     *key_file)
{
	char *exec;
	char *icon_name;
	char *path;

	exec = panel_util_key_file_get_string (key_file, "Exec");
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

	return icon_name;
}

/* This is nautilus_get_vfs_method_display_name() */
const char *
panel_util_get_vfs_method_display_name (const char *method)
{
	//FIXME: /apps/nautilus/desktop/computer_icon_visible (same for trash and network)
	if (g_ascii_strcasecmp (method, "computer") == 0 ) {
		return _("Computer");
	} else if (g_ascii_strcasecmp (method, "network") == 0 ) {
		return _("Network");
	} else if (g_ascii_strcasecmp (method, "fonts") == 0 ) {
		return _("Fonts");
	} else if (g_ascii_strcasecmp (method, "themes") == 0 ) {
		return _("Themes");
	} else if (g_ascii_strcasecmp (method, "burn") == 0 ) {
		return _("CD/DVD Creator");
	} else if (g_ascii_strcasecmp (method, "smb") == 0 ) {
		return _("Windows Network");
	} else if (g_ascii_strcasecmp (method, "dns-sd") == 0 ) {
		/* translators: this is the title of the "dns-sd:///" location */
		return _("Services in");
	}
	return NULL;
}

static const char *
panel_util_get_icon_for_uri_method (const char *uri)
{
	if (g_str_has_prefix (uri, "computer:")) {
		return PANEL_ICON_COMPUTER;
	} else if (g_str_has_prefix (uri, "network:")) {
		return PANEL_ICON_NETWORK;
	} else if (g_str_has_prefix (uri, "fonts:")) {
		return PANEL_ICON_FONTS;
	} else if (g_str_has_prefix (uri, "themes:")) {
		return PANEL_ICON_THEME;
	} else if (g_str_has_prefix (uri, "burn:")) {
		return PANEL_ICON_BURNER;
	} else if (g_str_has_prefix (uri, "smb:")) {
		return PANEL_ICON_NETWORK;
	} else if (g_str_has_prefix (uri, "dns-sd:")) {
		return PANEL_ICON_NETWORK;
	} else if (g_str_has_prefix (uri, "trash:")) {
		//FIXME: we should look if the trash is empty or not
		return PANEL_ICON_TRASH;
	} else if (g_str_has_prefix (uri, "x-nautilus-search:")) {
		return PANEL_ICON_SEARCHTOOL;
	} else
		return NULL;
}

static const char *
panel_util_get_icon_for_uri_known_folders (const char *uri)
{
	const char *icon;
	char       *path;
	int         len;

	icon = NULL;
	path = gnome_vfs_get_local_path_from_uri (uri);

	len = strlen (path);
	if (path[len] == '/')
		path[len] = '\0';

	if (strcmp (path, g_get_home_dir ()) == 0)
		icon = PANEL_ICON_HOME;
	else if (strcmp (path,
			 g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP)) == 0)
		icon = PANEL_ICON_DESKTOP;

	g_free (path);

	return icon;
}

/* This is based on nautilus_compute_title_for_uri() and
 * nautilus_file_get_display_name_nocopy() */
char *
panel_util_get_label_for_uri (const char *text_uri)
{
	GnomeVFSURI *uri;
	const char  *hostname;
	const char  *method;
	char        *displayname;
	char        *label;
	char        *buffer;

	//FIXME: we're not handling $HOME and $Desktop
	hostname = NULL;
	label = NULL;

	//FIXME: nautilus uses nautilus_query_to_readable_string() to have a nice name
	if (g_str_has_prefix (text_uri, "x-nautilus-search:"))
		return g_strdup (_("Search"));

	if (g_str_has_prefix (text_uri, "trash:"))
		return g_strdup (_("Trash"));

	if (g_str_has_prefix (text_uri, "file:")) {
		buffer = gnome_vfs_get_local_path_from_uri (text_uri);
		if (!buffer)
			return NULL;

		label = g_filename_display_basename (buffer);
		g_free (buffer);

		return label;
	}

	uri = gnome_vfs_uri_new (text_uri);
	if (uri) {
		char *short_name;

		hostname = gnome_vfs_uri_get_host_name (uri);

		buffer = gnome_vfs_uri_extract_short_path_name (uri);
		short_name = gnome_vfs_unescape_string_for_display (buffer);
		g_free (buffer);

		if (strcmp (short_name, GNOME_VFS_URI_PATH_STR) != 0) {
			displayname = short_name;
		} else {
			g_free (short_name);
			method = panel_util_get_vfs_method_display_name (uri->method_string);
			if (method == NULL)
				method = uri->method_string;

			buffer = gnome_vfs_uri_extract_short_name (uri);
			short_name = gnome_vfs_unescape_string_for_display (buffer);
			g_free (buffer);

			if (short_name == NULL ||
			    strcmp (short_name, GNOME_VFS_URI_PATH_STR) == 0) {
				displayname = g_strdup (method);
			} else {
				displayname = g_strdup_printf ("%s: %s",
							       method,
							       short_name);
			}
			g_free (short_name);
		}
	} else {
		displayname = gnome_vfs_format_uri_for_display (text_uri);
	}

	if (hostname) {
		/* Translators: the first string is a path and the second
		 * string is a hostname. nautilus contains the same string to
		 * translate. */
		label = g_strdup_printf (_("%1$s on %2$s"), displayname, hostname);
		g_free (displayname);
	} else {
		label = displayname;
	}

	if (uri)
		gnome_vfs_uri_unref (uri);

	return label;
}

char *
panel_util_get_icon_for_uri (const char *text_uri)
{
	const char *icon;

	icon = panel_util_get_icon_for_uri_method (text_uri);
	if (icon)
		return g_strdup (icon);

	if (!g_str_has_prefix (text_uri, "file:"))
		return NULL;

	icon = panel_util_get_icon_for_uri_known_folders (text_uri);
	if (icon)
		return g_strdup (icon);

	return gnome_icon_lookup_sync (gtk_icon_theme_get_default (),
				       NULL, text_uri, NULL,
				       GNOME_ICON_LOOKUP_FLAGS_NONE,
				       GNOME_ICON_LOOKUP_RESULT_FLAGS_NONE);
}
