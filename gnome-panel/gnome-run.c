/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 *   grun: Popup a command dialog. Original version by Elliot Lee, 
 *    bloatware edition by Havoc Pennington. Both versions written in 10
 *    minutes or less. :-)
 *   Copyright (C) 1998 Havoc Pennington <hp@pobox.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

#include "gnome-run.h"
#include "disclosure-widget.h"
#include "menu-fentry.h"
#include "menu.h"
#include "multiscreen-stuff.h"
#include "panel-main.h"
#include "panel-util.h"
#include "panel-gconf.h"
#include "quick-desktop-reader.h"
#include "nothing.h"
#include "egg-screen-exec.h"
#include "egg-screen-url.h"
#include "panel-stock-icons.h"

enum {
	COLUMN_ICON,
	COLUMN_ICON_FILE,
	COLUMN_FULLNAME,
	COLUMN_COMMENT,
	COLUMN_NAME,
	COLUMN_EXEC,
	NUM_COLUMNS
};

typedef enum {
	PANEL_RESPONSE_RUN
} PanelResponseType;

#define ENABLE_LIST_DEFAULT TRUE
#define SHOW_LIST_DEFAULT FALSE

extern GtkTooltips *panel_tooltips;
extern gboolean no_run_box;

static GtkWidget *run_dialog = NULL;
static GSList *add_icon_paths = NULL;
static guint add_icon_idle_id = 0;
static guint add_items_idle_id = 0;
static guint find_icon_timeout_id = 0;

static GList *executables = NULL;
static GCompletion *exe_completion = NULL;

static void       update_contents          (GtkWidget *dialog);
static void       unset_selected           (GtkWidget *dialog);
static void	  unset_pixmap		   (GtkWidget *gpixmap);
static gboolean	  find_icon_timeout	   (gpointer   data);

static void
fill_executables_from (const char *dirname)
{
	struct dirent *dent;
	DIR *dir;

	dir = opendir (dirname);

	if (dir == NULL)
		return;

	while ( (dent = readdir (dir)) != NULL) {
		char *file = g_strconcat (dirname, "/", dent->d_name, NULL);

		if (access (file, X_OK) == 0)
			executables = g_list_prepend (executables,
						      g_strdup (dent->d_name));
	}

	closedir (dir);
}

static void
fill_executables (void)
{
	int i;
	const char *path;
	char **pathv;

	panel_g_list_deep_free (executables);
	executables = NULL;

	path = g_getenv ("PATH");

	if (path == NULL ||
	    path[0] == '\0')
		return;

	pathv = g_strsplit (path, ":", 0);

	for (i = 0; pathv[i] != NULL; i++)
		fill_executables_from (pathv[i]);

	g_strfreev (pathv);
}

static void
ensure_completion (void)
{
	if (exe_completion == NULL) {
		exe_completion = g_completion_new (NULL);
		fill_executables ();

		g_completion_add_items (exe_completion, executables);
	}
}

static void
kill_completion (void)
{
	if (executables != NULL) {
		panel_g_list_deep_free (executables);
		executables = NULL;
	}

	if (exe_completion != NULL) {
		g_completion_free (exe_completion);
		exe_completion = NULL;
	}
}

static void
get_environment (int         *argc,
		 char      ***argv,
		 int         *envc,
		 char      ***envv,
		 GdkScreen   *screen)
{
	GList    *envar = NULL, *li;
	gboolean  display_found = FALSE;
	int       i, moveby;

	*envv = NULL;
	*envc = 0;

	moveby = 0;
	for (i = 0; i < *argc; i++) {
		if (!strchr ((*argv) [i], '='))
			break;

		if (!strncmp ((*argv) [i], "DISPLAY", 7))
			display_found = TRUE;

		envar = g_list_append (envar, g_strdup ((*argv)[i]));
		moveby ++;
	}

	if (!display_found && gdk_screen_get_default () != screen)
		envar = g_list_append (
				envar, egg_screen_exec_display_string (screen));

	if (moveby == *argc) {
		panel_g_list_deep_free (envar);
		return;
	}

	if (envar == NULL)
		return;

	for (i = 0; i < *argc && moveby; i++) {
		g_free ((*argv)[i]);
		if (i + moveby < *argc) {
			(*argv)[i] = (*argv)[i+moveby];
			(*argv)[i+moveby] = NULL;
		} else {
			(*argv)[i] = NULL;
		}
	}
	*argc -= moveby;

	*envc = g_list_length (envar);
	*envv = g_new0 (char *, *envc + 1);
	for (i = 0, li = envar; li != NULL; li = li->next, i++) {
		(*envv)[i] = li->data;
		li->data = NULL;
	}	
	(*envv)[i] = NULL;
	g_list_free (envar);
}

static void
launch_selected (GtkTreeModel *model,
		 GtkTreeIter  *iter,
		 GtkWidget    *dialog)
{
	GnomeDesktopItem *ditem;
	GnomeEntry	 *gnome_entry;
	GtkWidget 	 *entry;
	GError           *error = NULL;
	GtkToggleButton  *terminal;
	char             *name, *command;

	gtk_tree_model_get (model, iter, COLUMN_NAME, &name, -1);

	if (!name)
		return;
                        
	ditem = gnome_desktop_item_new_from_uri (name,
						 GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS,
						 &error);

	g_free (name);

	if (!ditem) {
		panel_error_dialog (
			gtk_window_get_screen (GTK_WINDOW (run_dialog)),
			"failed_to_load_desktop",
			_("<b>Failed to run this program</b>\n\nDetails: %s"),
			error->message);
		g_clear_error (&error);
		return;
	}

        terminal = GTK_TOGGLE_BUTTON (
			g_object_get_data (G_OBJECT (dialog), "terminal"));

	/* Honor "run in terminal" button */
	gnome_desktop_item_set_boolean (ditem,
					GNOME_DESKTOP_ITEM_TERMINAL,
					terminal->active);

	if (!panel_ditem_launch (
			ditem, NULL, 0,
			gtk_window_get_screen (GTK_WINDOW (run_dialog)),
			 &error)) {
		panel_error_dialog (
			gtk_window_get_screen (GTK_WINDOW (run_dialog)),
			"failed_to_load_desktop",
			_("<b>Failed to run this program</b>\n\nDetails: %s"),
			error->message);
		g_clear_error (&error);
	}

	/* save command history */
	gnome_entry = g_object_get_data (G_OBJECT (dialog), "gnome_entry");
	entry = g_object_get_data (G_OBJECT (dialog), "entry");
	command = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);	
	gnome_entry_prepend_history (gnome_entry, TRUE, command);
	g_free (command);
	
	gnome_desktop_item_unref (ditem);
}

