/*
 * GNOME panel menu module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <gnome.h>

#include "panel-include.h"

#define SMALL_ICON_SIZE 20
#define BIG_ICON_SIZE   48


#define MENU_PATH "menu_path"

#define MENU_PROPERTIES "menu_properties"

static char *gnome_folder = NULL;

/*for Red Hat menus to trigger rereading on directory writes*/
GSList *redhat_check = NULL;

extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

/*list of all panel widgets created*/
extern GSList *panel_list;

extern GlobalConfig global_config;

extern int config_sync_timeout;
extern int panels_to_sync;
extern GSList *applets_to_sync;
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
	GSList *finfo;
};

/*???? this might be ugly, but I guess we can safely assume that we can only
  have one menu open and that nothing weird will happen to the panel that
  opened that menu whilethe user is looking over the choices*/
PanelWidget *current_panel = NULL;

/*the most important dialog in the whole application*/
static void
about_cb (GtkWidget *widget, gpointer data)
{
	GtkWidget *about;
	char *authors[] = {
	  "George Lebl (jirka@5z.com)",
	  "Miguel de Icaza (miguel@kernel.org)",
	  "Federico Mena (quartic@gimp.org)",
	  "Tom Tromey (tromey@cygnus.com)",
	  "Ian Main (slow@intergate.bc.ca)",
	  "Elliot Lee (sopwith@redhat.com)",
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

static void
about_gnome_cb(GtkObject *object, char *program_path)
{
	gnome_execute_async(NULL, 1, &program_path);
}

static void
activate_app_def (GtkWidget *widget, char *item_loc)
{
	GnomeDesktopEntry *item = gnome_desktop_entry_load(item_loc);
	gnome_desktop_entry_launch (item);
	gnome_desktop_entry_free(item);
}

static void
add_app_to_panel (GtkWidget *widget, char *item_loc)
{
	load_launcher_applet(item_loc, current_panel,0);
}

/*reads in the order file and makes a list*/
static GSList *
get_presorted_from(char *dir)
{
	char buf[PATH_MAX+1];
	GSList *list = NULL;
	char *fname = g_concat_dir_and_file(dir,".order");
	FILE *fp = fopen(fname,"r");
	
	if(!fp) {
		g_free(fname);
		return NULL;
	}
	while(fgets(buf,PATH_MAX+1,fp)!=NULL) {
		char *p = strchr(buf,'\n');
		if(p) *p = '\0';
		list = g_slist_prepend(list,g_strdup(buf));
	}
	fclose(fp);
	g_free(fname);
	return g_slist_reverse(list);
}

static GSList *
get_files_from_menudir(char *menudir)
{
	struct dirent *dent;
	DIR *dir;
	GSList *out = NULL;
	GSList *pres = NULL;
	
	dir = opendir (menudir);
	if (dir == NULL)
		return NULL;
	
	pres = get_presorted_from(menudir);
	
	while((dent = readdir (dir)) != NULL) {
		/* Skip over dot files */
		if (dent->d_name [0] == '.')
			continue;
		if(!string_is_in_list(pres,dent->d_name))
			out = g_slist_prepend(out,g_strdup(dent->d_name));
	}

	closedir(dir);
	return g_slist_concat(pres,g_slist_reverse(out));
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
	GSList *list, *li;

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

	load_drawer_applet(-1,pixmap_name,subdir_name,
			   panel,pos);
	
	g_return_if_fail(applets_last!=NULL);
	info = applets_last->data;
	g_return_if_fail(info!=NULL);
	
	drawer = info->data;
	g_return_if_fail(drawer);
	newpanel = PANEL_WIDGET(DRAWER_WIDGET(drawer->drawer)->panel);
	
	list = get_files_from_menudir(dirname);
	for(li = list; li!= NULL; li = g_slist_next(li)) {
		char *filename = g_concat_dir_and_file(dirname, li->data);
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
		g_free(li->data);
	}
	g_slist_free(list);
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
	int flags = MAIN_MENU_SYSTEM|MAIN_MENU_USER;

	/*guess redhat menus*/
	if(g_file_exists("/etc/X11/wmconfig"))
		flags |= MAIN_MENU_REDHAT|MAIN_MENU_REDHAT_SUB;

	if(mf)
		load_menu_applet(mf->menudir,flags, current_panel, 0);
	else
		load_menu_applet(NULL,flags, current_panel, 0);
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

static void
dentry_apply_callback(GtkWidget *widget, int page, gpointer data)
{
	GnomeDesktopEntry *dentry;

	if (page != -1)
		return;
	
	g_return_if_fail(data!=NULL);
	g_return_if_fail(GNOME_IS_DENTRY_EDIT(data));

	dentry = gnome_dentry_get_dentry(GNOME_DENTRY_EDIT(data));
	dentry->location = g_strdup(gtk_object_get_data(data,"location"));
	gnome_desktop_entry_save(dentry);
	gnome_desktop_entry_free(dentry);
}

static void
edit_dentry(GtkWidget *widget, char *item_loc)
{
	GtkWidget *dialog;
	GtkObject *o;
	GnomeDesktopEntry *dentry = gnome_desktop_entry_load(item_loc);

	dialog = gnome_property_box_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Desktop entry properties"));
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	o = gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(GNOME_PROPERTY_BOX(dialog)->notebook));
	/*item loc will be alive all this time*/
	gtk_object_set_data(o,"location",item_loc);

	gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(o),dentry);

	gtk_signal_connect_object(GTK_OBJECT(o), "changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(dialog));

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(dentry_apply_callback),
			   o);
	gtk_widget_show(dialog);
	gnome_desktop_entry_free(dentry);
}

static void
destroy_dedit(GtkObject *dedit,gpointer data)
{
	char *s = gtk_object_get_data(dedit,"location");
	g_free(s);
}

static void
edit_direntry(GtkWidget *widget, MenuFinfo *mf)
{
	GtkWidget *dialog;
	GtkObject *o;
	char *dirfile = g_concat_dir_and_file(mf->menudir, ".directory");
	GnomeDesktopEntry *dentry;

	dialog = gnome_property_box_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Desktop entry properties"));
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	o = gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(GNOME_PROPERTY_BOX(dialog)->notebook));

	dentry = gnome_desktop_entry_load_unconditional(dirfile);
	if (dentry) {
		gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(o), dentry);
		gtk_object_set_data(o,"location",g_strdup(dentry->location));
		gnome_desktop_entry_destroy(dentry);
		g_free(dirfile);
	} else {
		dentry = g_new0(GnomeDesktopEntry, 1);
		dentry->name =
			mf->dir_name?g_strdup(mf->dir_name):g_strdup("Menu");
		dentry->type = g_strdup("Directory");
		/*we don't have to free dirfile here it will be freed as if
		  we had strduped it here*/
		gtk_object_set_data(o,"location",dirfile);
		gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(o), dentry);
		gnome_desktop_entry_destroy(dentry);
	}

	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->exec_entry, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->tryexec_entry, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->doc_entry, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->type_combo, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->terminal_button, FALSE);

	gtk_signal_connect_object(o, "changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(dialog));
	gtk_signal_connect(o, "destroy", GTK_SIGNAL_FUNC(destroy_dedit),NULL);

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(dentry_apply_callback),
			   o);
	gtk_widget_show(dialog);
}

typedef struct _ShowItemMenu ShowItemMenu;
struct _ShowItemMenu {
	int type;
	char *item_loc;
	MenuFinfo *mf;
	GtkWidget *menu;
	GtkWidget *prop_item;
};

static void
show_item_menu(GtkWidget *item, GdkEventButton *bevent, ShowItemMenu *sim)
{
	GtkWidget *menuitem;
	
	if(!sim->menu) {
		sim->menu = gtk_menu_new ();
		
		gtk_signal_connect(GTK_OBJECT(sim->menu),"deactivate",
				   GTK_SIGNAL_FUNC(restore_grabs),
				   item);

		if(sim->type == 1) {
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this launcher to panel"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_app_to_panel),
					   sim->item_loc);
		} else {
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this as drawer to panel"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				   GTK_SIGNAL_FUNC(add_menudrawer_to_panel),
				   sim->mf);

			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this as menu to panel"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_menu_to_panel),
					   sim->mf);
		}

		sim->prop_item = gtk_menu_item_new ();
		/*when activated we must pop down the first menu*/
		gtk_signal_connect_object(GTK_OBJECT(sim->prop_item),
					  "activate",
					  GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
					  GTK_OBJECT(item->parent));
		if(sim->type == 1)
			gtk_signal_connect(GTK_OBJECT(sim->prop_item),
					   "activate",
					   GTK_SIGNAL_FUNC(edit_dentry),
					   sim->item_loc);
		else
			gtk_signal_connect(GTK_OBJECT(sim->prop_item),
					   "activate",
					   GTK_SIGNAL_FUNC(edit_direntry),
					   sim->mf);
		gtk_object_set_data(GTK_OBJECT(item),"prop_item",
				    sim->prop_item);
		setup_menuitem (sim->prop_item, 0, _("Properties ..."));
		gtk_menu_append (GTK_MENU (sim->menu), sim->prop_item);
	}
	
	gtk_widget_set_sensitive(sim->prop_item,FALSE);
	if(sim->item_loc &&
	   /*A HACK: but it works, don't have it edittable if it's redhat
	     menus as they are auto generated!*/
	   !strstr(sim->item_loc,".gnome/apps-redhat/") &&
	   access(sim->item_loc,W_OK)==0) {
		puts(sim->item_loc);
		/*file exists and is writable, we're in bussines*/
		gtk_widget_set_sensitive(sim->prop_item,TRUE);
	} else if(!sim->item_loc || errno==ENOENT) {
		/*the dentry isn't there, check if we can write the
		  directory*/
		if(access(sim->mf->menudir,W_OK)==0 &&
		   !strstr(sim->mf->menudir,".gnome/apps-redhat/"))
			gtk_widget_set_sensitive(sim->prop_item,TRUE);
	}
	
	gtk_menu_popup (GTK_MENU (sim->menu),
			NULL,
			NULL,
			NULL,
			NULL,
			bevent->button,
			bevent->time);
}

