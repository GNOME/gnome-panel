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

	gnome_desktop_item_unref (ditem);

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
	errno = 0;
	if (loc != NULL &&
	    /*A HACK: but it works, don't have it edittable if it's redhat
	      menus as they are auto generated!*/
	    /* FIXME: crap, need to port to gnome-vfs */
	    strstr (loc,"/" GNOME_DOT_GNOME "/apps-redhat/") == NULL &&
	    /*if it's a kdelnk file, don't let it be editted*/
	    ! is_ext (loc, ".kdelnk") &&
	    access (loc, W_OK) == 0) {
#ifdef MENU_DEBUG
		puts (loc);
#endif
		/*file exists and is writable, we're in bussines*/
		return TRUE;
	} else if ((loc == NULL ||
		    errno == ENOENT) &&
		   dir != NULL) {
		/*the dentry isn't there, check if we can write the
		  directory*/
		/* FIXME: crap, need to port to gnome-vfs */
		if (access (dir, W_OK) == 0 &&
		   /*A HACK: but it works, don't have it edittable if it's redhat
		     menus as they are auto generated!*/
		   strstr (dir, GNOME_DOT_GNOME "apps-redhat/") == NULL)
			return TRUE;
	}
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
	dialog = gtk_dialog_new_with_buttons (_("Desktop entry properties"),
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
	char *dirfile = g_build_path ("/", dir, ".directory", NULL);
	GnomeDesktopItem *ditem;

	ditem = gnome_desktop_item_new_from_uri (dirfile,
						 0 /* flags */,
						 NULL /* error */);
	if (ditem == NULL) {
		g_free (dirfile);
		return NULL;
	}

	/* watch the enum at the top of the file */
	dialog = gtk_dialog_new_with_buttons (_("Desktop entry properties"),
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
		g_free (dirfile);
		dirfile = NULL;
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
		/*we don't have to free dirfile here it will be freed as if
		  we had strduped it here*/
		g_object_set_data_full (G_OBJECT (dedit),
					"location", dirfile,
					(GDestroyNotify)g_free);
		dirfile = NULL;
		gnome_ditem_edit_set_ditem (GNOME_DITEM_EDIT (dedit), ditem);
	}

	gnome_ditem_edit_set_directory_only (GNOME_DITEM_EDIT (dedit),
					     TRUE /* directory_only */);

	set_ditem_sensitive (GTK_DIALOG (dialog),
			     GNOME_DITEM_EDIT (dedit),
			     NULL /* loc */, dir);

	g_signal_connect (G_OBJECT (dedit), "changed",
			    G_CALLBACK (ditem_properties_changed),
			    NULL);

	g_signal_connect (G_OBJECT (dialog), "destroy",
			    G_CALLBACK (ditem_properties_close),
			    dedit);

	/* YAIKES, the problem here is that the notebook will attempt
	 * to destroy the dedit, so if we unref it in the close handler,
	 * it will be finalized by the time the notebook will destroy it,
	 * dedit is just a horrible thing */
	g_signal_connect (G_OBJECT (dedit), "destroy",
			  G_CALLBACK (g_object_unref),
			  NULL);

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

	gtk_widget_show (dialog);

	gnome_ditem_edit_grab_focus (GNOME_DITEM_EDIT (dedit));

	return dialog;
}
