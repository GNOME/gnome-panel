/* -*- c-basic-offset: 8; indent-tabs-mode: t -*-
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.

 * Authors:
 *	Frank Worsley <fworsley@shaw.ca>
 *
 * Based on code by:
 *	Havoc Pennington <hp@pobox.com>
 *      George Lebl <jirka@5z.com>
 *	Mark McLoughlin <mark@skynet.ie>
 *	Tom Tromey (Copyright (C) 1998)
 */

#include <config.h>

#include "panel-run-dialog.h"

#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gmenu-tree.h>

#include <libpanel-util/panel-error.h>
#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-keyfile.h>
#include <libpanel-util/panel-show.h>
#include <libpanel-util/panel-xdg.h>

#include "panel-util.h"
#include "panel-multiscreen.h"
#include "panel-lockdown.h"
#include "panel-xutils.h"
#include "panel-icon-names.h"
#include "panel-schemas.h"

#define PANEL_GTK_BUILDER_GET(builder, name) GTK_WIDGET (gtk_builder_get_object (builder, name))

typedef struct {
	GtkWidget        *run_dialog;

	GSettings        *run_settings;

	GtkWidget        *main_box;

	GtkWidget        *combobox;
	GtkWidget        *pixmap;
	GtkWidget        *run_button;
	GtkWidget        *file_button;
	GtkWidget        *list_expander;
	GtkWidget        *terminal_checkbox;
	GtkWidget        *program_label;
	GtkWidget        *program_list;
	
	long              changed_id;

	GtkListStore     *program_list_store;

	GHashTable       *dir_hash;
	GList		 *possible_executables;
	GList		 *completion_items;
	GCompletion      *completion;

	int	          add_items_idle_id;
	int		  find_command_idle_id;
	gboolean	  use_program_list;
	gboolean	  completion_started;
	
	GIcon		 *gicon;
	char             *desktop_path;
	char		 *item_name;	
} PanelRunDialog;

enum {
	COLUMN_ICON,
	COLUMN_NAME,
	COLUMN_COMMENT,
	COLUMN_PATH,
	COLUMN_EXEC,
	COLUMN_VISIBLE,
	NUM_COLUMNS
};

static PanelRunDialog *static_dialog = NULL;

#define PANEL_RUN_MAX_HISTORY 20

static GtkTreeModel *
_panel_run_get_recent_programs_list (PanelRunDialog *dialog)
{
	GtkListStore  *list;
	char         **commands;
	int            i;

	list = gtk_list_store_new (1, G_TYPE_STRING);

	commands = g_settings_get_strv (dialog->run_settings,
					PANEL_RUN_HISTORY_KEY);

	for (i = 0; commands[i] != NULL; i++) {
		GtkTreeIter iter;
		gtk_list_store_prepend (list, &iter);
		gtk_list_store_set (list, &iter, 0, commands[i], -1);
	}

	g_strfreev (commands);

	return GTK_TREE_MODEL (list);
}

static void
_panel_run_save_recent_programs_list (PanelRunDialog *dialog,
				      char           *last_command)
{
	char **commands;
	char **new_commands;
	int    i;
	int    size;

	commands = g_settings_get_strv (dialog->run_settings,
					PANEL_RUN_HISTORY_KEY);

	/* do not save the same command twice in a row */
	if (g_strcmp0 (commands[0], last_command) == 0)
		return;

	for (i = 0; commands[i] != NULL; i++);
	size = MIN (i + 1, PANEL_RUN_MAX_HISTORY);

	new_commands = g_new (char *, size + 1);

	new_commands[0] = last_command;
	new_commands[size] = NULL; /* last item */

	for (i = 1; i < size; i++)
		new_commands[i] = commands[i-1];

	g_settings_set_strv (dialog->run_settings,
			     PANEL_RUN_HISTORY_KEY,
			     (const char **) new_commands);

	g_free (new_commands); /* we don't own the strings */
	g_strfreev (commands);
}

static void
panel_run_dialog_destroy (PanelRunDialog *dialog)
{
	GList *l;
	
	dialog->changed_id = 0;

	g_object_unref (dialog->list_expander);

	if (dialog->gicon)
		g_object_unref (dialog->gicon);
	dialog->gicon = NULL;

	g_free (dialog->desktop_path);
	dialog->desktop_path = NULL;
	g_free (dialog->item_name);
	dialog->item_name = NULL;

	if (dialog->add_items_idle_id)
		g_source_remove (dialog->add_items_idle_id);
	dialog->add_items_idle_id = 0;

	if (dialog->find_command_idle_id)
		g_source_remove (dialog->find_command_idle_id);
	dialog->find_command_idle_id = 0;

	if (dialog->dir_hash)
		g_hash_table_destroy (dialog->dir_hash);
	dialog->dir_hash = NULL;

	for (l = dialog->possible_executables; l; l = l->next)
		g_free (l->data);
	g_list_free (dialog->possible_executables);
	dialog->possible_executables = NULL;
	
	for (l = dialog->completion_items; l; l = l->next)
		g_free (l->data);
	g_list_free (dialog->completion_items);
	dialog->completion_items = NULL;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	g_clear_pointer (&dialog->completion, g_completion_free);
	G_GNUC_END_IGNORE_DEPRECATIONS

	if (dialog->run_settings)
		g_object_unref (dialog->run_settings);
	dialog->run_settings = NULL;

	g_free (dialog);
}

static const char *
panel_run_dialog_get_combo_text (PanelRunDialog *dialog)
{
	GtkWidget *entry;

	entry = gtk_bin_get_child (GTK_BIN (dialog->combobox));

	return gtk_entry_get_text (GTK_ENTRY (entry));
}

static void
panel_run_dialog_set_default_icon (PanelRunDialog *dialog, gboolean set_drag)
{
	gtk_image_set_from_icon_name (GTK_IMAGE (dialog->pixmap),
				      PANEL_ICON_RUN,
				      GTK_ICON_SIZE_DIALOG);
	
	if (set_drag)
		gtk_drag_source_set_icon_name (dialog->run_dialog,
					       PANEL_ICON_LAUNCHER);
}

static void
panel_run_dialog_set_icon (PanelRunDialog *dialog,
			   GIcon          *gicon,
			   gboolean        force)
{
	if (!force && gicon && dialog->gicon &&
	    gicon == dialog->gicon)
		return;

	if (dialog->gicon)
		g_object_unref (dialog->gicon);
	dialog->gicon = NULL;
		
	if (gicon) {
		dialog->gicon = g_object_ref (gicon);
		gtk_image_set_from_gicon (GTK_IMAGE (dialog->pixmap),
					  gicon, GTK_ICON_SIZE_DIALOG);
		gtk_drag_source_set_icon_gicon (dialog->run_dialog, gicon);
	} else {
		panel_run_dialog_set_default_icon (dialog, TRUE);
	}
}

