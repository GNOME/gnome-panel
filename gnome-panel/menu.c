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
#include "swallow.h"
#include "mico-glue.h"

#define SMALL_ICON_SIZE 20
#define BIG_ICON_SIZE   48


#define MENU_PATH "menu_path"

static char *gnome_folder = NULL;

GList *small_icons = NULL;
int show_small_icons = TRUE;

extern GlobalConfig global_config;

extern int config_sync_timeout;
extern int panels_to_sync;
extern GList *applets_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

typedef struct _FileInfo FileInfo;
struct _FileInfo {
	char *name;
	time_t mtime;
};

typedef struct _MenuFinfo MenuFinfo;
struct _MenuFinfo {
	char *menudir;
	int create_app_menu;
	int applets;
	char *dir_name;
	int fake_menu;
	GList *finfo;
};

/*???? this might be ugly, but I guess we can safely assume that we can only
  have one menu open and that nothing weird will happen to the panel that
  opened that menu whilethe user is looking over the choices*/
PanelWidget *current_panel = NULL;

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
			(char **)authors,
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
	load_applet(applet,path,arg,
		    PANEL_UNKNOWN_APPLET_POSITION,
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
add_menu_to_panel (GtkWidget *widget, void *data)
{
	load_menu_applet(data,0,
			 PANEL_UNKNOWN_APPLET_POSITION,
			 current_panel);
	return TRUE;
}

static int
add_drawer_to_panel (GtkWidget *widget, void *data)
{
	load_drawer_applet(NULL,NULL,NULL,
			   PANEL_UNKNOWN_APPLET_POSITION,
			   current_panel);
	return TRUE;
}

static int
add_logout_to_panel (GtkWidget *widget, void *data)
{
	return add_to_panel(LOGOUT_ID,NULL,NULL);
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
		if(fi->mtime != s.st_mtime)
			return FALSE;
	}
	return TRUE;
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
	fi->mtime = s.st_mtime;
	return fi;
}

