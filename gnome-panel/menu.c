/*
 * GNOME panel menu module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include "gnome.h"
#include "panel-widget.h"
#include "panel.h"
#include "panel_config_global.h"
#include "menu.h"


#define MENU_PATH "menu_path"

static char *gnome_folder = NULL;

GtkWidget *root_menu = NULL;

GList *small_icons = NULL;
int show_small_icons = TRUE;

extern GlobalConfig global_config;

typedef struct {
	char *translated;
	char *original_id;
} AppletItem;


void
activate_app_def (GtkWidget *widget, void *data)
{
	GnomeDesktopEntry *item = data;

	gnome_desktop_entry_launch (item);
}

static void
kill_small_icon(GtkWidget *widget, gpointer data)
{
	if(small_icons) g_list_remove(small_icons,widget);
}

void
setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title)
{
	GtkWidget *label, *hbox, *align;

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);
	
	hbox = gtk_hbox_new (FALSE, 0);
	
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_container_border_width (GTK_CONTAINER (align), 1);

	gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	gtk_container_add (GTK_CONTAINER (menuitem), hbox);

	if (pixmap) {
		gtk_container_add (GTK_CONTAINER (align), pixmap);
		gtk_widget_set_usize (align, 22, 16);
	} else
		gtk_widget_set_usize (align, 22, 16);

	small_icons = g_list_prepend (small_icons, align);
	gtk_signal_connect(GTK_OBJECT(align),"destroy",
			   GTK_SIGNAL_FUNC(kill_small_icon),NULL);

	gtk_widget_show (align);
	gtk_widget_show (hbox);
	gtk_widget_show (menuitem);
}

void
free_app_def (GtkWidget *widget, void *data)
{
	GnomeDesktopEntry *item = data;

	gnome_desktop_entry_free (item);
}

void
add_menu_separator (GtkWidget *menu)
{
	GtkWidget *menuitem;
	
	menuitem = gtk_menu_item_new ();
	gtk_widget_show (menuitem);
	gtk_container_add (GTK_CONTAINER (menu), menuitem);
}

void
free_string (GtkWidget *widget, void *data)
{
	g_free (data);
}

void
add_to_panel (char *applet, char *arg)
{
	load_applet(applet,arg,PANEL_UNKNOWN_APPLET_POSITION,0);
}

void
add_app_to_panel (GtkWidget *widget, void *data)
{
	GnomeDesktopEntry *ii = data;

	add_to_panel ("Launcher", ii->location);
}

void
add_dir_to_panel (GtkWidget *widget, void *data)
{
	add_to_panel (MENU_ID, data);
}

static GtkWidget *
create_menu_at (GtkWidget *window, char *menudir, int create_app_menu)
{	
	GnomeDesktopEntry *item_info;
	GtkWidget *menu;
	struct dirent *dent;
	struct stat s;
	char *filename;
	DIR *dir;
	int items = 0;
	
	dir = opendir (menudir);
	if (dir == NULL)
		return NULL;

	menu = gtk_menu_new ();
	
	while ((dent = readdir (dir)) != NULL) {
		GtkWidget     *menuitem, *sub, *pixmap;
		GtkSignalFunc  activate_func;
		char          *thisfile, *pixmap_name;
		char          *p;
		char          *menuitem_name;

		thisfile = dent->d_name;
		/* Skip over . and .. */
		if ((thisfile [0] == '.' && thisfile [1] == 0) ||
		    (thisfile [0] == '.' && thisfile [1] == '.' && thisfile [2] == 0))
			continue;

		filename = g_concat_dir_and_file (menudir, thisfile);
		if (stat (filename, &s) == -1) {
			g_free (filename);
			continue;
		}

		sub = 0;
		item_info = 0;
		if (S_ISDIR (s.st_mode)) {
			char *dentry_name;

			sub = create_menu_at (window, filename,
					      create_app_menu);
			if (!sub) {
				g_free (filename);
				continue;
			}

			dentry_name = g_concat_dir_and_file (filename, ".directory");
			item_info = gnome_desktop_entry_load (dentry_name);
			g_free (dentry_name);

			if (item_info)
				menuitem_name = item_info->name;
			else
				menuitem_name = thisfile;

			/* just for now */
			pixmap_name = NULL;

			if (create_app_menu) {
				GtkWidget *pixmap = NULL;
				char *text;
				char *dirname;

				/* create separator */

				menuitem = gtk_menu_item_new ();
				gtk_menu_prepend (GTK_MENU (sub), menuitem);
				gtk_widget_show (menuitem);

				/* create menu item */

				menuitem = gtk_menu_item_new ();
				if (gnome_folder) {
					pixmap = gnome_pixmap_new_from_file (gnome_folder);
					gtk_widget_show (pixmap);
				}

				text = g_copy_strings ("Menu: ", menuitem_name, NULL);
				setup_menuitem (menuitem, pixmap, text);
				g_free (text);
				
				dirname = g_strdup (filename);
				gtk_menu_prepend (GTK_MENU (sub), menuitem);
				gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
						    (GtkSignalFunc) add_dir_to_panel,
						    dirname);
				gtk_signal_connect (GTK_OBJECT (menuitem), "destroy",
						    (GtkSignalFunc) free_string,
						    dirname);
			}
		} else {
			if (strstr (filename, ".desktop") == 0) {
				g_free (filename);
				continue;
			}
			item_info = gnome_desktop_entry_load (filename);
			if (!item_info) {
				g_free (filename);
				continue;
			}
			menuitem_name = item_info->name;
			pixmap_name = item_info->small_icon;
		}
		
		items++;
		
		menuitem = gtk_menu_item_new ();
		if (sub)
			gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), sub);

		pixmap = NULL;
		if (pixmap_name && g_file_exists (pixmap_name)) {
			pixmap = gnome_pixmap_new_from_file (pixmap_name);
			if (pixmap)
				gtk_widget_show (pixmap);
		}

 		setup_menuitem (menuitem, pixmap, menuitem_name);
		gtk_menu_append (GTK_MENU (menu), menuitem);

		gtk_signal_connect (GTK_OBJECT (menuitem), "destroy",
				    (GtkSignalFunc) free_app_def, item_info);

		activate_func = create_app_menu ? (GtkSignalFunc) add_app_to_panel : (GtkSignalFunc) activate_app_def;
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate", activate_func, item_info);

		g_free (filename);
	}
	closedir (dir);

	if (items == 0) {
		gtk_widget_destroy (menu);
		menu = NULL;
	}
	
	return menu;
}