static gboolean
command_is_executable (const char   *command,
		       int          *argcp,
		       char       ***argvp)
{
	gboolean   result;
	char     **argv;
	char      *path;
	int        argc;

	result = g_shell_parse_argv (command, &argc, &argv, NULL);

	if (!result)
		return FALSE;

	path = g_find_program_in_path (argv[0]);

	if (!path) {
		g_strfreev (argv);
		return FALSE;
	}

	/* If we pass an absolute path to g_find_program it just returns
	 * that absolute path without checking if it is executable. Also
	 * make sure its a regular file so we don't try to launch
	 * directories or device nodes.
	 */
	if (!g_file_test (path, G_FILE_TEST_IS_EXECUTABLE) ||
	    !g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
		g_free (path);
		g_strfreev (argv);
		return FALSE;
	}

	g_free (path);

	if (argcp)
		*argcp = argc;
	if (argvp)
		*argvp = argv;

	return TRUE;
}

/*
 * Set the DISPLAY variable, to be use by g_spawn_async.
 */
static void
set_environment (gpointer user_data)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();

  if (!g_setenv ("DISPLAY", gdk_display_get_name (display), TRUE))
    g_warning ("Failed to set DISPLAY environment variable");
}

static void
dummy_child_watch (GPid         pid,
		   gint         status,
		   gpointer user_data)
{
	/* Nothing, this is just to ensure we don't double fork
	 * and break pkexec:
	 * https://bugzilla.gnome.org/show_bug.cgi?id=675789
	 */
}


/**
 * panel_run_dialog_prepend_terminal_to_vector:
 * @argc: a pointer to the vector size
 * @argv: a pointer to the vector
 *
 * Description:  Prepends a terminal (either the one configured as default in
 * the user's GNOME setup, or one of the common xterm emulators) to the passed
 * in vector, modifying it in the process.  The vector should be allocated with
 * #g_malloc, as this will #g_free the original vector.  Also all elements must
 * have been allocated separately.  That is the standard glib/GNOME way of
 * doing vectors however.  If the integer that @argc points to is negative, the
 * size will first be computed.  Also note that passing in pointers to a vector
 * that is empty, will just create a new vector for you.
 **/
/* TODO: throw out this function if there ever is a proper GAppInfo port */
static void
panel_run_dialog_prepend_terminal_to_vector (int *argc, char ***argv)
{
        char **real_argv;
        int real_argc;
        int i, j;
	char **term_argv = NULL;
	int term_argc = 0;
	GSettings *settings;

	gchar *terminal = NULL;

	char **the_argv;

        g_return_if_fail (argc != NULL);
        g_return_if_fail (argv != NULL);

	/* sanity */
        if(*argv == NULL)
                *argc = 0;

	the_argv = *argv;

	/* compute size if not given */
	if (*argc < 0) {
		for (i = 0; the_argv[i] != NULL; i++)
			;
		*argc = i;
	}

	settings = g_settings_new ("org.gnome.desktop.default-applications.terminal");
	terminal = g_settings_get_string (settings, "exec");

	if (terminal) {
		gchar *command_line;
		gchar *exec_flag;
		GError *error;

		exec_flag = g_settings_get_string (settings, "exec-arg");

		if (exec_flag == NULL)
			command_line = g_strdup (terminal);
		else
			command_line = g_strdup_printf ("%s %s", terminal,
							exec_flag);

		error = NULL;
		if (!g_shell_parse_argv (command_line, &term_argc, &term_argv, &error)) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}

		g_free (command_line);
		g_free (exec_flag);
		g_free (terminal);
	}

	g_object_unref (settings);

	if (term_argv == NULL) {
		char *check;

		term_argc = 2;
		term_argv = g_new0 (char *, 3);

		check = g_find_program_in_path ("gnome-terminal");
		if (check != NULL) {
			term_argv[0] = check;
			/* Note that gnome-terminal takes -x and
			 * as -e in gnome-terminal is broken we use that. */
			term_argv[1] = g_strdup ("-x");
		} else {
			if (check == NULL)
				check = g_find_program_in_path ("nxterm");
			if (check == NULL)
				check = g_find_program_in_path ("color-xterm");
			if (check == NULL)
				check = g_find_program_in_path ("rxvt");
			if (check == NULL)
				check = g_find_program_in_path ("xterm");
			if (check == NULL)
				check = g_find_program_in_path ("dtterm");
			if (check == NULL) {
				g_warning (_("Cannot find a terminal, using "
					     "xterm, even if it may not work"));
				check = g_strdup ("xterm");
			}
			term_argv[0] = check;
			term_argv[1] = g_strdup ("-e");
		}
	}

        real_argc = term_argc + *argc;
        real_argv = g_new (char *, real_argc + 1);

        for (i = 0; i < term_argc; i++)
                real_argv[i] = term_argv[i];

        for (j = 0; j < *argc; j++, i++)
                real_argv[i] = (char *)the_argv[j];

	real_argv[i] = NULL;

	g_free (*argv);
	*argv = real_argv;
	*argc = real_argc;

	/* we use g_free here as we sucked all the inner strings
	 * out from it into real_argv */
	g_free (term_argv);
}

static gboolean
panel_run_dialog_launch_command (PanelRunDialog *dialog,
				 const char     *command,
				 const char     *locale_command)
{
	gboolean    result;
	GError     *error = NULL;
	char      **argv;
	int         argc;
	GPid        pid;

	if (!command_is_executable (locale_command, &argc, &argv))
		return FALSE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->terminal_checkbox)))
		panel_run_dialog_prepend_terminal_to_vector (&argc, &argv);

	result = g_spawn_async (NULL, /* working directory */
				argv,
				NULL, /* envp */
				G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
				set_environment,
				NULL,
				&pid,
				&error);

	if (!result) {
		char *primary;

		primary = g_markup_printf_escaped (_("Could not run command '%s'"),
						   command);
		panel_error_dialog (GTK_WINDOW (dialog->run_dialog), NULL,
				    "cannot_spawn_command", TRUE,
				    primary, error->message);
		g_free (primary);

		g_error_free (error);
	} else {
		g_child_watch_add (pid, dummy_child_watch, NULL);
	}

	g_strfreev (argv);

	return result;
}

