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
#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>

#include "panel-include.h"
#include "foobar-widget.h"
#include "menu-fentry.h"
#include "menu.h"
#include "main.h"
#include "multiscreen-stuff.h"

#include "gnome-run.h"

#define ADVANCED_DIALOG_KEY "advanced_run_dialog"

extern GSList *applets_last;
extern GtkTooltips *panel_tooltips;
extern GlobalConfig global_config;
extern gboolean no_run_box;

static GtkWidget *run_dialog = NULL;

static GList *executables = NULL;
static GCompletion *exe_completion = NULL;

static GtkWidget* create_advanced_contents (void);
static void       update_contents          (GtkWidget *dialog);
static void       unset_selected           (GtkWidget *dialog);

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
        GtkWidget *clist;
	GtkToggleButton *terminal;
	char **argv, **temp_argv = NULL;
	int argc, temp_argc;
	char *s;
	GSList *tofree = NULL;
	char **envv = NULL;
	int envc;
        gboolean use_advanced;
        gboolean add_to_favourites;
        GtkWidget *favorites;
        
        use_advanced = gnome_config_get_bool ("/panel/State/"ADVANCED_DIALOG_KEY"=false");
        
	if (button_num == 2/*help*/) {
		panel_show_help ("specialobjects.html#RUNBUTTON");
		/* just return as we don't want to close */
		return;
	} else if (button_num == 1/*cancel*/) {
		goto return_and_close;
	}

        
        
        clist = gtk_object_get_data (GTK_OBJECT (run_dialog), "dentry_list");
        terminal = GTK_TOGGLE_BUTTON (gtk_object_get_data(GTK_OBJECT(w),
                                                          "terminal"));
        favorites = gtk_object_get_data (GTK_OBJECT (run_dialog), "favorites");

        add_to_favourites = GTK_TOGGLE_BUTTON (favorites)->active;
        
        if (gtk_object_get_data (GTK_OBJECT (run_dialog), "use_list")) {
                char *name;
                
                if (GTK_CLIST (clist)->selection == NULL)
                        return;
        
                name = gtk_clist_get_row_data (GTK_CLIST (clist),
                                               GPOINTER_TO_INT (GTK_CLIST (clist)->selection->data));
                if (name) {
                        GnomeDesktopEntry *dentry;
                        
                        dentry = gnome_desktop_entry_load (name);
                        if (dentry && dentry->exec) {
                                /* Honor "run in terminal" button */
                                dentry->terminal = terminal->active;
                                
                                gnome_desktop_entry_launch (dentry);
                                gnome_desktop_entry_free (dentry);

                                if (add_to_favourites)
                                        panel_add_favourite (name);
                        } else {
                                panel_error_dialog (_("Failed to load this program!\n"));
                        }
                }
        } else {
                entry = GTK_ENTRY (gtk_object_get_data(GTK_OBJECT(w), "entry"));

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
                if (strcmp (s, "free the fish") == 0) {
			start_screen_check ();
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
                                g_free (term_argv);
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
        GtkWidget *clist;
        GtkWidget *entry;
        gboolean blocked;

        blocked = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (dialog),
                                                        "sync_entry_to_list_blocked"));
        if (blocked)
                return;
        
        clist = gtk_object_get_data (GTK_OBJECT (dialog), "dentry_list");
        entry = gtk_object_get_data(GTK_OBJECT(dialog), "entry");

        unset_selected (dialog);
}

static void
sync_list_to_entry (GtkWidget *dialog)
{
        GtkWidget *clist;
        GtkWidget *entry;
        GtkWidget *terminal_toggle;
        gchar *name;

        gtk_object_set_data (GTK_OBJECT (dialog),
                             "sync_entry_to_list_blocked",
                             GINT_TO_POINTER (TRUE));
        
        clist = gtk_object_get_data (GTK_OBJECT (dialog), "dentry_list");
        entry = gtk_object_get_data (GTK_OBJECT (dialog), "entry");
        terminal_toggle = gtk_object_get_data (GTK_OBJECT (dialog), "terminal");
        
        if (GTK_CLIST (clist)->selection) {
                name = gtk_clist_get_row_data (GTK_CLIST (clist),
                                               GPOINTER_TO_INT (GTK_CLIST (clist)->selection->data));
                if (name) {
                        GnomeDesktopEntry *dentry;

                        dentry = gnome_desktop_entry_load (name);
                        if (dentry && dentry->exec) {
                                char *command;

                                command = g_strjoinv (" ", dentry->exec);
                                
                                gtk_entry_set_text (GTK_ENTRY (entry),
                                                    command);

                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (terminal_toggle),
                                                              dentry->terminal);
                                
                                gnome_desktop_entry_free (dentry);
                                g_free (command);
                        }
                }
        }

        gtk_object_set_data (GTK_OBJECT (dialog),
                             "sync_entry_to_list_blocked",
                             GINT_TO_POINTER (FALSE));

        gtk_object_set_data (GTK_OBJECT (dialog), "use_list",
                             GINT_TO_POINTER (TRUE));
}

