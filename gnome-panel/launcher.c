/*
 * GNOME panel launcher module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 * CORBAized by George Lebl
 * de-CORBAized by George Lebl
 */

#include <config.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <gnome.h>

#include "panel-include.h"

#define LAUNCHER_PROPERTIES "launcher_properties"

extern GtkTooltips *panel_tooltips;
extern int applet_count;

static char *default_app_pixmap=NULL;

extern PanelWidget *current_panel;

static void
launch (GtkWidget *widget, void *data)
{
	GnomeDesktopEntry *item = data;

	gnome_desktop_entry_launch (item);
}

Launcher *
create_launcher (char *parameters, GnomeDesktopEntry *dentry)
{
	GtkWidget *pixmap;
	char *icon;
	Launcher *launcher;

	if (!default_app_pixmap)
		default_app_pixmap = gnome_pixmap_file ("gnome-unknown.png");

	if(!dentry) {
		if (!parameters)
			return NULL;
		else if (*parameters == '/')
			dentry = gnome_desktop_entry_load (parameters);
		else {
			char *apps_par, *entry, *extension;

			if (strstr (parameters, ".desktop"))
				extension = NULL;
			else
				extension = ".desktop";

			apps_par = g_copy_strings ("apps/", parameters,
						   extension, NULL);
			entry = gnome_datadir_file (apps_par);
			g_free (apps_par);

			if (!entry)
				return NULL;
			dentry = gnome_desktop_entry_load (entry);
			g_free (entry);
		}
	}
	if (!dentry)
		return NULL; /*button is null*/

	launcher = g_new(Launcher,1);

	launcher->button = gtk_button_new ();
	launcher->dedit = NULL;
	pixmap = NULL;
	icon = dentry->icon;
	if (icon && *icon) {
		/* Sigh, now we need to make them local to the gnome install */
		if (*icon != '/') {
			dentry->icon = gnome_pixmap_file (icon);
			g_free (icon);
		}
		pixmap = gnome_pixmap_new_from_file (dentry->icon);
	}
	if (!pixmap) {
		if (default_app_pixmap)
			pixmap = gnome_pixmap_new_from_file(default_app_pixmap);
		else
			pixmap = gtk_label_new (_("App"));
	}
	gtk_container_add (GTK_CONTAINER(launcher->button), pixmap);
	gtk_widget_show (pixmap);
	/*gtk_widget_set_usize (launcher->button, pixmap->requisition.width,
			      pixmap->requisition.height);*/
	/*FIXME: we'll do this better alter, but this makes it look fine with
	  normal icons*/
	gtk_widget_set_usize (launcher->button, 48, 48);
	
	gtk_widget_show (launcher->button);

	launcher->signal_click_tag =
		gtk_signal_connect (GTK_OBJECT(launcher->button),
				    "clicked",
				    (GtkSignalFunc) launch,
				    dentry);

	gtk_object_set_user_data(GTK_OBJECT(launcher->button), launcher);

	launcher->dentry = dentry;

	launcher->applet_id = -1;

	gtk_object_set_data(GTK_OBJECT(launcher->button),
			    LAUNCHER_PROPERTIES,NULL);

	return launcher;
}

static void
properties_apply_callback(GtkWidget *widget, int page, gpointer data)
{
	Launcher *launcher = data;
	GtkWidget *pixmap;
	char *icon;
	
	if (page != -1)
		return;
	
	gnome_desktop_entry_free(launcher->dentry);
	launcher->dentry =
		gnome_dentry_get_dentry(GNOME_DENTRY_EDIT(launcher->dedit));

	gtk_tooltips_set_tip (panel_tooltips,launcher->button->parent,
			      launcher->dentry->comment,NULL);
	
	pixmap=GTK_BUTTON(launcher->button)->child;

	gtk_container_remove(GTK_CONTAINER(launcher->button),pixmap);

	icon = launcher->dentry->icon;
	if (icon && *icon) {
		/* Sigh, now we need to make them local to the gnome install */
		if (*icon != '/') {
			launcher->dentry->icon = gnome_pixmap_file (icon);
			g_free (icon);
		}
		pixmap = gnome_pixmap_new_from_file (launcher->dentry->icon);
	}
	if (!pixmap) {
		if (default_app_pixmap)
			pixmap = gnome_pixmap_new_from_file (default_app_pixmap);
		else
			pixmap = gtk_label_new (_("App"));
	}
	gtk_container_add (GTK_CONTAINER(launcher->button), pixmap);
	gtk_widget_show(pixmap);

	/*FIXME: a bad hack to keep it all 48x48*/
	gtk_widget_set_usize (launcher->button, 48, 48);

	/*gtk_widget_set_usize (launcher->button, pixmap->requisition.width,
			      pixmap->requisition.height);*/

	gtk_signal_disconnect(GTK_OBJECT(launcher->button),
			      launcher->signal_click_tag);

	launcher->signal_click_tag =
		gtk_signal_connect (GTK_OBJECT(launcher->button), "clicked",
				    GTK_SIGNAL_FUNC(launch),
				    launcher->dentry);
}