static int
show_item_menu_b_cb(GtkWidget *w, GdkEvent *event, ShowItemMenu *sim)
{
	GdkEventButton *bevent = (GdkEventButton *)event;
	GtkWidget *item;
	
	if(event->type!=GDK_BUTTON_PRESS)
		return FALSE;
	
	item = w->parent->parent;
	show_item_menu(item,bevent,sim);
	
	return TRUE;
}

static int
show_item_menu_mi_cb(GtkWidget *w, GdkEvent *event, ShowItemMenu *sim)
{
	GdkEventButton *bevent = (GdkEventButton *)event;
	
	if(event->type==GDK_BUTTON_PRESS && bevent->button==3)
		show_item_menu(w,bevent,sim);
	
	return FALSE;
}

static void
destroy_item_menu(GtkWidget *w, ShowItemMenu *sim)
{
	/*NOTE: don't free item_loc or mf.. it's not ours and will be free'd
	  elsewhere*/
	if(sim->menu)
		gtk_widget_destroy(sim->menu);
	g_free(sim);
}

/* This is a _horrible_ hack to have this here. This needs to be added to the
 * GTK+ menuing code in some manner.
 */
static void  
drag_end_menu_cb (GtkWidget *widget, GdkDragContext     *context)
{
  GtkWidget *xgrab_shell;
  GtkWidget *parent;
  GdkEvent *current_event;
  GtkMenuShell *menu_shell;

  /* Find the last viewable ancestor, and make an X grab on it
   */
  parent = widget->parent;
  xgrab_shell = NULL;
  while (parent)
    {
      gboolean viewable = TRUE;
      GtkWidget *tmp = parent;
      
      while (tmp)
	{
	  if (!GTK_WIDGET_MAPPED (tmp))
	    {
	      viewable = FALSE;
	      break;
	    }
	  tmp = tmp->parent;
	}
      
      if (viewable)
	xgrab_shell = parent;
      
      parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
    }
  
  if (xgrab_shell)
    {
      GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);

      if ((gdk_pointer_grab (xgrab_shell->window, TRUE,
			     GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			     GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
			     GDK_POINTER_MOTION_MASK,
			     NULL, cursor, GDK_CURRENT_TIME) == 0))
	{
	  if (gdk_keyboard_grab (xgrab_shell->window, TRUE,
				 GDK_CURRENT_TIME) == 0)
	    GTK_MENU_SHELL (xgrab_shell)->have_xgrab = TRUE;
	  else
	    {
	      gdk_pointer_ungrab (GDK_CURRENT_TIME);
	    }
	}

      gdk_cursor_destroy (cursor);
    }
}

static void  
drag_data_get_menu_cb (GtkWidget *widget, GdkDragContext     *context,
		       GtkSelectionData   *selection_data, guint info,
		       guint time, char *item_loc)
{
	gchar *uri_list = g_strconcat ("file:", item_loc, NULL);
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, uri_list,
				strlen(uri_list));
	g_free(uri_list);
}

static void  
drag_data_get_dir_cb (GtkWidget *widget, GdkDragContext     *context,
		      GtkSelectionData   *selection_data, guint info,
		      guint time, char *directory)
{
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, directory,
				strlen(directory));
}

static void
setup_title_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title,
		      MenuFinfo *mf)
{
	GtkWidget *label, *hbox=NULL, *align;

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);

	if(global_config.show_small_icons ||
	   global_config.show_dot_buttons) {
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (hbox);
		gtk_container_add (GTK_CONTAINER (menuitem), hbox);
	} else
		gtk_container_add (GTK_CONTAINER (menuitem), label);
	
	if(global_config.show_small_icons) {
		align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		gtk_widget_show (align);
		gtk_container_set_border_width (GTK_CONTAINER (align), 1);

		if (pixmap) {
			gtk_container_add (GTK_CONTAINER (align), pixmap);
			gtk_widget_set_usize (align, 22, 16);
			gtk_widget_show (pixmap);
		} else
			gtk_widget_set_usize (align, 22, 16);

		gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
	}

	if(global_config.show_small_icons ||
	   global_config.show_dot_buttons)
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	if(mf) {
		ShowItemMenu *sim = g_new0(ShowItemMenu,1);
		sim->mf = mf;/*make sure you don't free this,
			       it's not ours!*/
		sim->type = 0;
		gtk_signal_connect(GTK_OBJECT(menuitem),"event",
				   GTK_SIGNAL_FUNC(show_item_menu_mi_cb),
				   sim);
		gtk_signal_connect(GTK_OBJECT(menuitem),"destroy",
				   GTK_SIGNAL_FUNC(destroy_item_menu),
				   sim);
		if(global_config.show_dot_buttons) {
			GtkWidget *w = gtk_button_new_with_label(_("..."));
			gtk_signal_connect(GTK_OBJECT(w),"event",
					   GTK_SIGNAL_FUNC(show_item_menu_b_cb),
					   sim);
			gtk_widget_show(w);
			gtk_box_pack_end (GTK_BOX (hbox), w, FALSE, FALSE, 0);

			/*this is not really a problem for large fonts but it
			  makes the button smaller*/
			gtk_widget_set_usize(w,0,16);
		}
	}

	gtk_widget_show (menuitem);

	gtk_signal_connect_object(GTK_OBJECT(menuitem),"select",
				  GTK_SIGNAL_FUNC(gtk_item_deselect),
				  GTK_OBJECT(menuitem));
}

static void
setup_full_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title,
		     char *item_loc)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "text/uri-list", 0, 0 }
	};

	GtkWidget *label, *hbox=NULL, *align;

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);
	
	if(global_config.show_small_icons ||
	   global_config.show_dot_buttons) {
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (hbox);
		gtk_container_add (GTK_CONTAINER (menuitem), hbox);
	} else
		gtk_container_add (GTK_CONTAINER (menuitem), label);
	
	if(global_config.show_small_icons) {
		align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		gtk_widget_show (align);
		gtk_container_set_border_width (GTK_CONTAINER (align), 1);

		if (pixmap) {
			gtk_container_add (GTK_CONTAINER (align), pixmap);
			gtk_widget_set_usize (align, 22, 16);
			gtk_widget_show (pixmap);
		} else
			gtk_widget_set_usize (align, 22, 16);

		gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
	}

	if(global_config.show_small_icons ||
	   global_config.show_dot_buttons)
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	if(item_loc) {
		ShowItemMenu *sim = g_new0(ShowItemMenu,1);
		sim->item_loc = item_loc; /*make sure you don't free this,
					    it's not ours!*/
		sim->type = 1;
		gtk_signal_connect(GTK_OBJECT(menuitem),"event",
				   GTK_SIGNAL_FUNC(show_item_menu_mi_cb),
				   sim);
		gtk_signal_connect(GTK_OBJECT(menuitem),"destroy",
				   GTK_SIGNAL_FUNC(destroy_item_menu),
				   sim);
		if(global_config.show_dot_buttons) {
			GtkWidget *w = gtk_button_new_with_label(_("..."));
			gtk_signal_connect(GTK_OBJECT(w),"event",
					   GTK_SIGNAL_FUNC(show_item_menu_b_cb),
					   sim);
			gtk_widget_show(w);
			gtk_box_pack_end (GTK_BOX (hbox), w, FALSE, FALSE, 0);
			/*this is not really a problem for large fonts but it
			  makes the button smaller*/
			gtk_widget_set_usize(w,0,16);
		}

		gtk_drag_source_set(menuitem,
				    GDK_BUTTON1_MASK,
				    menu_item_targets, 1,
				    GDK_ACTION_COPY);

		gtk_signal_connect(GTK_OBJECT(menuitem), "drag_data_get",
				   drag_data_get_menu_cb, item_loc);
		gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
				   drag_end_menu_cb, NULL);
	}

	gtk_widget_show (menuitem);
}

void
setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title)
{
	setup_full_menuitem(menuitem,pixmap,title,NULL);
}