static void
toggle_contents (GtkWidget *button,
                 GtkWidget *dialog)
{
        gboolean use_advanced;
        
        use_advanced = gnome_config_get_bool ("/panel/State/"ADVANCED_DIALOG_KEY"=false");

        gnome_config_set_bool ("/panel/State/"ADVANCED_DIALOG_KEY, !use_advanced);
        gnome_config_sync ();

        update_contents (dialog);
}

static GtkWidget*
create_toggle_advanced_button (const char *label)
{
        GtkWidget *align;
        GtkWidget *button;

        align = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);

        button = gtk_button_new_with_label (label);

        gtk_container_add (GTK_CONTAINER (align), button);

        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                            GTK_SIGNAL_FUNC (toggle_contents),
                            run_dialog);

        gtk_object_set_data (GTK_OBJECT (run_dialog),
                             "advanced_toggle_label",
                             GTK_BIN (button)->child);
        
        return align;
}

static void
entry_changed (GtkWidget *entry,
               gpointer   data)
{
        sync_entry_to_list (GTK_WIDGET (data));
}

/* Called when advanced contents are switched to or first shown */
static void
advanced_contents_shown (GtkWidget *vbox,
                         GtkWidget *dialog)
{
        /* does nothing at the moment */
}

static GtkWidget*
create_advanced_contents (void)
{
        GtkWidget *vbox;
        GtkWidget *entry;
        GtkWidget *gentry;
        GtkWidget *hbox;
        GtkWidget *w;
        
        vbox = gtk_vbox_new (FALSE, 0);

        hbox = gtk_hbox_new(0, FALSE);
	
        gentry = gnome_entry_new ("gnome-run");
        gtk_box_pack_start (GTK_BOX (hbox), gentry, TRUE, TRUE, 0);
        /* 1/4 the width of the first screen should be a good value */
        gtk_widget_set_usize (GTK_WIDGET (gentry),
                              multiscreen_width (0) / 4, -2);

        entry = gnome_entry_gtk_entry (GNOME_ENTRY (gentry));

        gtk_signal_connect (GTK_OBJECT (entry), "event",
                            GTK_SIGNAL_FUNC (entry_event),
                            NULL);
        gtk_signal_connect (GTK_OBJECT (entry), "destroy",
                            GTK_SIGNAL_FUNC (kill_completion),
                            NULL);
 
        gtk_window_set_focus (GTK_WINDOW (run_dialog), entry);
        gtk_combo_set_use_arrows_always (GTK_COMBO (gentry), TRUE);
        gtk_object_set_data (GTK_OBJECT (run_dialog), "entry", entry);

        gnome_dialog_editable_enters (GNOME_DIALOG (run_dialog),
                                      GTK_EDITABLE (entry));
        gtk_signal_connect (GTK_OBJECT (entry),
                            "changed",
                            GTK_SIGNAL_FUNC (entry_changed),
                            run_dialog);
        
        w = gtk_button_new_with_label(_("Browse..."));
        gtk_signal_connect(GTK_OBJECT(w), "clicked",
                           GTK_SIGNAL_FUNC (browse), entry);
        gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE,
                            GNOME_PAD_SMALL);

        gtk_box_pack_start (GTK_BOX (vbox), hbox,
                            FALSE, FALSE, GNOME_PAD_SMALL);

        w = gtk_check_button_new_with_label(_("Run in terminal"));
        gtk_object_set_data (GTK_OBJECT (run_dialog), "terminal", w);
        gtk_box_pack_start (GTK_BOX (vbox), w,
                            FALSE, FALSE, GNOME_PAD_SMALL);
        
        gtk_object_ref (GTK_OBJECT (vbox));
        
        gtk_object_set_data_full (GTK_OBJECT (run_dialog),
                                  "advanced",
                                  vbox,
                                  (GtkDestroyNotify) gtk_object_unref);
        
        gtk_signal_connect (GTK_OBJECT (vbox),
                            "show",
                            GTK_SIGNAL_FUNC (advanced_contents_shown),
                            run_dialog);

        return vbox;
}