static void
panel_run_dialog_execute (PanelRunDialog *dialog)
{
	GError   *error;
	gboolean  result;
	char     *command;
	char     *disk;
	char     *scheme;	
	
	command = g_strdup (panel_run_dialog_get_combo_text (dialog));
	command = g_strchug (command);

	if (command[0] == '\0') {
		g_free (command);
		return;
	}

	error = NULL;
	disk = g_locale_from_utf8 (command, -1, NULL, NULL, &error);

	if (error != NULL) {
		char *primary;

		primary = g_strdup_printf (_("Could not convert '%s' from UTF-8"),
					   command);
		panel_error_dialog (GTK_WINDOW (dialog->run_dialog), NULL,
				    "cannot_convert_command_from_utf8", TRUE,
				    primary, error->message);
		g_free (primary);

		g_error_free (error);
		g_free (command);
		g_free (disk);
		return;
	}

	result = FALSE;
	
	scheme = g_uri_parse_scheme (disk);
	/* if it's an absolute path or not a URI, it's possibly an executable,
	 * so try it before displaying it */
	if (g_path_is_absolute (disk) || !scheme)
		result = panel_run_dialog_launch_command (dialog, command, disk);
	
	if (!result) {
		GFile     *file;
		char      *uri;
		GdkScreen *screen;
		
		file = panel_util_get_file_optional_homedir (command);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		screen = gtk_window_get_screen (GTK_WINDOW (dialog->run_dialog));
		result = panel_show_uri (screen, uri,
					 gtk_get_current_event_time (), NULL);

		g_free (uri);
	}
		
	if (result) {
		/* only save working commands in history */
		_panel_run_save_recent_programs_list (dialog, command);
		
		/* only close the dialog if we successfully showed or launched
		 * something */
		gtk_widget_destroy (dialog->run_dialog);
	}

	g_free (command);
	g_free (disk);
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
	GtkWidget  *entry;
	
	/* Don't allow filenames beginning with '-' */
	if (!file || !file[0] || file[0] == '-')
		return;
	
	quoted = quote_string (file);
	entry = gtk_bin_get_child (GTK_BIN (dialog->combobox));
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (text && text [0]) {
		temp = g_strconcat (text, " ", quoted, NULL);
		gtk_entry_set_text (GTK_ENTRY (entry), temp);
		g_free (temp);
	} else
		gtk_entry_set_text (GTK_ENTRY (entry), quoted);
	
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

	g_return_val_if_fail (cmd1 && cmd2, TRUE);

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
	if (!tokens || !tokens [0]) {
		g_free (word1);
		g_strfreev (tokens);
		return FALSE;
	}

	word2 = g_path_get_basename (tokens [0]);
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
panel_run_dialog_make_all_list_visible (GtkTreeModel *model,
					GtkTreePath  *path,
					GtkTreeIter  *iter,
					gpointer      data)
{
	gtk_list_store_set (GTK_LIST_STORE (model), iter,
			    COLUMN_VISIBLE, TRUE,
			    -1);

	return FALSE;
}

static gboolean
panel_run_dialog_find_command_idle (PanelRunDialog *dialog)
{
	GtkTreeIter   iter;
	GtkTreeModel *model;
	GtkTreePath  *path;
	char         *text;
	GIcon        *found_icon;
	char         *found_name;
	gboolean      fuzzy;
	
	model = GTK_TREE_MODEL (dialog->program_list_store);
	path = gtk_tree_path_new_first ();
	
	if (!path || !gtk_tree_model_get_iter (model, &iter, path)) {
		if (path)
			gtk_tree_path_free (path);
		
		panel_run_dialog_set_icon (dialog, NULL, FALSE);
	
		dialog->find_command_idle_id = 0;
		return FALSE;
	}

	text = g_strdup (panel_run_dialog_get_combo_text (dialog));
	found_icon = NULL;
	found_name = NULL;
	fuzzy = FALSE;

	do {
		char *exec = NULL;
		char *name = NULL;
		char *comment = NULL;
		GIcon *icon = NULL;

		gtk_tree_model_get (model, &iter,
				    COLUMN_EXEC,    &exec,
				    COLUMN_ICON,    &icon,
				    COLUMN_NAME,    &name,
				    COLUMN_COMMENT, &comment,
				    -1);

		if (!fuzzy && exec && icon &&
		    fuzzy_command_match (text, exec, &fuzzy)) {
			if (found_icon)
				g_object_unref (found_icon);
			g_free (found_name);
			
			found_icon = g_object_ref (icon);
			found_name = g_strdup (name);
			
			gtk_list_store_set (dialog->program_list_store,
					    &iter,
					    COLUMN_VISIBLE, TRUE,
					    -1);
		} else if (panel_g_utf8_strstrcase (exec, text) != NULL ||
			   panel_g_utf8_strstrcase (name, text) != NULL ||
			   panel_g_utf8_strstrcase (comment, text) != NULL) {
			gtk_list_store_set (dialog->program_list_store,
					    &iter,
					    COLUMN_VISIBLE, TRUE,
					    -1);
		} else {
			gtk_list_store_set (dialog->program_list_store,
					    &iter,
					    COLUMN_VISIBLE, FALSE,
					    -1);
		}

		g_free (exec);
		g_clear_object (&icon);
		g_free (name);
		g_free (comment);
	
        } while (gtk_tree_model_iter_next (model, &iter));

	if (gtk_tree_model_get_iter (gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->program_list)),
				     &iter, path))
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (dialog->program_list),
					      path, NULL, FALSE, 0, 0);

	gtk_tree_path_free (path);

	panel_run_dialog_set_icon (dialog, found_icon, FALSE);
	//FIXME update dialog->program_label

	if (found_icon)
		g_object_unref (found_icon);
	g_free (text);
	
	g_free (dialog->item_name);
	dialog->item_name = found_name;
	
	dialog->find_command_idle_id = 0;
	return FALSE;
}

static int
compare_applications (GMenuTreeEntry *a,
		      GMenuTreeEntry *b)
{
	return g_utf8_collate (g_app_info_get_display_name ((GAppInfo*)(gmenu_tree_entry_get_app_info (a))),
			       g_app_info_get_display_name ((GAppInfo*)(gmenu_tree_entry_get_app_info (b))));
}

static GSList *get_all_applications_from_dir (GMenuTreeDirectory *directory,
					      GSList            *list);

static GSList *
get_all_applications_from_alias (GMenuTreeAlias *alias,
				 GSList         *list)
{
	switch (gmenu_tree_alias_get_aliased_item_type (alias)) {
	case GMENU_TREE_ITEM_ENTRY:
		/* pass on the reference */
		list = g_slist_append (list, gmenu_tree_alias_get_aliased_entry (alias));
		break;

	case GMENU_TREE_ITEM_DIRECTORY: {
		GMenuTreeDirectory *directory = gmenu_tree_alias_get_aliased_directory (alias);
		list = get_all_applications_from_dir (directory, list);
		gmenu_tree_item_unref (directory);
		break;
	}

	case GMENU_TREE_ITEM_SEPARATOR:
	case GMENU_TREE_ITEM_HEADER:
	case GMENU_TREE_ITEM_ALIAS:
		break;

	case GMENU_TREE_ITEM_INVALID:
	default:
		break;
	}

	return list;
}

