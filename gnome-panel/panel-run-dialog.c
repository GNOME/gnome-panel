/*
 * panel-run-dialog.c:
 *
 * Copyright (C) 2003 Frank Worsley <fworsley@shaw.ca>
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

 * Authors:
 *	Frank Worsley <fworsley@shaw.ca>
 *
 * Based on code by:
 *	Havoc Pennington <hp@pobox.com>
 *      George Lebl <jirka@5z.com>
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-run-dialog.h"

#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <glade/glade-xml.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-entry.h>
#include <libgnomeui/gnome-url.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <gconf/gconf-client.h>

#include "nothing.h"
#include "panel-gconf.h"
#include "panel-util.h"
#include "panel-globals.h"
#include "panel-enums.h"
#include "panel-profile.h"
#include "panel-stock-icons.h"
#include "panel-multiscreen.h"
#include "menu.h"
#include "panel-lockdown.h"
#include "panel-xutils.h"

#define ICON_SIZE 48.0

typedef struct {
	GtkWidget        *run_dialog;

	GtkWidget        *main_box;
	GtkWidget        *program_list_box;

	GtkWidget        *gnome_entry;
	GtkWidget        *gtk_entry;
	GtkWidget        *pixmap;
	GtkWidget        *run_button;
	GtkWidget        *file_button;
	GtkWidget        *list_expander;
	GtkWidget        *terminal_checkbox;
	GtkWidget        *program_label;
	GtkWidget        *program_list;
	
	GtkListStore     *program_list_store;

	GList		 *completion_items;
	GCompletion      *completion;
	
	GSList           *add_icon_paths;
	int	          add_icons_idle_id;
	int	          add_items_idle_id;
	int		  find_command_icon_idle_id;
	int		  content_notify_id;
	gboolean	  use_program_list;
	gboolean	  completion_started;
	
	char		 *icon_path;
	char		 *item_name;	
} PanelRunDialog;

enum {
	COLUMN_ICON,
	COLUMN_ICON_FILE,
	COLUMN_NAME,
	COLUMN_COMMENT,
	COLUMN_PATH,
	COLUMN_EXEC,
	NUM_COLUMNS
};

static PanelRunDialog *static_dialog = NULL;

static void
panel_run_dialog_destroy (PanelRunDialog *dialog)
{
	GList *l;
	
	g_object_unref (dialog->program_list_box);
	
	g_slist_foreach (dialog->add_icon_paths, (GFunc) gtk_tree_path_free, NULL);
	g_slist_free (dialog->add_icon_paths);
	dialog->add_icon_paths = NULL;

	g_free (dialog->icon_path);
	g_free (dialog->item_name);

	if (dialog->add_icons_idle_id)
		g_source_remove (dialog->add_icons_idle_id);
	dialog->add_icons_idle_id = 0;

	if (dialog->add_items_idle_id)
		g_source_remove (dialog->add_items_idle_id);
	dialog->add_items_idle_id = 0;

	if (dialog->find_command_icon_idle_id)
		g_source_remove (dialog->find_command_icon_idle_id);
	dialog->find_command_icon_idle_id = 0;

	if (dialog->content_notify_id)
		gconf_client_notify_remove (panel_gconf_get_client (),
					    dialog->content_notify_id);
	dialog->content_notify_id = 0;

	for (l = dialog->completion_items; l; l = l->next)
		g_free (l->data);
	g_list_free (dialog->completion_items);
	dialog->completion_items = NULL;
	
	if (dialog->completion)
		g_completion_free (dialog->completion);
	dialog->completion = NULL;
	
	g_free (dialog);
}

static void
set_window_icon_from_stock (GtkWindow *window,
			    const char *stock_id)
{
	GdkPixbuf *icon;

	icon = gtk_widget_render_icon (GTK_WIDGET (window),
				       stock_id,
				       GTK_ICON_SIZE_DIALOG,
				       NULL);

	gtk_window_set_icon (window, icon);

	g_object_unref (icon);
}

static void
panel_run_dialog_set_icon (PanelRunDialog *dialog,
			   const char     *icon_path)
{
	GdkPixbuf *pixbuf = NULL;
	char      *icon = NULL;

	g_free (dialog->icon_path);
	dialog->icon_path = NULL;
		
	if (icon_path)
		icon = gnome_desktop_item_find_icon (panel_icon_theme,
						     icon_path,
						     ICON_SIZE /* desired size */,
						     0 /* flags */);
						     
	if (icon)
		pixbuf = gdk_pixbuf_new_from_file_at_size (icon, ICON_SIZE, ICON_SIZE, NULL);

                        
	if (pixbuf) {
		dialog->icon_path = icon;

		/* Don't bother scaling the image if it's too small.
		 * Scaled looks worse than a smaller image.
		 */
		gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->pixmap), pixbuf);

		gtk_window_set_icon (GTK_WINDOW (dialog->run_dialog), pixbuf);
		
		/* Don't unref pixbuf here, GTK will do it for us. */
		gtk_drag_source_set_icon_pixbuf (dialog->run_dialog, pixbuf);		
	} else {
		g_free (icon);
		
		gtk_image_set_from_stock (GTK_IMAGE (dialog->pixmap), 
					  PANEL_STOCK_RUN,
					  GTK_ICON_SIZE_DIALOG);
		
		set_window_icon_from_stock (GTK_WINDOW (dialog->run_dialog),
					    PANEL_STOCK_RUN);
		
		gtk_drag_source_set_icon_stock (dialog->run_dialog,
						PANEL_STOCK_RUN);
	}
}

