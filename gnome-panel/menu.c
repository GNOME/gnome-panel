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

#include "panel-include.h"

#define SMALL_ICON_SIZE 20
#define BIG_ICON_SIZE   48


#define MENU_PATH "menu_path"

#define MENU_PROPERTIES "menu_properties"

static char *gnome_folder = NULL;

GList *small_icons = NULL;
int show_small_icons = TRUE;

extern GArray *applets;
extern int applet_count;

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
	int applets;
	char *dir_name;
	char *pixmap_name;	
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

	about = gnome_about_new ( _("The GNOME Panel"), VERSION,
			"(C) 1998 the Free Software Foundation",
			(const gchar **)authors,
			_("This program is responsible for launching "
			"other applications, embedding small applets "
			"within itself, world peace, and random X crashes."),
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

static void
add_app_to_panel (GtkWidget *widget, void *data)
{
	GnomeDesktopEntry *ii = data;
	
	load_launcher_applet(ii->location,current_panel,0);
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

static GList *
get_files_from_menudir(char *menudir)
{
	struct dirent *dent;
	DIR *dir;
	GList *out = NULL;
	
	dir = opendir (menudir);
	if (dir == NULL)
		return NULL;
	
	out = get_presorted_from(menudir);
	
	while((dent = readdir (dir)) != NULL) {
		/* Skip over dot files */
		if (dent->d_name [0] == '.')
			continue;
		if(!string_is_in_list(out,dent->d_name))
			out = g_list_append(out,g_strdup(dent->d_name));
	}
	return out;
}

static void
add_drawers_from_dir(char *dirname, char *name, int pos, PanelWidget *panel)
{
	AppletInfo *info;
	Drawer *drawer;
	PanelWidget *newpanel;
	GnomeDesktopEntry *item_info;
	char *dentry_name;
	char *subdir_name;
	char *pixmap_name;
	GList *list;

	if(!g_file_exists(dirname))
		return;

	dentry_name = g_concat_dir_and_file (dirname,
					     ".directory");
	item_info = gnome_desktop_entry_load (dentry_name);
	g_free (dentry_name);

	if(!name)
		subdir_name = item_info?item_info->name:NULL;
	else
		subdir_name = name;
	pixmap_name = item_info?item_info->icon:NULL;

	load_drawer_applet(NULL,pixmap_name,subdir_name,
			   panel,pos);
	
	info = get_applet_info(applet_count -1);
	g_return_if_fail(info);
	
	drawer = info->data;
	g_return_if_fail(drawer);
	newpanel = PANEL_WIDGET(DRAWER_WIDGET(drawer->drawer)->panel);
	
	list = get_files_from_menudir(dirname);
	while(list) {
		char *filename = g_concat_dir_and_file(dirname,
						       list->data);
		struct stat s;
		if (stat (filename, &s) == 0) {
			if (S_ISDIR (s.st_mode)) {
				add_drawers_from_dir(filename,NULL,INT_MAX/2,
						     newpanel);
			} else {
				char *p = strrchr(filename,'.');
				if(p && strcmp(p,".desktop")==0)
					/*we load the applet at the right
					  side, that is end of the drawer*/
					load_launcher_applet(filename,
							     newpanel,
							     INT_MAX/2);
			}
		}
		g_free(filename);
		g_free(list->data);
		list = my_g_list_pop_first(list);
	}
}

/*add a drawer with the contents of a menu to the panel*/
static void
add_menudrawer_to_panel(GtkWidget *w, gpointer data)
{
	MenuFinfo *mf = data;
	g_return_if_fail(mf);
	
	add_drawers_from_dir(mf->menudir,mf->dir_name,0,
			     current_panel);
}

static void
add_menu_to_panel (GtkWidget *widget, void *data)
{
	MenuFinfo *mf = data;
	if(mf)
		load_menu_applet(mf->menudir,0, current_panel, 0);
	else
		load_menu_applet(NULL,0, current_panel, 0);
}


/*most of this function stolen from the real gtk_menu_popup*/
static void
restore_grabs(GtkWidget *w, gpointer data)
{
	GtkWidget *menu_item = data;
	GtkMenu *menu = GTK_MENU(menu_item->parent); 
	GtkWidget *xgrab_shell;
	GtkWidget *parent;
	/* Find the last viewable ancestor, and make an X grab on it
	 */
	parent = GTK_WIDGET (menu);
	xgrab_shell = NULL;
	while (parent) {
		gboolean viewable = TRUE;
		GtkWidget *tmp = parent;

		while (tmp) {
			if (!GTK_WIDGET_MAPPED (tmp)) {
				viewable = FALSE;
				break;
			}
			tmp = tmp->parent;
		}

		if (viewable)
			xgrab_shell = parent;

		parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
	}

	/*only grab if this HAD a grab before*/
	if (xgrab_shell && (GTK_MENU_SHELL (xgrab_shell)->have_xgrab)) {
		GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);

		GTK_MENU_SHELL (xgrab_shell)->have_xgrab = 
			(gdk_pointer_grab (xgrab_shell->window, TRUE,
					   GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
					   GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK,
					   NULL, cursor, 0) == 0);
		gdk_cursor_destroy (cursor);
	}

	gtk_grab_add (GTK_WIDGET (menu));
}
	


static int
show_item_menu(GtkWidget *w, GdkEvent *event, gpointer data)
{
	GtkWidget *menu, *menuitem, *prop_item;
	GdkEventButton *bevent = (GdkEventButton *)event;
	GnomeDesktopEntry *dentry;
	MenuFinfo *mf;
	int type = GPOINTER_TO_INT(data);

	if(event->type!=GDK_BUTTON_PRESS)
		return FALSE;
	
	dentry = gtk_object_get_data(GTK_OBJECT(w),"dentry");
	mf = gtk_object_get_data(GTK_OBJECT(w),"mf");
	menu = gtk_object_get_data(GTK_OBJECT(w),"menu");
	prop_item = gtk_object_get_data(GTK_OBJECT(w),"prop_item");
	
	if(!menu) {
		menu = gtk_menu_new ();
		gtk_object_set_data(GTK_OBJECT(w),"menu",menu);
		gtk_signal_connect(GTK_OBJECT(menu),"deactivate",
				   GTK_SIGNAL_FUNC(restore_grabs),
				   w->parent->parent);


		if(type == 1) {
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this launcher to panel"));
			gtk_menu_append (GTK_MENU (menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_app_to_panel),
					   dentry);
		} else {
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this as drawer to panel"));
			gtk_menu_append (GTK_MENU (menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				   GTK_SIGNAL_FUNC(add_menudrawer_to_panel),
				   mf);

			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this as menu to panel"));
			gtk_menu_append (GTK_MENU (menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_menu_to_panel),
					   mf);
		}

		prop_item = gtk_menu_item_new ();
		gtk_object_set_data(GTK_OBJECT(w),"prop_item",prop_item);
		setup_menuitem (prop_item, 0, _("Properties ..."));
		gtk_menu_append (GTK_MENU (menu), prop_item);
	}
	
	/*FIXME: do properties*/
	gtk_widget_set_sensitive(prop_item,FALSE);

	gtk_menu_popup (GTK_MENU (menu),
			NULL,
			NULL,
			NULL,
			NULL,
			bevent->button,
			0);
	
	return TRUE;
}

static int
destroy_item_menu(GtkWidget *w, gpointer data)
{
	GtkWidget *menu;

	menu = gtk_object_get_data(GTK_OBJECT(w),"menu");
	
	if(menu)
		gtk_widget_destroy(menu);
	return FALSE;
}


static void
setup_title_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title,
		      MenuFinfo *mf)
{
	GtkWidget *label, *hbox, *align;

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);
	
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_widget_show (align);
	gtk_container_border_width (GTK_CONTAINER (align), 1);

	if (pixmap) {
		gtk_container_add (GTK_CONTAINER (align), pixmap);
		gtk_widget_set_usize (align, 22, 16);
		gtk_widget_show (pixmap);
	} else
		gtk_widget_set_usize (align, 22, 16);

	small_icons = g_list_prepend (small_icons, align);
	gtk_signal_connect(GTK_OBJECT(align),"destroy",
			   GTK_SIGNAL_FUNC(kill_small_icon),NULL);

	gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	if(mf) {
		GtkWidget *w = gtk_button_new_with_label(_("..."));
		gtk_object_set_data(GTK_OBJECT(w),"mf",mf);
		gtk_signal_connect(GTK_OBJECT(w),"event",
				   GTK_SIGNAL_FUNC(show_item_menu),
				   GINT_TO_POINTER(0));
		gtk_signal_connect(GTK_OBJECT(w),"destroy",
				   GTK_SIGNAL_FUNC(destroy_item_menu),
				   NULL);
		gtk_widget_show(w);
		gtk_box_pack_end (GTK_BOX (hbox), w, FALSE, FALSE, 0);
		/*this is not really a problem for large fonts but it
		  makes the button smaller*/
		gtk_widget_set_usize(w,0,16);

	}
	gtk_container_add (GTK_CONTAINER (menuitem), hbox);


	gtk_widget_show (menuitem);

	gtk_signal_connect_object(GTK_OBJECT(menuitem),"select",
				  GTK_SIGNAL_FUNC(gtk_item_deselect),
				  GTK_OBJECT(menuitem));
}

