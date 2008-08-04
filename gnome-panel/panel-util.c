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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libgnome/gnome-desktop-item.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-help.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-keyfile.h>

#include "applet.h"
#include "nothing.h"
#include "xstuff.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "panel-globals.h"
#include "launcher.h"
#include "panel-icon-names.h"
#include "panel-lockdown.h"

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

		value = panel_key_file_get_string (keyfile, keys [i]);
		if (value != NULL) {
			gnome_desktop_item_set_string (ditem, keys [i], value);
			g_free (value);
		}
	}

	for (i = 0; i < G_N_ELEMENTS (locale_keys); i++) {
		char *value;

		value = panel_key_file_get_locale_string (keyfile,
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
		if (g_mkdir (parsed, 0700) != 0 &&
		    errno != EEXIST && errno != ENOSYS) {
			g_free (parsed);
			return FALSE;
		}
		*p = '/';
		p = strchr (p+1, '/');
	}

	if (g_mkdir (parsed, 0700) != 0 &&
	    errno != EEXIST && errno != ENOSYS) {
		g_free (parsed);
		return FALSE;
	}

	g_free (parsed);
	return TRUE;
}

gboolean
panel_is_uri_writable (const char *uri)
{
	GFile     *file;
	GFileInfo *info;
	gboolean   retval;

	g_return_val_if_fail (uri != NULL, FALSE);

	retval = FALSE;

	file = g_file_new_for_uri (uri);

	if (!g_file_query_exists (file, NULL)) {
		GFile *parent;

		parent = g_file_get_parent (file);
		g_object_unref (file);

		if (!g_file_query_exists (parent, NULL)) {
			g_object_unref (parent);
			return FALSE;
		}

		file = parent;
	}

	info = g_file_query_info (file, "access::*",
				  G_FILE_QUERY_INFO_NONE, NULL, NULL);
	g_object_unref (file);

	if (info) {
		retval = g_file_info_get_attribute_boolean (info,
							    G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
		g_object_unref (info);
	}

	return retval;
}

gboolean
panel_uri_exists (const char *uri)
{
	GFile *suri;
	gboolean ret;

	g_return_val_if_fail (uri != NULL, FALSE);

	suri = g_file_new_for_uri (uri);
	ret = g_file_query_exists (suri, NULL);
	g_object_unref (suri);

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

	if (strcmp (action, "prefs") != 0 &&
	    panel_lockdown_get_disable_lock_screen ())
		return FALSE;

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

	if (strcmp (action, "prefs") != 0 &&
	    panel_lockdown_get_disable_lock_screen ())
		return;

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

	uri = g_filename_to_uri (path, NULL, NULL);
	g_free (path);

	return uri;
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

char *
panel_util_get_icon_name_from_g_icon (GIcon *gicon)
{
	const char * const *names;
	GtkIconTheme *icon_theme;
	int i;

	if (!G_IS_THEMED_ICON (gicon))
		return NULL;

	names = g_themed_icon_get_names (G_THEMED_ICON (gicon));
	icon_theme = gtk_icon_theme_get_default ();

	for (i = 0; names[i] != NULL; i++) {
		if (gtk_icon_theme_has_icon (icon_theme, names[i]))
			return g_strdup (names[i]);
	}

	return NULL;
}

GdkPixbuf *
panel_util_get_pixbuf_from_g_loadable_icon (GIcon *gicon,
					    int    size)
{
	GdkPixbuf    *pixbuf;
	GInputStream *stream;

	if (!G_IS_LOADABLE_ICON (gicon))
		return NULL;

	pixbuf = NULL;

	stream = g_loadable_icon_load (G_LOADABLE_ICON (gicon),
				       size,
				       NULL, NULL, NULL);
	if (stream) {
		pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
		g_object_unref (stream);
	}

	if (pixbuf) {
		gint width, height;

		width = gdk_pixbuf_get_width (pixbuf);
		height = gdk_pixbuf_get_height (pixbuf);

		if (width > size || height > size) {
			GdkPixbuf *tmp;

			tmp = gdk_pixbuf_scale_simple (pixbuf, size, size,
						       GDK_INTERP_BILINEAR);

			g_object_unref (pixbuf);
			pixbuf = tmp;
		}
	}

	return pixbuf;
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

#define HOME_NAME_KEY           "/apps/nautilus/desktop/home_icon_name"
static char *
panel_util_get_file_display_for_common_files (GFile *file)
{
	GFile *compare;

	compare = g_file_new_for_path (g_get_home_dir ());
	if (g_file_equal (file, compare)) {
		char *gconf_name;

		g_object_unref (compare);

		gconf_name = gconf_client_get_string (panel_gconf_get_client (),
						      HOME_NAME_KEY,
						      NULL);
		if (PANEL_GLIB_STR_EMPTY (gconf_name)) {
			g_free (gconf_name);
			return g_strdup (_("Home Folder"));
		} else {
			return gconf_name;
		}
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
	 *  + x-nautilus-search: URI
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

	//FIXME: see nautilus_query_to_readable_string() to have a nice name
	if (g_str_has_prefix (text_uri, "x-nautilus-search:"))
		return g_strdup (_("Search"));

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
	 *  + x-nautilus-search: URI
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

	if (g_str_has_prefix (text_uri, "x-nautilus-search:"))
		return g_strdup (PANEL_ICON_SAVED_SEARCH);
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

static gboolean
panel_util_query_tooltip_cb (GtkWidget  *widget,
			     gint        x,
			     gint        y,
			     gboolean    keyboard_tip,
			     GtkTooltip *tooltip,
			     const char *text)
{
	if (!panel_global_config_get_tooltips_enabled ())
		return FALSE;

	gtk_tooltip_set_text (tooltip, text);
	return TRUE;
}

void
panel_util_set_tooltip_text (GtkWidget  *widget,
			     const char *text)
{
        g_signal_handlers_disconnect_matched (widget,
					      G_SIGNAL_MATCH_FUNC,
					      0, 0, NULL,
					      panel_util_query_tooltip_cb,
					      NULL);

	if (PANEL_GLIB_STR_EMPTY (text)) {
		g_object_set (widget, "has-tooltip", FALSE, NULL);
		return;
	}

	g_object_set (widget, "has-tooltip", TRUE, NULL);
	g_signal_connect_data (widget, "query-tooltip",
			       G_CALLBACK (panel_util_query_tooltip_cb),
			       g_strdup (text), (GClosureNotify) g_free, 0);
}

/* This is similar to what g_file_new_for_commandline_arg() does, but
 * we end up with something relative to $HOME instead of the current working
 * directory */
GFile *
panel_util_get_file_optional_homedir (const char *location)
{
	GFile *file;
	char  *path;
	char  *scheme;

	if (g_path_is_absolute (location))
		return g_file_new_for_path (location);

	scheme = g_uri_parse_scheme (location);
	if (scheme) {
		file = g_file_new_for_uri (location);
		g_free (scheme);
		return file;
	}

	path = g_build_filename (g_get_home_dir (), location, NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	return file;
}