static void
setup_directory_drag (GtkWidget *menuitem, char *directory)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-directory", 0, 0 }
	};

	gtk_drag_source_set(menuitem,
			    GDK_BUTTON1_MASK,
			    menu_item_targets, 1,
			    GDK_ACTION_COPY);
	
	gtk_signal_connect_full(GTK_OBJECT(menuitem), "drag_data_get",
			   GTK_SIGNAL_FUNC (drag_data_get_dir_cb), NULL,
			   g_strdup (directory), (GtkDestroyNotify)g_free,
			   FALSE, FALSE);
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
			   GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);
}

static char *
get_applet_goad_id_from_dentry(GnomeDesktopEntry *ii)
{
	int i;
	int constantlen = strlen("--activate-goad-server");
	char *goad_id=NULL;
	/*FIXME:
	  this is a horrible horrible hack and should be taken out
	  and shot, once we add proper way to do this*/
	for(i=1;ii->exec[i];i++) {
		if(strncmp("--activate-goad-server",
			   ii->exec[i],constantlen)==0) {
			if(strlen(ii->exec[i])>constantlen)
				goad_id = g_strdup(&ii->exec[i][constantlen+1]);
			else
				goad_id = g_strdup(ii->exec[i+1]);
		}
	}
	return goad_id;
}


static void
setup_applet_drag (GtkWidget *menuitem, GnomeDesktopEntry *ii)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet", 0, 0 }
	};
	char *goad_id = get_applet_goad_id_from_dentry(ii);
	
	if(!goad_id)
		return;
	
	gtk_drag_source_set(menuitem,
			    GDK_BUTTON1_MASK,
			    menu_item_targets, 1,
			    GDK_ACTION_COPY);
	
	gtk_signal_connect_full(GTK_OBJECT(menuitem), "drag_data_get",
			   GTK_SIGNAL_FUNC (drag_data_get_dir_cb), NULL,
			   goad_id, (GtkDestroyNotify)g_free, FALSE, FALSE);
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
			   GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);

}

static void
add_menu_separator (GtkWidget *menu)
{
	GtkWidget *menuitem;
	
	menuitem = gtk_menu_item_new ();
	gtk_widget_show (menuitem);
	gtk_menu_append (GTK_MENU (menu), menuitem);
}

static int
add_drawer_to_panel (GtkWidget *widget, void *data)
{
	load_drawer_applet(-1,NULL,NULL, current_panel, 0);
	return TRUE;
}

static int
add_logout_to_panel (GtkWidget *widget, void *data)
{
	load_logout_applet(current_panel, 0);
	return TRUE;
}

static int
add_applet (GtkWidget *w, char *item_loc)
{
	GnomeDesktopEntry *ii = gnome_desktop_entry_load(item_loc);
	char *goad_id = get_applet_goad_id_from_dentry(ii);
	gnome_desktop_entry_free(ii);
	
	if(!goad_id) {
		g_warning(_("Can't get goad_id from desktop entry!"));
		return TRUE;
	}
	load_extern_applet(goad_id,NULL,current_panel,0);

	g_free(goad_id);

	return TRUE;
}

static int
check_finfo_list(GSList *finfo)
{
	struct stat s;
	FileInfo *fi;

	for(;finfo!=NULL;finfo=g_slist_next(finfo)) {
		fi = finfo->data;
		if (stat (fi->name, &s) == -1) {
			if(fi->mtime != 0)
				return FALSE;
		} else if(fi->mtime != s.st_mtime)
			return FALSE;
	}
	return TRUE;
}

static FileInfo *
make_finfo(char *name, int force)
{
	struct stat s;
	FileInfo *fi;

	if (stat (name, &s) == -1) {
		if(force) {
			fi = g_new(FileInfo,1);
			fi->name = g_strdup(name);
			fi->mtime = 0;
			return fi;
		}
		return NULL;
	}

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
	GSList *li;
	for(li=mf->finfo;li!=NULL;li=g_slist_next(li)) {
		FileInfo *fi = li->data;
		g_free(fi->name);
		g_free(fi);
	}
	g_slist_free(mf->finfo);
	mf->finfo = NULL;
	if(mf->menudir) g_free(mf->menudir);
	if(mf->dir_name) g_free(mf->dir_name);
	if(mf->pixmap_name) g_free(mf->pixmap_name);
	g_free(mf);
}


static void
menu_destroy(GtkWidget *menu, gpointer data)
{
	GSList *mfl = gtk_object_get_data(GTK_OBJECT(menu),"mf");
	GSList *li;
	for(li=mfl;li!=NULL;li=g_slist_next(li)) {
		MenuFinfo *mf = li->data;
		destroy_mf(mf);
	}
	g_slist_free(mfl);
	gtk_object_set_data(GTK_OBJECT(menu),"mf",NULL);
}

static void add_menu_widget (Menu *menu, GSList *menudirl,
			     int main_menu, int fake_subs);
static GtkWidget * create_menu_at (GtkWidget *menu,
				   char *menudir, 
				   int applets, char *dir_name,
				   char *pixmap_name, int fake_submenus,
				   int force);
static void create_rh_menu(void);

/*if menu is NULL that means just reread the menu don't do anything with the
  applet*/
static GtkWidget *
check_and_reread(GtkWidget *menuw,Menu *menu,int main_menu)
{
	/*we are creating a whole new menuf or an applet if menu isn't NULL*/
	if(menu) {
		GSList *mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");
		GSList *list;
		int need_reread = FALSE;
		
		/*we shouldn't warn, this is more for debugging anyway,
		  and nowdays we do have menus that don't have one, this
		  however might be needed for further debugging*/
		/*if(!mfl)
			g_warning("Weird menu doesn't have mf entry");*/

		/*check if we need to reread this*/
		for(list = mfl; list != NULL; list = g_slist_next(list)) {
			MenuFinfo *mf = list->data;
			if(mf->fake_menu ||
			   !check_finfo_list(mf->finfo)) {
				need_reread = TRUE;
				break;
			}
		}

		if(need_reread) {
			/*that will be destroyed in add_menu_widget*/
			menuw = NULL;
			if(main_menu)
				add_menu_widget(menu,NULL,main_menu,TRUE);
			else {
				GSList *dirlist = NULL;
				for(list = mfl; list != NULL;
				    list = g_slist_next(list)) {
					MenuFinfo *mf = list->data;
					dirlist = g_slist_append(dirlist,
								 mf->menudir);
				}
				add_menu_widget(menu,dirlist, main_menu,TRUE);
				g_slist_free(dirlist);
			}
		}
	} else {
		GSList *mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");
		GSList *list;
		int need_reread = FALSE;
		
		/*if(!mfl)
			g_warning("Weird menu doesn't have mf entry");*/

		/*check if we need to reread this*/
		for(list = mfl; list != NULL; list = g_slist_next(list)) {
			MenuFinfo *mf = list->data;
			if(!need_reread &&
			   (mf->fake_menu || !check_finfo_list(mf->finfo))) {
				need_reread = TRUE;

			}
		}
		if(need_reread) {
			menuw = NULL;
			for(list = mfl; list != NULL;
			    list = g_slist_next(list)) {
				MenuFinfo *mf = list->data;
				menuw = create_menu_at(menuw,
						       mf->menudir,
						       mf->applets,
						       mf->dir_name,
						       mf->pixmap_name,
						       TRUE,
						       FALSE);
			}
		}
	}
	return menuw;
}

static int
sel_idle(gpointer data)
{
	gtk_item_select(GTK_ITEM(data));
	return FALSE;
}

static void
submenu_to_display(GtkMenuItem *menuitem, gpointer data)
{
	GtkWidget *menu = check_and_reread(menuitem->submenu,
					   NULL,FALSE);
	g_return_if_fail(menu!=NULL);
	/*THIS IS A HACK, but a cool one at that*/
	if(menu!=menuitem->submenu) {
		/*it's not yet displayed we're ok here*/
		if(!GTK_WIDGET_VISIBLE(menuitem->submenu)) {
			gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), menu);
		/*now is when the fun begins, we kill the menu and do select
		  again ... nowdays, we'll be ok though*/
		} else {
			gtk_signal_emit_stop_by_name(GTK_OBJECT(menuitem),
						     "select");
			gtk_widget_destroy(menuitem->submenu);
			gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), menu);
			gtk_item_select(GTK_ITEM(menuitem));
		}
	}
}

static void
rh_submenu_to_display(GtkMenuItem *menuitem, gpointer data)
{
	char *rhdir;
	if(!g_file_exists("/etc/X11/wmconfig"))
		return;
	rhdir = gnome_util_home_file("apps-redhat/");
	if(!g_file_exists(rhdir) ||
	   !redhat_check ||
	   !check_finfo_list(redhat_check)) {
		create_rh_menu();
	}
	g_free(rhdir);
}