static void
setup_full_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title,
		     GnomeDesktopEntry *dentry)
{
	GtkWidget *label, *hbox, *align;

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);
	
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_widget_show (align);
	gtk_container_border_width (GTK_CONTAINER (align), 1);

	if (pixmap) {
		gtk_container_add (GTK_CONTAINER (align), pixmap);
		gtk_widget_set_usize (align, 22, 16);
		gtk_widget_show (pixmap);
	} else
		gtk_widget_set_usize (align, 22, 16);

	small_icons = g_list_prepend (small_icons, align);
	gtk_signal_connect(GTK_OBJECT(align),"destroy",
			   GTK_SIGNAL_FUNC(kill_small_icon),NULL);


	gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	if(dentry) {
		GtkWidget *w = gtk_button_new_with_label(_("..."));
		gtk_object_set_data(GTK_OBJECT(w),"dentry",dentry);
		gtk_signal_connect(GTK_OBJECT(w),"event",
				   GTK_SIGNAL_FUNC(show_item_menu),
				   GINT_TO_POINTER(1));
		gtk_signal_connect(GTK_OBJECT(w),"destroy",
				   GTK_SIGNAL_FUNC(destroy_item_menu),
				   NULL);
		gtk_widget_show(w);
		gtk_box_pack_end (GTK_BOX (hbox), w, FALSE, FALSE, 0);
		/*this is not really a problem for large fonts but it
		  makes the button smaller*/
		gtk_widget_set_usize(w,0,16);
	}
	gtk_container_add (GTK_CONTAINER (menuitem), hbox);

	gtk_widget_show (menuitem);
}

