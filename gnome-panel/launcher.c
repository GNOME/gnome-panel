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



static void
launch (GtkWidget *widget, void *data)
{
	GnomeDesktopEntry *item = data;

	gnome_desktop_entry_launch (item);
}

static int
destroy_launcher(GtkWidget *widget, gpointer data)
{
	Launcher *launcher = data;
	GtkWidget *prop_dialog = gtk_object_get_data(GTK_OBJECT(launcher->button),
						     LAUNCHER_PROPERTIES);
	if(prop_dialog)
		gtk_widget_destroy(prop_dialog);
	gnome_desktop_entry_free(launcher->dentry);
	g_free(launcher);
	return FALSE;
}


static Launcher *
create_launcher (char *parameters, GnomeDesktopEntry *dentry)
{
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

	launcher->button = NULL;
	launcher->dedit = NULL;
	icon = dentry->icon;
	if (icon && *icon) {
		/* Sigh, now we need to make them local to the gnome install */
		if (*icon != '/') {
			dentry->icon = gnome_pixmap_file (icon);
			g_free (icon);
		}
		launcher->button = button_widget_new_from_file(dentry->icon,
							       LAUNCHER_TILE,
							       FALSE,
							       ORIENT_UP);
	}
	if (!launcher->button) {
		if (default_app_pixmap)
			launcher->button =
				button_widget_new_from_file(default_app_pixmap,
							    LAUNCHER_TILE,
							    FALSE,ORIENT_UP);
		else
			launcher->button = gtk_button_new_with_label (_("App"));
	}
	gtk_widget_show (launcher->button);

	gtk_signal_connect (GTK_OBJECT(launcher->button),
			    "clicked",
			    (GtkSignalFunc) launch,
			    dentry);
	
	gtk_signal_connect (GTK_OBJECT(launcher->button), "destroy",
			    GTK_SIGNAL_FUNC(destroy_launcher),
			    launcher);

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
	char *icon;
	
	if (page != -1)
		return;
	
			/* remove the old launcher callback */
	gtk_signal_disconnect_by_func(GTK_OBJECT(launcher->button),
			(GtkSignalFunc) launch, launcher->dentry);

	gnome_desktop_entry_free(launcher->dentry);
	launcher->dentry =
		gnome_dentry_get_dentry(GNOME_DENTRY_EDIT(launcher->dedit));

			/* and install the new one with the right dentry pointer */
	gtk_signal_connect (GTK_OBJECT(launcher->button),
			    "clicked",
			    (GtkSignalFunc) launch,
			    launcher->dentry);

	gtk_tooltips_set_tip (panel_tooltips,launcher->button,
			      launcher->dentry->comment,NULL);
	
	/*it also might be a text button*/
	if(IS_BUTTON_WIDGET(launcher->button)) {
		icon = launcher->dentry->icon;
		if (icon && *icon) {
			/* Sigh, now we need to make them local to the gnome
			   install */
			if (*icon != '/') {
				launcher->dentry->icon = gnome_pixmap_file (icon);
				g_free (icon);
			}
			if(!button_widget_set_pixmap_from_file (BUTTON_WIDGET(launcher->button),
								 launcher->dentry->icon))
				/*we know default_app_pixmap is ok since otherwise we
				  wouldn't get here*/
				button_widget_set_pixmap_from_file (BUTTON_WIDGET(launcher->button),
								    default_app_pixmap);
		} else {
			/*we know default_app_pixmap is ok since otherwise we
			  wouldn't get here*/
			button_widget_set_pixmap_from_file (BUTTON_WIDGET(launcher->button),
							    default_app_pixmap);
		}
	}
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

static GtkWidget *
create_properties_dialog(Launcher *launcher)
{
	GtkWidget  *dialog;

	dialog = gnome_property_box_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Launcher properties"));
	/*gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	launcher->dedit =
		gnome_dentry_edit_new_notebook(
		      GTK_NOTEBOOK(GNOME_PROPERTY_BOX(dialog)->notebook));

	gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(launcher->dedit),
				     launcher->dentry);
	
	gtk_signal_connect_object(GTK_OBJECT(launcher->dedit), "changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(dialog));

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
	GtkWidget         *dialog;

	dialog = gtk_object_get_data(GTK_OBJECT(launcher->button),
				     LAUNCHER_PROPERTIES);
	if(dialog) {
		gdk_window_raise(dialog->window);
		return;
	}

	dialog = create_properties_dialog(launcher);
	gtk_object_set_data(GTK_OBJECT(launcher->button),
			    LAUNCHER_PROPERTIES,dialog);
	gtk_widget_show_all (dialog);
}

static void
_load_launcher_applet(char *params, GnomeDesktopEntry *dentry,
		      PanelWidget *panel, int pos)
{
	Launcher *launcher;

	launcher = create_launcher(params,dentry);

	if(launcher) {
		register_toy(launcher->button,launcher, panel, pos,
			     APPLET_LAUNCHER);

		gtk_tooltips_set_tip (panel_tooltips,
				      launcher->button,
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
	int pos = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(d),"pos"));
	PanelWidget *panel = gtk_object_get_data(GTK_OBJECT(d),"panel");
	if(button==0)
		_load_launcher_applet(NULL, gnome_dentry_get_dentry(dedit),
				      panel, pos);
	gtk_widget_destroy(d);
}

void
ask_about_launcher(char *file, PanelWidget *panel, int pos)
{
	GtkWidget *d;
	GtkWidget *notebook;
	GnomeDEntryEdit *dee;

	d = gnome_dialog_new(_("Create launcher applet"),
			     GNOME_STOCK_BUTTON_OK,
			     GNOME_STOCK_BUTTON_CANCEL,
			     NULL);
	/*gtk_window_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(d), FALSE, FALSE, TRUE);
	
	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),notebook,
			   TRUE,TRUE,5);
	dee = GNOME_DENTRY_EDIT(gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(notebook)));
	
	if(file)
		gtk_entry_set_text(GTK_ENTRY(dee->exec_entry), file);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(dee->type_combo)->entry),
			   "Application");
	
	
	gtk_object_set_data(GTK_OBJECT(d),"pos", GINT_TO_POINTER(pos));
	gtk_object_set_data(GTK_OBJECT(d),"panel",panel);

	gtk_signal_connect(GTK_OBJECT(d),"clicked",
			   GTK_SIGNAL_FUNC(really_add_launcher),
			   dee);

	gnome_dialog_close_hides(GNOME_DIALOG(d),FALSE);

	gnome_dialog_set_default(GNOME_DIALOG(d),0);

	gtk_widget_show_all(d);
}

void
load_launcher_applet(char *params, PanelWidget *panel, int pos)
{
	_load_launcher_applet(params,NULL,panel, pos);
}