static void 
run_dialog_response (GtkWidget *w, int response, gpointer data)
{
	GtkWidget *entry;
        GtkWidget *list;
	GnomeEntry *gnome_entry;
	char **argv = NULL;
	char **temp_argv = NULL;
	int argc, temp_argc;
	char *s = NULL;
	char *escaped = NULL;
	char *disk = NULL;
	char **envv = NULL;
	int envc;
	GError *error = NULL;

	if (response == GTK_RESPONSE_HELP) {
		panel_show_help (
			gtk_window_get_screen (GTK_WINDOW (w)),
			"wgoseditmainmenu.xml", "gospanel-23");
		/* just return as we don't want to close */
		return;
	} else if (response != PANEL_RESPONSE_RUN) {
		goto return_and_close;
	}
        
        if (g_object_get_data (G_OBJECT (run_dialog), "use_list")) {
		GtkTreeSelection *selection;
		GtkTreeModel *model;
		GtkTreeIter iter;

	        list = g_object_get_data (G_OBJECT (run_dialog), "program_list");
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));

		/* just return if nothing selected */
		if ( ! gtk_tree_selection_get_selected (selection, 
							&model, &iter))
			return;

		launch_selected (model, &iter, w);
        } else {
	
		GtkToggleButton *terminal;

                entry = g_object_get_data (G_OBJECT (w), "entry");

                s = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

                if (string_empty (s))
                        goto return_and_close;

		escaped = g_markup_escape_text (s, -1);
		disk = g_locale_from_utf8 (s, -1, NULL, NULL, NULL);

		/* save command in history */
		gnome_entry = g_object_get_data (G_OBJECT (w), "gnome_entry");
		gnome_entry_prepend_history (gnome_entry, TRUE, s);

                /* evil eggies, do not translate! */
                if (strcmp (s, "you shall bring us a shrubbery") == 0) {
                        panel_info_dialog (
				gtk_window_get_screen (GTK_WINDOW (run_dialog)),
				"ni_ni_ni_ni",
				"NI! NI! NI! NI! NI! NI!");
                        goto return_and_close;
                } else if (strcmp (s, "supreme executive power") == 0) {
                        panel_info_dialog (
				gtk_window_get_screen (GTK_WINDOW (run_dialog)),
				"evil",
				"Listen -- strange women lying in\n"
				"ponds distributing swords is no\n"
				"basis for a system of government.\n"
				"Supreme executive power derives from\n"
				"a mandate from the masses, not from\n"
				"some farcical aquatic ceremony!");
                        goto return_and_close;
                } else if (strcmp (s, "free the fish") == 0) {
			start_screen_check ();
                        goto return_and_close;
		} else if (strcmp (s, "gegls from outer space") == 0) {
			start_geginv ();
                        goto return_and_close;
		} else if (strcmp (s, "End world hunger") == 0) {
			egg_url_show_on_screen (
				"http://www.wfp.org",
				gtk_window_get_screen (GTK_WINDOW (run_dialog)),
				NULL);
                        goto return_and_close;
		}

                /* Somewhat of a hack I suppose */
                if (panel_is_url (s)) {
			/* FIXME: URLs are in UTF8 ... right? */
                        egg_url_show_on_screen (
				s, gtk_window_get_screen (GTK_WINDOW (run_dialog)), NULL);
                        goto return_and_close;
                }

		/* Note, the command is taken to have to be in disk encoding
		 * even though it could contain strings, but more likely
		 * it is all filenames and thus should be in disk encoding */
                if ( ! g_shell_parse_argv (disk, &temp_argc, &temp_argv, &error)) {
                        g_clear_error (&error);
                        error = NULL;
                        if ( ! g_shell_parse_argv (s, &temp_argc, &temp_argv, &error)) {
                                panel_error_dialog (
                                                    gtk_window_get_screen (GTK_WINDOW (run_dialog)),
                                                    "run_error",
                                                    _("<b>Failed to execute command:</b> '%s'\n\nDetails: %s"),
                                                    escaped, error->message);
                                g_clear_error (&error);
                                goto return_and_close;
                        }
                }

                get_environment (
			&temp_argc, &temp_argv, &envc, &envv,
			gtk_window_get_screen (GTK_WINDOW (run_dialog)));

		terminal = GTK_TOGGLE_BUTTON (
				g_object_get_data (G_OBJECT (w), "terminal"));

                if (terminal->active) {
                        char **term_argv;
                        int term_argc;
                        gnome_config_get_vector ("/Gnome/Applications/Terminal",
                                                 &term_argc, &term_argv);
                        if (term_argv) {
                                int i;
                                argv = g_new(char *, term_argc + temp_argc + 1);
                                argc = term_argc + temp_argc;
                                for(i = 0; i < term_argc; i++) {
                                        argv[i] = term_argv[i];
                                        term_argv[i] = NULL;
                                }
                                for(i = term_argc; i < term_argc+temp_argc; i++) {
                                        argv[i] = temp_argv[i-term_argc];
                                        temp_argv[i-term_argc] = NULL;
				}
                                argv[i] = NULL;
				g_free (term_argv);
                        } else {
                                char *check;
                                int i;
                                check = g_find_program_in_path ("gnome-terminal");
                                argv = g_new(char *, 2 + temp_argc + 1);
                                argc = 2 + temp_argc;
                                if(!check) {
                                        argv[0] = g_strdup ("xterm");
                                        argv[1] = g_strdup ("-e");
                                } else {
                                        argv[0] = check;
                                        argv[1] = g_strdup ("-x");
                                }
                                for(i = 2; i < 2+temp_argc; i++) {
                                        argv[i] = temp_argv[i-2];
                                        temp_argv[i-2] = NULL;
				}
                                argv[i] = NULL;
                        }
                } else {
                        argv = temp_argv;
                        temp_argv = NULL;
                        argc = temp_argc;
                }

                if (gnome_execute_async_with_env (g_get_home_dir (),
                                                  argc, argv,
                                                  envc, envv) < 0) {

			/* if all else fails we try to open the file with an app */
			char *path;
			char *command = NULL;
			GError *error = NULL;
			GnomeVFSFileInfo *info = NULL;

			if (!g_path_is_absolute (s)) {
				path = g_strconcat (g_get_home_dir (), "/", s, NULL);
			} else {
				path = g_strdup (s);
			}
			
			info = gnome_vfs_file_info_new ();
			if (gnome_vfs_get_file_info (path,info,
						     GNOME_VFS_FILE_INFO_FOLLOW_LINKS) != GNOME_VFS_OK) {
				panel_error_dialog(
					gtk_window_get_screen (GTK_WINDOW (run_dialog)),
					"run_error",
					_("<b>Failed to execute command:</b> '%s'\n\nDetails: %s"),
					escaped, g_strerror (errno));
				g_free (path);
				gnome_vfs_file_info_unref (info);
				goto return_and_close;
			}
			
			if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
				command = g_strconcat ("nautilus ", path, NULL);
			} else {
				char *mime_info;
				GnomeVFSMimeApplication *app;				
				
				mime_info = gnome_vfs_get_mime_type (path);
				app = gnome_vfs_mime_get_default_application (mime_info);
						
				if (app != NULL) {
					command = g_strconcat (app->command, " ", path, NULL);
				}
				
				gnome_vfs_mime_application_free (app);
				g_free (mime_info);				
				
				if (command == NULL) {
					panel_error_dialog (
						gtk_window_get_screen (GTK_WINDOW (run_dialog)),
						"run_error",
						_("<b>Failed to open file:</b> '%s'\n\n"
						  "Details: no application available to open file"),
						escaped);

					gnome_vfs_file_info_unref (info);
					g_free (path);
					goto return_and_close;
				}
			}
			
			if (!egg_screen_execute_command_line_async (
					gtk_window_get_screen (GTK_WINDOW (run_dialog)), command, &error)) {
				panel_error_dialog (
					gtk_window_get_screen (GTK_WINDOW (run_dialog)),
					"run_error",
					_("<b>Failed to open file:</b> '%s'\n\nDetails: %s"),
					escaped, error->message);
				g_clear_error (&error);
			}
	
			gnome_vfs_file_info_unref (info);
			g_free (path);
			g_free (command);
                }
        }
        
