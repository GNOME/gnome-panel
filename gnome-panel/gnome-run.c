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
#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>

#include "panel-include.h"
#include "foobar-widget.h"

#include "gnome-run.h"

extern GSList *applets_last;
extern GtkTooltips *panel_tooltips;
extern GlobalConfig global_config;

static GtkWidget *run_dialog = NULL;

static GList *executables = NULL;
static GCompletion *exe_completion = NULL;

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
	char *path;
	char **pathv;

	g_list_foreach (executables, (GFunc) g_free, NULL);
	g_list_free (executables);
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
		g_list_foreach (executables, (GFunc) g_free, NULL);
		g_list_free (executables);
		executables = NULL;
	}

	if (exe_completion != NULL) {
		g_completion_free (exe_completion);
		exe_completion = NULL;
	}
}

/* Note, this expects a vector allocated by popt, where we can 
 * just forget about entries as they are part of the same buffer */
static void
get_environment (int *argc, char ***argv, int *envc, char ***envv)
{
	GList *envar = NULL, *li;
	int i, moveby;

	*envv = NULL;
	*envc = 0;

	moveby = 0;
	for (i = 0; i < *argc; i++) {
		if (strchr ((*argv)[i], '=') == NULL) {
			break;
		}
		envar = g_list_append (envar, g_strdup ((*argv)[i]));
		moveby ++;
	}

	if (moveby == *argc) {
		g_list_foreach (envar, (GFunc) g_free, NULL);
		g_list_free (envar);
		return;
	}

	if (envar == NULL)
		return;

	for (i = 0; i < *argc; i++) {
		if (i + moveby < *argc)
			(*argv)[i] = (*argv)[i+moveby];
		else
			(*argv)[i] = NULL;
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
string_callback (GtkWidget *w, int button_num, gpointer data)
{
	GtkEntry *entry;
	GtkToggleButton *terminal;
	char **argv, **temp_argv = NULL;
	int argc, temp_argc;
	char *s;
	GSList *tofree = NULL;
	char **envv = NULL;
	int envc;

	if (button_num == 2/*help*/) {
		panel_show_help ("specialobjects.html#RUNBUTTON");
		/* just return as we don't want to close */
		return;
	} else if (button_num == 1/*cancel*/) {
		goto return_and_close;
	}

	entry = GTK_ENTRY (gtk_object_get_data(GTK_OBJECT(w), "entry"));
	terminal = GTK_TOGGLE_BUTTON (gtk_object_get_data(GTK_OBJECT(w),
							  "terminal"));

	s = gtk_entry_get_text(entry);

	if (string_empty (s))
		goto return_and_close;

	/* evil eggies, do not translate! */
	if (strcmp (s, "time shall be unixey") == 0) {
		foobar_widget_global_set_clock_format ("%s");
		goto return_and_close;
	}
	if (strcmp (s, "you shall bring us a shrubbery") == 0) {
		gnome_ok_dialog ("NI! NI! NI! NI! NI! NI!");
		goto return_and_close;
	}
	if (strcmp (s, "supreme executive power") == 0) {
		gnome_ok_dialog ("Listen -- strange women lying in\n"
				 "ponds distributing swords is no\n"
				 "basis for a system of government.\n"
				 "Supreme executive power derives from\n"
				 "a mandate from the masses, not from\n"
				 "some farcical aquatic ceremony!");
		goto return_and_close;
	}

	/* Somewhat of a hack I suppose */
	if (panel_is_url (s)) {
		gnome_url_show (s);
		goto return_and_close;
	}

	/* we use a popt function as it does exactly what we want to do and
	   gnome already uses popt */
	if (poptParseArgvString (s, &temp_argc, &temp_argv) != 0) {
		panel_error_dialog (_("Failed to execute command:\n"
				      "%s"), s);
		goto return_and_close;
	}

	get_environment (&temp_argc, &temp_argv, &envc, &envv);

	if (terminal->active) {
		char **term_argv;
		int term_argc;
		gnome_config_get_vector ("/Gnome/Applications/Terminal",
					 &term_argc, &term_argv);
		if (term_argv) {
			int i;
			argv = g_new(char *, term_argc + temp_argc + 1);
			tofree = g_slist_prepend(tofree, argv);
			argc = term_argc + temp_argc;
			for(i = 0; i < term_argc; i++) {
				argv[i] = term_argv[i];
				tofree = g_slist_prepend(tofree, argv[i]);
			}
			for(i = term_argc; i < term_argc+temp_argc; i++)
				argv[i] = temp_argv[i-term_argc];
			argv[i] = NULL;
			g_free(argv);
		} else {
			char *check;
			int i;
			check = panel_is_program_in_path("gnome-terminal");
			argv = g_new(char *, 2 + temp_argc + 1);
			tofree = g_slist_prepend(tofree, argv);
			argc = 2 + temp_argc;
			if(!check) {
				argv[0] = "xterm";
				argv[1] = "-e";
			} else {
				argv[0] = check;
				tofree = g_slist_prepend(tofree, check);
				argv[1] = "-x";
			}
			for(i = 2; i < 2+temp_argc; i++)
				argv[i] = temp_argv[i-2];
			argv[i] = NULL;
		}
	} else {
		argv = temp_argv;
		argc = temp_argc;
	}

	if (gnome_execute_async_with_env (g_get_home_dir (),
					  argc, argv,
					  envc, envv) < 0) {
		panel_error_dialog(_("Failed to execute command:\n"
				     "%s\n"
				     "%s"),
				   s, g_unix_error_string(errno));
	}

return_and_close:
	g_slist_foreach (tofree, (GFunc)g_free, NULL);
	g_slist_free (tofree);
	/* this was obtained from the popt function and thus free and not
	 * g_free */
	if (temp_argv)
		free (temp_argv);
	g_strfreev (envv);
	gnome_dialog_close (GNOME_DIALOG (w));
}

static void
browse_ok(GtkWidget *widget, GtkFileSelection *fsel)
{
	char *fname;
	GtkWidget *entry;

	g_return_if_fail(GTK_IS_FILE_SELECTION(fsel));

	entry = gtk_object_get_user_data(GTK_OBJECT(fsel));

	fname = gtk_file_selection_get_filename(fsel);
	if(fname != NULL) {
		char *s = gtk_entry_get_text (GTK_ENTRY (entry));
		if (string_empty (s)) {
			gtk_entry_set_text (GTK_ENTRY (entry), fname);
		} else {
			s = g_strconcat (s, " ", fname, NULL);
			gtk_entry_set_text (GTK_ENTRY (entry), s);
			g_free (s);
		}
	}
	gtk_widget_destroy(GTK_WIDGET(fsel));
}

static void
browse(GtkWidget *w, GtkWidget *entry)
{
	GtkFileSelection *fsel;

	fsel = GTK_FILE_SELECTION(gtk_file_selection_new(_("Browse...")));
	gtk_window_set_transient_for (GTK_WINDOW (fsel),
				      GTK_WINDOW (run_dialog));
	gtk_object_set_user_data(GTK_OBJECT(fsel), entry);

	gtk_signal_connect (GTK_OBJECT (fsel->ok_button), "clicked",
			    GTK_SIGNAL_FUNC (browse_ok), fsel);
	gtk_signal_connect_object
		(GTK_OBJECT (fsel->cancel_button), "clicked",
		 GTK_SIGNAL_FUNC (gtk_widget_destroy), 
		 GTK_OBJECT(fsel));
	gtk_signal_connect_object_while_alive
		(GTK_OBJECT (entry), "destroy",
		 GTK_SIGNAL_FUNC (gtk_widget_destroy),
		 GTK_OBJECT (fsel));

	gtk_window_position (GTK_WINDOW (fsel), GTK_WIN_POS_MOUSE);
	/* we must do show_now so that we can raise the window in the next
	 * call after set_dialog_layer */
	gtk_widget_show_now (GTK_WIDGET (fsel));
	panel_set_dialog_layer (GTK_WIDGET (fsel));
	gdk_window_raise (GTK_WIDGET (fsel)->window);
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

		pos = GTK_EDITABLE (entry)->current_pos;
		prefix = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, pos);

		g_completion_complete (exe_completion, prefix, &nprefix);

		if (nprefix != NULL &&
		    strlen (nprefix) > strlen (prefix)) {
			gtk_editable_insert_text (GTK_EDITABLE (entry),
						  nprefix + pos, 
						  strlen (nprefix) -
						    strlen (prefix),
						  &pos);
			GTK_EDITABLE (entry)->current_pos = pos;
		}

		g_free (nprefix);
		g_free (prefix);

		return TRUE;
	}

	return FALSE;
}

