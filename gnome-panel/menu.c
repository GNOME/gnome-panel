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
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <gnome.h>
#include "panel-widget.h"
#include "snapped-widget.h"
#include "drawer-widget.h"
#include "corner-widget.h"
#include "panel-util.h"
#include "panel.h"
#include "main.h"
#include "panel_config_global.h"
#include "menu.h"
#include "mico-glue.h"

#define SMALL_ICON_SIZE 20
#define BIG_ICON_SIZE   48


#define MENU_PATH "menu_path"

static char *gnome_folder = NULL;

GtkWidget *root_menu = NULL;

GList *small_icons = NULL;
int show_small_icons = TRUE;

extern GlobalConfig global_config;

typedef struct _FileInfo FileInfo;
struct _FileInfo {
	char *name;
	time_t ctime;
};

/*???? this might be ugly, but I guess we can safely assume that we can only have
  one menu open and that nothing weird will happen to the panel that opened that
  menu whilethe user is looking over the choices*/
static PanelWidget *current_panel = NULL;

/*the most important dialog in the whole application*/
void
about_cb (GtkWidget *widget, gpointer data)
{
	GtkWidget *about;
	char *authors[] = {
	  "George Lebl (jirka@5z.com)",
	  "Miguel de Icaza (miguel@kernel.org)",
	  "Federico Mena (quartic@gimp.org)",
	  "Tom Tromey (tromey@cygnus.com)",
	  "Ian Main (slow@intergate.bc.ca)",
	  "and finally, The Knights Who Say ... NI!",
	  NULL
	  };

	about = gnome_about_new ( "The GNOME Panel", VERSION,
			"(C) 1998 the Free Software Foundation",
			authors,
			"This program is responsible for launching "
			"other applications, embedding small applets "
			"within itself, world peace, and random X crashes.",
			"gnome-gegl.png");
	gtk_widget_show (about);

	return;
}




void
activate_app_def (GtkWidget *widget, void *data)
{
	GnomeDesktopEntry *item = data;

	gnome_desktop_entry_launch (item);
}