static GSList *
get_all_applications_from_dir (GMenuTreeDirectory *directory,
			       GSList             *list)
{
	GMenuTreeIter *iter;
	GMenuTreeItemType next_type;

	iter = gmenu_tree_directory_iter (directory);

	while ((next_type = gmenu_tree_iter_next (iter)) != GMENU_TREE_ITEM_INVALID) {
		switch (next_type) {
		case GMENU_TREE_ITEM_ENTRY:
			list = g_slist_append (list, gmenu_tree_iter_get_entry (iter));
			break;

		case GMENU_TREE_ITEM_DIRECTORY: {
			GMenuTreeDirectory *dir = gmenu_tree_iter_get_directory (iter);
			list = get_all_applications_from_dir (dir, list);
			gmenu_tree_item_unref (dir);
			break;
		}

		case GMENU_TREE_ITEM_ALIAS: {
			GMenuTreeAlias *alias = gmenu_tree_iter_get_alias (iter);
			list = get_all_applications_from_alias (alias, list);
			gmenu_tree_item_unref (alias);
			break;
		}

		case GMENU_TREE_ITEM_SEPARATOR:
		case GMENU_TREE_ITEM_HEADER:
			break;

		case GMENU_TREE_ITEM_INVALID:
		default:
			break;
		}
	}

	gmenu_tree_iter_unref (iter);

	return list;
}

static gchar *
get_applications_menu (void)
{
	const gchar *xdg_menu_prefx = g_getenv ("XDG_MENU_PREFIX");
	return g_strdup_printf ("%sapplications.menu",
	                        !PANEL_GLIB_STR_EMPTY (xdg_menu_prefx) ? xdg_menu_prefx : "gnome-");
}

static GSList *
get_all_applications (void)
{
	GMenuTree          *tree;
	GMenuTreeDirectory *root;
	GSList             *retval;
	gchar              *applications_menu;

	applications_menu = get_applications_menu ();
	tree = gmenu_tree_new (applications_menu, GMENU_TREE_FLAGS_SORT_DISPLAY_NAME);
	g_free (applications_menu);

	if (!gmenu_tree_load_sync (tree, NULL))
		return NULL;

	root = gmenu_tree_get_root_directory (tree);

	retval = get_all_applications_from_dir (root, NULL);

	gmenu_tree_item_unref (root);
	g_object_unref (tree);

	retval = g_slist_sort (retval,
			       (GCompareFunc) compare_applications);

	return retval;
}

static gboolean
panel_run_dialog_add_items_idle (PanelRunDialog *dialog)
{
	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;
	GtkTreeModel      *model_filter;
	GSList            *all_applications;
	GSList            *l;
	GSList            *next;
	const char        *prev_name;

	/* create list store */
	dialog->program_list_store = gtk_list_store_new (NUM_COLUMNS,
							 G_TYPE_ICON,
							 G_TYPE_STRING,
							 G_TYPE_STRING,
							 G_TYPE_STRING,
							 G_TYPE_STRING,
							 G_TYPE_BOOLEAN);

	all_applications = get_all_applications ();
	
	/* Strip duplicates */
	prev_name = NULL;
	for (l = all_applications; l; l = next) {
		GMenuTreeEntry  *entry = l->data;
		const char      *entry_name;
		GDesktopAppInfo *app_info;


		next = l->next;
		app_info = gmenu_tree_entry_get_app_info (entry);

		entry_name = g_app_info_get_display_name (G_APP_INFO (app_info));
		if (prev_name && entry_name && strcmp (entry_name, prev_name) == 0) {
			gmenu_tree_item_unref (entry);

			all_applications = g_slist_delete_link (all_applications, l);
		} else {
			prev_name = entry_name;
		}
	}

	for (l = all_applications; l; l = l->next) {
		GMenuTreeEntry *entry = l->data;
		GtkTreeIter     iter;
		GAppInfo       *app_info;

		app_info = G_APP_INFO (gmenu_tree_entry_get_app_info (entry));

		gtk_list_store_append (dialog->program_list_store, &iter);
		gtk_list_store_set (dialog->program_list_store, &iter,
				    COLUMN_ICON,    g_app_info_get_icon (app_info),
				    COLUMN_NAME,    g_app_info_get_display_name (app_info),
				    COLUMN_COMMENT, g_app_info_get_description (app_info),
				    COLUMN_EXEC,    g_app_info_get_executable (app_info),
				    COLUMN_PATH,    gmenu_tree_entry_get_desktop_file_path (entry),
				    COLUMN_VISIBLE, TRUE,
				    -1);

		gmenu_tree_item_unref (entry);
	}
	g_slist_free (all_applications);

	model_filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (dialog->program_list_store),
						  NULL);
	gtk_tree_model_filter_set_visible_column (GTK_TREE_MODEL_FILTER (model_filter),
						  COLUMN_VISIBLE);

	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->program_list), 
				 model_filter);
	//FIXME use the same search than the fuzzy one?
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (dialog->program_list),
					 COLUMN_NAME);

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
                                             "gicon", COLUMN_ICON,
                                             NULL);
        
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
                                             "text", COLUMN_NAME,
                                             NULL);
					          
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->program_list), column);

	dialog->add_items_idle_id = 0;					 
	return FALSE;
}

static char *
remove_field_codes (const char *exec)
{
	char *retval;
	char *p;

	if (exec == NULL || *exec == '\0')
		return g_strdup ("");

	retval = g_new0 (char, strlen (exec) + 1);
	p = retval;

	while (*exec != '\0') {
		if (*exec != '%') {
			*p++ = *exec++;
			continue;
		}

		switch (exec[1]) {
		case '%':
			*p++ = *exec++;
			exec++;
			break;
		case 'f':
		case 'F':
		case 'u':
		case 'U':
		case 'd':
		case 'D':
		case 'n':
		case 'N':
		case 'i':
		case 'c':
		case 'k':
		case 'v':
		case 'm':
			exec += 2;
			break;
		default:
			*p++ = *exec++;
			break;
		}
	}

	return retval;
}