void
setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title)
{
	setup_full_menuitem(menuitem,pixmap,title,NULL);
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
add_drawer_to_panel (GtkWidget *widget, void *data)
{
	load_drawer_applet(NULL,NULL,NULL,
			   current_panel, 0);
	return TRUE;
}

static int
add_logout_to_panel (GtkWidget *widget, void *data)
{
	load_logout_applet(current_panel, 0);
	return TRUE;
}

static int
add_applet (GtkWidget *w, gpointer data)
{
	GnomeDesktopEntry *ii = data;
	char *path;
	char *param;

	path = ii->exec[0];

	g_return_val_if_fail(path!=NULL,FALSE);
	
	if((ii->exec[1])!=NULL)
		param = gnome_string_joinv (" ", ii->exec + 1);
	else
		param = NULL;

	load_extern_applet(path,param,NULL,current_panel,0);

	if(param) g_free(param);
	return TRUE;
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

static void
destroy_mf(MenuFinfo *mf)
{
	while(mf->finfo) {
		FileInfo *fi = mf->finfo->data;
		if(fi->name) g_free(fi->name);
		g_free(fi);
		mf->finfo = my_g_list_pop_first(mf->finfo);
	}
	if(mf->menudir) g_free(mf->menudir);
	if(mf->dir_name) g_free(mf->dir_name);
	if(mf->pixmap_name) g_free(mf->pixmap_name);
	g_free(mf);
}


static int
menu_destroy(GtkWidget *menu, gpointer data)
{
	GList *mfl = gtk_object_get_data(GTK_OBJECT(menu),"mf");
	while(mfl) {
		MenuFinfo *mf = mfl->data;
		destroy_mf(mf);
		mfl = my_g_list_pop_first(mfl);
	}
	gtk_object_set_data(GTK_OBJECT(menu),"mf",NULL);
	return FALSE;
}

static void add_menu_widget (Menu *menu, GList *menudirl,
			     int main_menu, int fake_subs);
static GtkWidget * create_menu_at (GtkWidget *menu,
				   char *menudir, 
				   int applets, char *dir_name,
				   char *pixmap_name, int fake_submenus);

/*if menu is NULL that means just reread the menu don't do anything with the
  applet*/
static GtkWidget *
check_and_reread(GtkWidget *menuw,Menu *menu,int main_menu)
{
	/*we are creating a whole new menuf or an applet if menu isn't NULL*/
	if(menu) {
		GList *mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");
		GList *list;
		int need_reread = FALSE;
		
		/*we shouldn't warn, this is more for debugging anyway,
		  and nowdays we do have menus that don't have one, this
		  however might be needed for further debugging*/
		/*if(!mfl)
			g_warning("Weird menu doesn't have mf entry");*/

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
		
		/*if(!mfl)
			g_warning("Weird menu doesn't have mf entry");*/

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
				puts("debug: rereading menu");
				menuw = create_menu_at(old_menu,
						       mf->menudir,
						       mf->applets,
						       mf->dir_name,
						       mf->pixmap_name,
						       TRUE);
				old_menu = menuw;
			}
			/*free up stuff that won't be freed somewhere else*/
			while(free_list) {
				MenuFinfo *mf = free_list->data;
				destroy_mf(mf);
				free_list = my_g_list_pop_first(free_list);
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
		     int applets,
		     char *dir_name,
		     char *pixmap_name)
{	
	MenuFinfo *mf;
	GtkWidget *menu;
	GList *list;
	
	menu = gtk_menu_new ();
	
	mf = g_new(MenuFinfo,1);
	mf->menudir = g_strdup(menudir);
	mf->applets = applets;
	mf->dir_name = dir_name?g_strdup(dir_name):NULL;
	mf->pixmap_name = pixmap_name?g_strdup(pixmap_name):NULL;
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
		int applets,
		char *dir_name,
		char *pixmap_name,
		int fake_submenus)
{	
	GnomeDesktopEntry *item_info=NULL;
	GnomeDesktopEntry *dir_info=NULL;
	struct stat s;
	char *filename;
	int items = 0;
	FileInfo *fi;
	GList *finfo = NULL;
	GList *flist = NULL;
	GList *mfl = NULL;
	char *thisfile;
	int add_separator = FALSE;
	int first_item = 0;
	GtkWidget *menuitem;
	
	MenuFinfo *mf = NULL;
	
	/*try getting the dir_name and pixmap if we haven't gotten them*/
	if(!dir_name || !pixmap_name) {
		char *dentry_name;

		dentry_name = g_concat_dir_and_file (menudir,
						     ".directory");
		dir_info = gnome_desktop_entry_load (dentry_name);

		/*add the .directory file to the checked files list,
		  but only if we can stat it (if we can't it probably
		  doesn't exist)*/
		fi = make_finfo(dentry_name);
		if(fi)
			finfo = g_list_prepend(finfo,fi);
		g_free (dentry_name);

		if(!dir_name)
			dir_name = dir_info?dir_info->name:"Menu";
		if(!pixmap_name)
			pixmap_name = dir_info?dir_info->icon:NULL;
	}

	
	/*add dir to the checked files list*/
	fi = make_finfo(menudir);
	if(!fi)
		g_warning("Something is wrong, directory %s can't be stated",
			  menudir);
	else
		finfo = g_list_prepend(finfo,fi);

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
		first_item = g_list_length(GTK_MENU_SHELL(menu)->children);
		mfl = gtk_object_get_data(GTK_OBJECT(menu), "mf");
		if(GTK_MENU_SHELL(menu)->children)
			add_separator = TRUE;
	}

	flist = get_files_from_menudir(menudir);
	
	while (flist) {
		GtkWidget     *sub, *pixmap;
		char          *pixmap_name;
		char          *menuitem_name;
		
		thisfile = flist->data;
		filename = g_concat_dir_and_file(menudir,thisfile);
		flist = my_g_list_pop_first(flist);
		
		if (stat (filename, &s) == -1) {
			g_free (filename);
			g_free(thisfile);
			continue;
		}

		sub = NULL;
		item_info = NULL;
		if (S_ISDIR (s.st_mode)) {
			char *dentry_name;
			
			dentry_name = g_concat_dir_and_file (filename,
							     ".directory");
			item_info = gnome_desktop_entry_load (dentry_name);

			/*add the .directory file to the checked files list,
			  but only if we can stat it (if we can't it probably
			  doesn't exist)*/
			fi = make_finfo(dentry_name);
			if(fi)
				finfo = g_list_prepend(finfo,fi);
			g_free (dentry_name);

			menuitem_name = item_info?item_info->name:thisfile;
			pixmap_name = item_info?item_info->icon:NULL;

			if(fake_submenus)
				sub = create_fake_menu_at (filename,
							   applets,
							   menuitem_name,
							   pixmap_name);
			else
				sub = create_menu_at (NULL, filename, 
						      applets,
						      menuitem_name,
						      pixmap_name,
						      fake_submenus);

			if (!sub) {
				g_free(filename);
				g_free(thisfile);
				if(item_info)
					gnome_desktop_entry_free(item_info);
				continue;
			}
		} else {
			char *p = strrchr(filename,'.');
			if (!p || strcmp(p, ".desktop") != 0) {
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
				g_warning("Something is wrong, "
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

		/*setup the menuitem, pass item_info if this is not
		  a submenu or an applet, so that the item can be added*/
 		setup_full_menuitem (menuitem, pixmap, menuitem_name,
				     (sub||applets)?NULL:item_info);
		if(add_separator) {
			add_menu_separator(menu);
			first_item++;
			add_separator = FALSE;
		}
		gtk_menu_append (GTK_MENU (menu), menuitem);

		gtk_signal_connect (GTK_OBJECT (menuitem), "destroy",
				    (GtkSignalFunc) free_app_def, item_info);

		if(item_info && item_info->exec) {
			gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
					    applets?
					     GTK_SIGNAL_FUNC(add_applet):
					     GTK_SIGNAL_FUNC(activate_app_def),
					    item_info);
		}

		g_free(filename);
		g_free(thisfile);
	}
	
	mf = g_new(MenuFinfo,1);
	mf->menudir = g_strdup(menudir);
	mf->applets = applets;
	mf->dir_name = dir_name?g_strdup(dir_name):NULL;
	mf->pixmap_name = pixmap_name?g_strdup(pixmap_name):NULL;
	mf->fake_menu = FALSE;
	mf->finfo = finfo;
	
	if(!applets && items>0) {
		GtkWidget *pixmap;
		menuitem = gtk_menu_item_new();
		gtk_widget_show(menuitem);
		gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);

		pixmap = NULL;
		if (pixmap_name && g_file_exists (pixmap_name)) {
			pixmap = gnome_pixmap_new_from_file_at_size (pixmap_name,
								     SMALL_ICON_SIZE,
								     SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		} else if (gnome_folder && g_file_exists (gnome_folder)) {
			pixmap = gnome_pixmap_new_from_file_at_size (gnome_folder,
								     SMALL_ICON_SIZE,
								     SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		}
		menuitem = gtk_menu_item_new();
		setup_title_menuitem(menuitem,pixmap,
				     dir_name?dir_name:"Menu",mf);
		gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);
	}

	if(dir_info)
		gnome_desktop_entry_free(dir_info);

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
	g_free(menu->path);
	g_free(menu);
}

static void
menu_deactivate(GtkWidget *w, gpointer data)
{
	Menu *menu = data;
	GtkWidget *panel = get_panel_parent(menu->button);
	/* allow the panel to hide again */
	if(IS_SNAPPED_WIDGET(panel))
		SNAPPED_WIDGET(panel)->autohide_inhibit = FALSE;
}

static char *
get_real_menu_path(char *arguments, char *menu_base)
{
	char *this_menu;

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

	applet_menu = create_menu_at(NULL,menudir,TRUE,_("Applets"),NULL,
				     fake_submenus);
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
create_system_menu(GtkWidget *menu, int fake_submenus)
{
	char *menu_base = gnome_unconditional_datadir_file ("apps");
	char *menudir;
	
	menudir = g_concat_dir_and_file (menu_base, ".");
	g_free (menu_base);
	if (g_file_exists (menudir)) {
		menu = create_menu_at (menu,menudir,FALSE,_("System Menus"),
				       gnome_folder, fake_submenus);
		g_return_val_if_fail(menu,NULL);
		g_free (menudir);
	} else
		g_warning("No system menus found!");
	
	return menu;
}

static GtkWidget *
create_user_menu(GtkWidget *menu, int fake_submenus)
{
	char *menu_base = gnome_util_home_file ("apps");
	char *menudir = g_concat_dir_and_file (menu_base, ".");
	
	g_free (menu_base);
	if (!g_file_exists (menudir))
		mkdir (menudir, 0755);
	if (g_file_exists (menudir)) {
		menu = create_menu_at (menu,menudir, FALSE,
				       _("User Menus"),
				       gnome_folder,fake_submenus);
	}
	g_free (menudir); 
	return menu;
}

GtkWidget *
create_panel_root_menu(GtkWidget *panel)
{
	GtkWidget *menuitem;
	GtkWidget *panel_menu;
	GtkWidget *menu;

	panel_menu = gtk_menu_new();

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("System Menus"));
	gtk_menu_append (GTK_MENU (panel_menu), menuitem);
	menu = gtk_menu_new();
	create_system_menu(menu,TRUE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),menu);
	gtk_signal_connect(GTK_OBJECT(menuitem),"select",
			   GTK_SIGNAL_FUNC(submenu_to_display),
			   NULL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("User Menus"));
	gtk_menu_append (GTK_MENU (panel_menu), menuitem);
	menu = gtk_menu_new();
	create_user_menu(menu,TRUE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),menu);
	gtk_signal_connect(GTK_OBJECT(menuitem),"select",
			   GTK_SIGNAL_FUNC(submenu_to_display),
			   NULL);

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	make_panel_submenu(panel_menu,TRUE);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, NULL, _("Remove this panel"));
	gtk_menu_append (GTK_MENU (panel_menu), menuitem);
	gtk_signal_connect_object (GTK_OBJECT (menuitem), "activate",
				   GTK_SIGNAL_FUNC(gtk_widget_destroy),
				   GTK_OBJECT(panel));
	gtk_object_set_data(GTK_OBJECT(panel),"remove_item",menuitem);

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(panel_menu,
						  GNOME_STOCK_PIXMAP_QUIT),
			_("Log out"));
	gtk_menu_append (GTK_MENU (panel_menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_quit),
			    NULL);

	return panel_menu;
}


