#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomeui/gnome-ditem-edit.h>

#include "menu-ditem.h"
#include "panel-util.h"

enum {
	REVERT_BUTTON
};

static void
ditem_properties_clicked (GtkWidget *w, int response, gpointer data)
{
	GnomeDItemEdit *dee = g_object_get_data (G_OBJECT (w), "GnomeDItemEdit");
	GnomeDesktopItem *ditem = data;

	if (response == GTK_RESPONSE_HELP) {
		panel_show_help ("launchers", NULL);
	} else if (response == REVERT_BUTTON) {
		if (ditem != NULL)
			gnome_ditem_edit_set_ditem (dee, ditem);
		else
			gnome_ditem_edit_clear (dee);
	} else {
		gtk_widget_destroy (w);
	}
}

static gboolean
ditem_properties_apply_timeout (gpointer data)
{
	GtkWidget *dedit = data;
	GnomeDesktopItem *ditem;
	const char *loc;
	GError *error = NULL;

	g_object_set_data (G_OBJECT (dedit), "apply_timeout", NULL);

	ditem = gnome_ditem_edit_get_ditem (GNOME_DITEM_EDIT (dedit));
	loc = g_object_get_data (G_OBJECT (dedit), "location");
	gnome_desktop_item_save (ditem,
				 loc /* under */,
				 TRUE /* force */,
				 &error);
	/* save the error for later */
	if (error != NULL) {
		g_object_set_data_full (G_OBJECT (dedit), "SavingError",
					g_strdup (error->message),
					(GDestroyNotify) g_free);
		g_clear_error (&error);
	} else {
		g_object_set_data (G_OBJECT (dedit), "SavingError", NULL);
	}

	return FALSE;
}

/* 
 * Will save after 5 seconds of no changes.  If something is changed, the save
 * is postponed to another 5 seconds.  This seems to be a saner behaviour,
 * then just saving every N seconds.
 */
static void
ditem_properties_changed (GtkWidget *dedit, gpointer data)
{
	gpointer timeout_data = g_object_get_data (G_OBJECT (dedit),
						   "apply_timeout");
	guint timeout = GPOINTER_TO_UINT (timeout_data);

	g_object_set_data (G_OBJECT (dedit), "apply_timeout", NULL);

	if (timeout != 0)
		g_source_remove (timeout);

	/* Will save after 5 seconds */
	timeout = g_timeout_add (5 * 1000,
				   ditem_properties_apply_timeout,
				   dedit);

	g_object_set_data (G_OBJECT (dedit), "apply_timeout",
			   GUINT_TO_POINTER (timeout));
}


static void
ditem_properties_close (GtkWidget *widget, gpointer data)
{
	GtkWidget *dedit = data;
	const char *saving_error;
	gpointer timeout_data = g_object_get_data (G_OBJECT (dedit),
						   "apply_timeout");
	guint timeout = GPOINTER_TO_UINT (timeout_data);

	g_object_set_data (G_OBJECT (dedit), "apply_timeout", NULL);

	if (timeout != 0) {
		g_source_remove (timeout);

		ditem_properties_apply_timeout (dedit);
	}

	saving_error = g_object_get_data (G_OBJECT (dedit), "SavingError");

	if (saving_error != NULL) {
		panel_error_dialog ("cant_save_entry",
				    _("<b>Cannot save changes to menu entry</b>\n\n"
				      "Details: %s"), saving_error);
	}
}

static gboolean
is_item_writable (const char *loc, const char *dir)
{
	if (loc != NULL) {
		/* if old style kde link file, don't allow editing */
		if (is_ext (loc, ".kdelnk"))
			return FALSE;
		if (panel_is_uri_writable (loc))
			return TRUE;
		else
			return FALSE;
	}
	
	if (dir != NULL) {
		if (panel_is_uri_writable (dir))
			return TRUE;
		else
			return FALSE;
	}

	/* huh? */
	return FALSE;
}

static void
set_ditem_sensitive (GtkDialog *dialog,
		     GnomeDItemEdit *dedit,
		     const char *loc,
		     const char *dir)
{
	gboolean sensitive;

	sensitive = is_item_writable (loc, dir);

	gnome_ditem_edit_set_editable (dedit, sensitive);

	gtk_dialog_set_response_sensitive (dialog, REVERT_BUTTON, sensitive);
}