static GtkWidget *
create_fake_menu_at (char *menudir,
		     int applets,
		     char *dir_name,
		     char *pixmap_name)
{	
	MenuFinfo *mf;
	GtkWidget *menu;
	GSList *list;
	
	menu = gtk_menu_new ();
	
	mf = g_new(MenuFinfo,1);
	mf->menudir = g_strdup(menudir);
	mf->applets = applets;
	mf->dir_name = dir_name?g_strdup(dir_name):NULL;
	mf->pixmap_name = pixmap_name?g_strdup(pixmap_name):NULL;
	mf->fake_menu = TRUE;
	mf->finfo = NULL;
	
	list = g_slist_prepend(NULL,mf);
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
		int fake_submenus,
		int force)
{	
	GnomeDesktopEntry *item_info=NULL;
	GnomeDesktopEntry *dir_info=NULL;
	struct stat s;
	char *filename;
	int items = 0;
	FileInfo *fi;
	GSList *finfo = NULL;
	GSList *flist = NULL;
	GSList *li = NULL;
	GSList *mfl = NULL;
	int add_separator = FALSE;
	int first_item = 0;
	GtkWidget *menuitem;
	char *dentry_name;
	
	MenuFinfo *mf = NULL;
	
	if(!force && !g_file_exists(menudir))
		return menu;
	
	dentry_name = g_concat_dir_and_file (menudir,
					     ".directory");
	dir_info = gnome_desktop_entry_load (dentry_name);

	/*add the .directory file to the checked files list,
	  but only if we can stat it (if we can't it probably
	  doesn't exist)*/
	fi = make_finfo(dentry_name,FALSE);
	if(fi)
		finfo = g_slist_prepend(finfo,fi);
	g_free (dentry_name);

	/*get this info ONLY if we haven't gotten it already*/
	if(!dir_name)
		dir_name = dir_info?dir_info->name:_("Menu");
	if(!pixmap_name)
		pixmap_name = dir_info?dir_info->icon:gnome_folder;
	
	/*add dir to the checked files list*/
	fi = make_finfo(menudir,force);
	if(!fi)
		g_warning("Something is wrong, directory %s can't be stated",
			  menudir);
	else
		finfo = g_slist_prepend(finfo,fi);

	/*add the order file to the checked files list,
	  but only if we can stat it (if we can't it probably doesn't
	  exist)*/
	filename = g_concat_dir_and_file(menudir,".order");
	fi = make_finfo(filename,FALSE);
	if(fi)
		finfo = g_slist_prepend(finfo,fi);
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
	
	for(li = flist; li != NULL; li = g_slist_next(li)) {
		char *thisfile = li->data;
		GtkWidget *sub, *pixmap;
		char *pixmap_name;
		char *menuitem_name;
		
		filename = g_concat_dir_and_file(menudir,thisfile);
		
		if (stat (filename, &s) == -1) {
			g_warning("Something is wrong, "
				  "file %s can't be stated",
				  filename);
			g_free (filename);
			g_free(thisfile);
			continue;
		}

		sub = NULL;
		item_info = NULL;
		if (S_ISDIR (s.st_mode)) {
			dentry_name = g_concat_dir_and_file (filename,
							     ".directory");
			item_info = gnome_desktop_entry_load (dentry_name);

			/*add the .directory file to the checked files list*/
			fi = make_finfo(dentry_name,TRUE);
			finfo = g_slist_prepend(finfo,fi);
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
						      fake_submenus,
						      FALSE);

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
			finfo = g_slist_prepend(finfo,fi);
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
			pixmap = gnome_stock_pixmap_widget_at_size (NULL, pixmap_name,
								    SMALL_ICON_SIZE,
								    SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		}
		
		if(!sub && applets)
			setup_applet_drag (menuitem, item_info);
		/*setup the menuitem, pass item_loc if this is not
		  a submenu or an applet, so that the item can be added,
		  we can be sure that the finfo will live that long*/
 		setup_full_menuitem (menuitem, pixmap, menuitem_name,
				     (applets||sub)?NULL:fi->name);

		if(add_separator) {
			add_menu_separator(menu);
			first_item++;
			add_separator = FALSE;
		}
		gtk_menu_append (GTK_MENU (menu), menuitem);

		if(item_info && item_info->exec) {
			gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
					    applets?
					     GTK_SIGNAL_FUNC(add_applet):
					     GTK_SIGNAL_FUNC(activate_app_def),
					    fi->name);
		}
		if(item_info)
			gnome_desktop_entry_free(item_info);

		g_free(filename);
		g_free(thisfile);
	}
	g_slist_free(flist);
	
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
		gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);

		gtk_widget_show(menuitem);

		pixmap = NULL;
		if (pixmap_name && g_file_exists (pixmap_name)) {
			pixmap = gnome_stock_pixmap_widget_at_size (NULL, pixmap_name,
								    SMALL_ICON_SIZE,
								    SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		} else if (gnome_folder && g_file_exists (gnome_folder)) {
			pixmap = gnome_stock_pixmap_widget_at_size (NULL, gnome_folder,
								    SMALL_ICON_SIZE,
								    SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		}

		menuitem = gtk_menu_item_new();
		setup_title_menuitem(menuitem,pixmap,
				     dir_name?dir_name:"Menu",mf);
		gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);

		setup_directory_drag (menuitem, mf->menudir);
	}

	if(dir_info)
		gnome_desktop_entry_free(dir_info);

	mfl = g_slist_append(mfl,mf);

	gtk_object_set_data(GTK_OBJECT(menu),"mf",mfl);
	
	return menu;
}

static void
destroy_menu (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GtkWidget *prop_dialog = gtk_object_get_data(GTK_OBJECT(menu->button),
						     MENU_PROPERTIES);
	if(prop_dialog)
		gtk_widget_destroy(prop_dialog);
	gtk_widget_destroy(menu->menu);
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
	BUTTON_WIDGET(menu->button)->in_button = FALSE;
	BUTTON_WIDGET(menu->button)->ignore_leave = FALSE;
	button_widget_up(BUTTON_WIDGET(menu->button));
	menu->age = 0;
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
				     fake_submenus,FALSE);
	g_free (menudir);
	return applet_menu;
}

/*find the most empty spot, slightly hackish but kind of cool:)*/
static SnappedPos
find_empty_spos(void)
{
	GSList *li;
	int i;
	int low = 0;
	int posscore[4]={0,0,0,0};
	for(li=panel_list;li!=NULL;li=g_slist_next(li)) {
		PanelData *pd = li->data;
		if(IS_DRAWER_WIDGET(pd->panel))
			continue;
		else if(IS_SNAPPED_WIDGET(pd->panel)) {
			SnappedWidget *sw = SNAPPED_WIDGET(pd->panel);
			posscore[sw->pos]+=3;
			posscore[(sw->pos+1)%4]++;
			posscore[(sw->pos+3)%4]++;
		} else if(IS_CORNER_WIDGET(pd->panel)) {
			CornerWidget *cw = CORNER_WIDGET(pd->panel);
			PanelWidget *panel = PANEL_WIDGET(cw->panel);
			int cor = (cw->pos%2)?1-panel->orient:panel->orient;
			posscore[(cw->pos+cor)%4]+=3;
			posscore[(cw->pos+(1-cor))%4]++;
		}
	}

	for(i=1;i<4;i++) {
		if(posscore[i]<posscore[low])
			low = i;
	}
	return (SnappedPos)low;
}

/*find the most empty spot, slightly hackish but kind of cool:)*/
static void
find_empty_cpos_cori(CornerPos *cpos, PanelOrientation *cori)
{
	GSList *li;
	int i;
	int low = 0;
	int posscore[8]={0,0,0,0,0,0,0,0};
	for(li=panel_list;li!=NULL;li=g_slist_next(li)) {
		PanelData *pd = li->data;
		if(IS_DRAWER_WIDGET(pd->panel))
			continue;
		else if(IS_SNAPPED_WIDGET(pd->panel)) {
			SnappedWidget *sw = SNAPPED_WIDGET(pd->panel);
			posscore[(sw->pos*2)]+=3;
			posscore[((sw->pos*2)+6)%8]+=3;
			posscore[(sw->pos*2)+1]++;
			posscore[((sw->pos*2)+7)%8]++;
		} else if(IS_CORNER_WIDGET(pd->panel)) {
			CornerWidget *cw = CORNER_WIDGET(pd->panel);
			PanelWidget *panel = PANEL_WIDGET(cw->panel);
			int cor = (cw->pos%2)?1-panel->orient:panel->orient;
			posscore[(cw->pos*2)+cor]+=3;
			posscore[(cw->pos*2)+(1-cor)]++;
		}
	}

	for(i=1;i<8;i++) {
		if(posscore[i]<posscore[low])
			low = i;
	}
	*cpos = (CornerPos)(low/2);
	*cori = (PanelOrientation)(((*cpos)%2)?1-(low%2):(low%2));
}