void
show_run_dialog (void)
{
	GtkWidget *entry;
	GtkWidget *gentry;
	GtkWidget *hbox;
	GtkWidget *w;

	if(run_dialog != NULL) {
		gtk_widget_show_now(run_dialog);
		gdk_window_raise(run_dialog->window);
		return;
	}

	run_dialog = gnome_dialog_new(_("Run Program"), NULL);
	gnome_window_icon_set_from_file (GTK_WINDOW (run_dialog),
					 GNOME_ICONDIR"/gnome-run.png");
	gtk_signal_connect(GTK_OBJECT(run_dialog), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			   &run_dialog);
	gtk_window_position(GTK_WINDOW(run_dialog), GTK_WIN_POS_MOUSE);
	gtk_window_set_wmclass (GTK_WINDOW (run_dialog), "run_dialog", "Panel");
	gnome_dialog_append_button_with_pixmap (GNOME_DIALOG (run_dialog),
						_("Run"),
						GNOME_STOCK_PIXMAP_EXEC);
	gnome_dialog_append_button (GNOME_DIALOG (run_dialog),
				    GNOME_STOCK_BUTTON_CANCEL);
	gnome_dialog_append_button (GNOME_DIALOG (run_dialog),
				    GNOME_STOCK_BUTTON_HELP);

	gnome_dialog_set_default (GNOME_DIALOG (run_dialog), 0);

	hbox = gtk_hbox_new(0, FALSE);
	
	gentry = gnome_entry_new ("gnome-run");
	gtk_box_pack_start (GTK_BOX (hbox), gentry, TRUE, TRUE, 0);
	gtk_widget_set_usize (GTK_WIDGET (gentry),
			      gdk_screen_width () / 4, -2);

	entry = gnome_entry_gtk_entry (GNOME_ENTRY (gentry));

	gtk_signal_connect (GTK_OBJECT (entry), "event",
			    GTK_SIGNAL_FUNC (entry_event),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (entry), "destroy",
			    GTK_SIGNAL_FUNC (kill_completion),
			    NULL);
 
	gtk_window_set_focus (GTK_WINDOW (run_dialog), entry);
	gtk_combo_set_use_arrows_always (GTK_COMBO (gentry), TRUE);
	gtk_signal_connect (GTK_OBJECT (run_dialog), "clicked", 
			    GTK_SIGNAL_FUNC (string_callback), NULL);
	gtk_object_set_data (GTK_OBJECT (run_dialog), "entry", entry);

	gnome_dialog_editable_enters (GNOME_DIALOG (run_dialog),
				      GTK_EDITABLE (entry));

	w = gtk_button_new_with_label(_("Browse..."));
	gtk_signal_connect(GTK_OBJECT(w), "clicked",
			   GTK_SIGNAL_FUNC (browse), entry);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE,
			    GNOME_PAD_SMALL);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (run_dialog)->vbox), hbox,
			    FALSE, FALSE, GNOME_PAD_SMALL);

	w = gtk_check_button_new_with_label(_("Run in terminal"));
	gtk_object_set_data (GTK_OBJECT (run_dialog), "terminal", w);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (run_dialog)->vbox), w,
			    FALSE, FALSE, GNOME_PAD_SMALL);

	gtk_widget_show_all (run_dialog);
	panel_set_dialog_layer (run_dialog);
}