static gboolean
command_is_executable (const char *command)
{
	gboolean   result;
	char     **argv;
	char      *path;
	int        argc;
	
	path = gnome_vfs_get_local_path_from_uri (command);
	if (!path)
		path = g_strdup (command);
	
	result = g_shell_parse_argv (path, &argc, &argv, NULL);
	g_free (path);
	
	if (!result)
		return FALSE;
	
	path = g_find_program_in_path (argv [0]);
	g_strfreev (argv);
			
	if (!path)
		return FALSE;
	
	/* If we pass an absolute path to g_find_program it just returns
	 * that absolute path without checking if it is executable. Also
	 * make sure its a regular file so we don't try to launch
	 * directories or device nodes.
	 */
	if (!g_file_test (path, G_FILE_TEST_IS_EXECUTABLE) ||
	    !g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
		g_free (path);
		return FALSE;
	}
	
	g_free (path);
	
	return TRUE;
}

static gboolean
panel_run_dialog_launch_command (PanelRunDialog *dialog,
				 const char     *command,
				 const char     *escaped)
{
	GdkScreen  *screen;
	gboolean    result;	
	GError     *error = NULL;
	char      **argv;
	int         argc;
	
	if (!command_is_executable (command))
		return FALSE;

	argc = 3;
	argv = g_new0 (char *, 4);
	argv [0] = gnome_util_user_shell ();
	argv [1] = g_strdup ("-c");
	argv [2] = g_strdup (command);
	
	screen = gtk_window_get_screen (GTK_WINDOW (dialog->run_dialog));	
		
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->terminal_checkbox)))
		gnome_prepend_terminal_to_vector (&argc, &argv);
		   
	result = gdk_spawn_on_screen (screen,
				      NULL, /* working directory */
				      argv,
				      NULL, /* envp */
				      G_SPAWN_SEARCH_PATH,
				      NULL, /* child setup func */
				      NULL, /* user data */
				      NULL, /* child pid */
				      &error);
			
	if (!result) {
		panel_error_dialog (screen, "cannot_spawn_command", TRUE,
				    _("Cannot launch command '%s'"),
				    "%s",
				    escaped,
				    error->message);

		g_error_free (error);
	}
				
	g_strfreev (argv);
	
	return result;
}

static gboolean
panel_run_dialog_show_url (PanelRunDialog *dialog,
	                   const char     *url,
	                   const char     *escaped)
{
	GError    *error = NULL;
	GdkScreen *screen;
	
	screen = gtk_window_get_screen (GTK_WINDOW (dialog->run_dialog));

	gnome_url_show_on_screen (url, screen, &error);
	if (error) {
		panel_error_dialog (screen, "cannot_show_url", TRUE,
				    _("Cannot display location '%s'"),
				    "%s",
				    escaped,
				    error->message);

		g_error_free (error);
		return FALSE;
	}
	
	return TRUE;
}

static void
panel_run_dialog_execute (PanelRunDialog *dialog)
{
	GdkScreen *screen;
	GError    *error;
	gboolean   result;
	char      *command;
	char      *escaped;
	char      *disk, *url;
	char      *scheme;	
	
	screen = gtk_window_get_screen (GTK_WINDOW (dialog->run_dialog));	
	command = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->gtk_entry)));
	command = g_strchug (command);

	if (!command || !command [0]) {
		g_free (command);
		return;
	}
	
	/* evil eggies, do not translate! */
	if (!strcmp (command, "free the fish")) {
		start_screen_check ();

		g_free (command);
		gtk_widget_destroy (dialog->run_dialog);
		return;
	} else if (!strcmp (command, "gegls from outer space")) {
		start_geginv ();

		g_free (command);
		gtk_widget_destroy (dialog->run_dialog);
		return;
	}
		
	error = NULL;
	disk = g_locale_from_utf8 (command, -1, NULL, NULL, &error);

	if (!disk || error) {
		panel_error_dialog (screen, "cannot_convert_command_from_utf8",
				    TRUE, _("Cannot convert '%s' from UTF-8"),
				    "%s",
				    command,
				    error->message);

		g_error_free (error);
		return;
	}

	url = gnome_vfs_make_uri_from_input_with_dirs (disk,
						       GNOME_VFS_MAKE_URI_DIR_HOMEDIR);
	escaped = g_markup_escape_text (url, -1);
	scheme = gnome_vfs_get_uri_scheme (url);
	result = FALSE;
	
	if (!strcasecmp (scheme, "http") ||
	    !strcasecmp (scheme, "file"))
		/* If this returns an http or file url, the url might refer to a
		 * command that is somewhere in the path or an executable file.
		 * So try executing it before displaying it. We execute the 
		 * command in the user's shell so that it can do all the parameter
		 * expansion and other magic for us.
		 */
		result = panel_run_dialog_launch_command (dialog, command, escaped);
	
	if (!result)
		result = panel_run_dialog_show_url (dialog, url, escaped);
		
	if (result) {
		/* only save working commands in history */
		gnome_entry_prepend_history (GNOME_ENTRY (dialog->gnome_entry),
					     TRUE, /* save item in history */
					     command);
		
		/* only close the dialog if we successfully showed or launched something */
		gtk_widget_destroy (dialog->run_dialog);
	}

	g_free (command);
	g_free (disk);
	g_free (url);
	g_free (escaped);
	g_free (scheme);
}

static void
panel_run_dialog_response (PanelRunDialog *dialog,
			   int             response,
			   GtkWidget      *run_dialog)
{

	dialog->completion_started = FALSE;

	switch (response) {
	case GTK_RESPONSE_OK:
		panel_run_dialog_execute (dialog);
		break;
	case GTK_RESPONSE_CANCEL:
		gtk_widget_destroy (dialog->run_dialog);
		break;
	case GTK_RESPONSE_HELP:
		panel_show_help (gtk_window_get_screen (GTK_WINDOW (run_dialog)),
				 "user-guide.xml", "gospanel-23");
		break;
	default:
		break;
	}
}

