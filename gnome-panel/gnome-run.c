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
#include <errno.h>

#include "gnome-run.h"

static void 
string_callback (GtkWidget *w, int button_num, gpointer data)
{
	GnomeFileEntry *fentry;
	GnomeEntry *entry;
	char *s, *t;

	if (button_num != 0)
		return;

	fentry = GNOME_FILE_ENTRY (data);
	entry = GNOME_ENTRY (gnome_file_entry_gnome_entry (fentry));

	s = gtk_editable_get_chars (GTK_EDITABLE (
		gnome_file_entry_gtk_entry (fentry)),
				    0, -1);
	if (strlen (s) == 0) {
		g_free (s);
		return;
	}

	gnome_entry_prepend_history (entry, 1, s);
	gnome_entry_save_history (entry);
	gnome_config_sync ();

	if (gnome_execute_shell (NULL, s) >= 0) {
		g_free (s);
		return;
	}
	
	/* this probably will never happen, since most systems have sh */
	t = g_strdup_printf(_("Failed to execute command:\n"
			      "%s\n"
			      "%s"),
			    s, g_unix_error_string(errno));

	gnome_error_dialog (t);
	g_free (s);
	g_free (t);
}

void
show_run_dialog ()
{
	GtkWidget *dialog;
	GtkWidget *fentry;
	GtkWidget *entry;
	GtkWidget *gentry;

	dialog = gnome_dialog_new(_("Run Program"), 
				  _("Run"), _("Cancel"), NULL);

	gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
	gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
	
	fentry = gnome_file_entry_new ("gnome-run",
				       _("Select a program to run"));
	gentry = gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (fentry));
	gnome_entry_load_history (GNOME_ENTRY (gentry));
	gnome_entry_prepend_history (GNOME_ENTRY (gentry), FALSE, "");
	
	gtk_window_set_focus (GTK_WINDOW (dialog),
			      gnome_file_entry_gtk_entry (
				      GNOME_FILE_ENTRY (fentry)));
	gtk_combo_set_use_arrows_always (GTK_COMBO (
		gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (fentry))),
					 TRUE);
	gtk_signal_connect(GTK_OBJECT (dialog), "clicked", 
			   GTK_SIGNAL_FUNC (string_callback), fentry);

	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (fentry));
	gnome_dialog_editable_enters (GNOME_DIALOG (dialog), GTK_EDITABLE (
		gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (fentry))));

	gtk_container_add (GTK_CONTAINER (GNOME_DIALOG (dialog)->vbox),
			   GTK_WIDGET (fentry));

	gtk_widget_show_all (GTK_WIDGET (dialog));
}