return_and_close:
	g_strfreev (argv);
	g_strfreev (temp_argv);
	g_strfreev (envv);
	g_free (s);
	g_free (escaped);
	g_free (disk);

	gtk_widget_destroy (w);
        
}

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
append_file_utf8 (GtkWidget *entry, const char *file)
{
	const char *text;
	char *quoted = quote_string (file);
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (string_empty (text)) {
		gtk_entry_set_text (GTK_ENTRY (entry), quoted);
	} else {
		char *new = g_strconcat (text, " ", quoted, NULL);
		gtk_entry_set_text (GTK_ENTRY (entry), new);
		g_free (new);
	}
	g_free (quoted);
}

static void
append_file (GtkWidget *entry, const char *file)
{
	char *utf8_file = g_filename_to_utf8 (file, -1, NULL, NULL, NULL);
	if (utf8_file != NULL) {
		append_file_utf8 (entry, utf8_file);
		g_free (utf8_file);
	}
}

static void
browse_ok (GtkWidget *widget, GtkFileSelection *fsel)
{
	const char *fname;
	GtkWidget *entry;

	g_return_if_fail (GTK_IS_FILE_SELECTION (fsel));

	entry = g_object_get_data (G_OBJECT (fsel), "entry");

	fname = gtk_file_selection_get_filename (fsel);
	if (fname != NULL) {
		append_file (entry, fname);
	}
	
	gtk_widget_destroy (GTK_WIDGET (fsel));
}

static void
browse (GtkWidget *w, GtkWidget *entry)
{
	char *home;
	GtkFileSelection *fsel;

	fsel = GTK_FILE_SELECTION (gtk_file_selection_new (_("Choose a file")));
	
	home = g_strconcat (g_get_home_dir (), "/", NULL);
	gtk_file_selection_set_filename (fsel, home);
	g_free (home);
	
	gtk_window_set_transient_for (GTK_WINDOW (fsel),
				      GTK_WINDOW (run_dialog));
	g_object_set_data (G_OBJECT (fsel), "entry", entry);

	g_signal_connect (G_OBJECT (fsel->ok_button), "clicked",
			  G_CALLBACK (browse_ok), fsel);
	g_signal_connect_swapped (G_OBJECT (fsel->cancel_button), "clicked",
		 		  G_CALLBACK (gtk_widget_destroy), 
		 		  G_OBJECT (fsel));
	panel_signal_connect_object_while_alive
		(G_OBJECT (entry), "destroy",
		 G_CALLBACK (gtk_widget_destroy),
		 G_OBJECT (fsel));

	gtk_window_present (GTK_WINDOW (fsel));
}