/* only quote the string if really needed */
static char *
quote_string (const char *s)
{
	const char *p;
	
	for (p = s; *p != '\0'; p++) {
		if ((*p >= 'a' && *p <= 'z') ||
		    (*p >= 'A' && *p <= 'Z') ||
		    (*p >= '0' && *p <= '9') ||
		    strchr ("-_./=:", *p) != NULL)
			;
		else
			return g_shell_quote (s);
	}
	
	return g_strdup (s);
}

static void
panel_run_dialog_append_file_utf8 (PanelRunDialog *dialog,
				   const char     *file)
{
	const char *text;
	char       *quoted, *temp;
	
	/* Don't allow filenames beginning with '-' */
	if (!file || !file[0] || file[0] == '-')
		return;
	
	quoted = quote_string (file);
	text = gtk_entry_get_text (GTK_ENTRY (dialog->gtk_entry));
	
	if (text && text [0]) {
		temp = g_strconcat (text, " ", quoted, NULL);
		gtk_entry_set_text (GTK_ENTRY (dialog->gtk_entry), temp);
		g_free (temp);
	} else
		gtk_entry_set_text (GTK_ENTRY (dialog->gtk_entry), quoted);
	
	g_free (quoted);
}

static void
panel_run_dialog_append_file (PanelRunDialog *dialog,
			      const char *file)
{
	char *utf8_file;
	
	if (!file)
		return;
	
	utf8_file = g_filename_to_utf8 (file, -1, NULL, NULL, NULL);
	
	if (utf8_file)
		panel_run_dialog_append_file_utf8 (dialog, utf8_file);
	
	g_free (utf8_file);
}

static gboolean
fuzzy_command_match (const char *cmd1,
		     const char *cmd2,
		     gboolean   *fuzzy)
{
	char **tokens;
	char  *word1, *word2;

	*fuzzy = FALSE;

	if (!strcmp (cmd1, cmd2))
		return TRUE;

	/* find basename of exec from desktop item.
	   strip of all arguments after the initial command */
	tokens = g_strsplit (cmd1, " ", -1);
	if (!tokens || !tokens [0]) {
		g_strfreev (tokens);
		return FALSE;
	}

	word1 = g_path_get_basename (tokens [0]);
	g_strfreev (tokens);

	/* same for the user command */
	tokens = g_strsplit (cmd2, " ", -1);
	word2 = g_path_get_basename (tokens [0]);
	if (!tokens || !tokens [0]) {
		g_free (word1);
		g_strfreev (tokens);
		return FALSE;
	}

	g_strfreev (tokens);

	if (!strcmp (word1, word2)) {
		g_free (word1);
		g_free (word2);
		*fuzzy = TRUE;
		return TRUE;
	}

	g_free (word1);
	g_free (word2);

	return FALSE;
}

static gboolean
panel_run_dialog_find_command_icon_idle (PanelRunDialog *dialog)
{
	GtkTreeIter   iter;
	GtkTreeModel *model;
	GtkTreePath  *path;
	const char   *text;
	char         *found_icon;
	char         *found_name;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->program_list));
	path = gtk_tree_path_new_first ();
	text = gtk_entry_get_text (GTK_ENTRY (dialog->gtk_entry));
	found_icon = NULL;
	found_name = NULL;
	
	if (!path || !gtk_tree_model_get_iter (model, &iter, path)) {
		if (path)
			gtk_tree_path_free (path);
		
		panel_run_dialog_set_icon (dialog, NULL);
	
		dialog->find_command_icon_idle_id = 0;
		return FALSE;
	}

	do {
		char *exec = NULL;
		char *icon = NULL;
		char *name = NULL;
		char *path = NULL;

		gtk_tree_model_get (model, &iter,
				    COLUMN_EXEC,      &exec,
				    COLUMN_ICON_FILE, &icon,
				    COLUMN_NAME,      &name,
				    COLUMN_PATH,      &path,
				    -1);

		if (!exec) {
			GnomeDesktopItem *item;

			if ((item = gnome_desktop_item_new_from_file (path, 0, NULL))) {
				exec = g_strdup (gnome_desktop_item_get_string (item, GNOME_DESKTOP_ITEM_EXEC));
				gnome_desktop_item_unref (item);
			}

			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COLUMN_EXEC, exec ? exec : "",
					    -1);
		}

        	if (exec && icon) {
			gboolean fuzzy = FALSE;

			if (fuzzy_command_match (sure_string (text), exec, &fuzzy)) {
				g_free (found_icon);
				g_free (found_name);
				
				found_icon = g_strdup (icon);
				found_name = g_strdup (name);
				
				if (!fuzzy) {
					/*
					 * if not fuzzy then we have a precise
					 * match and we can quit, else keep
					 * searching for a better match
					 */
					g_free (exec);
					g_free (icon);
					g_free (name);
					g_free (path);
					break;
				}
			}
		}

		g_free (exec);
		g_free (icon);
		g_free (name);
		g_free (path);
	
        } while (gtk_tree_model_iter_next (model, &iter));

	gtk_tree_path_free (path);

	panel_run_dialog_set_icon (dialog, found_icon);

	g_free (found_icon);
	
	g_free (dialog->item_name);
	dialog->item_name = found_name;
	
	dialog->find_command_icon_idle_id = 0;
	return FALSE;
}