static void
current_panel_config(GtkWidget *w, gpointer data)
{
	GtkWidget *parent = gtk_object_get_data(GTK_OBJECT(current_panel),
						PANEL_PARENT);
	panel_config(parent);
}

void
make_panel_submenu (GtkWidget *menu, int fake_submenus)
{
	GtkWidget *menuitem;

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add applet"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   create_applets_menu(fake_submenus));
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
			   GTK_SIGNAL_FUNC(add_menu_to_panel),
			   NULL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add log out button"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(add_logout_to_panel),
			   NULL);


	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add swallowed app"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(ask_about_swallowing),NULL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add new launcher"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(ask_about_launcher),NULL);

	add_menu_separator(menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_PIXMAP_PREFERENCES),
			_("This panel properties..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(current_panel_config), 
			    NULL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_PIXMAP_PREFERENCES),
			_("Global properties..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_configure), 
			    NULL);

}

static void
panel_lock (GtkWidget *widget, void *data)
{
	system ("(xlock&)");
}

static void
add_special_entries (GtkWidget *menu, int fake_submenus)
{
	GtkWidget *menuitem;
	GtkWidget *panel_menu;

	/* Panel entry */

	add_menu_separator (menu);

	panel_menu = gtk_menu_new();
	make_panel_submenu(panel_menu,fake_submenus);
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),panel_menu);

	add_menu_separator (menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Lock screen"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_lock), 0);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_PIXMAP_ABOUT),
			_("About..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(about_cb),
			    NULL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_PIXMAP_QUIT),
			_("Log out"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_quit), 0);

}

