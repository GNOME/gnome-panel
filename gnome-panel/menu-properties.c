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
#include <sys/types.h>
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
extern int need_complete_save;

char *
get_real_menu_path(char *arguments)
{
	char *this_menu;
	
	/*if null, let's put the main menu up*/
	if (!arguments || !*arguments)
		arguments = ".";

	if(strcmp(arguments,".")==0)
		this_menu = gnome_unconditional_datadir_file ("gnome/apps");
	else if (*arguments == '/')
		this_menu = g_strdup (arguments);
	else if (*arguments == '~')
		this_menu = g_concat_dir_and_file (g_get_home_dir(),
						   &arguments[1]);
	else
		this_menu = gnome_unconditional_datadir_file (arguments);

	if (!g_file_exists (this_menu)) {
		g_warning("menu %s does not exist "
			  "(arguments are %s)",
			  this_menu, arguments);
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
	GtkWidget *system = gtk_object_get_data(GTK_OBJECT(widget), "system");
	GtkWidget *system_sub = gtk_object_get_data(GTK_OBJECT(widget), "system_sub");
	GtkWidget *user = gtk_object_get_data(GTK_OBJECT(widget), "user");
	GtkWidget *user_sub = gtk_object_get_data(GTK_OBJECT(widget), "user_sub");
	GtkWidget *applets = gtk_object_get_data(GTK_OBJECT(widget), "applets");
	GtkWidget *applets_sub = gtk_object_get_data(GTK_OBJECT(widget), "applets_sub");
	GtkWidget *distribution = gtk_object_get_data(GTK_OBJECT(widget), "distribution");
	GtkWidget *distribution_sub = gtk_object_get_data(GTK_OBJECT(widget), "distribution_sub");
 	GtkWidget *kde = gtk_object_get_data(GTK_OBJECT(widget), "kde");
 	GtkWidget *kde_sub = gtk_object_get_data(GTK_OBJECT(widget), "kde_sub");

	GtkWidget *panel = gtk_object_get_data(GTK_OBJECT(widget), "panel");
	GtkWidget *panel_sub = gtk_object_get_data(GTK_OBJECT(widget), "panel_sub");

	GtkWidget *desktop = gtk_object_get_data(GTK_OBJECT(widget), "desktop");
	GtkWidget *desktop_sub = gtk_object_get_data(GTK_OBJECT(widget), "desktop_sub");

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
	menu->main_menu_flags = 0;
	if (GTK_TOGGLE_BUTTON(system_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_SYSTEM_SUB;
	else if (GTK_TOGGLE_BUTTON(system)->active)
		menu->main_menu_flags |= MAIN_MENU_SYSTEM;

	if(GTK_TOGGLE_BUTTON(user_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_USER_SUB;
	else if (GTK_TOGGLE_BUTTON (user)->active)
		menu->main_menu_flags |= MAIN_MENU_USER;

	if(GTK_TOGGLE_BUTTON(applets_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_APPLETS_SUB;
	else if (GTK_TOGGLE_BUTTON (applets)->active)
		menu->main_menu_flags |= MAIN_MENU_APPLETS;

	if(GTK_TOGGLE_BUTTON(distribution_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_DISTRIBUTION_SUB;
	else if (GTK_TOGGLE_BUTTON (distribution)->active)
		menu->main_menu_flags |= MAIN_MENU_DISTRIBUTION;

	if(GTK_TOGGLE_BUTTON(kde_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_KDE_SUB;
	else if (GTK_TOGGLE_BUTTON (kde)->active)
		menu->main_menu_flags |= MAIN_MENU_KDE;

	if(GTK_TOGGLE_BUTTON(panel_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_PANEL_SUB;
	else if (GTK_TOGGLE_BUTTON(panel)->active)
		menu->main_menu_flags |= MAIN_MENU_PANEL;

	if(GTK_TOGGLE_BUTTON(desktop_sub)->active)
		menu->main_menu_flags |= MAIN_MENU_DESKTOP_SUB;
	else if (GTK_TOGGLE_BUTTON(desktop)->active)
		menu->main_menu_flags |= MAIN_MENU_DESKTOP;

	{
		char *this_menu = get_real_menu_path(menu->path);
		GSList *list = g_slist_append(NULL, this_menu);
		char *pixmap_name = get_pixmap(this_menu,
					       strcmp(menu->path,".")==0);
		/*make the pixmap*/
		button_widget_set_pixmap (BUTTON_WIDGET(menu->button),
					  pixmap_name, -1);
		g_free(pixmap_name);
		
		add_menu_widget(menu, PANEL_WIDGET(menu->button->parent), list, strcmp(menu->path,".")==0, TRUE);
		
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

	w = gtk_label_new(title);
	gtk_table_attach_defaults(table,w,0,1,row,row+1);
	
	rb = w = gtk_radio_button_new_with_label (NULL, _("Off"));
	gtk_table_attach_defaults(table,w,3,4,row,row+1);
	p = g_strconcat(ident,"_off",NULL);
	gtk_object_set_data(dialog,p,w);
	g_free(p);
	if(!on && !sub)
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
	gtk_object_set_data (dialog, ident, w);
	gtk_table_attach_defaults(table,w,1,2,row,row+1);
	if(on)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
}

static void
phelp_cb (GtkWidget *w, gint tab, gpointer data)
{
	char *page = GTK_TOGGLE_BUTTON (data)->active
		? "mainmenu.html#MAINMENUCONFIG"
		: "menus.html";
	panel_pbox_help_cb (NULL, 0, page);
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
	GtkWidget *the_toggle;

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
	the_toggle = w;
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
	
	table = gtk_table_new(7,4,FALSE);
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
			      _("Applets: "),"applets",
			      menu->main_menu_flags&MAIN_MENU_APPLETS,
			      menu->main_menu_flags&MAIN_MENU_APPLETS_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),3,
			      _("Distribution menu (if found): "),"distribution",
			      menu->main_menu_flags&MAIN_MENU_DISTRIBUTION,
			      menu->main_menu_flags&MAIN_MENU_DISTRIBUTION_SUB);
 	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),4,
 			      _("KDE menu (if found): "),"kde",
 			      menu->main_menu_flags&MAIN_MENU_KDE,
 			      menu->main_menu_flags&MAIN_MENU_KDE_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),6,
			      _("Panel menu: "),"panel",
			      menu->main_menu_flags&MAIN_MENU_PANEL,
			      menu->main_menu_flags&MAIN_MENU_PANEL_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),7,
			      _("Desktop menu: "),"desktop",
			      menu->main_menu_flags&MAIN_MENU_DESKTOP,
			      menu->main_menu_flags&MAIN_MENU_DESKTOP_SUB);

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
	if (menu->path) {
		char *s = get_real_menu_path(menu->path);
		gtk_entry_set_text(GTK_ENTRY(t), s);
		g_free(s);
	}
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
	gtk_signal_connect(GTK_OBJECT(dialog), "help",
			   GTK_SIGNAL_FUNC(phelp_cb), the_toggle);

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