static gboolean
panel_run_dialog_add_icon_idle (PanelRunDialog *dialog)
{
	GtkTreeIter  iter;
	GtkTreePath *path;
	GdkPixbuf   *pixbuf;
	char        *file;
	int          icon_height;
	gboolean     long_operation = FALSE;
	
	do {
		if (!dialog->add_icon_paths) {
			dialog->add_icons_idle_id = 0;
			return FALSE;
		}

		path = dialog->add_icon_paths->data;
		dialog->add_icon_paths->data = NULL;
		dialog->add_icon_paths = g_slist_delete_link (dialog->add_icon_paths,
						              dialog->add_icon_paths);

		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (dialog->program_list_store),
					      &iter,
					      path)) {
			gtk_tree_path_free (path);
			continue;
		}
		
		gtk_tree_path_free (path);

		gtk_tree_model_get (GTK_TREE_MODEL (dialog->program_list_store), &iter,
				    COLUMN_ICON_FILE, &file, -1);

		if (!gtk_icon_size_lookup (panel_menu_icon_get_size (), NULL, &icon_height)) {
			icon_height = PANEL_DEFAULT_MENU_ICON_SIZE;
		}

		pixbuf = panel_make_menu_icon (file, NULL, icon_height, &long_operation);
		if (pixbuf) {
			gtk_list_store_set (dialog->program_list_store, &iter, COLUMN_ICON, pixbuf, -1);
			g_object_unref (pixbuf);
		}
		g_free (file);
		
	/* don't go back into the main loop if this wasn't very hard to do */
	} while (!long_operation);

	return TRUE;
}

static int
compare_applications (MenuTreeEntry *a,
		      MenuTreeEntry *b)
{
	return g_utf8_collate (menu_tree_entry_get_name (a),
			       menu_tree_entry_get_name (b));
}

static GSList *
get_all_applications_from_dir (MenuTreeDirectory *directory,
			       GSList            *list)
{
	GSList *subdirs;
	GSList *l;

	list = g_slist_concat (list,
			       menu_tree_directory_get_entries (directory));

	subdirs = menu_tree_directory_get_subdirs (directory);
	for (l = subdirs; l; l = l->next) {
		MenuTreeDirectory *subdir = l->data;

		list = get_all_applications_from_dir (subdir, list);

		menu_tree_directory_unref (subdir);
	}
	g_slist_free (subdirs);

	return list;
}

static GSList *
get_all_applications (void)
{
	MenuTree          *tree;
	MenuTreeDirectory *root;
	GSList            *retval;

	tree = menu_tree_lookup ("applications.menu");

	root = menu_tree_get_root_directory (tree);

	retval = get_all_applications_from_dir (root, NULL);

	menu_tree_directory_unref (root);
	menu_tree_unref (tree);

	retval = g_slist_sort (retval,
			       (GCompareFunc) compare_applications);

	return retval;
}

static gboolean
panel_run_dialog_add_items_idle (PanelRunDialog *dialog)
{
	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;
	GSList            *all_applications;
	GSList            *l;
	GSList            *next;
	const char        *prev_name;

	/* create list store */
	dialog->program_list_store = gtk_list_store_new (NUM_COLUMNS,
							 GDK_TYPE_PIXBUF,
							 G_TYPE_STRING,
							 G_TYPE_STRING,
							 G_TYPE_STRING,
							 G_TYPE_STRING,
							 G_TYPE_STRING);

	all_applications = get_all_applications ();
	
	/* Strip duplicates */
	prev_name = NULL;
	for (l = all_applications; l; l = next) {
		MenuTreeEntry *entry = l->data;

		next = l->next;

		if (prev_name && strcmp (menu_tree_entry_get_name (entry), prev_name) == 0) {
			menu_tree_entry_unref (entry);

			all_applications = g_slist_delete_link (all_applications, l);
		} else {
			prev_name = menu_tree_entry_get_name (entry);
		}
	}

	for (l = all_applications; l; l = l->next) {
		MenuTreeEntry *entry = l->data;
		GtkTreeIter    iter;
		GtkTreePath   *path;

		gtk_list_store_append (dialog->program_list_store, &iter);
		gtk_list_store_set (dialog->program_list_store, &iter,
				    COLUMN_ICON,      NULL,
				    COLUMN_ICON_FILE, menu_tree_entry_get_icon (entry),
				    COLUMN_NAME,      menu_tree_entry_get_name (entry),
				    COLUMN_COMMENT,   menu_tree_entry_get_comment (entry),
				    COLUMN_PATH,      menu_tree_entry_get_desktop_file_path (entry),
				    -1);

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (dialog->program_list_store), &iter);
		if (path != NULL)
			dialog->add_icon_paths = g_slist_prepend (dialog->add_icon_paths, path);

		menu_tree_entry_unref (entry);
	}
	g_slist_free (all_applications);

	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->program_list), 
				 GTK_TREE_MODEL (dialog->program_list_store));

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
                                             "pixbuf", COLUMN_ICON,
                                             NULL);
        
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
                                             "text", COLUMN_NAME,
                                             NULL);
					          
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->program_list), column);

	dialog->add_icon_paths = g_slist_reverse (dialog->add_icon_paths);

	if (!dialog->add_icons_idle_id)
		dialog->add_icons_idle_id =
			g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc) panel_run_dialog_add_icon_idle,
					 dialog, NULL);

	dialog->add_items_idle_id = 0;					 
	return FALSE;
}

static char *
remove_parameters (const char *exec)
{
	GString *str;
	char    *retval, *p;

	str = g_string_new (exec);

	while ((p = strstr (str->str, "%"))) {
		switch (p [1]) {
		case '%':
			g_string_erase (str, p - str->str, 1);
			break;
		case 'U':
		case 'F':
		case 'N':
		case 'D':
		case 'f':
		case 'u':
		case 'd':
		case 'n':
		case 'm':
		case 'i':
		case 'c':
		case 'k':
		case 'v':
			g_string_erase (str, p - str->str, 2);
			break;
		default:
			break;
		}
	}

	retval = str->str;
	g_string_free (str, FALSE);

	return retval;
}