void
show_run_dialog_with_text (const char *text)
{
	GtkWidget *entry;

	g_return_if_fail(text != NULL);

	show_run_dialog();

	if(run_dialog == NULL) {
		/* this should NEVER happen, but I'm paranoid */
		g_warning("Eeeeeeek!");
		return;
	}

	entry = gtk_object_get_data(GTK_OBJECT(run_dialog), "entry");

	gtk_entry_set_text(GTK_ENTRY(entry), text);
}

static void  
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint               time,
		  gpointer            data)
{
	char *foo;

	foo = g_strdup_printf ("RUN:%d", find_applet (widget));

	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)foo,
				strlen (foo));

	g_free (foo);
}


static GtkWidget *
create_run_widget(void)
{
        static GtkTargetEntry dnd_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	GtkWidget *button;
	char *pixmap_name;

	pixmap_name = gnome_pixmap_file("gnome-run.png");

	button = button_widget_new(pixmap_name,-1,
				   MISC_TILE,
				   FALSE,
				   ORIENT_UP,
				   _("Run..."));

	/*A hack since this function only pretends to work on window
	  widgets (which we actually kind of are) this will select
	  some (already selected) events on the panel instead of
	  the button window (where they are also selected) but
	  we don't mind*/
	GTK_WIDGET_UNSET_FLAGS (button, GTK_NO_WINDOW);
	gtk_drag_source_set (button,
			     GDK_BUTTON1_MASK,
			     dnd_targets, 1,
			     GDK_ACTION_COPY | GDK_ACTION_MOVE);
	GTK_WIDGET_SET_FLAGS (button, GTK_NO_WINDOW);

	gtk_signal_connect (GTK_OBJECT (button), "drag_data_get",
			    GTK_SIGNAL_FUNC (drag_data_get_cb),
			    NULL);

	g_free(pixmap_name);
	gtk_tooltips_set_tip (panel_tooltips, button, _("Run..."), NULL);

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(show_run_dialog), NULL);

	return button;
}

void
load_run_applet(PanelWidget *panel, int pos, gboolean exactpos)
{
	GtkWidget *run;

	run = create_run_widget();
	if(!run)
		return;

	if (!register_toy(run, NULL, NULL, panel,
			  pos, exactpos, APPLET_RUN))
		return;

	applet_add_callback(applets_last->data, "help",
			    GNOME_STOCK_PIXMAP_HELP,
			    _("Help"));
}