static int
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Launcher *launcher = data;
	gtk_object_set_data(GTK_OBJECT(launcher->button),
			    LAUNCHER_PROPERTIES,NULL);
	launcher->dedit = NULL;
	return FALSE;
}

static void
notify_change (GtkWidget *widget, void *data)
{
	GnomePropertyBox *box = GNOME_PROPERTY_BOX (data);

	gnome_property_box_changed (box);
}

static GtkWidget *
create_properties_dialog(GnomeDesktopEntry *dentry, Launcher *launcher)
{
	GtkWidget  *dialog;

	dialog = gnome_property_box_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Launcher properties"));
	/*gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	launcher->dedit =
		gnome_dentry_edit_new(
		      GTK_NOTEBOOK(GNOME_PROPERTY_BOX(dialog)->notebook));

	gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(launcher->dedit),
				     launcher->dentry);
	
	gtk_signal_connect(GTK_OBJECT(launcher->dedit), "changed",
			   GTK_SIGNAL_FUNC(notify_change),
			   dialog);

	gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
			   GTK_SIGNAL_FUNC(properties_close_callback),
			   launcher);

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(properties_apply_callback),
			   launcher);

	return dialog;
}

void
launcher_properties(Launcher *launcher)
{
	GnomeDesktopEntry *dentry;
	char              *path;
	GtkWidget         *dialog;

	dialog = gtk_object_get_data(GTK_OBJECT(launcher->button),
				     LAUNCHER_PROPERTIES);
	if(dialog) {
		gdk_window_raise(dialog->window);
		return;
	}

	dialog = create_properties_dialog(dentry,launcher);
	gtk_object_set_data(GTK_OBJECT(launcher->button),
			    LAUNCHER_PROPERTIES,dialog);
	gtk_widget_show_all (dialog);
}

static void
_load_launcher_applet(char *params, GnomeDesktopEntry *dentry,
		      int pos, PanelWidget *panel)
{
	Launcher *launcher;

	launcher = create_launcher(params,dentry);

	if(launcher) {
		register_toy(launcher->button,launcher, pos,panel,
			     APPLET_LAUNCHER);

		gtk_tooltips_set_tip (panel_tooltips,
				      launcher->button->parent,
				      launcher->dentry->comment,NULL);

		applet_add_callback(applet_count-1,"properties",
				    GNOME_STOCK_MENU_PROP,
				    _("Properties..."));
	}
}

static void
really_add_launcher(GtkWidget *d,int button, gpointer data)
{
	GnomeDEntryEdit *dedit = GNOME_DENTRY_EDIT(data);
	if(button==0)
		_load_launcher_applet(NULL, gnome_dentry_get_dentry(dedit),
				      0, current_panel);
	gtk_widget_destroy(d);
}

void
ask_about_launcher(void)
{
	GtkWidget *d;
	GtkWidget *notebook;
	GtkObject *dedit;

	d = gnome_dialog_new(_("Create launcher applet"),
			     GNOME_STOCK_BUTTON_OK,
			     GNOME_STOCK_BUTTON_CANCEL,
			     NULL);
	/*gtk_window_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(d), FALSE, FALSE, TRUE);
	
	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),notebook,
			   TRUE,TRUE,5);
	dedit = gnome_dentry_edit_new(GTK_NOTEBOOK(notebook));
	gtk_signal_connect(GTK_OBJECT(d),"clicked",
			   GTK_SIGNAL_FUNC(really_add_launcher),
			   dedit);

	gnome_dialog_close_hides(GNOME_DIALOG(d),FALSE);

	gnome_dialog_set_default(GNOME_DIALOG(d),0);

	gtk_widget_show_all(d);
}

void
load_launcher_applet(char *params, int pos, PanelWidget *panel)
{
	_load_launcher_applet(params,NULL,pos,panel);
}