static void
scan_dir_recurse (DirRec  *dr,
                  GSList **entry_list)
{
        GSList *li;
        
        for (li = dr->recs; li != NULL; li = li->next) {
		FileRec *fr = li->data;
		
		if (fr->type == FILE_REC_FILE) {
                        if (fr->goad_id != NULL)
                                continue; /* applet */

                        *entry_list = g_slist_prepend (*entry_list, fr);
                        
		} else if (fr->type == FILE_REC_DIR) {
                        scan_dir_recurse ((DirRec*)fr, entry_list);
		} else {
			continue;
		}
        }
}

static int
sort_by_name (FileRec *fra,
              FileRec *frb)
{
        return strcoll (fra->fullname, frb->fullname);
}

#define CLIST_ICON_SIZE 20

/* Called when simple contents are switched to or first shown */
static void
simple_contents_shown (GtkWidget *vbox,
                       GtkWidget *dialog)
{
        GtkWidget *advanced;
        GSList *entries;
        GSList *tmp;
        GSList *files;
        GSList *prev;
        GtkWidget *clist;
        char *prev_name;
        
        clist = gtk_object_get_data (GTK_OBJECT (dialog), "dentry_list");
        advanced = gtk_object_get_data (GTK_OBJECT (dialog), "advanced");
        
        if (advanced) {
                /* If we have advanced contents containing a command,
                 * try to match that command to some desktop entry
                 * in order to fill in our default.
                 */
                
                /*  FIXME */
        }

        if (GTK_CLIST (clist)->rows == 0) {
                GdkPixmap *spacer_pixmap;
                GdkBitmap *spacer_mask;
                GdkGC *gc;
                GdkColor color;

                /* Create invisible pixmap/mask to put space
                 * before entries with no icon
                 */
                spacer_pixmap = gdk_pixmap_new (NULL,
                                                CLIST_ICON_SIZE,
                                                CLIST_ICON_SIZE,
                                                gtk_widget_get_visual (clist)->depth);

                spacer_mask = gdk_pixmap_new (NULL,
                                              CLIST_ICON_SIZE,
                                              CLIST_ICON_SIZE,
                                              1);

                gc = gdk_gc_new (spacer_mask);
                color.pixel = 0;
                gdk_gc_set_foreground (gc, &color);
                gdk_draw_rectangle (spacer_mask,
                                    gc,
                                    TRUE, 0, 0, CLIST_ICON_SIZE, CLIST_ICON_SIZE);
                gdk_gc_unref (gc);
                gc = NULL;
                
                entries = fr_get_all_dirs ();
                files = NULL;
                tmp = entries;
                while (tmp != NULL) {
                        DirRec *dr = tmp->data;

                        scan_dir_recurse (dr, &files);
                
                        tmp = tmp->next;
                }

                /* Collate */
                files = g_slist_sort (files, (GCompareFunc) sort_by_name);

                /* Strip duplicates */
                tmp = files;
                prev = NULL;
                prev_name = NULL;
                while (tmp) {
                        FileRec *fr;
                        
                        fr = tmp->data;
                        if (prev_name && strcmp (fr->fullname, prev_name) == 0) {
                                GSList *del = tmp;
                        
                                prev->next = del->next;
                                g_slist_free_1 (del);
                                tmp = prev->next;
                        } else {
                                prev = tmp;
                                prev_name = fr->fullname;
                                tmp = tmp->next;
                        }
                }
        
                tmp = files;
                while (tmp != NULL) {
                        FileRec *fr;
                        GdkPixbuf *pixbuf;
                        GdkPixmap *pixmap;
                        GdkBitmap *mask;
                        int row;
                        char *text[2];
                
                        fr = tmp->data;

			if (fr->icon != NULL) {
				pixbuf = gdk_pixbuf_new_from_file (fr->icon);
			} else {
				pixbuf = NULL;
			}
                
                        if (pixbuf) {
                                GdkPixbuf *scaled;
                                scaled = gdk_pixbuf_scale_simple (pixbuf, CLIST_ICON_SIZE, CLIST_ICON_SIZE, GDK_INTERP_BILINEAR);
                                gdk_pixbuf_render_pixmap_and_mask (scaled,
                                                                   &pixmap, &mask, 128);
                                gdk_pixbuf_unref (pixbuf);
                                gdk_pixbuf_unref (scaled);
                        } else {
                                pixmap = spacer_pixmap;
                                mask = spacer_mask;
                        }

                        text[0] = fr->fullname;
                        text[1] = fr->comment;
                        row = gtk_clist_append (GTK_CLIST (clist),
                                                text);

                        gtk_clist_set_pixtext (GTK_CLIST (clist),
                                               row, 0,
                                               fr->fullname,
                                               3,
                                               pixmap, mask);

                        if (pixbuf) {
                                if (pixmap)
                                        gdk_pixmap_unref (pixmap);
                                if (mask)
                                        gdk_bitmap_unref (mask);
                        }
                                
                        gtk_clist_set_row_data (GTK_CLIST (clist),
                                                row, fr->name);
                
                        tmp = tmp->next;
                }

                gdk_pixmap_unref (spacer_pixmap);
                gdk_bitmap_unref (spacer_mask);
                g_slist_free (files);

                gtk_clist_columns_autosize (GTK_CLIST (clist));
        }
}