static GtkWidget *
create_root_menu(int fake_submenus, MainMenuType type)
{
	GtkWidget *root_menu;
	GtkWidget *uroot_menu;

	root_menu = create_system_menu(NULL,fake_submenus);

	if(type == MAIN_MENU_BOTH)
		root_menu = create_user_menu(root_menu, fake_submenus);
	else
		uroot_menu = create_user_menu(NULL, fake_submenus);
	
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

	add_special_entries (root_menu, fake_submenus);
	
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
						     FALSE, NULL, NULL,
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
				get_panel_parent(menu->button);
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
				gtk_object_get_data(GTK_OBJECT(menu->button),
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
	Menu *menu;

	char *pixmap_name = NULL;

	menu = g_new(Menu,1);

	pixmap_name = get_pixmap(menudir,orient,main_menu);

	menu->orient = orient;
	
	menu->main_menu_type = main_menu_type;



	/*make the pixmap*/
	menu->button = gnome_pixmap_new_from_file_at_size (pixmap_name,
							   BIG_ICON_SIZE,
							   BIG_ICON_SIZE);
	gtk_widget_set_events(menu->button,
			      gtk_widget_get_events(menu->button) |
			      GDK_LEAVE_NOTIFY_MASK |
			      GDK_ENTER_NOTIFY_MASK |
			      GDK_BUTTON_PRESS_MASK |
			      GDK_BUTTON_RELEASE_MASK);
	gtk_signal_connect (GTK_OBJECT (menu->button), "event",
			    GTK_SIGNAL_FUNC (menu_button_press), menu);
	gtk_signal_connect (GTK_OBJECT (menu->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_menu), menu);
	gtk_widget_show(menu->button);

	{
		GList *list = g_list_append(NULL,menudir);
		add_menu_widget(menu,list,main_menu,FALSE);
		g_list_free(list);
	}

	g_free (pixmap_name);

	return menu;
}

static Menu *
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

	main_menu = (!arguments || !*arguments || (strcmp (arguments, ".") == 0));

	menu = create_panel_menu (this_menu, main_menu, orient,main_menu_type);
	menu->path=g_strdup((arguments && *arguments)?arguments:".");

	gtk_object_set_user_data(GTK_OBJECT(menu->button),menu);

	g_free(menu_base);
	g_free (this_menu);
	return menu;
}