GtkWidget *
panel_edit_dentry (const char *loc,
		   const char *dir)
{
	GtkWidget *dialog;
	GtkWidget *dedit;
	GnomeDesktopItem *ditem;
	
	g_return_val_if_fail (loc != NULL, NULL);

	ditem = gnome_desktop_item_new_from_uri (loc,
						 0 /* flags */,
						 NULL /* error */);		 

	/* watch the enum at the top of the file */
	dialog = gtk_dialog_new_with_buttons (_("Launcher Properties"),
					      NULL /* parent */,
					      0 /* flags */,
					      GTK_STOCK_HELP,
					      GTK_RESPONSE_HELP,
					      GTK_STOCK_REVERT_TO_SAVED,
					      REVERT_BUTTON,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_CLOSE,
					      NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	dedit = gnome_ditem_edit_new ();

	gtk_widget_show (dedit);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    dedit, TRUE, TRUE, 0);

	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "desktop_entry_properties","Panel");
	
	g_object_set_data_full (G_OBJECT (dedit), "location",
				g_strdup (loc),
				(GDestroyNotify)g_free);

	if (ditem != NULL)
		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (dedit), ditem);

	set_ditem_sensitive (GTK_DIALOG (dialog),
			     GNOME_DITEM_EDIT (dedit),
			     loc, dir);

	g_signal_connect (G_OBJECT (dedit), "changed",
			    G_CALLBACK (ditem_properties_changed),
			    NULL);

	g_signal_connect (G_OBJECT (dialog), "destroy",
			    G_CALLBACK (ditem_properties_close),
			    dedit);

	g_object_set_data (G_OBJECT (dialog), "GnomeDItemEdit", dedit);

	if (ditem != NULL) {
		/* pass the ditem as the data to clicked */
		g_signal_connect_data (G_OBJECT (dialog), "response",
				       G_CALLBACK (ditem_properties_clicked),
				       ditem,
				       (GClosureNotify) gnome_desktop_item_unref,
				       0 /* connect_flags */);
	} else {
		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (ditem_properties_clicked),
				  NULL);
	}

	gtk_widget_show (dialog);

	gnome_ditem_edit_grab_focus (GNOME_DITEM_EDIT (dedit));

	return dialog;
}

GtkWidget *
panel_edit_direntry (const char *dir, const char *dir_name)
{
	GtkWidget *dialog;
	GtkWidget *dedit;
	GnomeDesktopItem *ditem;
	char *dirfile;
	
	dirfile = g_strconcat (dir, "/", ".directory", NULL);

	ditem = gnome_desktop_item_new_from_uri (dirfile,
						 0 /* flags */,
						 NULL /* error */);

	/* watch the enum at the top of the file */
	dialog = gtk_dialog_new_with_buttons (_("Launcher Properties"),
					      NULL /* parent */,
					      0 /* flags */,
					      GTK_STOCK_HELP,
					      GTK_RESPONSE_HELP,
					      GTK_STOCK_REVERT_TO_SAVED,
					      REVERT_BUTTON,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_CLOSE,
					      NULL);

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
				"desktop_entry_properties", "Panel");
	
	dedit = gnome_ditem_edit_new ();
	gtk_widget_show (dedit);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    dedit, TRUE, TRUE, 0);

	if (ditem != NULL) {
		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (dedit), ditem);
		g_object_set_data_full (G_OBJECT (dedit), "location",
					g_strdup (gnome_desktop_item_get_location (ditem)),
					(GDestroyNotify)g_free);
	} else {
		ditem = gnome_desktop_item_new ();
		if (dir_name == NULL) {
			gnome_desktop_item_set_string (ditem,
						       GNOME_DESKTOP_ITEM_NAME,
						       "Menu");
			gnome_desktop_item_set_localestring
				(ditem,
				 GNOME_DESKTOP_ITEM_NAME,
				 _("Menu"));
		} else {
			gnome_desktop_item_set_string (ditem,
						       GNOME_DESKTOP_ITEM_NAME,
						       dir_name);
		}
		gnome_desktop_item_set_string (ditem,
					       GNOME_DESKTOP_ITEM_TYPE,
					       "Directory");

		/* we free dirfile below so make a copy here */
		g_object_set_data_full (G_OBJECT (dedit),
					"location", g_strdup (dirfile),
					(GDestroyNotify)g_free);
		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (dedit), ditem);
	}

	gnome_ditem_edit_set_directory_only (GNOME_DITEM_EDIT (dedit),
					     TRUE /* directory_only */);

	set_ditem_sensitive (GTK_DIALOG (dialog),
			     GNOME_DITEM_EDIT (dedit),
			     dirfile, NULL);

	g_signal_connect (G_OBJECT (dedit), "changed",
			    G_CALLBACK (ditem_properties_changed),
			    NULL);

	g_signal_connect (G_OBJECT (dialog), "destroy",
			    G_CALLBACK (ditem_properties_close),
			    dedit);

	g_object_set_data (G_OBJECT (dialog), "GnomeDItemEdit", dedit);

	if (ditem != NULL) {
		/* pass the dentry as the data to clicked */
		g_signal_connect_data (G_OBJECT (dialog), "response",
				       G_CALLBACK (ditem_properties_clicked),
				       ditem,
				       (GClosureNotify) gnome_desktop_item_unref,
				       0 /* connect_flags */);
	} else {
		g_signal_connect (G_OBJECT (dialog), "response",
				    G_CALLBACK (ditem_properties_clicked),
				    NULL);
	}

	g_free (dirfile);
		
	gtk_widget_show (dialog);

	gnome_ditem_edit_grab_focus (GNOME_DITEM_EDIT (dedit));

	return dialog;
}