#define DEFAULT_ICON "nautilus/i-executable.png"
#define FALLBACK_DEFAULT_ICON "gnome-logo-icon-transparent.png"

static void
unset_pixmap (GtkWidget *gpixmap)
{
       
        gchar *file;

        file = gnome_pixmap_file (DEFAULT_ICON);

        if (file == NULL)
                file = gnome_pixmap_file (FALLBACK_DEFAULT_ICON);
        
        if (file != NULL) {
                gnome_pixmap_load_file (GNOME_PIXMAP (gpixmap),
                                        file);
		g_free (file);
	} else {
                /* Clear the pixmap, yay GnomePixmap rules */
                gnome_pixmap_load_file (GNOME_PIXMAP (gpixmap),
                                        "I do not exist anywhere 3413hjrneljghlkjflkjf");
	}
}

static void
unset_selected (GtkWidget *dialog)
{
        GtkWidget *label;
        GtkWidget *gpixmap;
        GtkWidget *desc_label;
        GtkWidget *entry;
        GtkWidget *clist;
        char *text;
        
        label = gtk_object_get_data (GTK_OBJECT (dialog), "label");
        gpixmap = gtk_object_get_data (GTK_OBJECT (dialog), "pixmap");
        desc_label = gtk_object_get_data (GTK_OBJECT (dialog), "desc_label");
        entry = gtk_object_get_data (GTK_OBJECT (dialog), "entry");
        clist = gtk_object_get_data (GTK_OBJECT (dialog), "dentry_list");
        
	if (entry != NULL) {
		text = gtk_editable_get_chars (GTK_EDITABLE (entry),
					       0, -1);
	} else {
		text = NULL;
	}

        if ( ! string_empty (text)) {
                char *msg;
                msg = g_strdup_printf (_("Will run '%s'"),
                                       text);
                if (label)
                        gtk_label_set_text (GTK_LABEL (label), msg);
                
                if (desc_label)
                        gtk_label_set_text (GTK_LABEL (desc_label), msg);

                g_free (msg);
        } else {
                if (label)
                        gtk_label_set_text (GTK_LABEL (label), _("No program selected"));
                
                if (desc_label)
                        gtk_label_set_text (GTK_LABEL (desc_label), _("No program selected"));
        }

        g_free (text);
        
        unset_pixmap (gpixmap);

        gtk_object_set_data (GTK_OBJECT (dialog), "use_list",
                             GPOINTER_TO_INT (FALSE));

        gtk_clist_set_selection_mode (GTK_CLIST (clist),
                                      GTK_SELECTION_SINGLE);
        gtk_clist_unselect_all (GTK_CLIST (clist));

        /* Can't add non-listed items to favorites for now. */
        gtk_widget_set_sensitive (gtk_object_get_data (GTK_OBJECT (dialog),
                                                       "favorites"),
                                  FALSE);
}

