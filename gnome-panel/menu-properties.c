/*
 * GNOME panel menu-properties module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <gnome.h>

#include "panel-include.h"

/*#define PANEL_DEBUG 1*/

extern GlobalConfig global_config;

extern int config_sync_timeout;
extern int panels_to_sync;
extern int applets_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

char *
get_real_menu_path(char *arguments, char *menu_base)
{
	char *this_menu;
	
	g_return_val_if_fail(menu_base!=NULL,NULL);

	/*if null, let's put the main menu up*/
	if (!arguments || !*arguments)
		arguments = ".";

	if (*arguments == '/')
		this_menu = g_strdup (arguments);
	else
		this_menu = g_concat_dir_and_file (menu_base, arguments);

	if (!g_file_exists (this_menu)) {
		g_warning("menu %s does not exist "
			  "(base is %s, arguments are %s)",
			  this_menu, menu_base, arguments);
		g_free (this_menu);
		return NULL;
	}
	return this_menu;
}

char *
get_pixmap(char *menudir, int main_menu)
{
	char *pixmap_name;
	if (main_menu) {
		pixmap_name = gnome_unconditional_pixmap_file("gnome-logo-icon-transparent.png");
	} else {
		char *dentry_name;
		GnomeDesktopEntry *item_info;

		dentry_name = g_concat_dir_and_file (menudir,
						     ".directory");
		item_info = gnome_desktop_entry_load (dentry_name);
		g_free (dentry_name);

		if(item_info && item_info->icon)
		  pixmap_name = g_strdup(item_info->icon);
		else
		  pixmap_name =
		    gnome_unconditional_pixmap_file ("gnome-folder.png");

		if(item_info)
		  gnome_desktop_entry_free(item_info);
	}
	return pixmap_name;
}