static void
kill_small_icon(GtkWidget *widget, gpointer data)
{
	if(small_icons)
		small_icons = g_list_remove(small_icons,widget);
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
		gtk_widget_show (pixmap);
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

	if(item)
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

static int
add_to_panel (char *applet, char *path, char *arg)
{
	load_applet(applet,path,arg,NULL,NULL,PANEL_UNKNOWN_APPLET_POSITION,
		    current_panel,NULL);
	return TRUE;
}

static int
add_app_to_panel (GtkWidget *widget, void *data)
{
	GnomeDesktopEntry *ii = data;

	return add_to_panel (LAUNCHER_ID, NULL, ii->location);
}

static int
add_dir_to_panel (GtkWidget *widget, void *data)
{
	return add_to_panel (MENU_ID, NULL, data);
}


static int
add_applet (GtkWidget *w, gpointer data)
{
	GnomeDesktopEntry *ii = data;
	char *path;
	char *param;
	int r;

	path = ii->exec[0];

	g_return_val_if_fail(path!=NULL,FALSE);
	
	if((ii->exec[1])!=NULL)
		param = gnome_string_joinv (" ", ii->exec + 1);
	else
		param = NULL;

	r = add_to_panel(EXTERN_ID,path,param);

	if(param) g_free(param);
	return r;
}

static int
check_finfo_list(GList *finfo)
{
	struct stat s;
	FileInfo *fi;

	for(;finfo!=NULL;finfo=g_list_next(finfo)) {
		fi = finfo->data;
		if (stat (fi->name, &s) == -1)
			continue;
		if(fi->ctime != s.st_ctime)
			return FALSE;
	}
	return TRUE;
}

static void
free_finfo(FileInfo *fi)
{
	if(fi) {
		if(fi->name)
			g_free(fi->name);
		g_free(fi);
	}
}

static void
free_finfo_list(GList *finfo)
{
	while(finfo) {
		free_finfo(finfo->data);
		finfo = g_list_remove_link(finfo,finfo);
	}
}

static FileInfo *
make_finfo(char *name)
{
	struct stat s;
	FileInfo *fi;

	if (stat (name, &s) == -1)
		return NULL;

	fi = g_new(FileInfo,1);
	fi->name = g_strdup(name);
	fi->ctime = s.st_ctime;
	return fi;
}

static FileInfo *
make_finfo_s(char *name, struct stat *s)
{
	FileInfo *fi;

	fi = g_new(FileInfo,1);
	fi->name = g_strdup(name);
	fi->ctime = s->st_ctime;
	return fi;
}

/*reads in the order file and makes a list*/
static GList *
get_presorted_from(char *dir)
{
	char buf[PATH_MAX+1];
	GList *list = NULL;
	char *fname = g_concat_dir_and_file(dir,".order");
	FILE *fp = fopen(fname,"r");
	
	if(!fp) {
		g_free(fname);
		return NULL;
	}
	while(fgets(buf,PATH_MAX+1,fp)!=NULL) {
		char *p = strchr(buf,'\n');
		if(p) *p = '\0';
		list = g_list_append(list,g_strdup(buf));
	}
	fclose(fp);
	g_free(fname);
	return list;
}




static GtkWidget *
create_menu_at (char *menudir,
		int create_app_menu,
		int applets,
		GList **finfo)
{	
	GnomeDesktopEntry *item_info;
	GtkWidget *menu;
	struct dirent *dent;
	struct stat s;
	char *filename;
	DIR *dir;
	int items = 0;
	FileInfo *fi;
	GList *presorted = NULL;
	GList *done = NULL;
	char *thisfile;
	
	dir = opendir (menudir);
	if (dir == NULL)
		return NULL;
	
	/*add dir to the checked files list*/
	fi = make_finfo(menudir);
	if(!fi)
		g_warning("Something went wrong, directory %s can't be stated",
			  menudir);
	else
		*finfo = g_list_prepend(*finfo,fi);
	
	presorted = get_presorted_from(menudir);

	menu = gtk_menu_new ();
	
	while (presorted ||
	       (dent = readdir (dir)) != NULL) {
		GtkWidget     *menuitem, *sub, *pixmap;
		GtkSignalFunc  activate_func;
		char          *pixmap_name;
		char          *menuitem_name;
		
		if(presorted) {
			thisfile = presorted->data;
			filename = g_concat_dir_and_file(menudir,thisfile);
			presorted = g_list_remove_link(presorted,presorted);
		} else {
			thisfile = g_strdup(dent->d_name);
			/* Skip over . and .. */
			if ((thisfile [0] == '.' && thisfile [1] == 0) ||
			    (thisfile [0] == '.' && thisfile [1] == '.' &&
			     thisfile [2] == 0)) {
				g_free(thisfile);
				continue;
			}

			filename = g_concat_dir_and_file (menudir, thisfile);
		}
		if (stat (filename, &s) == -1 ||
		    string_is_in_list(done,filename)) {
			g_free (filename);
			g_free(thisfile);
			continue;
		}
		done = g_list_prepend(done,g_strdup(filename));

		sub = 0;
		item_info = 0;
		if (S_ISDIR (s.st_mode)) {
			char *dentry_name;

			sub = create_menu_at (filename, create_app_menu,
					      applets,finfo);
			if (!sub) {
				g_free (filename);
				g_free(thisfile);
				continue;
			}

			dentry_name = g_concat_dir_and_file (filename,
							     ".directory");
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
					pixmap = gnome_pixmap_new_from_file_at_size (gnome_folder,
										     BIG_ICON_SIZE,
										     BIG_ICON_SIZE);
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
				g_free(thisfile);
				continue;
			}
			item_info = gnome_desktop_entry_load (filename);
			if (!item_info) {
				g_free (filename);
				g_free(thisfile);
				continue;
			}
			menuitem_name = item_info->name;
			pixmap_name = item_info->icon;
			
			/*add file to the checked files list*/
			fi = make_finfo_s(filename,&s);
			if(!fi)
				g_warning("Something went wrong, "
					  "file %s can't be stated",
					  filename);
			else
				*finfo = g_list_prepend(*finfo,fi);
		}
		
		items++;
		
		menuitem = gtk_menu_item_new ();
		if (sub)
			gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), sub);

		pixmap = NULL;
		if (pixmap_name && g_file_exists (pixmap_name)) {
			pixmap = gnome_pixmap_new_from_file_at_size (pixmap_name,
								     SMALL_ICON_SIZE,
								     SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		}

 		setup_menuitem (menuitem, pixmap, menuitem_name);
		gtk_menu_append (GTK_MENU (menu), menuitem);

		gtk_signal_connect (GTK_OBJECT (menuitem), "destroy",
				    (GtkSignalFunc) free_app_def, item_info);

		if(item_info && item_info->exec) {
			activate_func = create_app_menu ?
				GTK_SIGNAL_FUNC(add_app_to_panel) :
				GTK_SIGNAL_FUNC(activate_app_def);
			if(applets && activate_app_def)
				activate_func = GTK_SIGNAL_FUNC(add_applet);
			gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
					    activate_func, item_info);
		}

		g_free (filename);
		g_free(thisfile);
	}
	closedir (dir);

	if (items == 0) {
		gtk_widget_destroy (menu);
		menu = NULL;
	}
	
	g_list_foreach(done,(GFunc)g_free,NULL);
	g_list_free(done);
	
	return menu;
}