static FileInfo *
make_finfo_s(char *name, struct stat *s)
{
	FileInfo *fi;

	fi = g_new(FileInfo,1);
	fi->name = g_strdup(name);
	fi->mtime = s->st_mtime;
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

static int
menu_destroy(GtkWidget *menu, gpointer data)
{
	GList *mfl = gtk_object_get_data(GTK_OBJECT(menu),"mf");
	while(mfl) {
		MenuFinfo *mf = mfl->data;
		while(mf->finfo) {
			FileInfo *fi = mf->finfo->data;
			if(fi->name) g_free(fi->name);
			g_free(fi);
			mf->finfo = g_list_remove_link(mf->finfo, mf->finfo);
		}
		if(mf->menudir) g_free(mf->menudir);
		if(mf->dir_name) g_free(mf->dir_name);
		g_free(mf);
		mfl = g_list_remove_link(mfl,mfl);
	}
	gtk_object_set_data(GTK_OBJECT(menu),"mf",NULL);
	return FALSE;
}

static void
make_app_menu(GtkWidget *sub, char *pixmap_name, 
	      char *filename, char *menuitem_name)
{
	GtkWidget *pixmap = NULL;
	char *text;
	char *dirname;
	GtkWidget *menuitem;

	/* create separator */

	menuitem = gtk_menu_item_new ();
	gtk_menu_prepend (GTK_MENU (sub), menuitem);
	gtk_widget_show (menuitem);

	/* create menu item */

	menuitem = gtk_menu_item_new ();
	if (pixmap_name) {
		pixmap = gnome_pixmap_new_from_file_at_size (pixmap_name,
							     SMALL_ICON_SIZE,
							     SMALL_ICON_SIZE);
		gtk_widget_show (pixmap);
	} else if (gnome_folder) {
		pixmap = gnome_pixmap_new_from_file_at_size (gnome_folder,
							     SMALL_ICON_SIZE,
							     SMALL_ICON_SIZE);
		gtk_widget_show (pixmap);
	}

	text = g_copy_strings ("Menu: ", menuitem_name, NULL);
	setup_menuitem (menuitem, pixmap, text);
	g_free (text);

	dirname = g_strdup (filename);
	gtk_menu_prepend (GTK_MENU (sub), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(add_menu_to_panel),
			    dirname);
	gtk_signal_connect (GTK_OBJECT (menuitem), "destroy",
			    GTK_SIGNAL_FUNC(free_string),
			    dirname);
}

static void add_menu_widget (Menu *menu, GList *menudirl,
			     int main_menu, int fake_subs);
static GtkWidget * create_menu_at (GtkWidget *menu,
				   char *menudir, int create_app_menu,
				   int applets, char *dir_name,
				   int fake_submenus);

/*if menu is NULL that means just reread the menu don't do anything with the
  applet*/
static GtkWidget *
check_and_reread(GtkWidget *menuw,Menu *menu,int main_menu)
{
	/*we arecreating a whole new menuf or an applet if menu isn't NULL*/
	if(menu) {
		GList *mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");
		GList *list;
		int need_reread = FALSE;

		if(!mfl)
			g_warning("Weird menu doesn't have mf entry");

		/*check if we need to reread this*/
		for(list = mfl; list != NULL; list = g_list_next(list)) {
			MenuFinfo *mf = list->data;
			if(mf->fake_menu || !check_finfo_list(mf->finfo)) {
				need_reread = TRUE;
				break;
			}
		}

		if(need_reread) {
			puts("debug: rereading menu");
			if(main_menu)
				add_menu_widget(menu,NULL,main_menu,TRUE);
			else {
				GList *dirlist = NULL;
				for(list = mfl; list != NULL;
				    list = g_list_next(list)) {
					MenuFinfo *mf = list->data;
					dirlist = g_list_append(dirlist,
								mf->menudir);
				}
				add_menu_widget(menu,dirlist, main_menu,TRUE);
				g_list_free(dirlist);
			}

			gtk_widget_unref(menuw);
		}
	} else {
		GList *mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");
		GList *list;
		int need_reread = FALSE;
		int all_fake = TRUE;

		if(!mfl)
			g_warning("Weird menu doesn't have mf entry");

		/*check if we need to reread this*/
		for(list = mfl; list != NULL; list = g_list_next(list)) {
			MenuFinfo *mf = list->data;
			if(!mf->fake_menu)
				all_fake = FALSE;
			if(!need_reread &&
			   (mf->fake_menu || !check_finfo_list(mf->finfo)))
				need_reread = TRUE;
		}
		if(need_reread) {
			GtkWidget *old_menu = NULL;
			GList *free_list = NULL;
			/*we are all fake so we want to use the previous
			  menu widget*/
			if(all_fake) {
				old_menu = menuw;
				gtk_object_set_data(GTK_OBJECT(old_menu), "mf",
						    NULL);
				/*set this so that we free it after we use it,
				  since it won't be freed during the
				  destruction of the menu since there will
				  be none*/
				free_list = mfl;
			}
			for(list = mfl; list != NULL;
			    list = g_list_next(list)) {
				MenuFinfo *mf = list->data;
				char *pixmap_name =
					gtk_object_get_data(GTK_OBJECT(menuw),
							    "pixmap");
				puts("debug: rereading menu");
				menuw = create_menu_at(old_menu,
						       mf->menudir,
						       mf->create_app_menu,
						       mf->applets,
						       mf->dir_name,
						       TRUE);
				if(mf->create_app_menu && mf->dir_name)
					make_app_menu(menuw,
						      pixmap_name,
						      mf->menudir,
						      mf->dir_name);
				old_menu = menuw;
			}
			/*free up stuff that won't be freed somewhere else*/
			while(free_list) {
				MenuFinfo *mf = free_list->data;
				free_list = g_list_remove_link(free_list,
							       free_list);
			}
		}
	}
	return menuw;
}

static void
submenu_to_display(GtkMenuItem *menuitem, gpointer data)
{
	GtkWidget *menu = check_and_reread(menuitem->submenu,
					   NULL,FALSE);
	g_return_if_fail(menu!=NULL);
	if(menu!=menuitem->submenu)
		gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), menu);
}

static GtkWidget *
create_fake_menu_at (char *menudir,
		     int create_app_menu,
		     int applets,
		     char *dir_name)
{	
	MenuFinfo *mf;
	GtkWidget *menu;
	GList *list;
	
	menu = gtk_menu_new ();
	
	mf = g_new(MenuFinfo,1);
	mf->menudir = g_strdup(menudir);
	mf->create_app_menu = create_app_menu;
	mf->applets = applets;
	mf->dir_name = dir_name?g_strdup(dir_name):NULL;
	mf->fake_menu = TRUE;
	mf->finfo = NULL;
	
	list = g_list_prepend(NULL,mf);
	gtk_object_set_data(GTK_OBJECT(menu),"mf",list);
	
	gtk_signal_connect(GTK_OBJECT(menu),"destroy",
			   GTK_SIGNAL_FUNC(menu_destroy),NULL);
	
	return menu;
}