static gboolean
entry_event (GtkEntry * entry, GdkEventKey * event, gpointer data)
{
	if (event->type != GDK_KEY_PRESS)
		return FALSE;

	/* completion */
	if ((event->keyval == GDK_Tab) &&
	    (event->state & GDK_CONTROL_MASK)) {
		gchar* prefix;
		gchar* nprefix = NULL;
		gint pos;

		ensure_completion ();

		pos = gtk_editable_get_position (GTK_EDITABLE (entry));
		prefix = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, pos);

		g_completion_complete (exe_completion, prefix, &nprefix);

		if (nprefix != NULL &&
		    strlen (nprefix) > strlen (prefix)) {
			gtk_editable_insert_text (GTK_EDITABLE (entry),
						  nprefix + pos, 
						  strlen (nprefix) -
						    strlen (prefix),
						  &pos);
			gtk_editable_set_position (GTK_EDITABLE (entry), pos);
		} else {
                        gdk_beep ();
                }

		g_free (nprefix);
		g_free (prefix);

		return TRUE;
	}

	return FALSE;
}

static void
sync_entry_to_list (GtkWidget *dialog)
{
        gboolean blocked;
	gboolean enable_program_list;
	GtkWidget *entry;
	const char *key;

        blocked = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog),
						      "sync_entry_to_list_blocked"));
        if (blocked)
                return;

	key = panel_gconf_general_key
		(panel_gconf_get_profile (), "enable_program_list"),
	enable_program_list = panel_gconf_get_bool (key, ENABLE_LIST_DEFAULT);
	
	if (enable_program_list) {
	        unset_selected (dialog);

		entry = g_object_get_data (G_OBJECT (dialog), "entry");
	
		if (find_icon_timeout_id != 0) {
			/* already a timeout registered so delay it for another half-second. */
			g_source_remove (find_icon_timeout_id);
			find_icon_timeout_id =
				g_idle_add_full (G_PRIORITY_LOW, find_icon_timeout,
                                                 entry, NULL);		
		} else {
			/* no timeout registered so start a new one. */
			find_icon_timeout_id =
				g_idle_add_full (G_PRIORITY_LOW, find_icon_timeout,
                                                 entry, NULL);	
		}
	}
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
sync_list_to_entry (GtkWidget *dialog)
{
        GtkWidget *list;
        GtkWidget *entry;
        GtkWidget *terminal_toggle;
        gchar *name;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

        g_object_set_data (G_OBJECT (dialog),
			   "sync_entry_to_list_blocked",
			   GINT_TO_POINTER (TRUE));
        
        list = g_object_get_data (G_OBJECT (dialog), "program_list");
        entry = g_object_get_data (G_OBJECT (dialog), "entry");
        terminal_toggle = g_object_get_data (G_OBJECT (dialog), "terminal");

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GValue value = {0, };

		gtk_tree_model_get_value (model, &iter,
					  COLUMN_NAME,
					  &value);
		name = g_strdup (g_value_get_string (&value));
		g_value_unset (&value);

                if (name != NULL) {
                        GnomeDesktopItem *ditem;

                        ditem = gnome_desktop_item_new_from_uri (name,
								 GNOME_DESKTOP_ITEM_LOAD_NO_TRANSLATIONS,
								 NULL /* error */);
                        if (ditem != NULL) {
				gboolean terminal;
                                const char *exec;

				exec = gnome_desktop_item_get_string (
						ditem, GNOME_DESKTOP_ITEM_EXEC);
				if (exec) {
					char *stripped;

					stripped = remove_parameters (exec);

					gtk_entry_set_text (GTK_ENTRY (entry), stripped);

					g_free (stripped);
				} else {
					exec = gnome_desktop_item_get_string (
							ditem, GNOME_DESKTOP_ITEM_URL);
					gtk_entry_set_text (GTK_ENTRY (entry), sure_string (exec));
				}

				terminal = gnome_desktop_item_get_boolean (
							ditem, GNOME_DESKTOP_ITEM_TERMINAL);

                                gtk_toggle_button_set_active (
					GTK_TOGGLE_BUTTON (terminal_toggle), terminal);
				
                                gnome_desktop_item_unref (ditem);
                        }

			g_free (name);
                }
        }

	g_object_set_data (G_OBJECT (dialog),
			   "sync_entry_to_list_blocked",
			   GINT_TO_POINTER (FALSE));

	g_object_set_data (G_OBJECT (dialog), "use_list",
			   GINT_TO_POINTER (TRUE));
}

static void
toggle_contents (GtkWidget *disclosure,
                 GtkWidget *dialog)
{
	const char *key;

	key = panel_gconf_general_key
		(panel_gconf_get_profile (), "show_program_list"),

	panel_gconf_set_bool (key, GTK_TOGGLE_BUTTON (disclosure)->active);

	/* FIXME: we need to listen on this key! */
        update_contents (dialog);
}

static GtkWidget*
create_disclosure_widget (void)
{
        GtkWidget *disclosure;
        gboolean show_program_list;
	const char *key;

        disclosure = cddb_disclosure_new (_("Known Applications"),
					  _("Known Applications"));

	key = panel_gconf_general_key
		(panel_gconf_get_profile (), "show_program_list"),
	show_program_list = panel_gconf_get_bool (key, SHOW_LIST_DEFAULT);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (disclosure),
				      show_program_list);
		
	g_object_set_data (G_OBJECT (run_dialog), "disclosure", disclosure);
	  
        g_signal_connect (G_OBJECT (disclosure), "toggled",
			  G_CALLBACK (toggle_contents),
			  run_dialog);

        return disclosure;
}

static void
entry_changed (GtkWidget *entry,
               gpointer   data)
{
	GtkWidget *button;
	char *text;
	
	/* desensitize run button if no text entered */
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	button = g_object_get_data (G_OBJECT (run_dialog), "run_button");
	if (strlen (text) == 0) {
		gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
	}
	g_free (text);

	if (run_dialog != NULL)
		sync_entry_to_list (run_dialog);
}

static void
activate_run (GtkWidget *entry, gpointer data)
{
	if (run_dialog != NULL)
		gtk_dialog_response (GTK_DIALOG (run_dialog),
				     PANEL_RESPONSE_RUN);
}