void
menu_position (GtkMenu *menu, int *x, int *y, gpointer data)
{
	Menu * menup = data;
	GtkWidget *widget = menup->button;
	int wx, wy;
	
	gdk_window_get_origin (widget->window, &wx, &wy);

	switch(menup->orient) {
		case ORIENT_DOWN:
			*x = wx;
			*y = wy + widget->allocation.height;
			break;
		case ORIENT_UP:
			*x = wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
			break;
		case ORIENT_RIGHT:
			*x = wx + widget->allocation.width;
			*y = wy;
			break;
		case ORIENT_LEFT:
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

static void
destroy_menu (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;

	if(menu->menu != root_menu)
		gtk_widget_unref(menu->menu);
	else 
		/*this will disconnect the "deactivate" signal*/
		gtk_signal_disconnect_by_data(GTK_OBJECT(root_menu),menu);
	g_free(menu);
}

static void
menu_deactivate(GtkWidget *w, gpointer data)
{
	Menu *menu = data;
	GtkWidget *panel = get_panel_parent(menu->button->parent);
	/* allow the panel to hide again */
	if(IS_SNAPPED_WIDGET(panel))
		SNAPPED_WIDGET(panel)->autohide_inhibit = FALSE;
}

static char *
get_real_menu_path(char *arguments, char *menu_base)
{
	char *this_menu;

	/*if null, let's put the main menu up*/
	if (arguments == NULL)
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



void
panel_configure (GtkWidget *widget, void *data)
{
	panel_config_global();
}

static void
add_applet_to_panel_data(GtkWidget *widget, gpointer data)
{
	add_to_panel((char *)data, NULL, NULL);
}

#ifdef _SWALLOW_
static int
act_really_add_swallow(GtkWidget *w, gpointer data)
{
	GtkWidget *entry = data;
	GtkWidget *d = gtk_object_get_user_data(GTK_OBJECT(entry));

	gtk_widget_hide(d);
	add_to_panel(SWALLOW_ID, NULL, gtk_entry_get_text(GTK_ENTRY(entry)));

	return TRUE;
}

static int
really_add_swallow(GtkWidget *d,int button, gpointer data)
{
	GtkWidget *entry = data;

	gtk_widget_hide(d);
	if(button == 0)
		add_to_panel(SWALLOW_ID, NULL,
			     gtk_entry_get_text(GTK_ENTRY(entry)));
	return TRUE;
}

static void
add_swallow_to_panel(GtkWidget *widget, gpointer data)
{
	static GtkWidget *d=NULL;

	if(!d) {
		GtkWidget *w;
		GtkWidget *box;
		d = gnome_dialog_new(_("Create swallow applet"),
				     GNOME_STOCK_BUTTON_OK,
				     GNOME_STOCK_BUTTON_CANCEL,
				     NULL);
		box = gtk_hbox_new(FALSE,5);
		gtk_widget_show(box);
		gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),box,
				   TRUE,TRUE,5);

		w = gtk_label_new(_("Title of application to swallow"));
		gtk_widget_show(w);
		gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,5);
		w = gtk_entry_new();
		gtk_widget_show(w);
		gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,5);

		gtk_object_set_user_data(GTK_OBJECT(w),d);

		gtk_signal_connect(GTK_OBJECT(d),"clicked",
				   GTK_SIGNAL_FUNC(really_add_swallow),
				   w);
		gtk_signal_connect(GTK_OBJECT(w),"activate",
				   GTK_SIGNAL_FUNC(act_really_add_swallow),
				   w);

		gnome_dialog_close_hides(GNOME_DIALOG(d),TRUE);

		gnome_dialog_set_default(GNOME_DIALOG(d),0);

		gtk_widget_grab_focus(w);
	}
	gtk_widget_show(d);
}
#endif