static void
create_new_panel(GtkWidget *w,gpointer data)
{
	PanelType type = GPOINTER_TO_INT(data);
	GdkColor bcolor = {0,0,0,1};
	GtkWidget *panel;

	CornerPos cpos;
	PanelOrientation cori;

	switch(type) {
	case DRAWER_PANEL:
		break; /*we don't handle this one here*/
	case SNAPPED_PANEL: 
		panel = snapped_widget_new(find_empty_spos(),
					   SNAPPED_EXPLICIT_HIDE,
					   SNAPPED_SHOWN,
					   TRUE,
					   TRUE,
					   PANEL_BACK_NONE,
					   NULL,
					   TRUE,
					   &bcolor);
		panel_setup(panel);
		gtk_widget_show(panel);
		break;
	case CORNER_PANEL: 
		find_empty_cpos_cori(&cpos,&cori);
		panel = corner_widget_new(cpos,
					  cori,
					  CORNER_SHOWN,
					  TRUE,
					  TRUE,
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
	setup_menuitem (menuitem, 0, _("Edge panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) create_new_panel,
			   (gpointer)SNAPPED_PANEL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Corner panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) create_new_panel,
			   (gpointer)CORNER_PANEL);

	return menu;
}

static GtkWidget *
create_system_menu(GtkWidget *menu, int fake_submenus, int fake)
{
	char *menu_base = gnome_unconditional_datadir_file ("apps");
	char *menudir;
	
	menudir = g_concat_dir_and_file (menu_base, ".");
	g_free (menu_base);
	if (g_file_exists (menudir)) {
		if(!fake || menu) {
			menu = create_menu_at (menu,menudir,FALSE,_("System menus"),
					       NULL, fake_submenus, FALSE);
		} else {
			menu = create_fake_menu_at (menudir, FALSE,
						    _("System menus"),
						    NULL);
		}
		g_return_val_if_fail(menu,NULL);
		g_free (menudir);
	} else
		g_warning("No system menus found!");
	
	return menu;
}

static GtkWidget *
create_user_menu(char *title, char *dir, GtkWidget *menu, int fake_submenus, int force, int fake)
{
	char *menu_base = gnome_util_home_file (dir);
	char *menudir = g_concat_dir_and_file (menu_base, ".");
	
	if (!g_file_exists (menudir))
		mkdir (menu_base, 0755);
	g_free (menu_base);
	if(!fake || menu) {
		menu = create_menu_at (menu,menudir, FALSE,
				       title,
				       NULL,fake_submenus,
				       force);
	} else {
		menu = create_fake_menu_at (menudir, FALSE,
					    title,NULL);
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
	setup_menuitem (menuitem, 0, _("System menus"));
	gtk_menu_append (GTK_MENU (panel_menu), menuitem);
	menu = create_system_menu(NULL,TRUE,TRUE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),menu);
	gtk_signal_connect(GTK_OBJECT(menuitem),"select",
			   GTK_SIGNAL_FUNC(submenu_to_display),
			   NULL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("User menus"));
	gtk_menu_append (GTK_MENU (panel_menu), menuitem);
	menu = create_user_menu(_("User menus"),"apps",NULL,TRUE,TRUE,TRUE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),menu);
	gtk_signal_connect(GTK_OBJECT(menuitem),"select",
			   GTK_SIGNAL_FUNC(submenu_to_display),
			   NULL);

	if(g_file_exists("/etc/X11/wmconfig")) {
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 0, _("Red Hat menus"));
		gtk_menu_append (GTK_MENU (panel_menu), menuitem);
		menu = create_user_menu(_("Red Hat menus"),"apps-redhat",
					NULL,TRUE,TRUE,TRUE);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),menu);
		gtk_signal_connect(GTK_OBJECT(menuitem),"enter_notify_event",
				   GTK_SIGNAL_FUNC(rh_submenu_to_display),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(menuitem),"enter_notify_event",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   NULL);
	}

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
	gtk_object_set_data(GTK_OBJECT(panel_menu),"remove_item",menuitem);

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

static void
ask_about_launcher_cb(GtkWidget *w, gpointer data)
{
	ask_about_launcher(NULL,current_panel,0);
}

static void
convert_to_panel(GtkWidget *w, gpointer data)
{
	PanelType type = GPOINTER_TO_INT(data);
	GdkColor bcolor = {0,0,0,1};
	GtkWidget *panel;
	PanelWidget *newpanel = NULL;
	GtkWidget *pw;
	CornerWidget *cw;
	SnappedWidget *sw;
	int i,cor;

	g_return_if_fail(current_panel != NULL);
	
	pw = gtk_object_get_data(GTK_OBJECT(current_panel),"panel_parent");
	g_return_if_fail(pw != NULL);
	
	switch(type) {
	case SNAPPED_PANEL:
		/*we know that it's a corner widget since only
		  corner widget is ever converted to snapped*/
		cw = CORNER_WIDGET(pw);

		cor = (cw->pos%2)?1-current_panel->orient:current_panel->orient;
		panel = snapped_widget_new((cw->pos+cor)%4,
					   SNAPPED_EXPLICIT_HIDE,
					   SNAPPED_SHOWN,
					   cw->hidebuttons_enabled,
					   cw->hidebutton_pixmaps_enabled,
					   current_panel->back_type,
					   current_panel->back_pixmap,
					   current_panel->fit_pixmap_bg,
					   &current_panel->back_color);
		panel_setup(panel);
		gtk_widget_show(panel);
		newpanel = PANEL_WIDGET(SNAPPED_WIDGET(panel)->panel);
		break;
	case CORNER_PANEL: 
		/*we know that it's a snapped widget since only
		  snapped widget is ever converted to corner*/
		sw = SNAPPED_WIDGET(pw);
		panel = corner_widget_new((sw->pos==SNAPPED_TOP ||
					   sw->pos==SNAPPED_RIGHT)?
					  CORNER_NE:CORNER_SW,
					  (sw->pos==SNAPPED_TOP ||
					   sw->pos==SNAPPED_BOTTOM)?
					  PANEL_HORIZONTAL:PANEL_VERTICAL,
					  CORNER_SHOWN,
					  sw->hidebuttons_enabled,
					  sw->hidebutton_pixmaps_enabled,
					  current_panel->back_type,
					  current_panel->back_pixmap,
					  current_panel->fit_pixmap_bg,
					  &current_panel->back_color);
		panel_setup(panel);
		gtk_widget_show(panel);
		newpanel = PANEL_WIDGET(CORNER_WIDGET(panel)->panel);
		break;
	default: g_assert_not_reached();
	}
	panels_to_sync = TRUE;
	for(i=0;current_panel->applet_list!=NULL;i++) {
		AppletData *ad = current_panel->applet_list->data;
		panel_widget_reparent(current_panel,newpanel,ad->applet,i);
		printf("%lX, %d\n",(long)ad->applet,i);
	}
	gtk_widget_destroy(pw);
}

static void
show_x_on_panels(GtkWidget *menu, gpointer data)
{
	GtkWidget *pw;
	GtkWidget *convcorn = gtk_object_get_data(GTK_OBJECT(menu),"convcorn");
	GtkWidget *convsnap = gtk_object_get_data(GTK_OBJECT(menu),"convsnap");
	g_return_if_fail(current_panel != NULL);
	g_return_if_fail(convcorn != NULL);
	g_return_if_fail(convsnap != NULL);
	
	pw = gtk_object_get_data(GTK_OBJECT(current_panel),"panel_parent");
	g_return_if_fail(pw != NULL);
	
	if(IS_SNAPPED_WIDGET(pw)) {
		gtk_widget_show(convcorn);
		gtk_widget_hide(convsnap);
	} else if(IS_CORNER_WIDGET(pw)) {
		gtk_widget_hide(convcorn);
		gtk_widget_show(convsnap);
	} else {
		gtk_widget_hide(convcorn);
		gtk_widget_hide(convsnap);
	}
}

void
make_panel_submenu (GtkWidget *menu, int fake_submenus)
{
	GtkWidget *menuitem;
	GtkWidget *a,*b;

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
	setup_menuitem (menuitem, 0, _("Convert to corner panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) convert_to_panel,
			   GINT_TO_POINTER(CORNER_PANEL));
	gtk_object_set_data(GTK_OBJECT(menu),"convcorn",menuitem);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Convert to edge panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) convert_to_panel,
			   GINT_TO_POINTER(SNAPPED_PANEL));
	gtk_object_set_data(GTK_OBJECT(menu),"convsnap",menuitem);

	/*actually show hide right now to get the first size
	  request correctly, still has a display problem when
	  main menu is dragged onto a different type of a panel,
	  but the problem is minimal (it only happens if the person
	  uses the same menu)*/
	show_x_on_panels(menu,NULL);
	
	gtk_signal_connect(GTK_OBJECT(menu),"show",
			   GTK_SIGNAL_FUNC(show_x_on_panels),NULL);


	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add main menu"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(add_menu_to_panel),
			   NULL);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add drawer"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_drawer_to_panel,
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
			   GTK_SIGNAL_FUNC(ask_about_launcher_cb),NULL);

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
			    GTK_SIGNAL_FUNC(panel_config_global), 
			    NULL);

}

static void
panel_lock (GtkWidget *widget, void *data)
{
	system ("(xscreensaver-command -activate&)");
}