static void
select_row_handler (GtkCList *clist,
                    gint      row,
                    gint      column,
                    GdkEvent *event,
                    gpointer  data)
{
        GtkWidget *label;
        GtkWidget *gpixmap;
        GtkWidget *desc_label;
        GtkWidget *dialog = data;
        gchar *name;

        if (clist->selection == NULL)
                return;

        /* Change selection mode once we have a selection */
        gtk_clist_set_selection_mode (GTK_CLIST (clist),
                                      GTK_SELECTION_BROWSE);
        
        label = gtk_object_get_data (GTK_OBJECT (dialog), "label");
        gpixmap = gtk_object_get_data (GTK_OBJECT (dialog), "pixmap");
        desc_label = gtk_object_get_data (GTK_OBJECT (dialog), "desc_label");

        name = gtk_clist_get_row_data (GTK_CLIST (clist),
                                       row);

        if (name) {
                GnomeDesktopEntry *dentry;
                
                dentry = gnome_desktop_entry_load (name);
		if (dentry != NULL) {
                        GdkPixbuf *pixbuf;

			if (label != NULL)
				gtk_label_set_text (GTK_LABEL (label),
						    dentry->name);

			if (desc_label != NULL)
				gtk_label_set_text (GTK_LABEL (desc_label),
						    dentry->comment);

			if (dentry->icon != NULL) {
				pixbuf = gdk_pixbuf_new_from_file (dentry->icon);
				if (pixbuf == NULL) {
					char *file = gnome_pixmap_file (dentry->icon);
					if (file != NULL)
						pixbuf = gdk_pixbuf_new_from_file (file);
					g_free (file);
				}
			} else {
				pixbuf = NULL;
			}
                        
                        if (pixbuf) {
                                GdkPixmap *pixmap;
                                GdkBitmap *mask;

                                gdk_pixbuf_render_pixmap_and_mask (pixbuf,
                                                                   &pixmap, &mask, 128);

                                /* GnomePixmap bites me */
                                gdk_pixmap_unref (GNOME_PIXMAP (gpixmap)->pixmap);
                                gdk_pixmap_unref (GNOME_PIXMAP (gpixmap)->mask);
                                GNOME_PIXMAP (gpixmap)->pixmap = pixmap;
                                GNOME_PIXMAP (gpixmap)->mask = mask;
                                gtk_widget_queue_resize (gpixmap);
                                gdk_pixbuf_unref (pixbuf);
                        } else {
                                unset_pixmap (gpixmap);
                        }
                        
			gnome_desktop_entry_free (dentry);
                }
        }

        sync_list_to_entry (dialog);

        /* Allow adding to favorites */
        gtk_widget_set_sensitive (gtk_object_get_data (GTK_OBJECT (dialog),
                                                       "favorites"),
                                  TRUE);
}

static GtkWidget*
create_simple_contents (void)
{
        GtkWidget *vbox;
        GtkWidget *w;
        GtkWidget *label;
        GtkWidget *pixmap;
        GtkWidget *clist;
        GtkWidget *hbox;
        GtkWidget *favorites;
        char *titles[2];
        
        vbox = gtk_vbox_new (FALSE, 1);
        
        titles[0] = _("Available Programs");
        titles[1] = _("Description");
        clist = gtk_clist_new_with_titles (1 /* 2 */, titles);
        gtk_object_set_data (GTK_OBJECT (run_dialog), "dentry_list", clist);

        gtk_clist_set_selection_mode (GTK_CLIST (clist),
                                      GTK_SELECTION_SINGLE);

        gtk_clist_column_titles_passive (GTK_CLIST (clist));
        
        gtk_widget_ensure_style (clist);
        gtk_clist_set_row_height (GTK_CLIST (clist),
                                  MAX (clist->style->font->ascent +
                                       clist->style->font->descent,
                                       CLIST_ICON_SIZE));

        gtk_signal_connect (GTK_OBJECT (clist),
                            "select_row",
                            GTK_SIGNAL_FUNC (select_row_handler),
                            run_dialog);
        
        w = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_container_add (GTK_CONTAINER (w), clist);
        
        gtk_box_pack_start (GTK_BOX (vbox), w,
                            TRUE, TRUE, GNOME_PAD_SMALL);


        w = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
        gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
        hbox = gtk_hbox_new (FALSE, 3);
        gtk_container_add (GTK_CONTAINER (w), hbox);
        
        pixmap = gtk_type_new (GNOME_TYPE_PIXMAP);
        gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);
        gtk_object_set_data (GTK_OBJECT (run_dialog), "pixmap", pixmap);
        
        label = gtk_label_new ("");
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        gtk_object_set_data (GTK_OBJECT (run_dialog), "desc_label", label);        