static GtkWidget *
create_applets_menu(GList **finfo)
{
	GtkWidget *applet_menu;
	char *menu_base = gnome_unconditional_datadir_file ("applets");
	char *menudir;

	menudir = g_concat_dir_and_file (menu_base, ".");
	g_free (menu_base);
	if (!g_file_exists (menudir)) {
		g_free (menudir);
		return NULL;
	}

	applet_menu = create_menu_at(menudir,0,TRUE,finfo);
	g_free (menudir);
	return applet_menu;
}

static void
create_new_panel(GtkWidget *w,gpointer data)
{
	PanelType type = PTOI(data);
	GdkColor bcolor = {0,0,0,1};
	GtkWidget *panel;
	
	switch(type) {
	case DRAWER_PANEL:
		break; /*we don't handle this one here*/
	case SNAPPED_PANEL: 
		panel = snapped_widget_new(SNAPPED_BOTTOM,
					   SNAPPED_EXPLICIT_HIDE,
					   SNAPPED_SHOWN,
					   PANEL_BACK_NONE,
					   NULL,
					   TRUE,
					   &bcolor);
		panel_setup(panel);
		gtk_widget_show(panel);
		break;
	case CORNER_PANEL: 
		panel = corner_widget_new(CORNER_NE,
					  PANEL_HORIZONTAL,
					  CORNER_SHOWN,
					  PANEL_BACK_NONE,
					  NULL,
					  TRUE,
					  &bcolor);
		panel_setup(panel);
		gtk_widget_show(panel);
		break;
	default: break;
	}
}


static GtkWidget *
create_add_panel_submenu (void)
{
	GtkWidget *menu, *menuitem;

	menu = gtk_menu_new ();
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Drawer"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_applet_to_panel_data,
			   DRAWER_ID);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Edge Panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) create_new_panel,
			   (gpointer)SNAPPED_PANEL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Corner Panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) create_new_panel,
			   (gpointer)CORNER_PANEL);

	return menu;
}

static GtkWidget *
create_panel_submenu (GtkWidget *app_menu, GtkWidget *applet_menu)
{
	GtkWidget *menu, *menuitem;

	menu = gtk_menu_new ();
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add app to panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), app_menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add applet"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), applet_menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add new panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   create_add_panel_submenu());

	add_menu_separator(menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add main menu"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_applet_to_panel_data,
			   MENU_ID);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add log out button"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_applet_to_panel_data,
			   LOGOUT_ID);


#ifdef _SWALLOW_
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add swallowed app"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_swallow_to_panel,NULL);
#endif

	add_menu_separator(menu);
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
					  GNOME_STOCK_PIXMAP_PREFERENCES),
			_("Global properties..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
        gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    (GtkSignalFunc) panel_configure, 0);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_PIXMAP_ABOUT),
			_("About..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    (GtkSignalFunc) about_cb, 0);

	return menu;
}

static void
panel_lock (GtkWidget *widget, void *data)
{
	system ("(xlock&)");
}

static void
panel_logout (GtkWidget *widget, void *data)
{
	panel_quit();
}