static GtkWidget *
create_menu_at (GtkWidget *menu,
		char *menudir,
		int create_app_menu,
		int applets,
		char *dir_name,
		int fake_submenus)
{	
	GnomeDesktopEntry *item_info;
	struct dirent *dent;
	struct stat s;
	char *filename;
	DIR *dir;
	int items = 0;
	FileInfo *fi;
	GList *finfo = NULL;
	GList *presorted = NULL;
	GList *done = NULL;
	GList *mfl = NULL;
	char *thisfile;
	int add_separator = FALSE;
	
	MenuFinfo *mf = NULL;
	
	dir = opendir (menudir);
	if (dir == NULL)
		return menu;
	
	/*add dir to the checked files list*/
	fi = make_finfo(menudir);
	if(!fi)
		g_warning("Something went wrong, directory %s can't be stated",
			  menudir);
	else
		finfo = g_list_prepend(finfo,fi);
	
	presorted = get_presorted_from(menudir);

	/*add the order file to the checked files list,
	  but only if we can stat it (if we can't it probably doesn't
	  exist)*/
	filename = g_concat_dir_and_file(menudir,".order");
	fi = make_finfo(filename);
	if(fi)
		finfo = g_list_prepend(finfo,fi);
	g_free(filename);


	if(!menu) {
		menu = gtk_menu_new ();
		gtk_signal_connect(GTK_OBJECT(menu),"destroy",
				   GTK_SIGNAL_FUNC(menu_destroy),NULL);
	} else {
		mfl = gtk_object_get_data(GTK_OBJECT(menu), "mf");
		if(GTK_MENU_SHELL(menu)->children)
			add_separator = TRUE;
	}
	
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

			dentry_name = g_concat_dir_and_file (filename,
							     ".directory");
			item_info = gnome_desktop_entry_load (dentry_name);
			g_free (dentry_name);

			if (item_info)
				menuitem_name = item_info->name;
			else
				menuitem_name = thisfile;

			if(fake_submenus)
				sub = create_fake_menu_at (filename,
							   create_app_menu,
							   applets,
							   menuitem_name);
			else
				sub = create_menu_at (NULL,
						      filename, create_app_menu,
						      applets, menuitem_name,
						      fake_submenus);
			if (!sub) {
				g_free(filename);
				g_free(thisfile);
				if(item_info)
					gnome_desktop_entry_free(item_info);
				continue;
			}


			pixmap_name = item_info?item_info->icon:NULL;
			
			gtk_object_set_data(GTK_OBJECT(sub),"pixmap",
					    pixmap_name);
			
			if (!fake_submenus && create_app_menu) {
				if(add_separator) {
					add_menu_separator(menu);
					add_separator = FALSE;
				}
				make_app_menu(sub,pixmap_name,filename,
					      menuitem_name);
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
				finfo = g_list_prepend(finfo,fi);
		}
		
		items++;
		
		menuitem = gtk_menu_item_new ();
		if (sub) {
			gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), sub);
			gtk_signal_connect(GTK_OBJECT(menuitem),"select",
					   GTK_SIGNAL_FUNC(submenu_to_display),
					   NULL);
		}

		pixmap = NULL;
		if (pixmap_name && g_file_exists (pixmap_name)) {
			pixmap = gnome_pixmap_new_from_file_at_size (pixmap_name,
								     SMALL_ICON_SIZE,
								     SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		}

 		setup_menuitem (menuitem, pixmap, menuitem_name);
		if(add_separator) {
			add_menu_separator(menu);
			add_separator = FALSE;
		}
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

		g_free(filename);
		g_free(thisfile);
	}
	closedir (dir);
	
	g_list_foreach(done,(GFunc)g_free,NULL);
	g_list_free(done);

	if (items == 0) {
		/*there are no other items in this menu*/
		if(!mfl) {
			gtk_widget_destroy (menu);
			return NULL;
		} 
	}
	
	mf = g_new(MenuFinfo,1);
	mf->menudir = g_strdup(menudir);
	mf->create_app_menu = create_app_menu;
	mf->applets = applets;
	mf->dir_name = dir_name?g_strdup(dir_name):NULL;
	mf->fake_menu = FALSE;
	mf->finfo = finfo;

	mfl = g_list_append(mfl,mf);

	gtk_object_set_data(GTK_OBJECT(menu),"mf",mfl);
	
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
	gtk_widget_unref(menu->menu);
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

static GtkWidget *
create_applets_menu(int fake_submenus)
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

	applet_menu = create_menu_at(NULL,menudir,0,TRUE,NULL,fake_submenus);
	g_free (menudir);
	return applet_menu;
}