static void
drag_data_received (GtkWidget        *widget,
		    GdkDragContext   *context,
		    gint              x,
		    gint              y,
		    GtkSelectionData *selection_data,
		    guint             info,
		    guint32           time,
		    gpointer          data)
{
	GtkWidget *entry = data;
	char **uris;
	int i;

	uris = g_strsplit (selection_data->data, "\r\n", -1);

	if (uris == NULL) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	for (i = 0; uris[i] != NULL; i++) {
		char *file = gnome_vfs_get_local_path_from_uri (uris[i]);

		/* FIXME: I assume the file is in utf8 encoding if coming
		 * from a URI? */
		if (file != NULL) {
			append_file_utf8 (entry, file);
			g_free (file);
		} else {
			append_file_utf8 (entry, uris[i]);
		}
	}

	g_strfreev (uris);

	gtk_drag_finish (context, TRUE, FALSE, time);
}

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

static GtkWidget*
create_simple_contents (GdkScreen *screen)
{
        GtkWidget *vbox;
        GtkWidget *entry;
        GtkWidget *gentry;
	GtkWidget *pixmap;
        GtkWidget *hbox;
	GtkWidget *vbox2;
	GtkWidget *hbox2;
        GtkWidget *w;
	const char *key;
	gboolean enable_program_list;
	int width_request;
	static GtkTargetEntry drop_types[] = { { "text/uri-list", 0, 0 } };
        
        vbox = gtk_vbox_new (FALSE, 0);

        hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), hbox,
                            TRUE, TRUE, GNOME_PAD_SMALL);
			    
        w = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
        pixmap = gtk_image_new ();
	g_object_set_data (G_OBJECT (run_dialog), "pixmap", pixmap);
	gtk_container_add (GTK_CONTAINER (w), pixmap);
        gtk_box_pack_start (GTK_BOX (hbox), w,
			    FALSE, FALSE, 10);
        unset_pixmap (pixmap);
	
	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), vbox2,
			    TRUE, TRUE, GNOME_PAD_SMALL);
	
        gentry = gnome_entry_new ("gnome-run");
        gtk_box_pack_start (GTK_BOX (vbox2), gentry,
			    TRUE, TRUE, GNOME_PAD_SMALL);

        /* 1/4 the width of the first monitor should be a good value */
	width_request = multiscreen_width (gdk_screen_get_number (screen), 0) / 4;
	g_object_set (G_OBJECT (gentry), "width_request", width_request, NULL);

        entry = gnome_entry_gtk_entry (GNOME_ENTRY (gentry));
	gtk_tooltips_set_tip (panel_tooltips, entry, _("Command to run"), NULL);
        gtk_combo_set_use_arrows_always (GTK_COMBO (gentry), TRUE);
        g_object_set_data (G_OBJECT (run_dialog), "entry", entry);
	g_object_set_data (G_OBJECT (run_dialog), "gnome_entry", gentry);

        g_signal_connect (G_OBJECT (entry), "event",
			  G_CALLBACK (entry_event),
			  NULL);
	g_signal_connect (G_OBJECT (entry), "destroy",
			  G_CALLBACK (kill_completion),
			  NULL);

	g_signal_connect (G_OBJECT (entry), "activate",
			  G_CALLBACK (activate_run),
			  NULL);
			  
        g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (entry_changed),
			  NULL);

	gtk_drag_dest_unset (entry);
	gtk_drag_dest_set (gentry,
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, ELEMENTS (drop_types), GDK_ACTION_COPY);

	g_signal_connect (gentry, "drag_data_received",
			  G_CALLBACK (drag_data_received),
			  entry);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox2,
			    TRUE, TRUE, GNOME_PAD_SMALL);

	hbox = gtk_hbox_new (FALSE, 0);

        w = gtk_check_button_new_with_mnemonic(_("Run in _terminal"));
	g_object_set_data (G_OBJECT (run_dialog), "terminal", w);
        gtk_box_pack_start (GTK_BOX (hbox2), w,
                            TRUE, TRUE, 0);

        w = gtk_button_new_with_mnemonic (_("_Append File..."));
        g_signal_connect(G_OBJECT(w), "clicked",
                         G_CALLBACK (browse), entry);
	gtk_box_pack_start (GTK_BOX (hbox2), w,
			    FALSE, FALSE, 0);

	key = panel_gconf_general_key
		(panel_gconf_get_profile (), "enable_program_list"),
	enable_program_list = panel_gconf_get_bool (key, ENABLE_LIST_DEFAULT);

	/* only create disclosure widget if really needed */
	if (enable_program_list) {
	        w = create_disclosure_widget ();
		gtk_box_pack_start (GTK_BOX (vbox), w,
				    FALSE, FALSE, GNOME_PAD_SMALL);
	}

        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (run_dialog)->vbox),
                            vbox,
                            FALSE, FALSE, 0);

        g_object_set_data_full (G_OBJECT (run_dialog),
				"advanced-entry",
				g_object_ref (entry),
				(GDestroyNotify) g_object_unref);
	
	gtk_widget_show_all (vbox);
        
        return vbox;
}

static void
add_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new ();
                
        gtk_tree_view_column_pack_start (column, renderer, FALSE);
        gtk_tree_view_column_set_attributes (column, renderer,
                                             "pixbuf", COLUMN_ICON,
                                             NULL);
        
	renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_start (column, renderer, TRUE);

        gtk_tree_view_column_set_attributes (column, renderer,
                                             "text", COLUMN_FULLNAME,
                                             NULL);
	gtk_tree_view_append_column (treeview, column);
}