static void
program_list_selection_changed (GtkTreeSelection *selection,
				PanelRunDialog   *dialog)
{
	GnomeDesktopItem *ditem;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	const char       *temp;
	char             *path, *stripped;
	gboolean          terminal;
		
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	path = NULL;
	gtk_tree_model_get (model, &iter,
			    COLUMN_PATH, &path,
			    -1);
				  
	if (path) {
		ditem = gnome_desktop_item_new_from_file (path,
							  GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS,
							  NULL /* error */);
		if (ditem) {
			dialog->use_program_list = TRUE;
			
			/* Order is important here. We have to set the text first so that the
			 * drag source is enabled, otherwise the drag icon can't be set by
			 * panel_run_dialog_set_icon.
			 */
			temp = gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_EXEC);
			if (temp) {
				stripped = remove_parameters (temp);
				gtk_entry_set_text (GTK_ENTRY (dialog->gtk_entry), stripped);
				g_free (stripped);
			} else {
				temp = gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_URL);
				gtk_entry_set_text (GTK_ENTRY (dialog->gtk_entry), sure_string (temp));
			}

			temp = gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_ICON);
			panel_run_dialog_set_icon (dialog, temp);
			
			temp = gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_COMMENT);
			gtk_label_set_text (GTK_LABEL (dialog->program_label), sure_string (temp));
			
			terminal = gnome_desktop_item_get_boolean (ditem, GNOME_DESKTOP_ITEM_TERMINAL);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->terminal_checkbox),
						      terminal);

			if (dialog->item_name)
				g_free (dialog->item_name);

			dialog->item_name = g_strdup (gnome_desktop_item_get_string (
							      ditem,
							      GNOME_DESKTOP_ITEM_NAME));

			gnome_desktop_item_unref (ditem);
                }

		g_free (path);
        }
}

static void
program_list_selection_activated (GtkTreeView       *view,
				  GtkTreePath       *path,
				  GtkTreeViewColumn *column,
				  PanelRunDialog    *dialog)
{
	GtkTreeSelection *selection;

	/* update the entry with the info from the selection */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->program_list));	
	program_list_selection_changed (selection, dialog);
	
	/* now launch the command */
	gtk_dialog_response (GTK_DIALOG (dialog->run_dialog), GTK_RESPONSE_OK);
}


static void
panel_run_dialog_setup_program_list (PanelRunDialog *dialog,
				     GladeXML       *gui)
{
	GtkTreeSelection *selection;
	
	dialog->program_list = glade_xml_get_widget (gui, "program_list");
	dialog->program_list_box = glade_xml_get_widget (gui, "program_list_box");
	dialog->program_label = glade_xml_get_widget (gui, "program_label");
	dialog->main_box = glade_xml_get_widget (gui, "main_box");
	
	/* Ref the box so it doesn't get destroyed when it is
	 * removed from the visible area of the dialog box.
	 */
	g_object_ref (dialog->program_list_box);
	
	if (panel_profile_get_enable_program_list ()) {
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->program_list));
		gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	        g_signal_connect (selection, "changed",
				  G_CALLBACK (program_list_selection_changed),
				  dialog);

	        g_signal_connect (dialog->program_list, "row-activated",
				  G_CALLBACK (program_list_selection_activated),
				  dialog);

		/* start loading the list of applications */
		dialog->add_items_idle_id = 
			g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc) panel_run_dialog_add_items_idle, 
					 dialog, NULL);
	}
}

static void
panel_run_dialog_update_content (PanelRunDialog *dialog,
				 gboolean        show_list)
{
	if (!panel_profile_get_enable_program_list ()) {
		if (dialog->list_expander->parent)
			gtk_container_remove (GTK_CONTAINER (dialog->list_expander->parent),
					      dialog->list_expander);

		gtk_window_set_resizable (GTK_WINDOW (dialog->run_dialog), FALSE);
                gtk_widget_grab_focus (dialog->gtk_entry);
		
	} else if (show_list) {
		gtk_window_resize (GTK_WINDOW (dialog->run_dialog), 100, 300);
		gtk_window_set_resizable (GTK_WINDOW (dialog->run_dialog), TRUE);
		gtk_widget_grab_focus (dialog->program_list);
		
        } else if (!show_list) {
		gtk_window_set_resizable (GTK_WINDOW (dialog->run_dialog), FALSE);
                gtk_widget_grab_focus (dialog->gtk_entry);
        }
}

static void
panel_run_dialog_content_notify (GConfClient    *client,
				 int             notify_id,
				 GConfEntry     *entry,
				 PanelRunDialog *dialog)
{
	panel_run_dialog_update_content (dialog, gconf_value_get_bool (entry->value));
}

static void
list_expander_toggled (GtkExpander    *expander,
		       GParamSpec     *pspec,
		       PanelRunDialog *dialog)
{
	panel_profile_set_show_program_list (gtk_expander_get_expanded (expander));
}

static void
panel_run_dialog_setup_list_expander (PanelRunDialog *dialog,
				      GladeXML       *gui)
{
	GConfClient *client;
	const char *key;
	
	dialog->list_expander = glade_xml_get_widget (gui, "list_expander");

	if (panel_profile_get_enable_program_list ()) {
		gtk_expander_set_expanded (GTK_EXPANDER (dialog->list_expander),
					   panel_profile_get_show_program_list ());

		if ( ! panel_profile_is_writable_show_program_list ())
			gtk_widget_set_sensitive (dialog->list_expander, FALSE);
		
	        g_signal_connect (dialog->list_expander, "notify::expanded",
				  G_CALLBACK (list_expander_toggled),
				  dialog);
	
		client = panel_gconf_get_client ();
		key = panel_gconf_general_key ("show_program_list");
	
		dialog->content_notify_id =
			gconf_client_notify_add (client, key,
						 (GConfClientNotifyFunc) panel_run_dialog_content_notify,
						 dialog, NULL, NULL);
					 
		if (!dialog->content_notify_id)
			g_warning ("error setting up content change notification");
	}
}

static void
file_button_browse_response (GtkWidget      *chooser,
			     gint            response,
			     PanelRunDialog *dialog)
{
	char *file;
	
	if (response == GTK_RESPONSE_OK) {
		file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		panel_run_dialog_append_file (dialog, file);
		g_free (file);
	}

	gtk_widget_destroy (chooser);
 
 	gtk_widget_grab_focus (dialog->gtk_entry);
}