static void
add_special_entries (GtkWidget *menu, GtkWidget *app_menu, GtkWidget *applet_menu)
{
	GtkWidget *menuitem;
	
	/* Panel entry */

	add_menu_separator (menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   create_panel_submenu (app_menu,applet_menu));

	add_menu_separator (menu);
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Lock screen"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_lock), 0);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_PIXMAP_QUIT),
			_("Log out"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_logout), 0);

}

static void
add_menu_widget (Menu *menu, char *menudir, int main_menu)
{
	GtkWidget *app_menu;
	GtkWidget *applet_menu;
	GList *finfo = NULL;
	
	if (main_menu) {
		/*this should not happen init root menu should have
		  been called (unless there are no panels*/
		if(!root_menu) {
			root_menu = create_menu_at(menudir,0,FALSE,&finfo);
			app_menu = create_menu_at (menudir, 1,FALSE,&finfo);
			applet_menu = create_applets_menu(&finfo);
			add_special_entries (root_menu, app_menu, applet_menu);
			gtk_object_set_data(GTK_OBJECT(root_menu),"finfo",
					    finfo);
		}
		menu->menu = root_menu;
	} else {
		menu->menu = create_menu_at (menudir, 0, FALSE,&finfo);
		gtk_object_set_data(GTK_OBJECT(menu->menu),"finfo",finfo);
	}
	gtk_signal_connect (GTK_OBJECT (menu->menu), "deactivate",
			    GTK_SIGNAL_FUNC (menu_deactivate), menu);

}

static int
menu_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	Menu *menu = data;
	if(event->button==1) {
		GtkWidget *wpanel = get_panel_parent(menu->button->parent);
			
		GList *finfo = gtk_object_get_data(GTK_OBJECT(menu->menu),
						   "finfo");
		
		if(!finfo)
			g_warning("Weird menu doesn't have finfo entry");
		else {
			/*if something has changed reread the menu*/
			if(!check_finfo_list(finfo)) {
				char *menu_base =
					gnome_unconditional_datadir_file ("apps");
				char *this_menu =
					get_real_menu_path(menu->path,
							   menu_base);
				
				puts("debug: rereading menu");

				if (this_menu) {
					int main_menu =
						(strcmp(menu->path,".")==0);
					
					/*free the menu releated data*/
					gtk_widget_unref(menu->menu);
					if(menu->menu == root_menu)
						root_menu = NULL;
					free_finfo_list(finfo);
					
					add_menu_widget(menu,this_menu,
							main_menu);

					g_free(menu_base);
					g_free(this_menu);
				} else
					g_free (menu_base);
			}
		}

		/*so that the panel doesn't pop down until we're done with
		  the menu */
		if(IS_SNAPPED_WIDGET(wpanel)) {
			SNAPPED_WIDGET(wpanel)->autohide_inhibit = TRUE;
			snapped_widget_queue_pop_down(SNAPPED_WIDGET(wpanel));
		}
		
		/*this HAS to be set everytime we popup the menu*/
		current_panel =
			gtk_object_get_data(GTK_OBJECT(menu->button->parent),
					    PANEL_APPLET_PARENT_KEY);

		gtk_menu_popup(GTK_MENU(menu->menu), 0,0, menu_position,
			       data, event->button, event->time);
		return TRUE;
	}
	return FALSE;
}