static gboolean
fuzzy_command_match (const char *cmd1, const char *cmd2, gboolean *fuzzy)
{
	char **tokens;
	char *word1, *word2;

	if (strcmp (cmd1, cmd2) == 0) {
		*fuzzy = FALSE;
		return TRUE;
	}

	/* find basename of exec from desktop item.
	   strip of all arguments after the initial command */
	tokens = g_strsplit (cmd1, " ", -1);
	if (tokens == NULL || tokens[0] == NULL) {
		g_strfreev (tokens);
		return FALSE;
	}
	word1 = g_path_get_basename (tokens[0]);
	g_strfreev (tokens);

	/* same for the user command */
	tokens = g_strsplit (cmd2, " ", -1);
	word2 = g_path_get_basename (tokens[0]);
	if (tokens == NULL || tokens[0] == NULL) {
		g_free (word1);
		g_strfreev (tokens);
		return FALSE;
	}
	g_strfreev (tokens);

	if (strcmp (word1, word2) == 0) {
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
find_icon_timeout (gpointer data)
{
	GtkWidget *entry = data;
	GdkPixbuf *pixbuf;
	GtkListStore *list;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkWidget *pixmap;
	GValue value = {0};
	char *exec, *icon;
	char *found_icon = NULL;

	pixmap = g_object_get_data (G_OBJECT (run_dialog), "pixmap");
	list = g_object_get_data (G_OBJECT (run_dialog), "program_list");
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));
	path = gtk_tree_path_new_root ();
	
	if (path == NULL ||
	     ! gtk_tree_model_get_iter (model, &iter, path)) {
		if (path != NULL)
			gtk_tree_path_free (path);
		unset_pixmap (pixmap);
	
		find_icon_timeout_id = 0;
		return FALSE;
	}

	do {
		gtk_tree_model_get_value (model, &iter,
					  COLUMN_EXEC,
					  &value);
				  
		exec = g_strdup (g_value_get_string (&value));
		g_value_unset (&value);

		gtk_tree_model_get_value (model, &iter,
					  COLUMN_ICON_FILE,
					  &value);
				  
		icon = g_strdup (g_value_get_string (&value));
		g_value_unset (&value);

        	if (exec != NULL && icon != NULL) {
			const char *text;
			gboolean fuzzy = FALSE;
			
			text = gtk_entry_get_text (GTK_ENTRY (entry));

			if (fuzzy_command_match (sure_string (text),
						 exec, &fuzzy)) {
				g_free (found_icon);
				found_icon = g_strdup (icon);
				if ( ! fuzzy) {
					/* if not fuzzy then we have a precise
					 * match and we can quit, else keep
					 * searching for a better match */
					g_free (exec);
					g_free (icon);
					break;
				}
			}
		}
		g_free (exec);
		g_free (icon);
	
        } while (gtk_tree_model_iter_next (model, &iter));

	gtk_tree_path_free (path);

	pixbuf = NULL;
	if (found_icon != NULL) {
		icon = gnome_desktop_item_find_icon (panel_icon_theme,
                                                     found_icon,
						     48 /* desired size */,
						     0 /* flags */);
		if (icon != NULL) {
			pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
			g_free (icon);
			if (pixbuf != NULL)
				gtk_image_set_from_pixbuf (GTK_IMAGE (pixmap),
							   pixbuf);
		}
		g_free (found_icon);
	}

	if (pixbuf == NULL)
		unset_pixmap (pixmap);
	
	find_icon_timeout_id = 0;
	return FALSE;
}

static gboolean
add_icon_idle (GtkListStore *list)
{
	GtkTreeIter  iter;
	GtkTreePath *path;
	gboolean     long_operation = FALSE;
	GdkPixbuf   *pixbuf;
	char        *file;
	int          icon_height;

	do {
		if (add_icon_paths == NULL) {
			add_icon_idle_id = 0;
			return FALSE;
		}

		path = add_icon_paths->data;
		add_icon_paths->data = NULL;
		add_icon_paths = g_slist_delete_link (add_icon_paths,
						      add_icon_paths);

		if ( ! gtk_tree_model_get_iter (GTK_TREE_MODEL (list),
						&iter,
						path)) {
			gtk_tree_path_free (path);
			continue;
		}
		gtk_tree_path_free (path);

		gtk_tree_model_get (GTK_TREE_MODEL (list), &iter,
				    COLUMN_ICON_FILE, &file, -1);

		if (!gtk_icon_size_lookup (panel_menu_icon_get_size (), NULL, &icon_height))
			icon_height = PANEL_DEFAULT_MENU_ICON_SIZE;

		pixbuf = panel_make_menu_icon (file, NULL, icon_height, &long_operation);
		if (pixbuf) {
			gtk_list_store_set (list, &iter, COLUMN_ICON, pixbuf, -1);
			g_object_unref (pixbuf);
		}
	/* don't go back into the main loop if this wasn't very hard to do */
	} while (!long_operation);

	if (add_icon_paths == NULL) {
		add_icon_idle_id = 0;
		return FALSE;
	}

	return TRUE;
}

/* Called when simple contents are switched to or first shown */
static void
fill_list (GtkWidget *list)
{
        GSList *tmp;
        GSList *files;
        GSList *prev;
        char *prev_name;
	GtkListStore *store;
	FileRec *all_dir;
        
	/* create list store */
	store = gtk_list_store_new (NUM_COLUMNS,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	all_dir = fr_get_dir ("all-applications:/");
	if (all_dir != NULL) {
		files = g_slist_copy (((DirRec *)all_dir)->recs);
	} else {
		files = NULL;
	}

	/* Collate */
	files = g_slist_sort (files, (GCompareFunc) fr_compare);

	/* Strip duplicates */
	tmp = files;
	prev = NULL;
	prev_name = NULL;
	while (tmp) {
		FileRec *fr;

		fr = tmp->data;
		if (prev_name && strcmp (fr->fullname, prev_name) == 0) {
			prev->next = tmp->next;
			tmp->data = NULL;
			g_slist_free_1 (tmp);
			tmp = prev->next;
		} else {
			prev = tmp;
			prev_name = fr->fullname;
			tmp = tmp->next;
		}
	}

	tmp = files;
	while (tmp != NULL) {
		GtkTreeIter iter;
		FileRec *fr;
		GtkTreePath *path;

		fr = tmp->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COLUMN_ICON, NULL,
				    COLUMN_ICON_FILE, (fr->icon) ? fr->icon : "",
				    COLUMN_FULLNAME, (fr->fullname) ? fr->fullname : "",
				    COLUMN_COMMENT, (fr->comment) ? fr->comment : "",
				    COLUMN_NAME, (fr->name) ? fr->name : "",
				    COLUMN_EXEC, (fr->exec) ? fr->exec : "",
				    -1);

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);

		if (path != NULL)
			add_icon_paths = g_slist_prepend (add_icon_paths, path);

		tmp = tmp->next;
	}

	g_slist_free (files);

	gtk_tree_view_set_model (GTK_TREE_VIEW (list), 
				 GTK_TREE_MODEL (store));

	add_columns (GTK_TREE_VIEW (list));

	add_icon_paths = g_slist_reverse (add_icon_paths);

	if (add_icon_idle_id == 0)
		add_icon_idle_id =
			g_idle_add_full (G_PRIORITY_LOW,
					 (GSourceFunc)add_icon_idle,
					 store, NULL);
}