static void
file_button_clicked (GtkButton      *button,
		     PanelRunDialog *dialog)
{
	GtkWidget *chooser;

	chooser = gtk_file_chooser_dialog_new (_("Choose a file to append to the command..."),
					       GTK_WINDOW (dialog->run_dialog),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OK, GTK_RESPONSE_OK,
					       NULL);
	
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
					     g_get_home_dir ());
	
	gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_OK);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (chooser), TRUE);

	g_signal_connect (chooser, "response",
			  G_CALLBACK (file_button_browse_response), dialog);

	gtk_window_present (GTK_WINDOW (chooser));
}

static void
panel_run_dialog_setup_file_button (PanelRunDialog *dialog,
				    GladeXML       *gui)
{
	dialog->file_button = glade_xml_get_widget (gui, "file_button");
		
        g_signal_connect (dialog->file_button, "clicked",
			  G_CALLBACK (file_button_clicked),
			  dialog);
}

static GList *
fill_files_from (GList      *list,
		 const char *dirname,
		 const char *prefix,
		 GList      *existing_items)
{
	struct dirent *dent;
	DIR           *dir;
	
	dir = opendir (dirname);
	
	if (!dir)
		return list;
	
	while ((dent = readdir (dir))) {
		char          *file;
		char          *item;
		const char    *suffix;
		
		file = g_build_filename (dirname, dent->d_name, NULL);
		
		suffix = NULL;
		if (g_file_test (file, G_FILE_TEST_IS_DIR))
			suffix = "/";
		
		g_free (file);
		
		item = g_strconcat (prefix, dent->d_name, suffix, NULL);
		
		if (g_list_find_custom (existing_items, item, (GCompareFunc) strcmp)) {
			g_free (item);
			closedir (dir);
			return list;
		}
		
		list = g_list_prepend (list, item);
	}

	closedir (dir);
	
	return list;
}	

static GList *
fill_executables_from (GList      *list,
		       const char *dirname)
{
	struct dirent *dent;
	DIR           *dir;
	char          *file;
	struct stat    file_stat;

	dir = opendir (dirname);

	if (!dir)
		return list;

	while ((dent = readdir (dir))) {
		file = g_build_filename (dirname, dent->d_name, NULL);

		if (!access (file, X_OK) &&
		    !stat (file, &file_stat) && !S_ISDIR (file_stat.st_mode))
			list = g_list_prepend (list, g_strdup (dent->d_name));
		
		g_free (file);
	}

	closedir (dir);
	
	return list;
}

static GList *
fill_executables (GList *list)
{
	const char  *path;
	char       **pathv;
	int          i;

	path = g_getenv ("PATH");

	if (!path || !path [0])
		return list;

	pathv = g_strsplit (path, ":", 0);

	for (i = 0; pathv [i]; i++)
		list = fill_executables_from (list, pathv [i]);
	
	g_strfreev (pathv);
	
	return list;
}

static void
panel_run_dialog_ensure_completion (PanelRunDialog *dialog)
{
	if (!dialog->completion) {
		dialog->completion = g_completion_new (NULL);
		
		dialog->completion_items = fill_executables (NULL);
		dialog->completion_items = fill_files_from  (dialog->completion_items, "/", "/", NULL);
		dialog->completion_items = fill_files_from  (dialog->completion_items, g_get_home_dir(), "", NULL);
		
		g_completion_add_items (dialog->completion,
					dialog->completion_items);
	}
}

static void
panel_run_dialog_update_completion (PanelRunDialog *dialog,
				    const char     *text)
{
	g_assert (dialog->completion != NULL);
	
	if (text [strlen (text) - 1] == '/') {
		const char *dirname;
		char       *freeme;
		GList      *list;
		
		dirname = text;
		freeme = NULL;
		
		if (dirname [0] != '/')
			dirname = freeme = g_build_filename(g_get_home_dir(), text, NULL);
		
		list = fill_files_from (NULL, dirname, text, dialog->completion_items);
		
		if (freeme)
			g_free (freeme);
		
		if (list == NULL)
			return;
		
		g_completion_add_items (dialog->completion, list);
		
		dialog->completion_items = g_list_concat (dialog->completion_items, list);
	}
}

static gboolean
entry_event (GtkEditable    *entry,
	     GdkEventKey    *event,
	     PanelRunDialog *dialog)
{
	char *prefix;
	char *nprefix;
	char *temp;
	int   pos, tmp;

	/* if user typed something we're not using the list anymore */
	if (event->type == GDK_KEY_PRESS)
		dialog->use_program_list = FALSE;

	/* tab completion */
	if (event->type == GDK_KEY_PRESS &&
	    event->keyval == GDK_Tab) {
		gtk_editable_get_selection_bounds (entry, &pos, &tmp);

		if (dialog->completion_started &&
		    pos != tmp &&
		    pos != 1 &&
		    tmp == strlen (gtk_entry_get_text (GTK_ENTRY (entry)))) {
	    		gtk_editable_select_region (entry, 0, 0);		
			gtk_editable_set_position (entry, -1);

			panel_run_dialog_update_completion (dialog,
			    				    gtk_entry_get_text (GTK_ENTRY (entry)));
			
			return TRUE;
		}
	} else if (event->type == GDK_KEY_PRESS &&
		   event->length > 0) {
		panel_run_dialog_ensure_completion (dialog);

		gtk_editable_get_selection_bounds (entry, &pos, &tmp);

		if (dialog->completion_started &&
		    pos != tmp &&
		    pos != 1 &&
		    tmp == strlen (gtk_entry_get_text (GTK_ENTRY (entry)))) {
			temp = gtk_editable_get_chars (entry, 0, pos);
			prefix = g_strconcat (temp, event->string, NULL);
			g_free (temp);
		} else if (pos == tmp &&
			   tmp == strlen (gtk_entry_get_text (GTK_ENTRY (entry)))) {
			prefix = g_strconcat (gtk_entry_get_text (GTK_ENTRY (entry)),
					      event->string, NULL);
		} else {
			return FALSE;
		}
		
		pos = strlen (prefix);
		nprefix = NULL;

		g_completion_complete (dialog->completion, prefix, &nprefix);

		if (nprefix) {
		    	gtk_entry_set_text (GTK_ENTRY (entry), nprefix);
 			gtk_editable_set_position (entry, pos);
			gtk_editable_select_region (entry, pos, -1);
			
			dialog->completion_started = TRUE;

			if (strcmp(event->string, "/") == 0)
				panel_run_dialog_update_completion (dialog, nprefix);
			
			g_free (nprefix);
			g_free (prefix);
			
			return TRUE;
		}

		if (strcmp(event->string, "/") == 0)
			panel_run_dialog_update_completion (dialog, prefix);
		
		g_free (prefix);
	}
	
	return FALSE;
}