static void
add_special_entries (GtkWidget *menu, int fake_submenus)
{
	GtkWidget *menuitem;
	GtkWidget *panel_menu;
	char *char_tmp;

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

	char_tmp = gnome_is_program_in_path("guname");
	if(char_tmp) {
	  menuitem = gtk_menu_item_new ();
	  setup_menuitem (menuitem,
			  gnome_stock_pixmap_widget(menu,
						    GNOME_STOCK_PIXMAP_ABOUT),
			  _("About GNOME..."));
	  gtk_menu_append (GTK_MENU (menu), menuitem);
	  gtk_signal_connect_full(GTK_OBJECT (menuitem), "activate",
				  GTK_SIGNAL_FUNC(about_gnome_cb),NULL,
				  char_tmp, (GtkDestroyNotify)g_free,
				  FALSE,TRUE);
	}

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_PIXMAP_QUIT),
			_("Log out"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_quit), 0);

}

enum { RH_MENU_ITEM,RH_MENU_GROUP };
typedef struct _RHMenuItem RHMenuItem;
struct _RHMenuItem {
	int type;
	char *name;
	union {
		struct {
			char *description;
			char *icon;
			char *mini_icon;
			char *exec;
		} item;
		GSList *items;
	} u;
};

static void
free_rh_item(RHMenuItem *rh)
{
	if(!rh) return;
	g_free(rh->name);
	if(rh->type == RH_MENU_ITEM) {
		g_free(rh->u.item.description);
		g_free(rh->u.item.icon);
		g_free(rh->u.item.mini_icon);
		g_free(rh->u.item.exec);
	} else {
		g_slist_foreach(rh->u.items,(GFunc)free_rh_item,NULL);
		g_slist_free(rh->u.items);
	}
	g_free(rh);
}

/*most likely one of the ugliest functions in existence*/
static char *
read_word(char **p)
{
	char *buf;
	char *w;
	int quoted;
	int len;
	if(!p) return NULL;
	if(!*p) return NULL;
	while(*(*p) == ' ' || *(*p) == '\t') (*p)++;
	if(!(*(*p))) return NULL;
	quoted = FALSE;
	for(w=*p,len=0;
	    *w && (quoted || (*w!=' ' && *w!='\t'));
	    w++) {
		if(*w=='"')
			quoted = !quoted;
		else
			len++;
	}
	buf = g_malloc(len+1);
	quoted = FALSE;
	for(w=buf;*(*p) && (quoted || (*(*p)!=' ' && *(*p)!='\t'));(*p)++) {
		if(*(*p)=='"')
			quoted = !quoted;
		else {
			*w++ = *(*p);
		}
	}
	*w='\0';
	return buf;
}

static int
sort_rh(RHMenuItem *a, RHMenuItem *b)
{
	if(a->type > b->type) return -1;
	else if(a->type < b->type) return 1;
	return strcmp(a->name,b->name);
}

static GSList *
insert_rh_into_tree(GSList *list,RHMenuItem *rh,GSList *group)
{
	GSList *li;
	RHMenuItem *ri;
	if(!group) 
		return g_slist_insert_sorted(list,rh,(GCompareFunc)sort_rh);
	for(li=list;li!=NULL;li=g_slist_next(li)) {
		ri = li->data;
		if(ri->type == RH_MENU_GROUP &&
		   strcmp(ri->name,group->data)==0) {
			ri->u.items = insert_rh_into_tree(ri->u.items, rh,
							  group->next);
			break;
		}
	}
	/*we inserted it already*/
	if(li) return list;
	
	ri = g_new0(RHMenuItem,1);
	ri->type = RH_MENU_GROUP;
	ri->name = g_strdup(group->data);
	ri->u.items = insert_rh_into_tree(ri->u.items, rh,
					  group->next);
	return g_slist_insert_sorted(list,ri,(GCompareFunc)sort_rh);
}

static GSList *
add_redhat_entry(GSList *list, char *file)
{
	FILE *fp;
	char *p,*w;
	char buf[256];
	GSList *group = NULL;
	
	RHMenuItem *rh = NULL;
	
	fp = fopen(file,"r");
	if(!fp) return list;
	
	rh = g_new0(RHMenuItem,1);
	while(fgets(buf,256,fp)) {
		char **param = NULL;
		p = strchr(buf,'\n');
		if(p) *p='\0';
		p = buf;
		g_free(read_word(&p)); /*XXX:ignore??*/
		w = read_word(&p);
		if(strcmp(w,"name")==0) {
			g_free(rh->name);
			rh->name = read_word(&p);
		} else if(strcmp(w,"description")==0) {
			g_free(rh->u.item.description);
			rh->u.item.description = read_word(&p);
		} else if(strcmp(w,"icon")==0) {
			char *s = read_word(&p);
			g_free(rh->u.item.icon);
			if(*s == '/') {
				if(g_file_exists(s))
					rh->u.item.icon = s;
				else {
					g_free(s);
					rh->u.item.icon = NULL;
				}
			} else {
				rh->u.item.icon = g_concat_dir_and_file("/usr/share/icons", s);
				if(!g_file_exists(rh->u.item.icon)) {
					g_free(rh->u.item.icon);
					rh->u.item.icon = gnome_pixmap_file(s);
				}
				g_free(s);
			}
		} else if(strcmp(w,"mini-icon")==0) {
			char *s = read_word(&p);
			g_free(rh->u.item.mini_icon);
			if(*s == '/') {
				if(g_file_exists(s))
					rh->u.item.mini_icon = s;
				else {
					g_free(s);
					rh->u.item.mini_icon = NULL;
				}
			} else {
				rh->u.item.mini_icon = g_concat_dir_and_file("/usr/share/icons/mini", s);
				if(!g_file_exists(rh->u.item.mini_icon)) {
					g_free(rh->u.item.mini_icon);
					rh->u.item.mini_icon = gnome_pixmap_file(s);
				}
				g_free(s);
			}
		} else if(strcmp(w,"exec")==0) {
			g_free(rh->u.item.exec);
			rh->u.item.exec = read_word(&p);
		} else if(strcmp(w,"group")==0) {
			char *sc;
			char *s = read_word(&p);

			if(group) {
				g_slist_foreach(group,(GFunc)g_free,NULL);
				g_slist_free(group);
				group = NULL;
			}
			sc = strtok(s,"/");
			while(sc) {
				group = g_slist_append(group, g_strdup(sc));
				sc = strtok(NULL,"/");
			}
			g_free(s);
		}
		g_free(w);
	}
	fclose(fp);
	if(!rh->name || !rh->u.item.exec) {
		if(group) {
			g_slist_foreach(group,(GFunc)g_free,NULL);
			g_slist_free(group);
		}
		free_rh_item(rh);
		return list;
	}
	list = insert_rh_into_tree(list,rh,group);
	g_slist_foreach(group,(GFunc)g_free,NULL);
	g_slist_free(group);
	return list;
}


static void
make_rh_submenu(char *dir, GSList *rhlist)
{
	GSList *li;
	FILE *fp;
	char *order_file = g_concat_dir_and_file(dir,".order");
	fp = fopen(order_file,"w");
	for(li = rhlist;li!=NULL;li = g_slist_next(li)) {
		RHMenuItem *ri = li->data;
		GnomeDesktopEntry *dentry = g_new0(GnomeDesktopEntry,1);

		dentry->name = g_strdup(ri->name);
		if(ri->type == RH_MENU_GROUP) {
			char *s = g_strdup(ri->name);
			char *p;
			dentry->type = g_strdup("Directory");
			while((p=strchr(s,' '))) *p='_';
			p = g_concat_dir_and_file(dir,s);
			if(fp) fprintf(fp,"%s\n",g_basename(p));
			mkdir(p,0755);
			dentry->location = g_concat_dir_and_file(p,".directory");
			g_free(s);
			
			make_rh_submenu(p,ri->u.items);

			g_free(p);
		} else {
			char *s = g_strconcat(ri->name,".desktop",NULL);
			char *p;
			while((p=strchr(s,' '))) *p='_';

			dentry->type = g_strdup("Application");
			dentry->comment = g_strdup(ri->u.item.description);
			dentry->icon = g_strdup(ri->u.item.icon?
						ri->u.item.icon:
						ri->u.item.mini_icon);
			gnome_config_make_vector(ri->u.item.exec,
						 &dentry->exec_length,
						 &dentry->exec);
			dentry->location = g_concat_dir_and_file(dir,s);
			if(fp) fprintf(fp,"%s\n",s);
			g_free(s);
		}
		gnome_desktop_entry_save(dentry);
		gnome_desktop_entry_free(dentry);
	}
	if(fp) fclose(fp);
	g_free(order_file);
}