static void
program_list_selection_changed (GtkTreeSelection *selection,
				PanelRunDialog   *dialog)
{
	GtkTreeModel *filter_model;
	GtkTreeModel *child_model;
	GtkTreeIter   iter;
	GtkTreeIter   filter_iter;
	char         *temp;
	char         *path, *stripped;
	gboolean      terminal;
	GKeyFile     *key_file;
	GtkWidget    *entry;

	if (!gtk_tree_selection_get_selected (selection, &filter_model,
					      &filter_iter))
		return;

	gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (filter_model),
							  &iter, &filter_iter);

	path = NULL;
	child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (filter_model));
	gtk_tree_model_get (child_model, &iter,
			    COLUMN_PATH, &path,
			    -1);
				  
	if (!path)
		return;

	key_file = g_key_file_new ();

	if (!g_key_file_load_from_file (key_file, path,
					G_KEY_FILE_NONE, NULL)) {
		g_key_file_free (key_file);
		g_free (path);
		return;
	}

	dialog->use_program_list = TRUE;
	if (dialog->desktop_path)
		g_free (dialog->desktop_path);
	dialog->desktop_path = g_strdup (path);
	if (dialog->item_name)
		g_free (dialog->item_name);
	dialog->item_name = NULL;

	/* Order is important here. We have to set the text first so that the
	 * drag source is enabled, otherwise the drag icon can't be set by
	 * panel_run_dialog_set_icon.
	 */
	entry = gtk_bin_get_child (GTK_BIN (dialog->combobox));
	temp = panel_key_file_get_string (key_file, "Exec");
	if (temp) {
		stripped = remove_field_codes (temp);
		gtk_entry_set_text (GTK_ENTRY (entry), stripped);
		g_free (stripped);
	} else {
		temp = panel_key_file_get_string (key_file, "URL");
		gtk_entry_set_text (GTK_ENTRY (entry), sure_string (temp));
	}
	g_free (temp);

	temp = panel_key_file_get_locale_string (key_file, "Icon");
	if (!PANEL_GLIB_STR_EMPTY (temp)) {
		GIcon *gicon;

		stripped = panel_xdg_icon_remove_extension (temp);
		gicon = g_themed_icon_new (stripped);
		panel_run_dialog_set_icon (dialog, gicon, FALSE);
		g_object_unref (gicon);
		g_free (stripped);
	} else {
		panel_run_dialog_set_icon (dialog, NULL, FALSE);
	}
	g_free (temp);
	
	temp = panel_key_file_get_locale_string (key_file, "Comment");
	//FIXME: if sure_string () == "", we should display "Will run..." as in entry_changed()
	gtk_label_set_text (GTK_LABEL (dialog->program_label),
			    sure_string (temp));
	g_free (temp);

	terminal = panel_key_file_get_boolean (key_file, "Terminal", FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->terminal_checkbox),
				      terminal);

	g_key_file_free (key_file);

	g_free (path);
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
				     GtkBuilder     *gui)
{
	GtkTreeSelection *selection;
	
	dialog->program_list = PANEL_GTK_BUILDER_GET (gui, "program_list");
	dialog->program_label = PANEL_GTK_BUILDER_GET (gui, "program_label");
	dialog->main_box = PANEL_GTK_BUILDER_GET (gui, "main_box");
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->program_list));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (program_list_selection_changed),
			  dialog);

	g_signal_connect (dialog->program_list, "row-activated",
			  G_CALLBACK (program_list_selection_activated),
			  dialog);
}

static void
panel_run_dialog_setup_list_expander (PanelRunDialog *dialog,
				      GtkBuilder     *gui)
{
	dialog->list_expander = PANEL_GTK_BUILDER_GET (gui, "list_expander");

	/* Ref the expander so it doesn't get destroyed when it is
	 * removed from the visible area of the dialog box. */
	g_object_ref (dialog->list_expander);

	g_settings_bind (dialog->run_settings,
			 PANEL_RUN_SHOW_LIST_KEY,
			 dialog->list_expander,
			 "expanded",
			 G_SETTINGS_BIND_DEFAULT);
}