static void
properties_apply_callback(GtkWidget *widget, int page, gpointer data)
{
	Menu *menu = data;
	GtkWidget *main_menu = gtk_object_get_data(GTK_OBJECT(widget), "main_menu");
	GtkWidget *system_off = gtk_object_get_data(GTK_OBJECT(widget), "system_off");
	GtkWidget *system_sub = gtk_object_get_data(GTK_OBJECT(widget), "system_sub");
	GtkWidget *user_off = gtk_object_get_data(GTK_OBJECT(widget), "user_off");
	GtkWidget *user_sub = gtk_object_get_data(GTK_OBJECT(widget), "user_sub");
	GtkWidget *redhat_off = gtk_object_get_data(GTK_OBJECT(widget), "redhat_off");
	GtkWidget *redhat_sub = gtk_object_get_data(GTK_OBJECT(widget), "redhat_sub");
 	GtkWidget *kde_off = gtk_object_get_data(GTK_OBJECT(widget), "kde_off");
 	GtkWidget *kde_sub = gtk_object_get_data(GTK_OBJECT(widget), "kde_sub");
	GtkWidget *debian_off = gtk_object_get_data(GTK_OBJECT(widget), "debian_off");
	GtkWidget *debian_sub = gtk_object_get_data(GTK_OBJECT(widget), "debian_sub");
	GtkWidget *pathentry = gtk_object_get_data(GTK_OBJECT(widget), "path");
	char *s;

	if (page != -1)
		return;
	
	applet_remove_callback(menu->info,"edit_menus");
	if(GTK_TOGGLE_BUTTON(main_menu)->active) {
		char *tmp;
		g_free(menu->path);
		menu->path = g_strdup(".");
		if((tmp = gnome_is_program_in_path("gmenu")))  {
			g_free(tmp);
			applet_add_callback(menu->info,"edit_menus",
					    NULL,
					    _("Edit menus..."));
		}
	} else {
		g_free(menu->path);
		s = gnome_file_entry_get_full_path(GNOME_FILE_ENTRY(pathentry),
						   TRUE);
		if(!s) {
			g_warning(_("Can't open directory, using main menu!"));
			menu->path = g_strdup(".");
		} else if(!*s)
			menu->path = g_strdup(".");
		else
			menu->path = g_strdup(s);
	}
	if(GTK_TOGGLE_BUTTON(system_off)->active)
		menu->main_menu_flags &=~ (MAIN_MENU_SYSTEM|MAIN_MENU_SYSTEM_SUB);
	else if(GTK_TOGGLE_BUTTON(system_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_SYSTEM|MAIN_MENU_SYSTEM_SUB;
	else {
		menu->main_menu_flags |= MAIN_MENU_SYSTEM;
		menu->main_menu_flags &=~ MAIN_MENU_SYSTEM_SUB;
	}
	if(GTK_TOGGLE_BUTTON(user_off)->active)
		menu->main_menu_flags &=~ (MAIN_MENU_USER|MAIN_MENU_USER_SUB);
	else if(GTK_TOGGLE_BUTTON(user_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_USER|MAIN_MENU_USER_SUB;
	else {
		menu->main_menu_flags |= MAIN_MENU_USER;
		menu->main_menu_flags &=~ MAIN_MENU_USER_SUB;
	}
	if(GTK_TOGGLE_BUTTON(redhat_off)->active)
		menu->main_menu_flags &=~ (MAIN_MENU_REDHAT|MAIN_MENU_REDHAT_SUB);
	else if(GTK_TOGGLE_BUTTON(redhat_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_REDHAT|MAIN_MENU_REDHAT_SUB;
	else {
		menu->main_menu_flags |= MAIN_MENU_REDHAT;
		menu->main_menu_flags &=~ MAIN_MENU_REDHAT_SUB;
	}
	if(GTK_TOGGLE_BUTTON(debian_off)->active)
		menu->main_menu_flags &=~ (MAIN_MENU_DEBIAN|MAIN_MENU_DEBIAN_SUB);
	else if(GTK_TOGGLE_BUTTON(debian_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_DEBIAN|MAIN_MENU_DEBIAN_SUB;
	else {
		menu->main_menu_flags |= MAIN_MENU_DEBIAN;
		menu->main_menu_flags &=~ MAIN_MENU_DEBIAN_SUB;
	}
	if(GTK_TOGGLE_BUTTON(kde_off)->active)
		menu->main_menu_flags &=~ (MAIN_MENU_KDE|MAIN_MENU_KDE_SUB);
	else if(GTK_TOGGLE_BUTTON(kde_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_KDE|MAIN_MENU_KDE_SUB;
	else {
		menu->main_menu_flags |= MAIN_MENU_KDE;
		menu->main_menu_flags &=~ MAIN_MENU_KDE_SUB;
	}

	if(menu->menu)
		gtk_widget_destroy(menu->menu);	
	menu->menu = NULL;

	{
		char *menu_base = gnome_unconditional_datadir_file ("gnome/apps");
		char *this_menu = get_real_menu_path(menu->path,menu_base);
		GSList *list = g_slist_append(NULL,this_menu);
		char *pixmap_name = get_pixmap(this_menu,
					       strcmp(menu->path,".")==0);
		/*make the pixmap*/
		button_widget_set_pixmap_from_file (BUTTON_WIDGET(menu->button),
						    pixmap_name, -1);
		g_free(pixmap_name);
		
		add_menu_widget(menu,list, strcmp(menu->path,".")==0, TRUE);
		
		g_free(menu_base);
		g_free(this_menu);

		g_slist_free(list);
	}
}

static void
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	gtk_object_set_data(GTK_OBJECT(menu->button),
			    MENU_PROPERTIES,NULL);
}

static void
toggle_prop(GtkWidget *widget, gpointer data)
{
	GnomePropertyBox *box = GNOME_PROPERTY_BOX (data);

	if(GTK_TOGGLE_BUTTON(widget)->active)
		gnome_property_box_changed (box);
}

static void
toggle_main_menu(GtkWidget *widget, gpointer data)
{
	GnomePropertyBox *box = GNOME_PROPERTY_BOX (data);
	GtkWidget *main_frame = gtk_object_get_data(GTK_OBJECT(box),
						    "main_frame");
	GtkWidget *normal_frame = gtk_object_get_data(GTK_OBJECT(box),
						      "normal_frame");
	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_set_sensitive(main_frame,TRUE);
		gtk_widget_set_sensitive(normal_frame,FALSE);
		gnome_property_box_changed (box);
	}
}
static void
toggle_normal_menu(GtkWidget *widget, gpointer data)
{
	GnomePropertyBox *box = GNOME_PROPERTY_BOX (data);
	GtkWidget *main_frame = gtk_object_get_data(GTK_OBJECT(box),
						    "main_frame");
	GtkWidget *normal_frame = gtk_object_get_data(GTK_OBJECT(box),
						      "normal_frame");
	if(GTK_TOGGLE_BUTTON(widget)->active) {
		gtk_widget_set_sensitive(main_frame,FALSE);
		gtk_widget_set_sensitive(normal_frame,TRUE);
		gnome_property_box_changed (box);
	}
}

static void
add_menu_type_options(GtkObject *dialog, GtkTable *table, int row,
		      char *title,char *ident, int on, int sub)
{
	char *p;
	GtkWidget *w;
	GtkWidget *rb;

	/*sanity checks*/
	if(!on)
		sub = FALSE;
	
	w = gtk_label_new(title);
	gtk_table_attach_defaults(table,w,0,1,row,row+1);
	
	rb = w = gtk_radio_button_new_with_label (NULL, _("Off"));
	gtk_table_attach_defaults(table,w,3,4,row,row+1);
	p = g_strconcat(ident,"_off",NULL);
	gtk_object_set_data(dialog,p,w);
	g_free(p);
	if(!on)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
	
	w = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(rb)),
					     _("In a submenu"));
	gtk_table_attach_defaults(table,w,2,3,row,row+1);
	p = g_strconcat(ident,"_sub",NULL);
	gtk_object_set_data(dialog,p,w);
	g_free(p);
	if(sub)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
	
	w = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(rb)),
					     _("On the main menu"));
	gtk_table_attach_defaults(table,w,1,2,row,row+1);
	if(on && !sub)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
}
	

static GtkWidget *
create_properties_dialog(Menu *menu)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *box;
	GtkWidget *table;
	GtkWidget *w,*w2;
	GtkWidget *f;
	GtkWidget *t;

	dialog = gnome_property_box_new();
	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "menu_properties","Panel");
	gtk_window_set_title(GTK_WINDOW(dialog), _("Menu properties"));
	/*gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	vbox = gtk_vbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(vbox),GNOME_PAD_SMALL);

	f = gtk_frame_new(_("Menu type"));
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(box),GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(f),box);
	
	w = gtk_radio_button_new_with_label (NULL, _("Main menu"));
	gtk_object_set_data(GTK_OBJECT(dialog),"main_menu",w);
	if(!menu->path || strcmp(menu->path,".")==0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_main_menu), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);

	w2 = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (w)),
			  _("Normal menu"));
	if(menu->path && strcmp(menu->path,".")!=0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w2), TRUE);
	gtk_signal_connect (GTK_OBJECT (w2), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_normal_menu), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w2,TRUE,TRUE,0);


	f = gtk_frame_new(_("Main menu"));
	if(menu->path && strcmp(menu->path,".")!=0)
		gtk_widget_set_sensitive(f,FALSE);
	gtk_object_set_data(GTK_OBJECT(dialog),"main_frame",f);
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	table = gtk_table_new(5,4,FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table),GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(f),table);

	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),0,
			      _("Programs: "),"system",
			      menu->main_menu_flags&MAIN_MENU_SYSTEM,
			      menu->main_menu_flags&MAIN_MENU_SYSTEM_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),1,
			      _("Favorites: "),"user",
			      menu->main_menu_flags&MAIN_MENU_USER,
			      menu->main_menu_flags&MAIN_MENU_USER_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),2,
			      _("AnotherLevel menu (if found): "),"redhat",
			      menu->main_menu_flags&MAIN_MENU_REDHAT,
			      menu->main_menu_flags&MAIN_MENU_REDHAT_SUB);
 	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),3,
 			      _("KDE menu (if found): "),"kde",
 			      menu->main_menu_flags&MAIN_MENU_KDE,
 			      menu->main_menu_flags&MAIN_MENU_KDE_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),4,
			      _("Debian menu (if found): "),"debian",
			      menu->main_menu_flags&MAIN_MENU_DEBIAN,
			      menu->main_menu_flags&MAIN_MENU_DEBIAN_SUB);
	
	f = gtk_frame_new(_("Normal menu"));
	if(!menu->path || strcmp(menu->path,".")==0)
		gtk_widget_set_sensitive(f,FALSE);
	gtk_object_set_data(GTK_OBJECT(dialog),"normal_frame",f);
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_container_set_border_width(GTK_CONTAINER(box),GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(f),box);
	
	w = gtk_label_new(_("Menu path"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);

	w = gnome_file_entry_new("menu_path",_("Browse"));
	gnome_file_entry_set_directory(GNOME_FILE_ENTRY(w), TRUE);

	t = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
	gtk_object_set_data(GTK_OBJECT(dialog),"path",w);
	if (menu->path)
		gtk_entry_set_text(GTK_ENTRY(t), menu->path);
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);
	gtk_signal_connect_object_while_alive (GTK_OBJECT (t), "changed",
					       GTK_SIGNAL_FUNC(gnome_property_box_changed),
					       GTK_OBJECT(dialog));
	
	gtk_notebook_append_page (GTK_NOTEBOOK(GNOME_PROPERTY_BOX (dialog)->notebook),
				  vbox, gtk_label_new (_("Menu")));
	
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
			   (GtkSignalFunc) properties_close_callback,
			   menu);

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(properties_apply_callback),
			   menu);

	return dialog;
}

void
menu_properties(Menu *menu)
{
	GtkWidget *dialog;

	dialog = gtk_object_get_data(GTK_OBJECT(menu->button),
				     MENU_PROPERTIES);
	if(dialog) {
		gdk_window_raise(dialog->window);
		gtk_widget_show(dialog);
		return;
	}

	dialog = create_properties_dialog(menu);
	gtk_object_set_data(GTK_OBJECT(menu->button),
			    MENU_PROPERTIES,dialog);
	gtk_widget_show_all (dialog);
}