static Menu *
create_panel_menu (char *menudir, int main_menu,
		   PanelOrientType orient)
{
	GtkWidget *pixmap;
	Menu *menu;
	
	char *pixmap_name = NULL;

	menu = g_new(Menu,1);

	if (main_menu)
		switch(orient) {
			case ORIENT_DOWN:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-down.png");
				break;
			case ORIENT_UP:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-up.png");
				break;
			case ORIENT_RIGHT:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-right.png");
				break;
			case ORIENT_LEFT:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-left.png");
				break;
		}
	else
		/*FIXME: these guys need arrows as well*/
		pixmap_name = gnome_unconditional_pixmap_file ("gnome-folder.png");

	menu->orient = orient;

	/* main button */
	menu->button = gtk_event_box_new ();
	
	/*make the pixmap*/
	pixmap = gnome_pixmap_new_from_file_at_size (pixmap_name,
						     BIG_ICON_SIZE,
						     BIG_ICON_SIZE);
	gtk_widget_show(pixmap);
	/*FIXME:this is not right, but it's how we can get the buttons to
	  be 48x48 (given the icons are 48x48)*/
	gtk_widget_set_usize (menu->button, pixmap->requisition.width,
			      pixmap->requisition.height);

	/* put pixmap in button */
	gtk_container_add (GTK_CONTAINER(menu->button), pixmap);
	gtk_widget_show (menu->button);

	add_menu_widget(menu,menudir,main_menu);

	gtk_signal_connect (GTK_OBJECT (menu->button), "button_press_event",
			    GTK_SIGNAL_FUNC (menu_button_press), menu);
	gtk_signal_connect (GTK_OBJECT (menu->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_menu), menu);

	g_free (pixmap_name);

	return menu;
}

void
init_main_menu(void)
{
	GtkWidget *app_menu;
	GtkWidget *applet_menu;
	char *menu_base = gnome_unconditional_datadir_file ("apps");
	char *menudir;
	GList *finfo = NULL;

	menudir = g_concat_dir_and_file (menu_base, ".");
	g_free (menu_base);
	if (!g_file_exists (menudir)) {
		g_free (menudir);
		return;
	}

	root_menu = create_menu_at(menudir,0,FALSE,&finfo);
	app_menu = create_menu_at (menudir, 1,FALSE,&finfo);
	applet_menu = create_applets_menu(&finfo);
	add_special_entries (root_menu, app_menu, applet_menu);
	g_free (menudir);
	
	gtk_object_set_data(GTK_OBJECT(root_menu),"finfo",finfo);
}

Menu *
create_menu_applet(char *arguments, PanelOrientType orient)
{
	Menu *menu;
	int main_menu;

	char *menu_base = gnome_unconditional_datadir_file ("apps");
	char *this_menu = get_real_menu_path(arguments,menu_base);

	if (!this_menu) {
		g_free (menu_base);
		return NULL;
	}
	

	if(!gnome_folder) {
		gnome_folder = gnome_unconditional_pixmap_file
					("gnome-folder.png");
		if (!g_file_exists (gnome_folder)) {
			free (gnome_folder);
			gnome_folder = NULL;
		}
	}
	
	main_menu = (!arguments || (strcmp (arguments, ".") == 0));

	menu = create_panel_menu (this_menu, main_menu, orient);
	menu->path=g_strdup(arguments?arguments:".");

	gtk_object_set_user_data(GTK_OBJECT(menu->button),menu);
	
	g_free(menu_base);
	g_free (this_menu);
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

void
set_menu_applet_orient(Menu *menu, PanelOrientType orient)
{
	GtkWidget *pixmap;
	char *pixmap_name = NULL;

	g_return_if_fail(menu!=NULL);
	g_return_if_fail(menu->path!=NULL);

	menu->orient = orient;

	if (strcmp (menu->path, ".") == 0)
		switch (menu->orient) {
			case ORIENT_DOWN:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-down.png");
				break;
			case ORIENT_UP:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-up.png");
				break;
			case ORIENT_RIGHT:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-right.png");
				break;
			case ORIENT_LEFT:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-left.png");
				break;
		}
	else
		/*FIXME: these guys need arrows as well*/
		pixmap_name = gnome_unconditional_pixmap_file ("gnome-folder.png");
		
	pixmap=GTK_BIN(menu->button)->child;
	gtk_container_remove(GTK_CONTAINER(menu->button),pixmap);
	/*this is done by remove right?*/
	/*gtk_widget_unref(pixmap);*/

	/*make the pixmap*/
	pixmap = gnome_pixmap_new_from_file_at_size (pixmap_name,
						     BIG_ICON_SIZE,
						     BIG_ICON_SIZE);

	gtk_container_add (GTK_CONTAINER(menu->button), pixmap);
	gtk_widget_show (pixmap);
	
	g_free(pixmap_name);
}