static void
panel_run_dialog_update_program_list (GSettings      *settings,
				      char           *key,
				      PanelRunDialog *dialog)
{
	gboolean   enabled;
	gboolean   shown;
	GtkWidget *parent;

	enabled = g_settings_get_boolean (dialog->run_settings,
					  PANEL_RUN_ENABLE_LIST_KEY);

	parent = gtk_widget_get_parent (dialog->list_expander);

	if (enabled) {
		if (dialog->program_list_store == NULL) {
			/* start loading the list of applications */
			dialog->add_items_idle_id =
				g_idle_add_full (G_PRIORITY_LOW,
						 (GSourceFunc) panel_run_dialog_add_items_idle,
						 dialog, NULL);
		}

		if (!parent)
			gtk_box_pack_end (GTK_BOX (dialog->main_box),
					  dialog->list_expander,
					  TRUE, TRUE, 0);
	} else {
		if (parent)
			gtk_container_remove (GTK_CONTAINER (parent),
					      dialog->list_expander);
	}

	shown = g_settings_get_boolean (dialog->run_settings,
					PANEL_RUN_SHOW_LIST_KEY);

	if (enabled && shown) {
		gtk_window_resize (GTK_WINDOW (dialog->run_dialog), 100, 300);
		gtk_window_set_resizable (GTK_WINDOW (dialog->run_dialog), TRUE);
		gtk_widget_grab_focus (dialog->program_list);
        } else {
		gtk_window_set_resizable (GTK_WINDOW (dialog->run_dialog), FALSE);
                gtk_widget_grab_focus (dialog->combobox);
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
 
	gtk_widget_grab_focus (dialog->combobox);
}

static void
file_button_clicked (GtkButton      *button,
		     PanelRunDialog *dialog)
{
	GtkWidget *chooser;

	chooser = gtk_file_chooser_dialog_new (_("Choose a file to append to the command..."),
					       GTK_WINDOW (dialog->run_dialog),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       _("_Cancel"), GTK_RESPONSE_CANCEL,
					       _("_OK"), GTK_RESPONSE_OK,
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
				    GtkBuilder     *gui)
{
	dialog->file_button = PANEL_GTK_BUILDER_GET (gui, "file_button");
		
        g_signal_connect (dialog->file_button, "clicked",
			  G_CALLBACK (file_button_clicked),
			  dialog);
}

static GList *
fill_files_from (const char *dirname,
		 const char *dirprefix,
		 char        prefix,
		 GList      *existing_items)
{
	GList         *list;
	DIR           *dir;
	struct dirent *dent;
	
	list = NULL;
	dir = opendir (dirname);
	
	if (!dir)
		return list;
	
	while ((dent = readdir (dir))) {
		char       *file;
		char       *item;
		const char *suffix;

		if (dent->d_name [0] != prefix)
			continue;

		file = g_build_filename (dirname, dent->d_name, NULL);
		
		suffix = NULL;
		if (
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
		/* don't use g_file_test at first so we don't stat() */
		    dent->d_type == DT_DIR ||
		    (dent->d_type == DT_LNK &&
		     g_file_test (file, G_FILE_TEST_IS_DIR))
#else
		    g_file_test (file, G_FILE_TEST_IS_DIR)
#endif
		   )
			suffix = "/";
		
		g_free (file);
		
		item = g_build_filename (dirprefix, dent->d_name, suffix, NULL);
		
		list = g_list_prepend (list, item);
	}

	closedir (dir);
	
	return list;
}	

static GList *
fill_possible_executables (void)
{
	GList         *list;
	const char    *path;
	char         **pathv;
	int            i;
	
	list = NULL;
	path = g_getenv ("PATH");

	if (!path || !path [0])
		return list;

	pathv = g_strsplit (path, ":", 0);
	
	for (i = 0; pathv [i]; i++) {
		const char *file;
		char       *filename;
		GDir       *dir;

		dir = g_dir_open (pathv [i], 0, NULL);

		if (!dir)
			continue;

		while ((file = g_dir_read_name (dir))) {
			filename = g_build_filename (pathv [i], file, NULL);
			list = g_list_prepend (list, filename);
		}

		g_dir_close (dir);
	}
	
	g_strfreev (pathv);
	
	return list;
}

static GList *
fill_executables (GList *possible_executables,
		  GList *existing_items,
		  char   prefix)
{
	GList *list;
	GList *l;
	
	list = NULL;	
	
	for (l = possible_executables; l; l = l->next) {
		const char *filename;
		char       *basename;
			
		filename = l->data;
		basename = g_path_get_basename (filename);
			
		if (basename [0] == prefix &&
		    g_file_test (filename, G_FILE_TEST_IS_REGULAR) &&
		    g_file_test (filename, G_FILE_TEST_IS_EXECUTABLE)) {
			    
			if (g_list_find_custom (existing_items, basename,
						(GCompareFunc) strcmp)) {
				g_free (basename);
				return NULL;
			}

			list = g_list_prepend (list, basename);
		 } else {
			g_free (basename);
		 }
	}
	
	return list;
}

static void
panel_run_dialog_update_completion (PanelRunDialog *dialog,
				    const char     *text)
{
	GList *list;
	GList *executables;
	char   prefix;
	char  *buf;
	char  *dirname;
	char  *dirprefix;
	char  *key;

	g_assert (text != NULL && *text != '\0' && !g_ascii_isspace (*text));

	list = NULL;
	executables = NULL;

	if (!dialog->completion) {
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		dialog->completion = g_completion_new (NULL);
		G_GNUC_END_IGNORE_DEPRECATIONS

		dialog->possible_executables = fill_possible_executables ();
		dialog->dir_hash = g_hash_table_new_full (g_str_hash,
							  g_str_equal,
							  g_free, NULL);
	}
	
	buf = g_path_get_basename (text);
	prefix = buf[0];
	g_free (buf);
	if (prefix == '/' || prefix == '.')
		return;

	if (text [0] == '/') {
		/* complete against absolute path */
		dirname = g_path_get_dirname (text);
		dirprefix = g_strdup (dirname);
	} else {
		/* complete against relative path and executable name */
		if (!strchr (text, '/')) {
			executables = fill_executables (dialog->possible_executables,
							dialog->completion_items,
							text [0]);
			dirprefix = g_strdup ("");
		} else {
			dirprefix = g_path_get_dirname (text);
		}

		dirname = g_build_filename (g_get_home_dir (), dirprefix, NULL);
	}

	key = g_strdup_printf ("%s%c%c", dirprefix, G_DIR_SEPARATOR, prefix);

	if (!g_hash_table_lookup (dialog->dir_hash, key)) {
		g_hash_table_insert (dialog->dir_hash, key, dialog);

		list = fill_files_from (dirname, dirprefix, prefix,
					dialog->completion_items);
	} else {
		g_free (key);
	}

	list = g_list_concat (list, executables);

	g_free (dirname);
	g_free (dirprefix);

	if (list == NULL)
		return;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	g_completion_add_items (dialog->completion, list);
	G_GNUC_END_IGNORE_DEPRECATIONS

	dialog->completion_items = g_list_concat (dialog->completion_items,
						  list);	
}

static gboolean
entry_event (GtkEditable    *entry,
	     GdkEventKey    *event,
	     PanelRunDialog *dialog)
{
	GtkTreeSelection *selection;
	char             *prefix;
	char             *nospace_prefix;
	char             *nprefix;
	char             *temp;
	int               pos, tmp;
	int               text_len;

	if (event->type != GDK_KEY_PRESS)
		return FALSE;

	/* if user typed something we're not using the list anymore */
	dialog->use_program_list = FALSE;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->program_list));
	gtk_tree_selection_unselect_all (selection);

	if (!g_settings_get_boolean (dialog->run_settings,
				     PANEL_RUN_ENABLE_COMPLETION_KEY))
		return FALSE;

	text_len = strlen (gtk_entry_get_text (GTK_ENTRY (entry)));

	/* tab completion */
	if (event->keyval == GDK_KEY_Tab) {
		gtk_editable_get_selection_bounds (entry, &pos, &tmp);

		if (dialog->completion_started &&
		    pos != tmp &&
		    pos != 1 &&
		    tmp == text_len) {
	    		gtk_editable_select_region (entry, 0, 0);		
			gtk_editable_set_position (entry, -1);
			
			return TRUE;
		}
	} else if (event->length > 0) {
			   
		gtk_editable_get_selection_bounds (entry, &pos, &tmp);

		if (dialog->completion_started &&
		    pos != tmp &&
		    pos != 0 &&
		    tmp == text_len) {
			temp = gtk_editable_get_chars (entry, 0, pos);
			prefix = g_strconcat (temp, event->string, NULL);
			g_free (temp);
		} else if (pos == tmp &&
			   tmp == text_len) {
			prefix = g_strconcat (gtk_entry_get_text (GTK_ENTRY (entry)),
					      event->string, NULL);
		} else {
			return FALSE;
		}
		
		nospace_prefix = prefix;
		while (*nospace_prefix != '\0' &&
		       g_ascii_isspace (*nospace_prefix))
			nospace_prefix++;
		if (*nospace_prefix == '\0') {
			g_free (prefix);
			return FALSE;
		}

		panel_run_dialog_update_completion (dialog, nospace_prefix);
		
		if (!dialog->completion) {
			g_free (prefix);
			return FALSE;
		}
		
		pos = strlen (prefix);
		nprefix = NULL;

		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		g_completion_complete_utf8 (dialog->completion, nospace_prefix, &nprefix);
		G_GNUC_END_IGNORE_DEPRECATIONS

		if (nprefix) {
			int insertpos;
			insertpos = 0;

			temp = g_strndup (prefix, nospace_prefix - prefix);
			g_free (prefix);

			prefix = g_strconcat (temp, nprefix, NULL);

			g_signal_handler_block (dialog->combobox,
						dialog->changed_id);
			gtk_editable_delete_text (entry, 0, -1);
			g_signal_handler_unblock (dialog->combobox,
						  dialog->changed_id);

			gtk_editable_insert_text (entry,
						  prefix, strlen (prefix),
						  &insertpos);

 			gtk_editable_set_position (entry, pos);
			gtk_editable_select_region (entry, pos, -1);
			
			dialog->completion_started = TRUE;

			g_free (temp);
			g_free (nprefix);
			g_free (prefix);
			
			return TRUE;
		}
		
		g_free (prefix);
	}
	
	return FALSE;
}