#define DEFAULT_ICON "document-icons/i-executable.png"
#define FALLBACK_DEFAULT_ICON "gnome-logo-icon-transparent.png"

static void
unset_pixmap (GtkWidget *gpixmap)
{
        gchar *file;

        file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
					  DEFAULT_ICON, TRUE, NULL);

	if (file == NULL)
		file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP, 
						  DEFAULT_ICON, TRUE, NULL);
        if (file == NULL)
                file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
						  FALLBACK_DEFAULT_ICON, TRUE, NULL);
        if (file == NULL)
                file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP, 
						  FALLBACK_DEFAULT_ICON, TRUE, NULL);
        
	gtk_image_set_from_file (GTK_IMAGE (gpixmap), file);

	g_free (file);
}

static void
unset_selected (GtkWidget *dialog)
{
        GtkWidget *gpixmap;
        GtkWidget *desc_label;
        GtkWidget *entry;
        GtkWidget *list;
        char *text;
        
        gpixmap = g_object_get_data (G_OBJECT (dialog), "pixmap");
        desc_label = g_object_get_data (G_OBJECT (dialog), "desc_label");
        entry = g_object_get_data (G_OBJECT (dialog), "entry");
        list = g_object_get_data (G_OBJECT (dialog), "program_list");
        
	if (entry != NULL) {
		text = gtk_editable_get_chars (GTK_EDITABLE (entry),
					       0, -1);
	} else {
		text = NULL;
	}

        if ( ! string_empty (text)) {
                char *msg;
                msg = g_strdup_printf (_("Will run command: '%s'"),
                                       text);
                if (desc_label != NULL)
                        gtk_label_set_text (GTK_LABEL (desc_label), msg);

                g_free (msg);
        } else {
                
                if (desc_label != NULL)
                        gtk_label_set_text (GTK_LABEL (desc_label), _("No application selected"));
        }

        g_free (text);
        
	if (gpixmap != NULL)
		unset_pixmap (gpixmap);

        g_object_set_data (G_OBJECT (dialog), "use_list",
			   GPOINTER_TO_INT (FALSE));
	gtk_tree_selection_unselect_all
		(gtk_tree_view_get_selection (GTK_TREE_VIEW (list)));
}

static void
selection_activated (GtkTreeView       *tree_view,
		     GtkTreePath       *path,
		     GtkTreeViewColumn *column,
		     GtkWidget         *dialog)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;

	model = gtk_tree_view_get_model (tree_view);
	if (gtk_tree_model_get_iter (model, &iter, path))
		launch_selected (model, &iter, dialog);

	gtk_widget_destroy (dialog);
}

static void
selection_changed (GtkTreeSelection *selection,
		   GtkWidget        *dialog)
{
        GtkWidget *gpixmap;
        GtkWidget *desc_label;
        gchar *name;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GValue value = {0, };

	if ( ! gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get_value (model, &iter,
				  COLUMN_NAME,
				  &value);
	name = g_strdup (g_value_get_string (&value));
	g_value_unset (&value);

        gpixmap = g_object_get_data (G_OBJECT (dialog), "pixmap");
        desc_label = g_object_get_data (G_OBJECT (dialog), "desc_label");

        if (name != NULL && gpixmap != NULL) {
                QuickDesktopItem *qitem;

		qitem = quick_desktop_item_load_uri (name /*file */,
						     "Application" /* expected type */,
						     TRUE /* run tryexec */);
		if (qitem != NULL) {
                        GdkPixbuf *pixbuf;
			char *icon;
			
			if (desc_label != NULL)
				gtk_label_set_text (GTK_LABEL (desc_label),
						    sure_string (qitem->comment));

			icon = gnome_desktop_item_find_icon (panel_icon_theme,
                                                             qitem->icon,
							     48 /* desired size */,
							     0 /* flags */);
			if (icon != NULL) {
				pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
				g_free (icon);
			} else {
				pixbuf = NULL;
			}
                        
                        if (pixbuf != NULL) {
				gtk_image_set_from_pixbuf (GTK_IMAGE (gpixmap), pixbuf);
                        } else {
                                unset_pixmap (gpixmap);
                        }
                        
			quick_desktop_item_destroy (qitem);
                }

		g_free (name);
        }

        sync_list_to_entry (dialog);
}

static gboolean
add_items_idle (gpointer data)
{
	GtkWidget *list = data;
	fill_list (list);
	add_items_idle_id = 0;
	return FALSE;
}

static GtkWidget*
create_program_list_contents (void)
{
        GtkWidget *vbox;
        GtkWidget *w;
        GtkWidget *label;
        GtkWidget *list;
	GtkTreeSelection *selection;
        
        vbox = gtk_vbox_new (FALSE, 0);
        
        list = gtk_tree_view_new ();

        g_object_set_data (G_OBJECT (run_dialog), "program_list", list);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list), FALSE);

	panel_set_atk_name_desc (list,
				 _("List of known applications"),
				 _("Choose an application to run from the list"));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

        g_signal_connect (selection, "changed",
			  G_CALLBACK (selection_changed), run_dialog);

        g_signal_connect (list, "row-activated",
			  G_CALLBACK (selection_activated), run_dialog);
        
        w = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (w),
                                             GTK_SHADOW_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);

        gtk_container_add (GTK_CONTAINER (w), list);
        gtk_box_pack_start (GTK_BOX (vbox), w,
                            TRUE, TRUE, 0);

        label = gtk_label_new ("");
        gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 5);
        g_object_set_data (G_OBJECT (run_dialog), "desc_label", label);

        g_object_set_data_full (G_OBJECT (run_dialog),
				"program_list_box",
				g_object_ref (vbox),
				(GtkDestroyNotify) g_object_unref);

        unset_selected (run_dialog);

        return vbox;
}