void
menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	Menu * menup = data;
	GtkWidget *widget = menup->button;
	int wx, wy;
	
	gdk_window_get_origin (widget->window, &wx, &wy);

	switch(menup->orient) {
		case MENU_DOWN:
			*x = wx;
			*y = wy + widget->allocation.height;
			break;
		case MENU_UP:
			*x = wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
			break;
		case MENU_RIGHT:
			*x = wx + widget->allocation.width;
			*y = wy;
			break;
		case MENU_LEFT:
			*x = wx - GTK_WIDGET (menu)->allocation.width;
			*y = wy;
			break;
	}

	if(*x + GTK_WIDGET (menu)->allocation.width > gdk_screen_width())
		*x=gdk_screen_width() - GTK_WIDGET (menu)->allocation.width;
	if(*x < 0) *x =0;

	if(*y + GTK_WIDGET (menu)->allocation.height > gdk_screen_height())
		*y=gdk_screen_height() - GTK_WIDGET (menu)->allocation.height;
	if(*y < 0) *y =0;
}

void
destroy_menu (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;

	if(menu->menu != root_menu)
		gtk_widget_unref(menu->menu);
	g_free(menu);
}

static gint
menu_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	Menu *menu = data;
	if(event->button==1) {
		gtk_menu_popup(GTK_MENU(menu->menu), 0,0, menu_position,
			       data, event->button, event->time);
		return TRUE;
	}
	return FALSE;
}

void
panel_configure (GtkWidget *widget, void *data)
{
	panel_config_global();
}

static AppletItem *
applet_item_new(char *translated, char *original_id)
{
	AppletItem *ai;

	ai = g_new(AppletItem, 1);
	ai->translated = translated;
	ai->original_id = original_id;

	return ai;
}

static void
applet_item_destroy(AppletItem *ai)
{
	g_free(ai->translated);
	g_free(ai->original_id);
	g_free(ai);
}

static void
add_applet_to_panel_data(GtkWidget *widget, gpointer data)
{
	add_to_panel((char *)data, NULL);
}