static void
create_new_panel(GtkWidget *w,gpointer data)
{
	PanelType type = GPOINTER_TO_INT(data);
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
	panels_to_sync = TRUE;
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
			   (GtkSignalFunc) add_drawer_to_panel,
			   NULL);

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
	gtk_signal_connect(GTK_OBJECT(menuitem),"select",
			   GTK_SIGNAL_FUNC(submenu_to_display),
			   NULL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add applet"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), applet_menu);
	gtk_signal_connect(GTK_OBJECT(menuitem),"select",
			   GTK_SIGNAL_FUNC(submenu_to_display),
			   NULL);

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
			   (GtkSignalFunc) add_menu_to_panel,
			   NULL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add log out button"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_logout_to_panel,
			   NULL);


	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add swallowed app"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) ask_about_swallowing,NULL);

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

static GtkWidget *
create_root_menu(int fake_submenus, MainMenuType type)
{
	GtkWidget *root_menu;
	GtkWidget *uroot_menu;
	GtkWidget *app_menu;
	GtkWidget *applet_menu;
	char *menu_base = gnome_unconditional_datadir_file ("apps");
	char *menudir;
	char *user_menudir;

	menudir = g_concat_dir_and_file (menu_base, ".");
	g_free (menu_base);
	if (!g_file_exists (menudir)) {
		g_free (menudir);
		return;
	}

	root_menu = create_menu_at(NULL,menudir,FALSE,FALSE,NULL,fake_submenus);
	app_menu = create_menu_at (NULL,menudir, TRUE,FALSE,NULL,fake_submenus);
	g_free (menudir);
	
	menu_base = gnome_util_home_file ("apps");
	menudir = g_concat_dir_and_file (menu_base, ".");
	g_free (menu_base);
	if (!g_file_exists (menudir))
		mkdir (menudir, 0755);
	if (g_file_exists (menudir)) {
		if(type == MAIN_MENU_BOTH)
			root_menu = create_menu_at(root_menu,menudir,FALSE,
						   FALSE, NULL,fake_submenus);
		else
			uroot_menu = create_menu_at(NULL,menudir,FALSE,FALSE,
						    NULL,fake_submenus);
		app_menu = create_menu_at (app_menu,menudir, TRUE,FALSE,
					   NULL,fake_submenus);
	} else if(type != MAIN_MENU_BOTH)
		uroot_menu = gtk_menu_new();
	g_free (menudir);
	
	if(type == MAIN_MENU_USER) {
		GtkWidget *menuitem;
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 0, _("System Menus"));
		gtk_menu_append (GTK_MENU (uroot_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
					   root_menu);
		gtk_signal_connect(GTK_OBJECT(menuitem),"select",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   NULL);
		root_menu = uroot_menu;
	} else if(type == MAIN_MENU_SYSTEM) {
		GtkWidget *menuitem;
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 0, _("User Menus"));
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
					   uroot_menu);
		gtk_signal_connect(GTK_OBJECT(menuitem),"select",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   NULL);
	}

	applet_menu = create_applets_menu(FALSE);
	add_special_entries (root_menu, app_menu, applet_menu);
	
	return root_menu;
}

static void
add_menu_widget (Menu *menu, GList *menudirl, int main_menu, int fake_subs)
{
	GList *li;

	if (main_menu)
		menu->menu = create_root_menu(fake_subs, menu->main_menu_type);
	else {
		menu->menu = NULL;
		for(li=menudirl;li!=NULL;li=g_list_next(li))
			menu->menu = create_menu_at (menu->menu,li->data,
						     FALSE, FALSE, NULL,
						     fake_subs);
	}
	gtk_signal_connect (GTK_OBJECT (menu->menu), "deactivate",
			    GTK_SIGNAL_FUNC (menu_deactivate), menu);
}

static GtkWidget *
listening_parent(GtkWidget *widget)
{
	if (GTK_WIDGET_NO_WINDOW(widget))
		return listening_parent(widget->parent);

	return widget;
}