static void
combobox_changed (GtkComboBox    *combobox,
		  PanelRunDialog *dialog)
{
	gboolean  program_list_enabled;
	char     *text;
	char     *start;
	char     *msg;

	program_list_enabled = g_settings_get_boolean (dialog->run_settings,
						       PANEL_RUN_ENABLE_LIST_KEY);

        text = g_strdup (panel_run_dialog_get_combo_text (dialog));

	start = text;
	while (*start != '\0' && g_ascii_isspace (*start))
		start++;

	/* update item name to use for dnd */
	if (!dialog->use_program_list) {
		if (dialog->desktop_path) {
			g_free (dialog->desktop_path);
			dialog->desktop_path = NULL;
		}
		if (dialog->item_name) {
			g_free (dialog->item_name);
			dialog->item_name = NULL;
		}
	}

	/* desensitize run button if no text entered */
	if (start[0] == '\0') {
		g_free (text);

		gtk_widget_set_sensitive (dialog->run_button, FALSE);
		gtk_drag_source_unset (dialog->run_dialog);

		if (program_list_enabled)
			gtk_label_set_text (GTK_LABEL (dialog->program_label),
					    _("Select an application to view its description."));

		panel_run_dialog_set_default_icon (dialog, FALSE);

		if (dialog->find_command_idle_id) {
			g_source_remove (dialog->find_command_idle_id);
			dialog->find_command_idle_id = 0;
		}

		if (program_list_enabled) {
			GtkTreeIter  iter;
			GtkTreePath *path;

			gtk_tree_model_foreach (GTK_TREE_MODEL (dialog->program_list_store),
						panel_run_dialog_make_all_list_visible,
						NULL);

			path = gtk_tree_path_new_first ();
			if (gtk_tree_model_get_iter (gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->program_list)),
						     &iter, path))
				gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (dialog->program_list),
							      path, NULL,
							      FALSE, 0, 0);
			gtk_tree_path_free (path);
		}

		return;
	}

	gtk_widget_set_sensitive (dialog->run_button, TRUE);
	gtk_drag_source_set (dialog->run_dialog,
			     GDK_BUTTON1_MASK,
			     NULL, 0,
			     GDK_ACTION_COPY);
	gtk_drag_source_add_uri_targets (dialog->run_dialog);

	if (program_list_enabled &&
	    !dialog->use_program_list) {
		msg = g_strdup_printf (_("Will run command: '%s'"),
				       start);
		gtk_label_set_text (GTK_LABEL (dialog->program_label), msg);
		g_free (msg);
	}
	
	/* look up icon for the command */
	if (program_list_enabled &&
	    !dialog->use_program_list &&
	    !dialog->find_command_idle_id)
		dialog->find_command_idle_id =
			g_idle_add_full (G_PRIORITY_LOW,
					 (GSourceFunc) panel_run_dialog_find_command_idle,
					 dialog, NULL);

	g_free (text);
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
	char **uris;
	char  *file;
	int    i;

        if (gtk_selection_data_get_format (selection_data) != 8 || gtk_selection_data_get_length (selection_data) == 0) {
        	g_warning (_("URI list dropped on run dialog had wrong format (%d) or length (%d)\n"),
			   gtk_selection_data_get_format (selection_data),
			   gtk_selection_data_get_length (selection_data));
		return;
        }

	uris = g_uri_list_extract_uris ((const char *)gtk_selection_data_get_data (selection_data));

	if (!uris) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	for (i = 0; uris [i]; i++) {
		if (!uris [i] || !uris [i][0])
			continue;
		
		file = g_filename_from_uri (uris [i], NULL, NULL);

		/* FIXME: I assume the file is in utf8 encoding if coming from a URI? */
		if (file) {
			panel_run_dialog_append_file_utf8 (dialog, file);
			g_free (file);
		} else
			panel_run_dialog_append_file_utf8 (dialog, uris [i]);
	}

	g_strfreev (uris);
	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