static void
entry_changed (GtkEntry       *entry,
	       PanelRunDialog *dialog)
{
	const char            *text;
	char                  *msg;

	text = gtk_entry_get_text (entry);

	/* desensitize run button if no text entered */
	if (!text || !text [0]) {
		gtk_widget_set_sensitive (dialog->run_button, FALSE);
		gtk_drag_source_unset (dialog->run_dialog);
	} else {
		gtk_widget_set_sensitive (dialog->run_button, TRUE);
		gtk_drag_source_set (dialog->run_dialog,
				     GDK_BUTTON1_MASK,
				     NULL, 0,
				     GDK_ACTION_COPY);
		gtk_drag_source_add_uri_targets (dialog->run_dialog);
	}
	
	/* update description label */
        if (text && text [0]) {
		if (!dialog->use_program_list) {
			msg = g_strdup_printf (_("Will run command: '%s'"), text);
			gtk_label_set_text (GTK_LABEL (dialog->program_label), msg);
			g_free (msg);
		}
        } else
		gtk_label_set_text (GTK_LABEL (dialog->program_label),
				    _("Select an application to view its description."));

	/* update item name to use for dnd */
	if (!dialog->use_program_list && dialog->item_name) {
		g_free (dialog->item_name);
		dialog->item_name = NULL;
	}

	/* look up icon for the command */
	if (text && text [0] &&
	    panel_profile_get_enable_program_list () &&
	    !dialog->use_program_list &&
	    !dialog->find_command_icon_idle_id)
		dialog->find_command_icon_idle_id =
			g_idle_add_full (G_PRIORITY_LOW,
					 (GSourceFunc) panel_run_dialog_find_command_icon_idle,
					 dialog, NULL);
}