static void
add_applet_to_panel(GtkWidget *widget, gpointer data)
{
	add_to_panel(gtk_object_get_user_data(GTK_OBJECT(widget)), NULL);
}

static void
munge_applet_item(gpointer untrans, gpointer user_data)
{
	GList      **list;
	GList       *node;
	int          pos;
	char        *trans;

	list = user_data;

	/* Insert applet id in alphabetical order */
	
	node = *list;
	pos  = 0;

	trans = _(untrans);
	
	for (pos = 0; node; node = node->next, pos++)
		if (strcmp(trans, _(node->data)) < 0)
			break;

	*list = g_list_insert(*list,
			      applet_item_new(g_strdup(trans), untrans),
			      pos);
}

static void
append_applet_item_to_menu(gpointer data, gpointer user_data)
{
	GtkMenu    *menu;
	GtkWidget  *menuitem;
	AppletItem *ai;
	char       *oid;

	ai = data;
	menu = GTK_MENU(user_data);

	oid = g_strdup(ai->original_id);

	menuitem = gtk_menu_item_new();
	setup_menuitem(menuitem, NULL, ai->translated);
	gtk_object_set_user_data(GTK_OBJECT(menuitem), oid);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_applet_to_panel,
			   NULL);
	gtk_signal_connect(GTK_OBJECT(menuitem), "destroy",
			   (GtkSignalFunc) free_string,
			   oid);
	
	gtk_menu_append(menu, menuitem);

	/* Free applet item */

	applet_item_destroy(ai);
}

static GtkWidget *
create_applets_menu(void)
{
	GtkWidget    *menu;
	GList        *list;
	GList        *applets_list;

	return gtk_menu_new();

	/* Get list of applet types */

	/*FIXME:  */
	list = NULL;

	/* Now translate and sort them */

	applets_list = NULL;
	g_list_foreach(list, munge_applet_item, &applets_list);

	/* Create a menu of the translated and sorted ones */

	g_list_free(list);

	menu = gtk_menu_new();

	g_list_foreach(applets_list, append_applet_item_to_menu, menu);

	/* Destroy the list (the list items have already been freed by
	 * append_applet_item_to_menu()), and return the finished menu.
	 */

	g_list_free(applets_list);
	return menu;
}

static GtkWidget *
create_panel_submenu (GtkWidget *app_menu)
{
	GtkWidget *menu, *menuitem;

	menu = gtk_menu_new ();
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add to panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), app_menu);

	/*FIXME: */
	/*menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add applet"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
		create_applets_menu());*/

	add_menu_separator(menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add main menu"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_applet_to_panel_data,
			   MENU_ID);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add drawer"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_applet_to_panel_data,
			   DRAWER_ID);

	add_menu_separator(menu);
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Global properties..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
        gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    (GtkSignalFunc) panel_configure, 0);

	return menu;
}

static void
panel_lock (GtkWidget *widget, void *data)
{
	system ("gnome-lock");
}

static void
panel_logout (GtkWidget *widget, void *data)
{
	panel_quit();
}

static void
add_special_entries (GtkWidget *menu, GtkWidget *app_menu)
{
	GtkWidget *menuitem;
	
	/* Panel entry */

	add_menu_separator (menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_panel_submenu (app_menu));

	add_menu_separator (menu);
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Lock screen"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate", (GtkSignalFunc) panel_lock, 0);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Log out"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate", (GtkSignalFunc) panel_logout, 0);
}