static int
menu_button_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	Menu *menu = data;
	if(event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *bevent = (GdkEventButton *)event; 
		if(bevent->button==1) {
			GtkWidget *wpanel =
				get_panel_parent(menu->button->parent);
			int main_menu = (strcmp (menu->path, ".") == 0);

			check_and_reread(menu->menu,menu,main_menu);

			/*so that the panel doesn't pop down until we're
			  done with the menu */
			if(IS_SNAPPED_WIDGET(wpanel)) {
				SNAPPED_WIDGET(wpanel)->autohide_inhibit = TRUE;
				snapped_widget_queue_pop_down(SNAPPED_WIDGET(wpanel));
			}

			/*this HAS to be set everytime we popup the menu*/
			current_panel =
				gtk_object_get_data(GTK_OBJECT(menu->button->parent),
						    PANEL_APPLET_PARENT_KEY);

			gtk_menu_popup(GTK_MENU(menu->menu), 0,0, menu_position,
				       data, bevent->button, bevent->time);
			return TRUE;
		}
		return gtk_widget_event(listening_parent(widget->parent), event);
	} else if(event->type == GDK_BUTTON_RELEASE)
		return gtk_widget_event(listening_parent(widget->parent), event);
	return FALSE;
}

static char *
get_pixmap(char *menudir, PanelOrientType orient, int main_menu)
{
	char *pixmap_name;
	if (main_menu) {
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
	} else {
		char *dentry_name;
		GnomeDesktopEntry *item_info;

		dentry_name = g_concat_dir_and_file (menudir,
						     ".directory");
		item_info = gnome_desktop_entry_load (dentry_name);
		g_free (dentry_name);

		if(item_info) {
			pixmap_name = g_strdup(item_info->icon);
			gnome_desktop_entry_free(item_info);
		} else
			pixmap_name = gnome_unconditional_pixmap_file ("gnome-folder.png");
	}
	return pixmap_name;
}

static Menu *
create_panel_menu (char *menudir, int main_menu,
		   PanelOrientType orient, MainMenuType main_menu_type)
{
	GtkWidget *pixmap;
	Menu *menu;

	char *pixmap_name = NULL;

	menu = g_new(Menu,1);

	pixmap_name = get_pixmap(menudir,orient,main_menu);

	menu->orient = orient;
	
	menu->main_menu_type = main_menu_type;

	/* main button */
	menu->button = gtk_button_new ();
	gtk_signal_connect (GTK_OBJECT (menu->button), "event",
			    GTK_SIGNAL_FUNC (menu_button_press), menu);
	gtk_signal_connect (GTK_OBJECT (menu->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_menu), menu);


	/*make the pixmap*/
	pixmap = gnome_pixmap_new_from_file_at_size (pixmap_name,
						     BIG_ICON_SIZE,
						     BIG_ICON_SIZE);
	gtk_widget_show(pixmap);
	/*FIXME:this is not right, but it's how we can get the buttons to
	  be 48x48 (given the icons are 48x48)*/
	/*gtk_widget_set_usize (menu->button, pixmap->requisition.width,
			      pixmap->requisition.height);*/
	gtk_widget_set_usize (menu->button, 48,48);

	/* put pixmap in button */
	gtk_container_add (GTK_CONTAINER(menu->button), pixmap);
	gtk_widget_show (menu->button);

	{
		GList *list = g_list_append(NULL,menudir);
		add_menu_widget(menu,list,main_menu,FALSE);
		g_list_free(list);
	}

	g_free (pixmap_name);

	return menu;
}

Menu *
create_menu_applet(char *arguments, PanelOrientType orient, MainMenuType main_menu_type)
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

	menu = create_panel_menu (this_menu, main_menu, orient,main_menu_type);
	menu->path=g_strdup(arguments?arguments:".");

	gtk_object_set_user_data(GTK_OBJECT(menu->button),menu);

	g_free(menu_base);
	g_free (this_menu);
	return menu;
}

static void
set_show_small_icons_foreach(gpointer data, gpointer user_data)
{
	GtkWidget *w = data;
	if (!w) {
		g_warning("Internal error in set_show_small_icons_foreach (!w)");
		return;
	}
	if (global_config.show_small_icons)
		gtk_widget_show(w);
	else
		gtk_widget_hide(w);
}

void
set_show_small_icons(void)
{
	g_list_foreach(small_icons,set_show_small_icons_foreach,NULL);
}

void
set_menu_applet_orient(Menu *menu, PanelOrientType orient)
{
	GtkWidget *pixmap;
	char *pixmap_name = NULL;
	char *this_menu;
	char *menu_base;

	g_return_if_fail(menu!=NULL);
	g_return_if_fail(menu->path!=NULL);

	menu->orient = orient;

	menu_base = gnome_unconditional_datadir_file ("apps");
	this_menu = get_real_menu_path(menu->path,menu_base);
	pixmap_name = get_pixmap(this_menu,menu->orient,
				 strcmp (menu->path, ".") == 0);
	g_free(menu_base);
	g_free(this_menu);

	pixmap=GTK_BUTTON(menu->button)->child;
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