static void
entry_drag_data_received (GtkEditable      *entry,
			  GdkDragContext   *context,
			  gint              x,
			  gint              y,
			  GtkSelectionData *selection_data,
			  guint             info,
			  guint32           time,
			  PanelRunDialog   *dialog)
{
	char  *uri_list;
	char **uris;
	char  *file;
	int    i;

        if (selection_data->format != 8 || selection_data->length == 0) {
        	g_warning (_("URI list dropped on run dialog had wrong format (%d) or length (%d)\n"),
			   selection_data->format,
			   selection_data->length);
		return;
        }

	uri_list = g_strndup (selection_data->data, selection_data->length);
	uris = g_strsplit (uri_list, "\r\n", -1);

	if (!uris) {
		g_free (uri_list);
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	for (i = 0; uris [i]; i++) {
		if (!uris [i] || !uris [i][0])
			continue;
		
		file = gnome_vfs_get_local_path_from_uri (uris [i]);

		/* FIXME: I assume the file is in utf8 encoding if coming from a URI? */
		if (file) {
			panel_run_dialog_append_file_utf8 (dialog, file);
			g_free (file);
		} else
			panel_run_dialog_append_file_utf8 (dialog, uris [i]);
	}

	g_strfreev (uris);
	g_free (uri_list);
	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
panel_run_dialog_setup_entry (PanelRunDialog *dialog,
			      GladeXML       *gui)
{
	GdkScreen             *screen;
	int                    width_request;
	
	dialog->gnome_entry = glade_xml_get_widget (gui, "gnome_entry");
	dialog->gtk_entry   = glade_xml_get_widget (gui, "gtk_entry");

	screen = gtk_window_get_screen (GTK_WINDOW (dialog->run_dialog));

        /* 1/4 the width of the first monitor should be a good value */
	width_request = panel_multiscreen_width (screen, 0) / 4;
	g_object_set (G_OBJECT (dialog->gnome_entry),
		      "width_request", width_request,
		      NULL);

        g_signal_connect (dialog->gtk_entry, "event",
			  G_CALLBACK (entry_event), dialog);
			  
        g_signal_connect (dialog->gtk_entry, "changed",
			  G_CALLBACK (entry_changed), dialog);

	gtk_drag_dest_unset (dialog->gtk_entry);
	
	gtk_drag_dest_set (dialog->gtk_entry,
			   GTK_DEST_DEFAULT_MOTION|GTK_DEST_DEFAULT_HIGHLIGHT,
			   NULL, 0,
			   GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets (dialog->gtk_entry);

	g_signal_connect (dialog->gtk_entry, "drag_data_received",
			  G_CALLBACK (entry_drag_data_received), dialog);
}

static void  
pixmap_drag_data_get (GtkWidget          *run_dialog,
	  	      GdkDragContext     *context,
		      GtkSelectionData   *selection_data,
		      guint               info,
		      guint               time,
		      PanelRunDialog     *dialog)
{
	GnomeDesktopItem *ditem;
	gboolean          exec = FALSE;
	const char       *text;
	char             *uri;
	char             *disk;
	char             *scheme;
	
	text = gtk_entry_get_text (GTK_ENTRY (dialog->gtk_entry));
	
	if (!text || !text [0])
		return;
		
	ditem = gnome_desktop_item_new ();

	disk = g_locale_from_utf8 (text, -1, NULL, NULL, NULL);
	uri = gnome_vfs_make_uri_from_input_with_dirs (disk,
						       GNOME_VFS_MAKE_URI_DIR_HOMEDIR);
	scheme = gnome_vfs_get_uri_scheme (uri);
	
	if (!strcasecmp (scheme, "http"))
		exec = command_is_executable (text);
		
	else if (!strcasecmp (scheme, "file"))
		exec = command_is_executable (uri);
		
	if (exec) {
		gnome_desktop_item_set_entry_type (ditem, GNOME_DESKTOP_ITEM_TYPE_APPLICATION);
		gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_EXEC, text);
	} else {
		gnome_desktop_item_set_entry_type (ditem, GNOME_DESKTOP_ITEM_TYPE_LINK);
		gnome_desktop_item_set_string (ditem, GNOME_DESKTOP_ITEM_URL, uri);
	}
		
	gnome_desktop_item_set_string (ditem,
				       GNOME_DESKTOP_ITEM_NAME, (dialog->item_name) ?
				              dialog->item_name : text);

	gnome_desktop_item_set_boolean (ditem,
					GNOME_DESKTOP_ITEM_TERMINAL,
					gtk_toggle_button_get_active (
						 GTK_TOGGLE_BUTTON (dialog->terminal_checkbox)));

	gnome_desktop_item_set_string (ditem,
				       GNOME_DESKTOP_ITEM_ICON, 
				       dialog->icon_path);
	
	g_free (uri);

	uri = panel_make_unique_uri (g_get_tmp_dir (), ".desktop");
	gnome_desktop_item_set_location (ditem, uri);

	if (gnome_desktop_item_save (ditem, NULL, FALSE, NULL))
		gtk_selection_data_set (selection_data,
					selection_data->target, 8,
					uri, strlen (uri));
	gnome_desktop_item_unref (ditem);

	g_free (uri);
	g_free (disk);
	g_free (scheme);
}

static void
panel_run_dialog_setup_pixmap (PanelRunDialog *dialog,
			       GladeXML       *gui)
{
	dialog->pixmap = glade_xml_get_widget (gui, "icon_pixmap");
	
	g_signal_connect (dialog->run_dialog, "drag_data_get",
			  G_CALLBACK (pixmap_drag_data_get),
			  dialog);

	gtk_image_set_from_stock (GTK_IMAGE (dialog->pixmap), 
				  PANEL_STOCK_RUN,
				  GTK_ICON_SIZE_DIALOG);
}

static PanelRunDialog *
panel_run_dialog_new (GdkScreen *screen,
		      GladeXML  *gui,
		      guint32    activate_time)
{
	PanelRunDialog *dialog;

	dialog = g_new0 (PanelRunDialog, 1);

	dialog->run_dialog = glade_xml_get_widget (gui, "panel_run_dialog");
	
	g_signal_connect_swapped (dialog->run_dialog, "response",
				  G_CALLBACK (panel_run_dialog_response), dialog);
				  
	g_signal_connect_swapped (dialog->run_dialog, "destroy",
				  G_CALLBACK (panel_run_dialog_destroy), dialog);

	dialog->run_button = glade_xml_get_widget (gui, "run_button");
	dialog->terminal_checkbox = glade_xml_get_widget (gui, "terminal_checkbox");
	
	panel_run_dialog_setup_pixmap        (dialog, gui);
	panel_run_dialog_setup_entry         (dialog, gui);
	panel_run_dialog_setup_file_button   (dialog, gui);
	panel_run_dialog_setup_program_list  (dialog, gui);
	panel_run_dialog_setup_list_expander (dialog, gui);

	panel_run_dialog_update_content (dialog, panel_profile_get_show_program_list ());

	gtk_widget_set_sensitive (dialog->run_button, FALSE);
	
	gtk_dialog_set_default_response (GTK_DIALOG (dialog->run_dialog),
					 GTK_RESPONSE_OK);

	gtk_window_set_screen (GTK_WINDOW (dialog->run_dialog), screen);

	set_window_icon_from_stock (GTK_WINDOW (dialog->run_dialog), PANEL_STOCK_RUN);

	gtk_widget_grab_focus (dialog->gtk_entry);
	gtk_widget_realize (dialog->run_dialog);
	gdk_x11_window_set_user_time (dialog->run_dialog->window,
				      activate_time);
	gtk_widget_show (dialog->run_dialog);
	
	return dialog;
}

static void
panel_run_dialog_static_dialog_destroyed (PanelRunDialog *dialog)
{
	/* just reset the static dialog to NULL for next time */
	static_dialog = NULL;
}

void
panel_run_dialog_present (GdkScreen *screen,
			  guint32    activate_time)
{
	GladeXML *gui;

	if (panel_lockdown_get_disable_command_line ())
		return;

	if (static_dialog) {
		gtk_window_set_screen (GTK_WINDOW (static_dialog->run_dialog), screen);
		gdk_x11_window_set_user_time (static_dialog->run_dialog->window,
					      activate_time);
		gtk_window_present (GTK_WINDOW (static_dialog->run_dialog));
		gtk_widget_grab_focus (static_dialog->gtk_entry);
		return;
	}

	gui = glade_xml_new (GLADEDIR "/panel-run-dialog.glade",
			     "panel_run_dialog",
			     NULL);

	static_dialog = panel_run_dialog_new (screen, gui, activate_time);

	g_signal_connect_swapped (static_dialog->run_dialog, "destroy",
				  G_CALLBACK (panel_run_dialog_static_dialog_destroyed),
				  static_dialog);

	g_object_unref (gui);
}