static void
create_rh_menu(void)
{
	char *userrh = gnome_util_prepend_user_home(".wmconfig/");
	char *rhdir = gnome_util_home_file("apps-redhat/");
	GSList *rhlist = NULL;
	GSList *li = NULL;
	GtkWidget *w;
	int i;
	char *dirs[3] = {"/etc/X11/wmconfig/",userrh,NULL};
	g_return_if_fail(userrh!=NULL);
	g_return_if_fail(rhdir!=NULL);
	
	for(li = redhat_check; li!=NULL; li = g_slist_next(li)) {
		FileInfo *finfo = li->data;
		g_free(finfo->name);
		g_free(finfo);
	}
	g_slist_free(redhat_check);
	redhat_check = NULL;
	
	remove_directory(rhdir,FALSE);

	redhat_check = g_slist_prepend(redhat_check,make_finfo("/etc/X11/wmconfig/",TRUE));
	redhat_check = g_slist_prepend(redhat_check,make_finfo(userrh,TRUE));

	/*read redhat wmconfig files*/
	for(i=0;dirs[i];i++) {
		DIR *dir;
		struct dirent *dent;

		dir = opendir (dirs[i]);
		if(!dir) continue;
		while((dent = readdir (dir)) != NULL) {
			char *p;
			if(strcmp(dent->d_name,".")==0 ||
			   strcmp(dent->d_name,"..")==0)
				continue;
			p = g_concat_dir_and_file(dirs[i],dent->d_name);
			rhlist = add_redhat_entry(rhlist,p);
			g_free(p);
		}
		closedir(dir);
	}
	g_free(userrh);
	mkdir(rhdir,0755);
	if(rhlist) {
		if (g_file_exists("/usr/share/icons/mini/mini-redhat.xpm")) {
			GnomeDesktopEntry *dentry = g_new0(GnomeDesktopEntry,1);
			dentry->name = g_strdup(_("Red Hat menus"));
			dentry->type = g_strdup("Directory");
			dentry->icon = g_strdup("/usr/share/icons/mini/mini-redhat.xpm");
			dentry->location = g_concat_dir_and_file(rhdir,".directory");
			gnome_desktop_entry_save(dentry);
			gnome_desktop_entry_free(dentry);
		}
		make_rh_submenu(rhdir,rhlist);

		g_slist_foreach(rhlist,(GFunc)free_rh_item,NULL);
		g_slist_free(rhlist);
	}
	g_free(rhdir);
}

static GtkWidget *
create_root_menu(int fake_submenus, int flags)
{
	GtkWidget *root_menu;
	GtkWidget *menu;
	GtkWidget *menuitem;
	int need_separ = FALSE;
	
	root_menu = NULL;
	
	if(flags&MAIN_MENU_SYSTEM && !(flags&MAIN_MENU_SYSTEM_SUB)) {
		root_menu = create_system_menu(root_menu,fake_submenus,FALSE);
		need_separ = TRUE;
	}
	if(flags&MAIN_MENU_USER && !(flags&MAIN_MENU_USER_SUB)) {
		root_menu = create_user_menu(_("User menus"), "apps",
					     root_menu, fake_submenus, FALSE,
					     FALSE);
		need_separ = TRUE;
	}
	if(flags&MAIN_MENU_REDHAT && !(flags&MAIN_MENU_REDHAT_SUB)) {
		rh_submenu_to_display(NULL,NULL);
		root_menu = create_user_menu(_("Red Hat menus"), "apps-redhat",
					     root_menu, fake_submenus, FALSE,
					     FALSE);
		need_separ = TRUE;
	}
	/*others here*/
	
	if(!root_menu)
		root_menu = gtk_menu_new();

	if(flags&MAIN_MENU_SYSTEM && flags&MAIN_MENU_SYSTEM_SUB) {
		if(need_separ)
			add_menu_separator(root_menu);
		need_separ = FALSE;
		menu = create_system_menu(NULL,fake_submenus, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 0, _("System menus"));
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menuitem),"select",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   NULL);
	}
	if(flags&MAIN_MENU_USER && flags&MAIN_MENU_USER_SUB) {
		if(need_separ)
			add_menu_separator(root_menu);
		need_separ = FALSE;
		menu = create_user_menu(_("User menus"), "apps", 
					NULL, fake_submenus, TRUE, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, 0, _("User menus"));
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menuitem),"select",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   NULL);
	}
	if(flags&MAIN_MENU_REDHAT && flags&MAIN_MENU_REDHAT_SUB) {
		GtkWidget *pixmap = NULL;
		if(need_separ)
			add_menu_separator(root_menu);
		need_separ = FALSE;
		menu = create_user_menu(_("Red Hat menus"), "apps-redhat", 
					NULL, fake_submenus, TRUE, TRUE);
		if (g_file_exists("/usr/share/icons/mini/mini-redhat.xpm")) {
			pixmap = gnome_stock_pixmap_widget_at_size (NULL, "/usr/share/icons/mini/mini-redhat.xpm",
								    SMALL_ICON_SIZE,
								    SMALL_ICON_SIZE);
			if (pixmap)
				gtk_widget_show (pixmap);
		}
		menuitem = gtk_menu_item_new ();
		setup_menuitem (menuitem, pixmap, _("Red Hat menus"));
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menuitem),"select",
				   GTK_SIGNAL_FUNC(rh_submenu_to_display),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(menuitem),"select",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   NULL);
	}
	add_special_entries (root_menu, fake_submenus);
	
	return root_menu;
}

static void
add_menu_widget (Menu *menu, GSList *menudirl, int main_menu, int fake_subs)
{
	GSList *li;
	if(menu->menu)
		gtk_widget_destroy(menu->menu);

	if (main_menu)
		menu->menu = create_root_menu(fake_subs, menu->main_menu_flags);
	else {
		menu->menu = NULL;
		for(li=menudirl;li!=NULL;li=g_slist_next(li))
			menu->menu = create_menu_at (menu->menu,li->data,
						     FALSE, NULL, NULL,
						     fake_subs, FALSE);
	}
	gtk_signal_connect (GTK_OBJECT (menu->menu), "deactivate",
			    GTK_SIGNAL_FUNC (menu_deactivate), menu);
}

static void
menu_button_pressed(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GdkEventButton *bevent = (GdkEventButton*)gtk_get_current_event();
	GtkWidget *wpanel = get_panel_parent(menu->button);
	AppletInfo *info = gtk_object_get_data(GTK_OBJECT(menu->button),
					       "applet_info");
	int main_menu = (strcmp (menu->path, ".") == 0);

	/*this HAS to be set everytime we popup the menu*/
	current_panel = PANEL_WIDGET(menu->button->parent);

	if(!menu->menu) {
		char *menu_base = gnome_unconditional_datadir_file ("apps");
		char *this_menu = get_real_menu_path(menu->path,menu_base);
		GSList *list = g_slist_append(NULL,this_menu);
		
		add_menu_widget(menu,list, strcmp(menu->path,".")==0, TRUE);
		
		g_free(menu_base);
		g_free(this_menu);

		g_slist_free(list);
	} else {
		if(menu->main_menu_flags&MAIN_MENU_REDHAT &&
		   !(menu->main_menu_flags&MAIN_MENU_REDHAT_SUB))
			rh_submenu_to_display(NULL,NULL);

		check_and_reread(menu->menu,menu,main_menu);
	}

	/*so that the panel doesn't pop down until we're
	  done with the menu */
	if(IS_SNAPPED_WIDGET(wpanel)) {
		SNAPPED_WIDGET(wpanel)->autohide_inhibit = TRUE;
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(wpanel));
	}

	BUTTON_WIDGET(menu->button)->ignore_leave = TRUE;
	gtk_grab_remove(menu->button);

	menu->age = 0;
	gtk_menu_popup(GTK_MENU(menu->menu), 0,0, applet_menu_position,
		       info, bevent->button, bevent->time);
}

static char *
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
		   PanelOrientType orient, int main_menu_flags)
{
	Menu *menu;
	
	char *pixmap_name;

	menu = g_new(Menu,1);
	menu->age = 0;
	menu->menu = NULL;
	menu->path = NULL;

	pixmap_name = get_pixmap(menudir,main_menu);

	menu->main_menu_flags = main_menu_flags;



	/*make the pixmap*/
	menu->button = button_widget_new_from_file (pixmap_name,
						    MENU_TILE,
						    TRUE,orient);
	gtk_signal_connect_after (GTK_OBJECT (menu->button), "pressed",
				  GTK_SIGNAL_FUNC (menu_button_pressed), menu);
	gtk_signal_connect (GTK_OBJECT (menu->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_menu), menu);
	gtk_widget_show(menu->button);

	/*{
		GSList *list = g_slist_append(NULL,menudir);
		add_menu_widget(menu,list,main_menu,TRUE);
		g_slist_free(list);
	}*/

	g_free (pixmap_name);

	return menu;
}

static Menu *
create_menu_applet(char *arguments, PanelOrientType orient,
		   int main_menu_flags)
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
		gnome_folder =
			gnome_unconditional_pixmap_file("gnome-folder.png");
		if (!g_file_exists (gnome_folder)) {
			g_free (gnome_folder);
			gnome_folder = NULL;
		}
	}

	main_menu = (!arguments || !*arguments || (strcmp (arguments, ".") == 0));

	menu = create_panel_menu (this_menu, main_menu, orient,main_menu_flags);
	menu->path=g_strdup((arguments && *arguments)?arguments:".");

	gtk_object_set_user_data(GTK_OBJECT(menu->button),menu);

	g_free(menu_base);
	g_free (this_menu);
	return menu;
}