/* replaces '/' with returns _'s, originally from gmenu */
static void
validate_for_filename (char *file)
{
	char *ptr;

	g_return_if_fail (file != NULL);
	
	ptr = file;
	while (*ptr != '\0') {
		if (*ptr == '/')
			*ptr = '_';
		ptr++;
	}
}

static char *
get_schema (const char *dir)
{
	char *schema = g_strdup (dir);
	char *p = strchr (schema, ':');
	if (p == NULL) {
		g_free (schema);
		return NULL;
	} else {
		*p = '\0';
		return schema;
	}
}

static char *
get_unique_name (const char *dir, const char *name)
{
	int i;
	char *schema;
	char *full, *test;

	schema = get_schema (dir);

	full = g_strdup_printf ("%s/%s.desktop", dir, name);
	if ( ! panel_uri_exists (full)) {
		test = g_strdup_printf ("%s:%s.desktop", schema, name);
		if ( ! panel_uri_exists (test)) {
			g_free (schema);
			g_free (test);
			return full;
		}
		g_free (test);
	}
	g_free (full);

	i = 2;
	for (;;) {
		full = g_strdup_printf ("%s/%s%d.desktop", dir, name, i++);

		if ( ! panel_uri_exists (full)) {
			test = g_strdup_printf ("%s:%s.desktop", schema, name);
			if ( ! panel_uri_exists (test)) {
				g_free (schema);
				g_free (test);
				return full;
			}
			g_free (test);
		}
		g_free (full);
	}
}

static void
really_add_new_menu_item (GtkWidget *d, int response, gpointer data)
{
	GnomeDItemEdit *dedit = GNOME_DITEM_EDIT(data);
	char *dir = g_object_get_data (G_OBJECT (d), "dir");
	GnomeDesktopItem *ditem;
	GError *error = NULL;
	char *name, *loc;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (d);
		return;
	}

	g_return_if_fail (dir != NULL);

	panel_push_window_busy (d);

	ditem = gnome_ditem_edit_get_ditem (dedit);

	if ((gnome_desktop_item_get_entry_type (ditem) == GNOME_DESKTOP_ITEM_TYPE_APPLICATION &&
	     string_empty (gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_EXEC))) ||
	    (gnome_desktop_item_get_entry_type (ditem) == GNOME_DESKTOP_ITEM_TYPE_LINK &&
	     string_empty (gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_URL)))) {
		gnome_desktop_item_unref (ditem);
		panel_error_dialog ("cannot_create_launcher",
				    _("Cannot create the launcher.\n\n"
				      "No command or url specified."));
		return;
	}

	/* assume we are making a new file */
	name = g_strdup (gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_NAME));

	validate_for_filename (name);

	loc = get_unique_name (dir, name);
	gnome_desktop_item_set_location_file (ditem, loc);
	g_free (name);

	error = NULL;
	gnome_desktop_item_save (ditem,
				 NULL /* under */,
				 TRUE /* force */,
				 &error);
	if (error != NULL) {
		panel_error_dialog ("cannot_save_menu_item" /* class */,
				    _("Cannot save menu item to disk, "
				      "the following error occured:\n\n"
				      "%s"),
				    error->message);
		g_clear_error (&error);
	}

	gnome_desktop_item_unref (ditem);

	panel_pop_window_busy (d);

	gtk_widget_destroy (d);
	g_free (loc);
}

GtkWidget *
panel_new_launcher (const char *item_loc)
{
	GtkWidget *dialog;
	GtkWidget *dee;

	if (!is_item_writable (item_loc, NULL)) {
		
		dialog = panel_error_dialog ("can_not_create_launcher",
				             _("You can not create a new launcher at this location since the location is not writable."));

		return dialog;
	}

	dialog = gtk_dialog_new_with_buttons (_("Create Launcher"),
					      NULL /* parent */,
					      0 /* flags */,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      NULL);

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
			       "create_menu_item", "Panel");
	
	dee = gnome_ditem_edit_new ();
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), dee,
			    TRUE, TRUE, GNOME_PAD_SMALL);

	gnome_ditem_edit_set_entry_type (GNOME_DITEM_EDIT (dee), 
					 "Application");

	g_object_set_data_full (G_OBJECT (dialog), "dir",
				g_strdup (item_loc),
				(GDestroyNotify)g_free);
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (really_add_new_menu_item),
			  dee);

	gtk_dialog_set_default_response (GTK_DIALOG(dialog),
					 GTK_RESPONSE_OK);

	gtk_widget_show_all (dialog);

	gnome_ditem_edit_grab_focus (GNOME_DITEM_EDIT (dee));

	return dialog;
}