#if 0
        label = gtk_label_new ("");
        gtk_object_set_data (GTK_OBJECT (run_dialog), "label", label);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
#endif

        favorites = gtk_check_button_new_with_label (_("Add this program to Favorites"));
        gtk_object_set_data (GTK_OBJECT (run_dialog), "favorites", favorites);
        gtk_box_pack_start (GTK_BOX (vbox), favorites, FALSE, FALSE, 0);
        
        unset_selected (run_dialog);
        
        w = create_toggle_advanced_button ("");
        gtk_box_pack_end (GTK_BOX (GNOME_DIALOG (run_dialog)->vbox), w,
                          FALSE, FALSE, GNOME_PAD_SMALL);
        
        gtk_object_ref (GTK_OBJECT (vbox));
        
        gtk_object_set_data_full (GTK_OBJECT (run_dialog),
                                  "simple",
                                  vbox,
                                  (GtkDestroyNotify) gtk_object_unref);

        gtk_signal_connect (GTK_OBJECT (vbox),
                            "show",
                            GTK_SIGNAL_FUNC (simple_contents_shown),
                            run_dialog);

        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (run_dialog)->vbox),
                            vbox,
                            TRUE, TRUE, 0);
        
        return vbox;
}


static void
update_contents (GtkWidget *dialog)
{
        GtkWidget *advanced = NULL;
        GtkWidget *advanced_toggle;
        gboolean use_advanced;
        GtkWidget *clist;
        
        use_advanced = gnome_config_get_bool ("/panel/State/"ADVANCED_DIALOG_KEY"=false");        
        advanced_toggle = gtk_object_get_data (GTK_OBJECT (dialog),
                                               "advanced_toggle_label");

        clist = gtk_object_get_data (GTK_OBJECT (dialog), "dentry_list");
        
        if (use_advanced) {
                advanced = gtk_object_get_data (GTK_OBJECT (dialog), "advanced");
                
                if (advanced && advanced->parent == NULL) {
                        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
                                            advanced,
                                            FALSE, FALSE, 0);
                
                        gtk_widget_show_all (advanced);

                        gtk_widget_grab_focus (advanced);
                }

                gtk_label_set_text (GTK_LABEL (advanced_toggle),
                                    _("Hide advanced options"));

                gtk_tooltips_set_tip (panel_tooltips, advanced_toggle->parent,
                                      _("Hide the advanced controls below this button."),
                                      NULL);                

        } else {                
                advanced = gtk_object_get_data (GTK_OBJECT (dialog), "advanced");
                
                if (advanced && advanced->parent != NULL)
                        gtk_container_remove (GTK_CONTAINER (advanced->parent), advanced);                
                gtk_label_set_text (GTK_LABEL (advanced_toggle),
                                    _("Advanced..."));

                gtk_tooltips_set_tip (panel_tooltips, advanced_toggle->parent,
                                      _("Allow typing in a command line instead of choosing an application from the list"),
                                      NULL);

                gtk_widget_grab_focus (clist);
        }
}

void
show_run_dialog (void)
{
        gboolean use_advanced;          

	if (no_run_box)
		return;

	if(run_dialog != NULL) {
		gtk_widget_show_now(run_dialog);
		gdk_window_raise(run_dialog->window);
		return;
	}

        use_advanced = gnome_config_get_bool ("/panel/State/"ADVANCED_DIALOG_KEY"=false");
        
	run_dialog = gnome_dialog_new(_("Run Program"), NULL);

        /* This is lame in advanced mode, but if you change it on mode
         * toggle it creates weird effects, so always use this policy
         */
        gtk_window_set_policy (GTK_WINDOW (run_dialog),
                               FALSE, TRUE, FALSE);

        /* Get some reasonable height in simple list mode */
        if (!use_advanced)
                gtk_window_set_default_size (GTK_WINDOW (run_dialog),
                                             -1, 400);
        
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
        gtk_signal_connect (GTK_OBJECT (run_dialog), "clicked", 
                            GTK_SIGNAL_FUNC (string_callback), NULL);

        create_simple_contents ();
        create_advanced_contents ();
        update_contents (run_dialog);
        
	gtk_widget_show_all (run_dialog);
	panel_set_dialog_layer (run_dialog);
}

void
show_run_dialog_with_text (const char *text)
{
	GtkWidget *entry;

	g_return_if_fail(text != NULL);

	show_run_dialog ();

	if(run_dialog == NULL) {
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