static Menu *
create_panel_menu (GtkWidget *window, char *menudir, int main_menu,
		   MenuOrient orient)
{
	GtkWidget *pixmap;
	Menu *menu;
	GtkWidget *app_menu;
	
	char *pixmap_name;

	menu = g_new(Menu,1);

	if (main_menu)
		switch(orient) {
			case MENU_DOWN:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-down.xpm");
				break;
			case MENU_UP:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-up.xpm");
				break;
			case MENU_RIGHT:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-right.xpm");
				break;
			case MENU_LEFT:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-left.xpm");
				break;
		}
	else
		/*FIXME: these guys need arrows as well*/
		pixmap_name = gnome_unconditional_pixmap_file ("panel-folder.xpm");
		
	menu->orient = orient;

	/* main button */
	menu->button = gtk_event_box_new ();
	
	/*make the pixmap*/
	pixmap = gnome_pixmap_new_from_file (pixmap_name);
	gtk_widget_show(pixmap);
	/*FIXME:this is not right, but it's how we can get the buttons to
	  be 48x48 (given the icons are 48x48)*/
	gtk_widget_set_usize (menu->button, pixmap->requisition.width,
			      pixmap->requisition.height);

	/* put pixmap in button */
	gtk_container_add (GTK_CONTAINER(menu->button), pixmap);
	gtk_widget_show (menu->button);

	if (main_menu) {
		/*this should not happen init root menu should have
		  been called (unless there are no panels*/
		if(!root_menu) {
			root_menu = create_menu_at(window,menudir,0);
			app_menu = create_menu_at (window, menudir, 1);
			add_special_entries (root_menu, app_menu);
		}
		menu->menu = root_menu;
	} else {
		menu->menu = create_menu_at (window, menudir, 0);
	}
	gtk_signal_connect (GTK_OBJECT (menu->button), "button_press_event",
			    GTK_SIGNAL_FUNC (menu_button_press), menu);
	gtk_signal_connect (GTK_OBJECT (menu->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_menu), menu);

	g_free (pixmap_name);
	return menu;
}

GtkWidget *
init_main_menu(GtkWidget *window)
{
	GtkWidget *app_menu;
	char *menu_base = gnome_unconditional_datadir_file ("apps");
	char *menudir;

	menudir = g_concat_dir_and_file (menu_base, ".");
	g_free (menu_base);
	if (!g_file_exists (menudir)) {
		g_free (menudir);
		return NULL;
	}

	root_menu = create_menu_at(window,menudir,0);
	app_menu = create_menu_at (window,menudir, 1);
	add_special_entries (root_menu, app_menu);
	g_free (menudir);
}

Menu *
create_menu_applet(GtkWidget *window, char *arguments, MenuOrient orient)
{
	Menu *menu;
	int main_menu;

	char *menu_base = gnome_unconditional_datadir_file ("apps");
	char *this_menu;

	/*if null, let's put the main menu up*/
	if (arguments == NULL)
		arguments = ".";

	if (*arguments == '/')
		this_menu = g_strdup (arguments);
	else 
		this_menu = g_concat_dir_and_file (menu_base, arguments);

	if (!g_file_exists (this_menu)) {
		g_free (menu_base);
		g_free (this_menu);
		return NULL;
	}
	
	if(!gnome_folder) {
		gnome_folder = gnome_unconditional_pixmap_file
					("gnome-folder-small.xpm");
		if (!g_file_exists (gnome_folder)) {
			free (gnome_folder);
			gnome_folder = NULL;
		}
	}
	
	main_menu = (strcmp (arguments, ".") == 0);

	menu = create_panel_menu (window, this_menu, main_menu, orient);

	gtk_object_set_user_data(GTK_OBJECT(menu->button),menu);

	return menu;
}

void
set_show_small_icons(gpointer data, gpointer user_data)
{
	GtkWidget *w = data;
	if (!w) {
		g_warning("Internal error in set_show_small_icons (!w)");
		return;
	}
	if (global_config.show_small_icons)
		gtk_widget_show(w);
	else
		gtk_widget_hide(w);
}

static void
set_orientation(GtkWidget *window, Menu *menu)
{
	GtkWidget *pixmap;
	char *pixmap_name;

	if(!menu || !menu->path)
		return;

	if (strcmp (menu->path, ".") == 0)
		switch (menu->orient) {
			case MENU_DOWN:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-down.xpm");
				break;
			case MENU_UP:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-up.xpm");
				break;
			case MENU_RIGHT:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-right.xpm");
				break;
			case MENU_LEFT:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-left.xpm");
				break;
		}
	else
		/*FIXME: these guys need arrows as well*/
		pixmap_name = gnome_unconditional_pixmap_file ("panel-folder.xpm");
		
	pixmap=GTK_BUTTON(menu->button)->child;
	gtk_container_remove(GTK_CONTAINER(menu->button),pixmap);
	gtk_widget_destroy(pixmap);

	/*make the pixmap*/
	pixmap = gnome_pixmap_new_from_file (pixmap_name);

	gtk_container_add (GTK_CONTAINER(menu->button), pixmap);
	gtk_widget_show (pixmap);
	
	g_free(pixmap_name);
}