void
set_menu_applet_orient(Menu *menu, PanelOrientType orient)
{
	g_return_if_fail(menu!=NULL);

	button_widget_set_params(BUTTON_WIDGET(menu->button),
				 MENU_TILE,TRUE,orient);
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
	GtkWidget *pathentry = gtk_object_get_data(GTK_OBJECT(widget), "path");
	char *s;

	if (page != -1)
		return;
	
	if(GTK_TOGGLE_BUTTON(main_menu)->active) {
		g_free(menu->path);
		menu->path = g_strdup(".");
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

	gtk_widget_destroy(menu->menu);	
	menu->menu = NULL;

	{
		char *menu_base = gnome_unconditional_datadir_file ("apps");
		char *this_menu = get_real_menu_path(menu->path,menu_base);
		GSList *list = g_slist_append(NULL,this_menu);
		char *pixmap_name = get_pixmap(this_menu,
					       strcmp(menu->path,".")==0);
		/*make the pixmap*/
		button_widget_set_pixmap_from_file (BUTTON_WIDGET(menu->button),
						    pixmap_name);
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

static void
add_menu_type_options(GtkObject *dialog, GtkTable *table, int row,
		      char *title,char *ident, int on, int sub)
{
	char *p;
	GtkWidget *w;
	GtkWidget *rb;
	GtkWidget *off;

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
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w),TRUE);
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
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w),TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_prop), 
			    dialog);
	
	w = gtk_radio_button_new_with_label (gtk_radio_button_group(GTK_RADIO_BUTTON(rb)),
					     _("On the main menu"));
	gtk_table_attach_defaults(table,w,1,2,row,row+1);
	if(on && !sub)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w),TRUE);
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
	gtk_window_set_title(GTK_WINDOW(dialog), _("Menu properties"));
	/*gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	vbox = gtk_vbox_new(FALSE,5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox),5);

	f = gtk_frame_new(_("Menu type"));
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	box = gtk_hbox_new(FALSE,5);
	gtk_container_set_border_width(GTK_CONTAINER(box),5);
	gtk_container_add(GTK_CONTAINER(f),box);
	
	w = gtk_radio_button_new_with_label (NULL, _("Main menu"));
	gtk_object_set_data(GTK_OBJECT(dialog),"main_menu",w);
	if(!menu->path || strcmp(menu->path,".")==0)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), TRUE);
	gtk_signal_connect (GTK_OBJECT (w), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_main_menu), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);

	w2 = gtk_radio_button_new_with_label (
			  gtk_radio_button_group (GTK_RADIO_BUTTON (w)),
			  _("Normal menu"));
	if(menu->path && strcmp(menu->path,".")!=0)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w2), TRUE);
	gtk_signal_connect (GTK_OBJECT (w2), "toggled", 
			    GTK_SIGNAL_FUNC (toggle_normal_menu), 
			    dialog);
	gtk_box_pack_start(GTK_BOX(box),w2,TRUE,TRUE,0);


	f = gtk_frame_new(_("Main menu"));
	if(menu->path && strcmp(menu->path,".")!=0)
		gtk_widget_set_sensitive(f,FALSE);
	gtk_object_set_data(GTK_OBJECT(dialog),"main_frame",f);
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	table = gtk_table_new(3,4,FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table),5);
	gtk_container_add(GTK_CONTAINER(f),table);

	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),0,
			      _("System menu: "),"system",
			      menu->main_menu_flags&MAIN_MENU_SYSTEM,
			      menu->main_menu_flags&MAIN_MENU_SYSTEM_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),1,
			      _("User menu: "),"user",
			      menu->main_menu_flags&MAIN_MENU_USER,
			      menu->main_menu_flags&MAIN_MENU_USER_SUB);
	add_menu_type_options(GTK_OBJECT(dialog),GTK_TABLE(table),2,
			      _("Red Hat menu (if found): "),"redhat",
			      menu->main_menu_flags&MAIN_MENU_REDHAT,
			      menu->main_menu_flags&MAIN_MENU_REDHAT_SUB);
	
	f = gtk_frame_new(_("Normal menu"));
	if(!menu->path || strcmp(menu->path,".")==0)
		gtk_widget_set_sensitive(f,FALSE);
	gtk_object_set_data(GTK_OBJECT(dialog),"normal_frame",f);
	gtk_box_pack_start(GTK_BOX(vbox),f,FALSE,FALSE,0);
	
	box = gtk_hbox_new(FALSE,5);
	gtk_container_set_border_width(GTK_CONTAINER(box),5);
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
		return;
	}

	dialog = create_properties_dialog(menu);
	gtk_object_set_data(GTK_OBJECT(menu->button),
			    MENU_PROPERTIES,dialog);
	gtk_widget_show_all (dialog);
}

void
load_menu_applet(char *params, int main_menu_flags,
		 PanelWidget *panel, int pos)
{
	Menu *menu;

	menu = create_menu_applet(params, ORIENT_UP,main_menu_flags);

	if(menu) {
		register_toy(menu->button,menu,
			     panel,pos,APPLET_MENU);

		applet_add_callback(applets_last->data,"properties",
				    GNOME_STOCK_MENU_PROP,
				    _("Properties..."));
	}
}

/*menu related utility functions .. also used elswhere*/

static void
get_menu_position (GtkMenu *menu, int *x, int *y,
		   int wx, int wy, int ww, int wh,
		   GtkWidget *pwidget)
{
	if(IS_DRAWER_WIDGET(pwidget)) {
		PanelWidget *panel =
			PANEL_WIDGET(DRAWER_WIDGET(pwidget)->panel);
		if(panel->orient==PANEL_VERTICAL) {
			*x = wx + ww;
			*y += wy;
		} else {
			*x += wx;
			*y = wy - GTK_WIDGET (menu)->requisition.height;
		}
	} else if(IS_SNAPPED_WIDGET(pwidget)) {
		switch(SNAPPED_WIDGET(pwidget)->pos) {
		case SNAPPED_BOTTOM:
			*x += wx;
			*y = wy - GTK_WIDGET (menu)->requisition.height;
			break;
		case SNAPPED_TOP:
			*x += wx;
			*y = wy + wh;
			break;
		case SNAPPED_LEFT:
			*x = wx + ww;
			*y += wy;
			break;
		case SNAPPED_RIGHT:
			*x = wx - GTK_WIDGET (menu)->requisition.width;
			*y += wy;
			break;
		}
	} else if(IS_CORNER_WIDGET(pwidget)) {
		PanelWidget *panel =
			PANEL_WIDGET(CORNER_WIDGET(pwidget)->panel);
		if(panel->orient==PANEL_HORIZONTAL) {
			switch(CORNER_WIDGET(pwidget)->pos) {
			case CORNER_SE:
			case CORNER_SW:
				*x += wx;
				*y = wy - GTK_WIDGET (menu)->requisition.height;
				break;
			case CORNER_NE:
			case CORNER_NW:
				*x += wx;
				*y = wy + wh;
				break;
			}
		} else {
			switch(CORNER_WIDGET(pwidget)->pos) {
			case CORNER_NW:
			case CORNER_SW:
				*x = wx + ww;
				*y += wy;
				break;
			case CORNER_NE:
			case CORNER_SE:
				*x = wx - GTK_WIDGET (menu)->requisition.width;
				*y += wy;
				break;
			}
		}
	}

	if(*x + GTK_WIDGET (menu)->requisition.width > gdk_screen_width())
		*x=gdk_screen_width() - GTK_WIDGET (menu)->requisition.width;
	if(*x < 0) *x =0;

	if(*y + GTK_WIDGET (menu)->requisition.height > gdk_screen_height())
		*y=gdk_screen_height() - GTK_WIDGET (menu)->requisition.height;
	if(*y < 0) *y =0;
}

void
panel_menu_position (GtkMenu *menu, int *x, int *y, gpointer data)
{
	GtkWidget *w = data;
	int wx, wy;

	g_return_if_fail(w != NULL);

	gdk_window_get_origin (w->window, &wx, &wy);
	
	gtk_widget_get_pointer(w, x, y);
	get_menu_position(menu,x,y,wx,wy,w->allocation.width,w->allocation.height,w);
}

void
applet_menu_position (GtkMenu *menu, int *x, int *y, gpointer data)
{
	AppletInfo *info = data;
	int wx, wy;
	PanelWidget *panel;
	GtkWidget *w; /*the panel window widget*/

	g_return_if_fail(info != NULL);
	g_return_if_fail(info->widget != NULL);

	panel = PANEL_WIDGET(info->widget->parent);
	g_return_if_fail(panel != NULL);
	
	w = gtk_object_get_data(GTK_OBJECT(panel), PANEL_PARENT);

	gdk_window_get_origin (info->widget->window, &wx, &wy);
	if(GTK_WIDGET_NO_WINDOW(info->widget)) {
		wx += info->widget->allocation.x;
		wy += info->widget->allocation.y;
	}
	*x = *y = 0;
	get_menu_position(menu,x,y,wx,wy,
			  info->widget->allocation.width,
			  info->widget->allocation.height,
			  w);
}