static void
set_show_small_icons_foreach(gpointer data, gpointer user_data)
{
	GtkWidget *w = data;
	g_return_if_fail(w);
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

	/*make the pixmap*/
	gnome_pixmap_load_file_at_size (GNOME_PIXMAP(menu->button),
					pixmap_name,
					BIG_ICON_SIZE,
					BIG_ICON_SIZE);

	g_free(pixmap_name);
}

static void
properties_apply_callback(GtkWidget *widget, int page, gpointer data)
{
	Menu *menu = data;
	GtkWidget *main_menu = gtk_object_get_data(GTK_OBJECT(widget), "main_menu");
	GtkWidget *nomral_menu = gtk_object_get_data(GTK_OBJECT(widget), "nomral_menu");
	GtkWidget *menu_both = gtk_object_get_data(GTK_OBJECT(widget), "menu_both");
	GtkWidget *menu_system = gtk_object_get_data(GTK_OBJECT(widget), "menu_system");
	GtkWidget *menu_user = gtk_object_get_data(GTK_OBJECT(widget), "menu_user");
	GtkWidget *pathentry = gtk_object_get_data(GTK_OBJECT(widget), "path");
	char *s;

	if (page != -1)
		return;
	
	if(GTK_TOGGLE_BUTTON(main_menu)->active) {
		g_free(menu->path);
		menu->path = g_strdup(".");
	} else {
		g_free(menu->path);
		s = gtk_entry_get_text(GTK_ENTRY(pathentry));
		if(!s || !*s)
			menu->path = g_strdup(".");
		else
			menu->path = g_strdup(s);
	}
	if(GTK_TOGGLE_BUTTON(menu_both)->active)
		menu->main_menu_type = MAIN_MENU_BOTH;
	else if(GTK_TOGGLE_BUTTON(menu_system)->active)
		menu->main_menu_type = MAIN_MENU_SYSTEM;
	else if(GTK_TOGGLE_BUTTON(menu_user)->active)
		menu->main_menu_type = MAIN_MENU_USER;
	
	gtk_widget_unref(menu->menu);	
	menu->menu = NULL;

	{
		char *menu_base = gnome_unconditional_datadir_file ("apps");
		char *this_menu = get_real_menu_path(menu->path,menu_base);
		GList *list = g_list_append(NULL,this_menu);
		char *pixmap_name = get_pixmap(this_menu,menu->orient,
					       strcmp(menu->path,".")==0);
		/*make the pixmap*/
		gnome_pixmap_load_file_at_size (GNOME_PIXMAP(menu->button),
						pixmap_name,
						BIG_ICON_SIZE,
						BIG_ICON_SIZE);
		g_free(pixmap_name);
		
		add_menu_widget(menu,list, strcmp(menu->path,".")==0, TRUE);
		
		g_free(menu_base);
		g_free(this_menu);

		g_list_free(list);
	}
}