panel_run_dialog_setup_entry (PanelRunDialog *dialog,
			      GtkBuilder     *gui)
{
	GtkWidget             *entry;
	GdkRectangle           geometry;
	GdkMonitor            *monitor;
	
	dialog->combobox = PANEL_GTK_BUILDER_GET (gui, "comboboxentry");

	entry = gtk_bin_get_child (GTK_BIN (dialog->combobox));
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

	gtk_combo_box_set_model (GTK_COMBO_BOX (dialog->combobox),
				 _panel_run_get_recent_programs_list (dialog));
	gtk_combo_box_set_entry_text_column
		(GTK_COMBO_BOX (dialog->combobox), 0);

	monitor = gdk_display_get_primary_monitor (gdk_display_get_default ());

	if (!monitor)
		monitor = gdk_display_get_monitor (gdk_display_get_default (), 0);

	g_assert (monitor != NULL);

	gdk_monitor_get_geometry (monitor, &geometry);

	/* 1/4 the width of the monitor should be a good value */
	gtk_widget_set_size_request (dialog->combobox, geometry.width / 4, -1);

        g_signal_connect (entry, "key-press-event",
			  G_CALLBACK (entry_event), dialog);
			  
        dialog->changed_id = g_signal_connect (dialog->combobox, "changed",
					       G_CALLBACK (combobox_changed),
					       dialog);

	gtk_drag_dest_unset (dialog->combobox);
	
	gtk_drag_dest_set (dialog->combobox,
			   GTK_DEST_DEFAULT_MOTION|GTK_DEST_DEFAULT_HIGHLIGHT,
			   NULL, 0,
			   GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets (dialog->combobox);

	g_signal_connect (dialog->combobox, "drag_data_received",
			  G_CALLBACK (entry_drag_data_received), dialog);
}

static char *
panel_run_dialog_create_desktop_file (PanelRunDialog *dialog)
{
	GKeyFile *key_file;
	gboolean  exec = FALSE;
	char     *text;
	char     *name;
	char     *icon;
	char     *disk;
	char     *scheme;
	char     *save_uri;

        text = g_strdup (panel_run_dialog_get_combo_text (dialog));

	if (text[0] == '\0') {
		g_free (text);
		return NULL;
	}
		
	key_file = panel_key_file_new_desktop ();
	disk = g_locale_from_utf8 (text, -1, NULL, NULL, NULL);

	scheme = g_uri_parse_scheme (disk);
	/* if it's an absolute path or not a URI, it's possibly an executable */
	if (g_path_is_absolute (disk) || !scheme)
		exec = command_is_executable (disk, NULL, NULL);
	g_free (scheme);
		
	if (exec) {
		panel_key_file_set_string (key_file, "Type", "Application");
		panel_key_file_set_string (key_file, "Exec", text);
		name = g_strdup (text);
	} else {
		GFile *file;
		char  *uri;

		file = panel_util_get_file_optional_homedir (disk);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		panel_key_file_set_string (key_file, "Type", "Link");
		panel_key_file_set_string (key_file, "URL", uri);
		name = uri;
	}

	g_free (disk);

	panel_key_file_set_locale_string (key_file, "Name",
					  (dialog->item_name) ?
					  dialog->item_name : text);

	panel_key_file_set_boolean (key_file, "Terminal",
				    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->terminal_checkbox)));

	icon = NULL;
	if (dialog->gicon)
		icon = panel_util_get_icon_name_from_g_icon (dialog->gicon);
	if (icon != NULL) {
		panel_key_file_set_locale_string (key_file, "Icon",
						  icon);
		g_free (icon);
	} else
		panel_key_file_set_locale_string (key_file, "Icon",
						  PANEL_ICON_LAUNCHER);

	save_uri = panel_make_unique_desktop_uri (g_get_tmp_dir (), name);
	disk = g_filename_from_uri (save_uri, NULL, NULL);

	if (!disk || !panel_key_file_to_file (key_file, disk, NULL)) {
		g_free (save_uri);
		save_uri = NULL;
	}

	g_key_file_free (key_file);
	g_free (disk);
	g_free (name);
	g_free (text);

	return save_uri;
}

static void
pixmap_drag_data_get (GtkWidget          *run_dialog,
	  	      GdkDragContext     *context,
		      GtkSelectionData   *selection_data,
		      guint               info,
		      guint               time,
		      PanelRunDialog     *dialog)
{
	char *uri;

	if (dialog->use_program_list && dialog->desktop_path)
		uri = g_filename_to_uri (dialog->desktop_path, NULL, NULL);
	else
		uri = panel_run_dialog_create_desktop_file (dialog);

	if (uri) {
		gtk_selection_data_set (selection_data,
					gtk_selection_data_get_target (selection_data), 8,
					(unsigned char *) uri, strlen (uri));
		g_free (uri);
	}
}

static void
panel_run_dialog_setup_pixmap (PanelRunDialog *dialog,
			       GtkBuilder     *gui)
{
	dialog->pixmap = PANEL_GTK_BUILDER_GET (gui, "icon_pixmap");
	
	g_signal_connect (dialog->run_dialog, "drag_data_get",
			  G_CALLBACK (pixmap_drag_data_get),
			  dialog);
}

static PanelRunDialog *
panel_run_dialog_new (GdkScreen  *screen,
		      GtkBuilder *gui,
		      guint32    activate_time)
{
	PanelRunDialog *dialog;

	dialog = g_new0 (PanelRunDialog, 1);

	dialog->run_dialog = PANEL_GTK_BUILDER_GET (gui, "panel_run_dialog");

	dialog->run_settings = g_settings_new (PANEL_RUN_SCHEMA);
	
	g_signal_connect_swapped (dialog->run_dialog, "response",
				  G_CALLBACK (panel_run_dialog_response), dialog);
				  
	g_signal_connect_swapped (dialog->run_dialog, "destroy",
				  G_CALLBACK (panel_run_dialog_destroy), dialog);

	dialog->run_button = PANEL_GTK_BUILDER_GET (gui, "run_button");
	dialog->terminal_checkbox = PANEL_GTK_BUILDER_GET (gui, "terminal_checkbox");
	
	panel_run_dialog_setup_pixmap        (dialog, gui);
	panel_run_dialog_setup_entry         (dialog, gui);
	panel_run_dialog_setup_file_button   (dialog, gui);
	panel_run_dialog_setup_program_list  (dialog, gui);
	panel_run_dialog_setup_list_expander (dialog, gui);

	gtk_window_set_icon_name (GTK_WINDOW (dialog->run_dialog),
				  PANEL_ICON_RUN);
	panel_run_dialog_set_default_icon (dialog, FALSE);

	g_signal_connect (dialog->run_settings, "changed::"PANEL_RUN_ENABLE_LIST_KEY,
			  G_CALLBACK (panel_run_dialog_update_program_list), dialog);
	g_signal_connect (dialog->run_settings, "changed::"PANEL_RUN_SHOW_LIST_KEY,
			  G_CALLBACK (panel_run_dialog_update_program_list), dialog);

	panel_run_dialog_update_program_list (dialog->run_settings, NULL, dialog);

	gtk_widget_set_sensitive (dialog->run_button, FALSE);
	
	gtk_dialog_set_default_response (GTK_DIALOG (dialog->run_dialog),
					 GTK_RESPONSE_OK);

	gtk_window_set_screen (GTK_WINDOW (dialog->run_dialog), screen);

	gtk_widget_grab_focus (dialog->combobox);
	gtk_widget_realize (dialog->run_dialog);
	gdk_x11_window_set_user_time (gtk_widget_get_window (dialog->run_dialog),
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
	GtkBuilder *gui;

	if (panel_lockdown_get_disable_command_line_s ())
		return;

	if (static_dialog) {
		gtk_window_set_screen (GTK_WINDOW (static_dialog->run_dialog), screen);
		gtk_window_present_with_time (GTK_WINDOW (static_dialog->run_dialog),
					      activate_time);
		gtk_widget_grab_focus (static_dialog->combobox);
		return;
	}

	gui = gtk_builder_new ();
	gtk_builder_set_translation_domain (gui, GETTEXT_PACKAGE);
	gtk_builder_add_from_resource (gui,
				       PANEL_RESOURCE_PATH "panel-run-dialog.ui",
				       NULL);

	static_dialog = panel_run_dialog_new (screen, gui, activate_time);

	g_signal_connect_swapped (static_dialog->run_dialog, "destroy",
				  G_CALLBACK (panel_run_dialog_static_dialog_destroyed),
				  static_dialog);

	g_object_unref (gui);
}