static void
update_contents (GtkWidget *dialog)
{
        GtkWidget *program_list_box = NULL;
        gboolean show_program_list;
	const char *key;

	key = panel_gconf_general_key
		(panel_gconf_get_profile (), "show_program_list"),
	show_program_list = panel_gconf_get_bool (key, SHOW_LIST_DEFAULT);
        
        if (show_program_list) {
                program_list_box = g_object_get_data (G_OBJECT (dialog), "program_list_box");

                if (program_list_box && program_list_box->parent == NULL) {
			GtkWidget *list;
			
			gtk_window_resize (GTK_WINDOW (dialog), 100, 300);
			gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
			
                        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                                            program_list_box,
                                            TRUE, TRUE, GNOME_PAD_SMALL);
                

                        gtk_widget_show_all (GTK_WIDGET (GTK_DIALOG (dialog)->vbox));

			list = g_object_get_data (G_OBJECT (dialog), "program_list");
			gtk_widget_grab_focus (list);
		}
        } else {    
		GtkWidget *entry;           

                program_list_box = g_object_get_data (G_OBJECT (dialog), "program_list_box");
                
                if (program_list_box && program_list_box->parent != NULL) {
                        gtk_container_remove (GTK_CONTAINER (program_list_box->parent), program_list_box);
			gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		}

		entry = g_object_get_data (G_OBJECT (dialog), "entry");
                gtk_widget_grab_focus (entry);
        }
}

static void
run_dialog_destroyed (GtkWidget *widget)
{
	run_dialog = NULL;
	g_slist_foreach (add_icon_paths, (GFunc)gtk_tree_path_free, NULL);
	g_slist_free (add_icon_paths);
	add_icon_paths = NULL;

	if (add_icon_idle_id)
		g_source_remove (add_icon_idle_id);
	add_icon_idle_id = 0;

	if (add_items_idle_id)
		g_source_remove (add_items_idle_id);
	add_items_idle_id = 0;

	if (find_icon_timeout_id)
		g_source_remove (find_icon_timeout_id);
	find_icon_timeout_id = 0;
}

void
show_run_dialog (GdkScreen *screen)
{
        gboolean  enable_program_list;
	GtkWidget *w;
	const char *key;
	char      *run_icon;

	if (no_run_box)
		return;

	if (run_dialog) {
		gtk_window_set_screen (GTK_WINDOW (run_dialog), screen);
		gtk_window_present (GTK_WINDOW (run_dialog));
		/* always focus the entry initially */
		w = g_object_get_data (G_OBJECT (run_dialog), "entry");
		gtk_widget_grab_focus (w);
		return;
	}

	run_dialog = gtk_dialog_new_with_buttons (
				_("Run Program"),
				NULL, 0 /* flags */,
				GTK_STOCK_HELP, GTK_RESPONSE_HELP,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	gtk_window_set_resizable (GTK_WINDOW (run_dialog), FALSE);
	gtk_window_set_screen (GTK_WINDOW (run_dialog), screen);

	w = gtk_dialog_add_button (GTK_DIALOG (run_dialog),
				   PANEL_STOCK_EXECUTE, PANEL_RESPONSE_RUN);
	gtk_widget_set_sensitive (w, FALSE);
	g_object_set_data (G_OBJECT (run_dialog), "run_button", w);

	run_icon = gnome_program_locate_file (
			NULL, GNOME_FILE_DOMAIN_PIXMAP, "gnome-run.png", TRUE, NULL);
	if (run_icon) {
		gnome_window_icon_set_from_file (GTK_WINDOW (run_dialog), run_icon);
		g_free (run_icon);
	}

	g_signal_connect (G_OBJECT (run_dialog), "destroy",
			  G_CALLBACK (run_dialog_destroyed),
			  NULL);

	gtk_window_set_wmclass (GTK_WINDOW (run_dialog), "run_dialog", "Panel");

	gtk_dialog_set_default_response (GTK_DIALOG (run_dialog), PANEL_RESPONSE_RUN);

        g_signal_connect (G_OBJECT (run_dialog), "response", 
			  G_CALLBACK (run_dialog_response), NULL);


        create_simple_contents (screen);
	
	key = panel_gconf_general_key
		(panel_gconf_get_profile (), "enable_program_list"),
	enable_program_list = panel_gconf_get_bool (key, ENABLE_LIST_DEFAULT);

	if (enable_program_list) {
		create_program_list_contents ();
	        update_contents (run_dialog);        
		
		/* start loading the list of applications */
		w = g_object_get_data (G_OBJECT (run_dialog), "program_list");
		add_items_idle_id =
			g_idle_add_full (G_PRIORITY_LOW, add_items_idle,
			 		 w, NULL);

		gtk_widget_grab_focus (w);
	} else {
		w = g_object_get_data (G_OBJECT (run_dialog), "entry");
		gtk_widget_grab_focus (w);
	}
	
	gtk_widget_show_all (run_dialog);
}

void
show_run_dialog_with_text (GdkScreen  *screen,
			   const char *text)
{
	GtkWidget *entry;
	char *exec;

	g_return_if_fail (text != NULL);

	show_run_dialog (screen);

	if (run_dialog == NULL) {
		return;
	}
        
	entry = g_object_get_data (G_OBJECT (run_dialog), "entry");

	exec = remove_parameters (text);

	gtk_entry_set_text (GTK_ENTRY (entry), exec);

	g_free (exec);
}