static int
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GtkWidget *pathentry = gtk_object_get_data(GTK_OBJECT(widget),"path");
	gtk_object_set_data(GTK_OBJECT(menu->button),
			    MENU_PROPERTIES,NULL);
	gtk_signal_disconnect_by_data(GTK_OBJECT(pathentry),widget);
	return FALSE;
}

static void
notify_entry_change (GtkWidget *widget, void *data)
{
	GnomePropertyBox *box = GNOME_PROPERTY_BOX (data);

	gnome_property_box_changed (box);
}
static void
toggle_prop(GtkWidget *widget, void *data)
{
	GnomePropertyBox *box = GNOME_PROPERTY_BOX (data);

	if(GTK_TOGGLE_BUTTON(widget)->active)
		gnome_property_box_changed (box);
}

static void
toggle_main_menu(GtkWidget *widget, void *data)
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
toggle_normal_menu(GtkWidget *widget, void *data)
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
static GtkWidget *
create_properties_dialog(Menu *menu)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *box;
	GtkWidget *w,*w2;
	GtkWidget *f;
	GtkWidget *t;

	dialog = gnome_property_box_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Menu properties"));
	/*gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	vbox = gtk_vbox_new(FALSE,5);
	gtk_container_border_width(GTK_CONTAINER(vbox),5);

	f = gtk_frame_new(_("Menu Type"));
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	box = gtk_hbox_new(FALSE,5);
	gtk_container_border_width(GTK_CONTAINER(box),5);
	gtk_container_add(GTK_CONTAINER(f),box);
	
	w = gtk_radio_button_new_with_label (NULL, _("Main Menu"));
	gtk_object_set_data(GTK_OBJECT(dialog),"main_menu",w);
	if(!menu->path || strcmp(menu->path,".")==0)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_main_menu), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);

	w2 = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (w)),
			  _("Normal Menu"));
	gtk_object_set_data(GTK_OBJECT(dialog),"normal_menu",w2);
	if(menu->path && strcmp(menu->path,".")!=0)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w2), TRUE);
	gtk_signal_connect (GTK_OBJECT (w2), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_normal_menu), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w2,TRUE,TRUE,0);


	f = gtk_frame_new(_("Main Menu"));
	if(menu->path && strcmp(menu->path,".")!=0)
		gtk_widget_set_sensitive(f,FALSE);
	gtk_object_set_data(GTK_OBJECT(dialog),"main_frame",f);
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	box = gtk_vbox_new(FALSE,5);
	gtk_container_border_width(GTK_CONTAINER(box),5);
	gtk_container_add(GTK_CONTAINER(f),box);

	w = gtk_radio_button_new_with_label (NULL, _("Both System and User menus on the "
						     "same menu"));
	gtk_object_set_data(GTK_OBJECT(dialog),"menu_both",w);
	if(menu->main_menu_type == MAIN_MENU_BOTH)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);

	w2 = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (w)),
			  _("System on the main menu, User menu as a submenu"));
	gtk_object_set_data(GTK_OBJECT(dialog),"menu_system",w2);
	if(menu->main_menu_type == MAIN_MENU_SYSTEM)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w2), TRUE);
	gtk_signal_connect (GTK_OBJECT (w2), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w2,TRUE,TRUE,0);
	
	w2 = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (w)),
			  _("User on the main menu, System menu as a submenu"));
	gtk_object_set_data(GTK_OBJECT(dialog),"menu_user",w2);
	if(menu->main_menu_type == MAIN_MENU_USER)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w2), TRUE);
	gtk_signal_connect (GTK_OBJECT (w2), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w2,TRUE,TRUE,0);
	
	f = gtk_frame_new(_("Normal Menu"));
	if(!menu->path || strcmp(menu->path,".")==0)
		gtk_widget_set_sensitive(f,FALSE);
	gtk_object_set_data(GTK_OBJECT(dialog),"normal_frame",f);
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	box = gtk_hbox_new(FALSE,5);
	gtk_container_border_width(GTK_CONTAINER(box),5);
	gtk_container_add(GTK_CONTAINER(f),box);
	
	w = gtk_label_new(_("Menu path"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);

	w = gnome_file_entry_new("menu_path",_("Browse"));
	t = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
	gtk_object_set_data(GTK_OBJECT(dialog),"path",t);
	if (menu->path)
		gtk_entry_set_text(GTK_ENTRY(t), menu->path);
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);
	gtk_signal_connect (GTK_OBJECT (t), "changed",
			    GTK_SIGNAL_FUNC(notify_entry_change),
			    dialog);
	
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
		return;
	}

	dialog = create_properties_dialog(menu);
	gtk_object_set_data(GTK_OBJECT(menu->button),
			    MENU_PROPERTIES,dialog);
	gtk_widget_show_all (dialog);
}

void
load_menu_applet(char *params, int main_menu_type,
		 PanelWidget *panel, int pos)
{
	Menu *menu;

	menu = create_menu_applet(params, ORIENT_UP,main_menu_type);

	if(menu) {
		register_toy(menu->button,menu,
			     panel,pos,APPLET_MENU);

		applet_add_callback(applet_count-1,"properties",
				    GNOME_STOCK_MENU_PROP,
				    _("Properties..."));
	}
}
