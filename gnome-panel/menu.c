/*
 * GNOME panel menu module.
 * (C) 1997, 1998, 1999, 2000 The Free Software Foundation
 * Copyright 200 Helix Code, Inc.
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
#include "panel-widget.h"
#include "tearoffitem.h"
#include "gnome-run.h"

#define TEAROFF_MENUS 1
/*#define PANEL_DEBUG 1*/

typedef enum {
	SMALL_ICON_SIZE  = 20,
	MEDIUM_ICON_SIZE = 32,
	BIG_ICON_SIZE    = 48
} IconSize;

static char *gnome_folder = NULL;

extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

/*list of all toplevel panel widgets (basep) created*/
extern GSList *panel_list;
/*list of all PanelWidgets created*/
extern GSList *panels;

extern GlobalConfig global_config;

extern int config_sync_timeout;
extern int panels_to_sync;
extern int applets_to_sync;
extern int need_complete_save;

extern int base_panels;

extern char *kde_menudir;
extern char *kde_icondir;
extern char *kde_mini_icondir;

typedef struct _TearoffMenu TearoffMenu;
struct _TearoffMenu {
	GtkWidget *menu;
	GSList *mfl;
	char *special;
	char *title;
	char *wmclass;
};

/* list of TearoffMenu s */
static GSList *tearoffs = NULL;

/*to be called on startup to load in some of the directories,
  this makes the startup a little bit slower, and take up slightly
  more ram, but it also speeds up later operation*/
void
init_menus(void)
{
	/*just load the menus from disk, don't make the widgets
	  this just reads the .desktops of the top most directory
	  and a level down*/
	char *menu = gnome_datadir_file("gnome/apps");
	if(menu)
		fr_read_dir(NULL,menu,NULL,2);
	g_free(menu);
	menu = gnome_datadir_file("applets");
	if(menu)
		fr_read_dir(NULL,menu,NULL,2);
	g_free(menu);
	menu = gnome_util_home_file("apps");
	if(menu)
		fr_read_dir(NULL,menu,NULL,2);
	g_free(menu);
	/*if redhat menus, use the fork version to read*/
	if(g_file_exists(REDHAT_MENUDIR))
		create_rh_menu(TRUE);
}

/*the most important dialog in the whole application*/
static void
about_cb (GtkWidget *widget, gpointer data)
{
	static GtkWidget *about;
	char *authors[] = {
	  "George Lebl (jirka@5z.com)",
	  "Jacob Berkman (jberkman@andrew.cmu.edu)",
	  "Miguel de Icaza (miguel@kernel.org)",
	  "Federico Mena (quartic@gimp.org)",
	  "Tom Tromey (tromey@cygnus.com)",
	  "Ian Main (slow@intergate.bc.ca)",
	  "Elliot Lee (sopwith@redhat.com)",
	  "Owen Taylor (otaylor@redhat.com)",
	  "Many many others ...",
	  "and finally, The Knights Who Say ... NI!",
	  NULL
	  };

	if (about) {
		gdk_window_show (about->window);
		gdk_window_raise (about->window);
		return;
	}

	about = gnome_about_new ( _("The GNOME Panel"), VERSION,
			"(C) 1997-2000 the Free Software Foundation",
			(const gchar **)authors,
			_("This program is responsible for launching "
			"other applications, embedding small applets "
			"within itself, world peace, and random X crashes."),
			"gnome-gegl2.png");
	gtk_signal_connect (GTK_OBJECT (about), "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			    &about);
	gtk_widget_show (about);
}

static void
about_gnome_cb(GtkObject *object, char *program_path)
{
	if(gnome_execute_async(NULL, 1, &program_path)<0)
		gnome_error_dialog(_("Can't execute 'About GNOME'"));
}

static void
activate_app_def (GtkWidget *widget, char *item_loc)
{
	GnomeDesktopEntry *item = gnome_desktop_entry_load(item_loc);
	if(item) {
		gnome_desktop_entry_launch (item);
		gnome_desktop_entry_free(item);
	} else {
		gnome_error_dialog(_("Can't load entry"));
	}
}

/* FIXME: this is an extremely ugly function, I mean this is so
   bad it's not even funny */
static void
add_app_to_personal (GtkWidget *widget, char *item_loc)
{
	char *s;
	char *p;
	p = gnome_util_home_file("apps");
	s = g_strdup_printf("cp -r -f '%s' '%s'",item_loc,p);
	g_free(p);
	system(s);
	g_free(s);
}

/* returns a g_strdup'd string with filesystem reserved chars replaced */
/* again from gmenu */
static char *
validate_filename(char *file)
{
	char *ret;
	char *ptr;

	g_return_val_if_fail(file != NULL, NULL);
	
	ret = g_strdup(file);
	ptr = ret;
	while (*ptr != '\0') {
		if (*ptr == '/') *ptr = '_';
		ptr++;
	}

	return ret;
}

static void
really_add_new_menu_item (GtkWidget *d, int button, gpointer data)
{
	GnomeDEntryEdit *dedit = GNOME_DENTRY_EDIT(data);
	char *file, *dir = gtk_object_get_data(GTK_OBJECT(d),"dir");
	GnomeDesktopEntry *dentry;
	FILE *fp;
	
	if(button == 0) {
		dentry = gnome_dentry_get_dentry(dedit);

		if(!dentry->name || !(*(dentry->name)))
			dentry->name=g_strdup(_("untitled"));
		
		
		/* assume we are making a new file */
		if (!dentry->location) {
			int i=2;
			char *tmp=NULL;

			tmp = validate_filename(dentry->name);

			file = g_strdup_printf("%s.desktop", tmp);
			dentry->location = g_concat_dir_and_file(dir, file);
				
			while (g_file_exists(dentry->location)) {
				g_free(dentry->location);
				g_free(file);
				file = g_strdup_printf("%s%d", tmp, i++);
				dentry->location = g_concat_dir_and_file(dir, file);
			}
			g_free(tmp);
			g_free(file);
		}

		file = g_concat_dir_and_file(dir, ".order");
		fp = fopen(file, "a");
		if (fp) {
			char *file2 = g_basename(dentry->location);
			if (file2)
				fprintf(fp, "%s\n", file2);
			else
				g_warning(_("Could not get file from path: %s"), 
					  dentry->location);
			fclose(fp);
		} else
			g_warning(_("Could not open .order file: %s"), file);
		g_free(file);

		gnome_desktop_entry_save(dentry);
		gnome_desktop_entry_free(dentry);

	}
	gtk_widget_destroy(d);
}

static void
add_new_app_to_menu (GtkWidget *widget, char *item_loc)
{
	GtkWidget *d;
	GtkWidget *notebook;
	GnomeDEntryEdit *dee;
	GList *types = NULL;

	d = gnome_dialog_new(_("Create menu item"),
			     GNOME_STOCK_BUTTON_OK,
			     GNOME_STOCK_BUTTON_CANCEL,
			     NULL);
	gtk_window_set_wmclass(GTK_WINDOW(d),
			       "create_menu_item","Panel");
	gtk_window_set_policy(GTK_WINDOW(d), FALSE, FALSE, TRUE);
	
	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),notebook,
			   TRUE,TRUE,GNOME_PAD_SMALL);
	dee = GNOME_DENTRY_EDIT(gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(notebook)));
	types = g_list_append(types, "Application");
	types = g_list_append(types, "URL");
	types = g_list_append(types, "PanelApplet");
	gtk_combo_set_popdown_strings(GTK_COMBO(dee->type_combo), types);
	g_list_free(types);
	
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(dee->type_combo)->entry),
			   "Application");

	gtk_object_set_data(GTK_OBJECT(d),"dir", g_strdup(item_loc));
	
	gtk_signal_connect(GTK_OBJECT(d),"clicked",
			   GTK_SIGNAL_FUNC(really_add_new_menu_item),
			   dee);

	gnome_dialog_close_hides(GNOME_DIALOG(d),FALSE);

	gnome_dialog_set_default(GNOME_DIALOG(d),0);

	gtk_widget_show_all(d);	
}

static void
remove_menuitem (GtkWidget *widget, char *item_loc)
{
	char *file, *dir, buf[256], *order_in_name, *order_out_name;
	FILE *order_in_file, *order_out_file;

	g_return_if_fail (item_loc);
	if (unlink(item_loc) < 0) {
		char *s;
		s = g_strdup_printf(_("Could not remove the menu item %s: %s\n"), 
				    item_loc, g_strerror(errno));
		gnome_error_dialog(s);
		g_free(s);
		return;
	}

	file = g_basename(item_loc);
	if (!file) {
		g_warning(_("Could not get file name from path: %s"),
			  item_loc);
		return;
	}

	dir = g_strdup(item_loc);
	dir[g_basename(dir)-dir] = '\0';
	
	order_in_name = g_concat_dir_and_file(dir, ".order");
	order_in_file = fopen(order_in_name, "r");

	if (!order_in_file) {
		/*no .order file so we can just leave*/
		g_free(order_in_name);
		return;
	}

	order_out_name = g_concat_dir_and_file(dir, ".order.tmp");
	order_out_file = fopen(order_out_name, "w");

	if (!order_out_file) {
		char *s;
		s = g_strdup_printf(_("Could not open .order file: %s"),
				    order_in_name);
		gnome_error_dialog(s);
		g_free(s);

		g_free(order_in_name);
		g_free(order_out_name);
		fclose(order_in_file);
		return;
	}

	while (fgets(buf, 255, order_in_file)) {
		g_strchomp (buf);  /* remove trailing '\n' */
		if (strcmp(buf, file))
			fprintf(order_out_file, "%s\n", buf);
	}

	fclose(order_out_file);
	fclose(order_in_file);

	if (rename(order_out_name, order_in_name) == -1) {
		char *s;
		s = g_strdup_printf(_("Could not rename tmp file %s"),
				    order_out_name);
		gnome_error_dialog(s);
		g_free(s);
	}

	g_free(order_out_name);
	g_free(order_in_name);
}

static PanelWidget *
get_panel_from_menu_data(GtkWidget *menu)
{
	g_return_val_if_fail (menu != NULL, NULL);
	g_return_val_if_fail (GTK_IS_MENU(menu) || GTK_IS_MENU_ITEM(menu),
			      NULL);

	if(GTK_IS_MENU_ITEM(menu))
		menu = menu->parent;

	while(menu) {
		PanelWidget *panel = gtk_object_get_data(GTK_OBJECT(menu),
							 "menu_panel");
		if(panel) {
			if(IS_PANEL_WIDGET(panel))
				return panel;
			else
				g_warning("Menu is on crack");
		}
		menu = GTK_MENU_SHELL(menu)->parent_menu_shell;
	}
	g_warning("Something went quite terribly wrong and we can't "
		  "find where this menu belongs");
	return panels->data;
}


static void
add_app_to_panel (GtkWidget *widget, char *item_loc)
{
	PanelWidget *panel = get_panel_from_menu_data (widget);
	load_launcher_applet(item_loc, panel, 0, FALSE);
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
	char *p;
	char *filename = NULL;
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

	if(!load_drawer_applet(-1, pixmap_name, subdir_name, panel, pos, FALSE) ||
	   !applets_last) {
		g_warning("Can't load a drawer");
		return;
	}
	info = applets_last->data;
	g_assert(info!=NULL);
	if(info->type != APPLET_DRAWER) {
		g_warning("Something weird happened and we didn't get a drawer");
		return;
	}
	
	drawer = info->data;
	g_assert(drawer);
	newpanel = PANEL_WIDGET(BASEP_WIDGET(drawer->drawer)->panel);
	
	list = get_files_from_menudir(dirname);
	for(li = list; li!= NULL; li = g_slist_next(li)) {
		struct stat s;
		GnomeDesktopEntry *dentry;

		g_free (filename);
		g_free(li->data);

		filename = g_concat_dir_and_file(dirname, li->data);

		if (stat (filename, &s) != 0)
			continue;
		if (S_ISDIR (s.st_mode)) {
			add_drawers_from_dir(filename,NULL,INT_MAX/2,
					     newpanel);
			continue;
		}
			
		p = strrchr(filename,'.');
		if (p && (strcmp(p,".desktop")==0 || 
			  strcmp(p,".kdelnk")==0)) {
			/*we load the applet at the right
			  side, that is end of the drawer*/
			dentry = gnome_desktop_entry_load (filename);
			if (dentry)
				load_launcher_applet_full (filename,
							   dentry,
							   newpanel,
							   INT_MAX/2,
							   FALSE);
		}
	}
	g_free(filename);
	g_slist_free(list);
}

/*add a drawer with the contents of a menu to the panel*/
static void
add_menudrawer_to_panel(GtkWidget *w, gpointer data)
{
	MenuFinfo *mf = data;
	PanelWidget *panel = get_panel_from_menu_data (w);
	g_return_if_fail(mf);
	
	add_drawers_from_dir(mf->menudir, mf->dir_name, 0, panel);
}

static void
add_menu_to_panel (GtkWidget *widget, gpointer data)
{
	char *menudir = data;
	PanelWidget *panel;
	int flags = MAIN_MENU_SYSTEM_SUB | MAIN_MENU_USER_SUB |
		MAIN_MENU_APPLETS_SUB | MAIN_MENU_PANEL_SUB |
		MAIN_MENU_DESKTOP_SUB;
	
	/*guess redhat menus*/
	if (g_file_exists(REDHAT_MENUDIR))
		flags |= MAIN_MENU_REDHAT_SUB;

	/*guess KDE menus*/
	if(g_file_exists(kde_menudir))
		flags |= MAIN_MENU_KDE_SUB;

	/*guess debian menus*/
	if (g_file_exists(DEBIAN_MENUDIR))
		flags |= MAIN_MENU_DEBIAN_SUB;

	panel = get_panel_from_menu_data (widget);

	if(menudir)
		load_menu_applet(menudir, flags, panel, 0, FALSE);
	else
		load_menu_applet(NULL, flags, panel, 0, FALSE);
}

static void
setup_menu_panel(GtkWidget *menu)
{
	PanelWidget *menu_panel = gtk_object_get_data(GTK_OBJECT(menu),
						      "menu_panel");
	if(!menu_panel) {
		menu_panel = get_panel_from_menu_data(menu);
		gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);
	}
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
					   GDK_BUTTON_PRESS_MASK |
					   GDK_BUTTON_RELEASE_MASK |
					   GDK_ENTER_NOTIFY_MASK |
					   GDK_LEAVE_NOTIFY_MASK,
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
	GnomeDesktopEntry *dentry;
	GList *types = NULL;
	
	g_return_if_fail(item_loc!=NULL);

	dentry = gnome_desktop_entry_load(item_loc);
	/* We'll screw up a KDE menu entry if we edit it */
	if (dentry && dentry->is_kde) {
		gnome_desktop_entry_free (dentry);
		return;
	}

	dialog = gnome_property_box_new();
	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "desktop_entry_properties","Panel");
	gtk_window_set_title(GTK_WINDOW(dialog), _("Desktop entry properties"));
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	o = gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(GNOME_PROPERTY_BOX(dialog)->notebook));
	types = g_list_append(types, "Application");
	types = g_list_append(types, "URL");
	types = g_list_append(types, "PanelApplet");
	gtk_combo_set_popdown_strings(GTK_COMBO(GNOME_DENTRY_EDIT(o)->type_combo), types);
	g_list_free(types);

	/*item loc will be alive all this time*/
	gtk_object_set_data(o,"location",item_loc);

	if(dentry) {
		gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(o),dentry);
		gnome_desktop_entry_free(dentry);
	}

	gtk_signal_connect_object(GTK_OBJECT(o), "changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(dialog));

	gtk_signal_connect(GTK_OBJECT(dialog), "apply",
			   GTK_SIGNAL_FUNC(dentry_apply_callback),
			   o);
	gtk_widget_show(dialog);
}

static void
edit_direntry(GtkWidget *widget, MenuFinfo *mf)
{
	GtkWidget *dialog;
	GtkObject *o;
	char *dirfile = g_concat_dir_and_file(mf->menudir, ".directory");
	GnomeDesktopEntry *dentry;
	GList *types = NULL;

	dentry = gnome_desktop_entry_load_unconditional(dirfile);
	/* We'll screw up a KDE menu entry if we edit it */
	if (dentry && dentry->is_kde) {
		gnome_desktop_entry_free (dentry);
		return;
	}

	dialog = gnome_property_box_new();
	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "desktop_entry_properties","Panel");
	gtk_window_set_title(GTK_WINDOW(dialog), _("Desktop entry properties"));
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	o = gnome_dentry_edit_new_notebook(GTK_NOTEBOOK(GNOME_PROPERTY_BOX(dialog)->notebook));
	types = g_list_append(types, "Directory");
	gtk_combo_set_popdown_strings(GTK_COMBO(GNOME_DENTRY_EDIT(o)->type_combo), types);
	g_list_free(types);

	if (dentry) {
		gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(o), dentry);
		gtk_object_set_data_full(o,"location",
					 g_strdup(dentry->location),
					 (GtkDestroyNotify)g_free);
		gnome_desktop_entry_free(dentry);
		g_free(dirfile);
	} else {
		dentry = g_new0(GnomeDesktopEntry, 1);
		dentry->name =
			mf->dir_name?g_strdup(mf->dir_name):g_strdup("Menu");
		dentry->type = g_strdup("Directory");
		/*we don't have to free dirfile here it will be freed as if
		  we had strduped it here*/
		gtk_object_set_data_full(o,"location",dirfile,
					 (GtkDestroyNotify)g_free);
		gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(o), dentry);
		gnome_desktop_entry_free(dentry);
	}

	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->exec_entry, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->tryexec_entry, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->doc_entry, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->type_combo, FALSE);
	gtk_widget_set_sensitive(GNOME_DENTRY_EDIT(o)->terminal_button, FALSE);

	gtk_signal_connect_object(o, "changed",
				  GTK_SIGNAL_FUNC(gnome_property_box_changed),
				  GTK_OBJECT(dialog));

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
	GtkWidget *menuitem;
	int applet;
};

static int
is_ext(char *f, char *ext)
{
	char *p = strrchr(f,'.');
	if(!p) return FALSE;
	else if(strcmp(p,ext) == 0) return TRUE;
	else return FALSE;
}

static void
show_item_menu(GtkWidget *item, GdkEventButton *bevent, ShowItemMenu *sim)
{
	GtkWidget *menuitem;

	if(!sim->menu) {
		sim->menu = gtk_menu_new ();

		gtk_object_set_data (GTK_OBJECT(sim->menu), "menu_panel",
				     get_panel_from_menu_data (sim->menuitem));
		
		gtk_signal_connect(GTK_OBJECT(sim->menu),"deactivate",
				   GTK_SIGNAL_FUNC(restore_grabs),
				   item);

		if(sim->type == 1) {
			char *tmp;
			menuitem = gtk_menu_item_new ();
			if(!sim->applet)
				setup_menuitem (menuitem, 0,
						_("Add this launcher to panel"));
			else
				setup_menuitem (menuitem, 0,
						_("Add this applet as a launcher to panel"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_app_to_panel),
					   sim->item_loc);
			
			if(!sim->applet) {
				menuitem = gtk_menu_item_new ();
				setup_menuitem (menuitem, 0,
						_("Add this to personal menu"));
				gtk_menu_append (GTK_MENU (sim->menu), menuitem);
				gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
						   GTK_SIGNAL_FUNC(add_app_to_personal),
						   sim->item_loc);
				/*ummmm slightly ugly but should work 99% of time*/
				if(strstr(sim->item_loc,"/.gnome/apps/"))
					gtk_widget_set_sensitive(menuitem,FALSE);
			}

			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Remove this item"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
					    GTK_SIGNAL_FUNC (remove_menuitem),
					    sim->item_loc);
			tmp = g_dirname(sim->item_loc);
			if(access(tmp,W_OK)!=0)
				gtk_widget_set_sensitive(menuitem,FALSE);
			g_free(tmp);
			gtk_signal_connect_object(GTK_OBJECT(menuitem),
						  "activate",
						  GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
						  GTK_OBJECT(item->parent));
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
					   sim->mf->menudir);
			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add this to personal menu"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_app_to_personal),
					   sim->mf->menudir);
			/*ummmm slightly ugly but should work 99% of time*/
			if(strstr(sim->mf->menudir,"/.gnome/apps"))
				gtk_widget_set_sensitive(menuitem,FALSE);

			menuitem = gtk_menu_item_new ();
			setup_menuitem (menuitem, 0,
					_("Add new item to this menu"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			/*when activated we must pop down the first menu*/
			gtk_signal_connect_object(GTK_OBJECT(menuitem),
						  "activate",
						  GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
						  GTK_OBJECT(item->parent));
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_new_app_to_menu),
					   sim->mf->menudir);
			if(access(sim->mf->menudir,W_OK)!=0)
				gtk_widget_set_sensitive(menuitem,FALSE);
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
		setup_menuitem (sim->prop_item, 0, _("Properties..."));
		gtk_menu_append (GTK_MENU (sim->menu), sim->prop_item);
	}
	
	gtk_widget_set_sensitive(sim->prop_item,FALSE);
	if(sim->item_loc &&
	   /*A HACK: but it works, don't have it edittable if it's redhat
	     menus as they are auto generated!*/
	   !strstr(sim->item_loc,"/.gnome/apps-redhat/") &&
	   /*if it's a kdelnk file, don't let it be editted*/
	   !is_ext(sim->item_loc,".kdelnk") &&
	   access(sim->item_loc,W_OK)==0) {
#ifdef PANEL_DEBUG
		puts(sim->item_loc);
#endif
		/*file exists and is writable, we're in bussines*/
		gtk_widget_set_sensitive(sim->prop_item,TRUE);
	} else if(!sim->item_loc || errno==ENOENT) {
		/*the dentry isn't there, check if we can write the
		  directory*/
		if(access(sim->mf->menudir,W_OK)==0 &&
		   /*A HACK: but it works, don't have it edittable if it's redhat
		     menus as they are auto generated!*/
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
  
  if (xgrab_shell && !GTK_MENU(xgrab_shell)->torn_off)
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
	gchar *uri_list = g_strconcat ("file:", item_loc, "\r\n", NULL);
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)uri_list,
				strlen(uri_list));
	g_free(uri_list);
}

static void  
drag_data_get_string_cb (GtkWidget *widget, GdkDragContext     *context,
		      GtkSelectionData   *selection_data, guint info,
		      guint time, char *string)
{
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)string,
				strlen(string));
}

static void
setup_title_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title,
		      MenuFinfo *mf)
{
	GtkWidget *label, *hbox=NULL, *align;
	IconSize size = global_config.use_large_icons 
		? MEDIUM_ICON_SIZE : SMALL_ICON_SIZE;
	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);

	if (gnome_preferences_get_menus_have_icons () ||
	    global_config.show_dot_buttons) {
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (hbox);
		gtk_container_add (GTK_CONTAINER (menuitem), hbox);
	} else
		gtk_container_add (GTK_CONTAINER (menuitem), label);
	
	if (gnome_preferences_get_menus_have_icons ()) {
		align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		gtk_widget_show (align);
		gtk_container_set_border_width (GTK_CONTAINER (align), 1);

		if (pixmap) {
			gtk_container_add (GTK_CONTAINER (align), pixmap);
			gtk_widget_set_usize (align, size + 2, size - 4);
			gtk_widget_show (pixmap);
		} else
			gtk_widget_set_usize (align, size + 2, size - 4);

		gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
	}

	if (gnome_preferences_get_menus_have_icons () ||
	    global_config.show_dot_buttons)
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	if(mf) {
		ShowItemMenu *sim = g_new0(ShowItemMenu,1);
		sim->mf = mf;/*make sure you don't free this,
			       it's not ours!*/
		sim->type = 0;
		sim->menuitem = menuitem;
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
setup_full_menuitem_with_size (GtkWidget *menuitem, GtkWidget *pixmap, 
			       char *title, char *item_loc, gboolean applet, 
			       IconSize icon_size)
			       
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "text/uri-list", 0, 0 }
	};

	GtkWidget *label, *hbox=NULL, *align;

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);
	
	if (gnome_preferences_get_menus_have_icons () ||
	    global_config.show_dot_buttons) {
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (hbox);
		gtk_container_add (GTK_CONTAINER (menuitem), hbox);
	} else
		gtk_container_add (GTK_CONTAINER (menuitem), label);
	
	if (gnome_preferences_get_menus_have_icons ()) {
		align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		gtk_widget_show (align);
		gtk_container_set_border_width (GTK_CONTAINER (align), 1);

		if (pixmap) {
			gtk_container_add (GTK_CONTAINER (align), pixmap);
			gtk_widget_set_usize (align, icon_size + 2,
					      icon_size - 4);
			gtk_widget_show (pixmap);
		} else
			gtk_widget_set_usize (align, icon_size + 2,
					      icon_size - 4);

		gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
	} else if(pixmap)
		gtk_widget_destroy(pixmap);

	if (gnome_preferences_get_menus_have_icons () ||
	    global_config.show_dot_buttons)
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	if(item_loc) {
		ShowItemMenu *sim = g_new0(ShowItemMenu,1);
		sim->item_loc = item_loc; /*make sure you don't free this,
					    it's not ours!*/
		sim->type = 1;
		sim->applet = applet;
		sim->menuitem = menuitem;
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

		/*applets have their own drag'n'drop*/
		if(!applet) {
			gtk_drag_source_set(menuitem,
					    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
					    menu_item_targets, 1,
					    GDK_ACTION_COPY);

			gtk_signal_connect(GTK_OBJECT(menuitem), "drag_data_get",
					   drag_data_get_menu_cb, item_loc);
			gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
					   drag_end_menu_cb, NULL);
		}
	}

	gtk_widget_show (menuitem);
}

static void
setup_full_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title,
		     char *item_loc, gboolean applet)
{
	setup_full_menuitem_with_size (menuitem, pixmap, title, 
				       item_loc, applet, SMALL_ICON_SIZE);
}

void
setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title)
{
	setup_full_menuitem(menuitem, pixmap, title, NULL, FALSE);
}

static void
setup_menuitem_with_size (GtkWidget *menuitem, GtkWidget *pixmap,
			  char *title, int icon_size)
{
	setup_full_menuitem_with_size (menuitem, pixmap, title, NULL,
				       FALSE, icon_size);
}

static void
setup_directory_drag (GtkWidget *menuitem, char *directory)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-directory", 0, 0 }
	};

	gtk_drag_source_set(menuitem,
			    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			    menu_item_targets, 1,
			    GDK_ACTION_COPY);
	
	gtk_signal_connect_full(GTK_OBJECT(menuitem), "drag_data_get",
			   GTK_SIGNAL_FUNC (drag_data_get_string_cb), NULL,
			   g_strdup (directory), (GtkDestroyNotify)g_free,
			   FALSE, FALSE);
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
			   GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);
}

static void
setup_internal_applet_drag (GtkWidget *menuitem, char *applet_type)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	
	if(!applet_type)
		return;
	
	gtk_drag_source_set(menuitem,
			    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			    menu_item_targets, 1,
			    GDK_ACTION_COPY);
	
	gtk_signal_connect_full(GTK_OBJECT(menuitem), "drag_data_get",
			   GTK_SIGNAL_FUNC (drag_data_get_string_cb), NULL,
			   g_strdup (applet_type), (GtkDestroyNotify)g_free,
			   FALSE, FALSE);
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
			   GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);

}

static void
setup_applet_drag (GtkWidget *menuitem, char *goad_id)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet", 0, 0 }
	};
	
	if(!goad_id)
		return;
	
	gtk_drag_source_set(menuitem,
			    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			    menu_item_targets, 1,
			    GDK_ACTION_COPY);
	
	/*note: goad_id should be alive long enough!!*/
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_data_get",
			   GTK_SIGNAL_FUNC (drag_data_get_string_cb),
			   goad_id);
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
			   GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);

}

static void
add_drawer_to_panel (GtkWidget *widget, gpointer data)
{
	load_drawer_applet(-1, NULL, NULL,
			   get_panel_from_menu_data(widget), 0, FALSE);
}

static void
add_logout_to_panel (GtkWidget *widget, gpointer data)
{
	load_logout_applet(get_panel_from_menu_data(widget), 0, FALSE);
}

static void
add_lock_to_panel (GtkWidget *widget, gpointer data)
{
	load_lock_applet(get_panel_from_menu_data(widget), 0, FALSE);
}
static void
try_add_status_to_panel (GtkWidget *widget, gpointer data)
{
	if(!load_status_applet(get_panel_from_menu_data(widget),
			       0, FALSE)) {
		GtkWidget *mbox;
		mbox = gnome_message_box_new(_("You already have a status "
					       "dock on the panel. You can "
					       "only have one"),
					     GNOME_MESSAGE_BOX_INFO,
					     GNOME_STOCK_BUTTON_CLOSE,
					     NULL);
		gtk_window_set_wmclass(GTK_WINDOW(mbox),
				       "no_more_status_dialog","Panel");
		gtk_widget_show_all(mbox);
	}
}

static void
add_applet (GtkWidget *w, char *item_loc)
{
	GnomeDesktopEntry *ii;
	char *goad_id;

	ii = gnome_desktop_entry_load(item_loc);
	if(!ii) {
		gnome_error_dialog(_("Can't load entry"));
		return;
	}

	goad_id = get_applet_goad_id_from_dentry(ii);
	gnome_desktop_entry_free(ii);
	
	if(!goad_id) {
		gnome_error_dialog(_("Can't get goad_id from desktop entry!"));
		return;
	}
	load_extern_applet(goad_id, NULL,
			   get_panel_from_menu_data(w),
			   0, FALSE, FALSE);

	g_free(goad_id);
}

static void
destroy_mf(MenuFinfo *mf)
{
	if(mf->fr) {
		DirRec *dr = (DirRec *)mf->fr;
		dr->mfl = g_slist_remove(dr->mfl,mf);
	}
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

static GtkWidget * create_menu_at (GtkWidget *menu, char *menudir, 
				   gboolean applets, char *dir_name,
				   char *pixmap_name, gboolean fake_submenus,
				   gboolean force);
static GtkWidget * create_menu_at_fr (GtkWidget *menu, FileRec *fr,
				      gboolean applets, char *dir_name,
				      char *pixmap_name, gboolean fake_submenus,
				      gboolean force);

/*reread the applet menu, not a submenu*/
static void
check_and_reread_applet(Menu *menu, gboolean main_menu)
{
	GSList *mfl = gtk_object_get_data(GTK_OBJECT(menu->menu), "mf");
	GSList *list;
	gboolean need_reread = FALSE;

	/*we shouldn't warn, this is more for debugging anyway,
	  and nowdays we do have menus that don't have one, this
	  however might be needed for further debugging*/
	/*if(!mfl)
	  g_warning("Weird menu doesn't have mf entry");*/

	/*check if we need to reread this*/
	for(list = mfl; list != NULL; list = g_slist_next(list)) {
		MenuFinfo *mf = list->data;
		if(mf->fake_menu ||
		   mf->fr == NULL) {
			if(mf->fr)
				mf->fr = fr_replace(mf->fr);
			else
				mf->fr = fr_get_dir(mf->menudir);
			need_reread = TRUE;
		} else {
			FileRec *fr;
			fr = fr_check_and_reread(mf->fr);
			if(fr!=mf->fr ||
			   fr == NULL) {
				need_reread = TRUE;
				mf->fr = fr;
			}
		}
	}

	if(need_reread) {
		/*that will be destroyed in add_menu_widget*/
		if(main_menu)
			add_menu_widget(menu, NULL, NULL, main_menu, TRUE);
		else {
			GSList *dirlist = NULL;
			for(list = mfl; list != NULL;
			    list = g_slist_next(list)) {
				MenuFinfo *mf = list->data;
				dirlist = g_slist_append(dirlist,
							 g_strdup(mf->menudir));
			}
			add_menu_widget(menu, NULL, dirlist, main_menu, TRUE);

			g_slist_foreach(dirlist, (GFunc)g_free, NULL);
			g_slist_free(dirlist);
		}
	}
}

/* XXX: hmmm stolen GTK code, the gtk_menu_reposition only calls
   gtk_menu_position if the widget is drawable, but that's not the
   case when we want to do it*/
static void
gtk_menu_position (GtkMenu *menu)
{
  GtkWidget *widget;
  GtkRequisition requisition;
  gint x, y;
 
  g_return_if_fail (menu != NULL);
  g_return_if_fail (GTK_IS_MENU (menu));

  widget = GTK_WIDGET (menu);

  gdk_window_get_pointer (NULL, &x, &y, NULL);

  /* We need the requisition to figure out the right place to
   * popup the menu. In fact, we always need to ask here, since
   * if one a size_request was queued while we weren't popped up,
   * the requisition won't have been recomputed yet.
   */
  gtk_widget_size_request (widget, &requisition);
      
  if (menu->position_func)
    (* menu->position_func) (menu, &x, &y, menu->position_func_data);
  else
    {
      gint screen_width;
      gint screen_height;
      
      screen_width = gdk_screen_width ();
      screen_height = gdk_screen_height ();
	  
      x -= 2;
      y -= 2;
      
      if ((x + requisition.width) > screen_width)
	x -= ((x + requisition.width) - screen_width);
      if (x < 0)
	x = 0;
      if ((y + requisition.height) > screen_height)
	y -= ((y + requisition.height) - screen_height);
      if (y < 0)
	y = 0;
    }
  
  gtk_widget_set_uposition (GTK_MENU_SHELL (menu)->active ?
			        menu->toplevel : menu->tearoff_window, 
			    x, y);
}

/* Stolen from GTK+
 * Reparent the menu, taking care of the refcounting
 */
static void 
gtk_menu_reparent (GtkMenu      *menu, 
		   GtkWidget    *new_parent, 
		   gboolean      unrealize)
{
  GtkObject *object = GTK_OBJECT (menu);
  GtkWidget *widget = GTK_WIDGET (menu);
  gboolean was_floating = GTK_OBJECT_FLOATING (object);

  gtk_object_ref (object);
  gtk_object_sink (object);

  if (unrealize)
    {
      gtk_object_ref (object);
      gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      gtk_object_unref (object);
    }
  else
    gtk_widget_reparent (GTK_WIDGET (menu), new_parent);
  gtk_widget_set_usize (new_parent, -1, -1);
  
  if (was_floating)
    GTK_OBJECT_SET_FLAGS (object, GTK_FLOATING);
  else
    gtk_object_unref (object);
}

/*stolen from GTK+ */
static gint
gtk_menu_window_event (GtkWidget *window,
		       GdkEvent  *event,
		       GtkWidget *menu)
{
  gboolean handled = FALSE;

  gtk_widget_ref (window);
  gtk_widget_ref (menu);

  switch (event->type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      gtk_widget_event (menu, event);
      handled = TRUE;
      break;
    default:
      break;
    }

  gtk_widget_unref (window);
  gtk_widget_unref (menu);

  return handled;
}

static gulong wmclass_number = 0;
static char *
get_unique_tearoff_wmclass(void)
{
	static char buf[256];
	g_snprintf(buf,256,"panel_tearoff_%lu",wmclass_number++);
	return buf;
}

static void
show_tearoff_menu(GtkWidget *menu, char *title, gboolean cursor_position,
		  int x, int y, char *wmclass)
{
	GtkWidget *win;
	win = GTK_MENU(menu)->tearoff_window =
		gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(win),
			       wmclass, "Panel");
	gtk_widget_set_app_paintable(win, TRUE);
	gtk_signal_connect(GTK_OBJECT(win),  "event",
			   GTK_SIGNAL_FUNC(gtk_menu_window_event), 
			   GTK_OBJECT(menu));
	gtk_widget_realize(win);
	      
	gdk_window_set_title(win->window,
			     title);
	
	gdk_window_set_decorations(win->window, 
				   GDK_DECOR_ALL |
				   GDK_DECOR_RESIZEH |
				   GDK_DECOR_MINIMIZE |
				   GDK_DECOR_MAXIMIZE);
	gtk_window_set_policy(GTK_WINDOW(win),
			      FALSE, FALSE, TRUE);
	gtk_menu_reparent(GTK_MENU(menu), win,
			  FALSE);
	/* set sticky so that we mask the fact that we have no clue
	   how to restore non sticky windows */
	gnome_win_hints_set_state(win, gnome_win_hints_get_state(win) |
				  WIN_STATE_STICKY);
	
	GTK_MENU(menu)->torn_off = TRUE;

	if(cursor_position)
		gtk_menu_position(GTK_MENU(menu));
	else
		gtk_widget_set_uposition(win,x,y);

	gtk_widget_show(GTK_WIDGET(menu));
	gtk_widget_show(win);
}

static void
tearoff_destroyed(GtkWidget *tearoff, TearoffMenu *tm)
{
	tearoffs = g_slist_remove(tearoffs,tm);
	g_free(tm->title);
	g_free(tm->wmclass);
	g_free(tm->special);
	g_free(tm);
}

static void
tearoff_new_menu(GtkWidget *item, GtkWidget *menuw)
{
	GSList *mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");
	GSList *list;
	GtkWidget *menu;
	GString *title;
	TearoffMenu *tm;
	char *wmclass;
	PanelWidget *menu_panel;
	
	if(!mfl)
		return;

	menu = gtk_menu_new();
	gtk_signal_connect(GTK_OBJECT(menu), "show",
			   GTK_SIGNAL_FUNC(setup_menu_panel), NULL);

	menu_panel = get_panel_from_menu_data(menuw);

	/*set the panel to use as the data*/
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);

	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroy),
		      GTK_OBJECT(menu));
	
	title = g_string_new("");

	for(list = mfl; list != NULL; list = g_slist_next(list)) {
		MenuFinfo *mf = list->data;

		menu = create_menu_at_fr(menu,
					 mf->fr,
					 mf->applets,
					 mf->dir_name,
					 mf->pixmap_name,
					 TRUE,
					 FALSE);
		
		if(list!=mfl)
			g_string_append_c(title,' ');
		g_string_append(title,mf->dir_name);
	}

	wmclass = get_unique_tearoff_wmclass();
	show_tearoff_menu(menu,title->str,TRUE,0,0,wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = gtk_object_get_data(GTK_OBJECT(menu), "mf");
	tm->special = NULL;
	tm->title = title->str;
	tm->wmclass = g_strdup(wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);

	g_string_free(title,FALSE);

	need_complete_save = TRUE;
}

static void
add_tearoff(GtkMenu *menu)
{
	GtkWidget *w;

	if (!gnome_preferences_get_menus_have_tearoff ()){
		g_warning ("Adding tearoff when tearoffs are disabled");
		return;
	}

	w = tearoff_item_new();
	gtk_widget_show(w);
	gtk_menu_prepend(menu,w);
	
	gtk_signal_connect(GTK_OBJECT(w),"activate",
			   GTK_SIGNAL_FUNC(tearoff_new_menu),
			   menu);
}


static void
submenu_to_display(GtkWidget *menuw, GtkMenuItem *menuitem)
{
	GSList *mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");
	GSList *list;
	int need_reread = FALSE;

	/*if(!mfl)
	  g_warning("Weird menu doesn't have mf entry");*/

	if (GTK_MENU(menuw)->torn_off)
		return;
	
	/*check if we need to reread this*/
	for(list = mfl; list != NULL; list = g_slist_next(list)) {
		MenuFinfo *mf = list->data;
		if(mf->fake_menu ||
		   mf->fr == NULL) {
			if(mf->fr)
				mf->fr = fr_replace(mf->fr);
			else
				mf->fr = fr_get_dir(mf->menudir);
			need_reread = TRUE;
		} else {
			FileRec *fr;
			fr = fr_check_and_reread(mf->fr);
			if(fr!=mf->fr ||
			   fr == NULL) {
				need_reread = TRUE;
				mf->fr = fr;
			}
		}
	}

	/*this no longer constitutes a bad hack, now it's purely cool :)*/
	if(need_reread) {
		while(GTK_MENU_SHELL(menuw)->children)
			gtk_widget_destroy(GTK_MENU_SHELL(menuw)->children->data);
#ifdef TEAROFF_MENUS
		if (gnome_preferences_get_menus_have_tearoff ())
			add_tearoff(GTK_MENU(menuw));
#endif

		gtk_object_set_data(GTK_OBJECT(menuw), "mf",NULL);
		for(list = mfl; list != NULL;
		    list = g_slist_next(list)) {
			MenuFinfo *mf = list->data;

			menuw = create_menu_at_fr(menuw,
						  mf->fr,
						  mf->applets,
						  mf->dir_name,
						  mf->pixmap_name,
						  TRUE,
						  FALSE);
			destroy_mf(mf);
		}
		g_slist_free(mfl);

		gtk_menu_position(GTK_MENU(menuw));
	}
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
	gtk_signal_connect(GTK_OBJECT(menu), "show",
			   setup_menu_panel, NULL);
	
	mf = g_new0(MenuFinfo,1);
	mf->menudir = g_strdup(menudir);
	mf->applets = applets;
	mf->dir_name = dir_name?g_strdup(dir_name):NULL;
	mf->pixmap_name = pixmap_name?g_strdup(pixmap_name):NULL;
	mf->fake_menu = TRUE;
	mf->fr = NULL;
	
	list = g_slist_prepend(NULL,mf);
	gtk_object_set_data(GTK_OBJECT(menu),"mf",list);
	
	gtk_signal_connect(GTK_OBJECT(menu),"destroy",
			   GTK_SIGNAL_FUNC(menu_destroy),NULL);
	
	return menu;
}

static void
create_menuitem(GtkWidget *menu,
		FileRec *fr,
		gboolean applets,
		gboolean fake_submenus,
		gboolean *add_separator,
		int *first_item)
{
	GtkWidget *menuitem, *sub, *pixmap;
	IconSize size = global_config.use_large_icons
		? MEDIUM_ICON_SIZE : SMALL_ICON_SIZE;
	char *itemname;
	
	g_return_if_fail(fr != NULL);

	if(fr->type == FILE_REC_EXTRA)
		return;


	if(fr->type == FILE_REC_FILE && applets &&
	   !fr->goad_id) {
		g_warning(_("Can't get goad_id for applet, ignoring it"));
		return;
	}

	sub = NULL;
	if(fr->fullname) {
		itemname = g_strdup(fr->fullname);
	} else {
		char *p;
		itemname = g_strdup(g_basename(fr->name));
		p = strrchr(itemname,'.');
		if(p) *p = '\0';
	}

	if(fr->type == FILE_REC_DIR) {
		if(fake_submenus)
			sub = create_fake_menu_at (fr->name,
						   applets,
						   itemname,
						   fr->icon);
		else
			sub = create_menu_at_fr (NULL, fr,
						 applets,
						 itemname,
						 fr->icon,
						 fake_submenus,
						 FALSE);

		if (!sub) {
			g_free(itemname);
			return;
		}
	}

	menuitem = gtk_menu_item_new ();
	if (sub) {
		gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), sub);
		gtk_signal_connect(GTK_OBJECT(sub),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}

	pixmap = NULL;
	if (gnome_preferences_get_menus_have_icons ()) {
		if (fr->icon && g_file_exists (fr->icon)) {
			pixmap = gnome_stock_pixmap_widget_at_size (NULL, fr->icon,
								    size, size);
			if (pixmap)
				gtk_widget_show (pixmap);
		}
	}

	if(!sub && strstr(fr->name,"/applets/") &&  fr->goad_id) {
		setup_applet_drag (menuitem, fr->goad_id);
		setup_full_menuitem_with_size (menuitem, pixmap,itemname,
					       fr->name, TRUE, size);
	} else {
		/*setup the menuitem, pass item_loc if this is not
		  a submenu, so that the item can be added,
		  we can be sure that the FileRec will live that long,
		  (when it dies, the menu will not be used again, it will
		  be recreated at the next available opportunity)*/
		setup_full_menuitem_with_size (menuitem, pixmap,itemname,
					       sub?NULL:fr->name, FALSE, size);
	}

	if(*add_separator) {
		add_menu_separator(menu);
		(*first_item)++;
		*add_separator = FALSE;
	}
	gtk_menu_append (GTK_MENU (menu), menuitem);

	if(!sub) {
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    applets?
				    GTK_SIGNAL_FUNC(add_applet):
				    GTK_SIGNAL_FUNC(activate_app_def),
				    fr->name);
	}
	g_free(itemname);
}

static GtkWidget *
create_menu_at (GtkWidget *menu,
		char *menudir,
		gboolean applets,
		char *dir_name,
		char *pixmap_name,
		gboolean fake_submenus,
		gboolean force)
{
	return create_menu_at_fr(menu, fr_get_dir(menudir),
				 applets, dir_name, pixmap_name,
				 fake_submenus, force);
}

static GtkWidget *
create_menu_at_fr (GtkWidget *menu,
		   FileRec *fr,
		   gboolean applets,
		   char *dir_name,
		   char *pixmap_name,
		   gboolean fake_submenus,
		   gboolean force)
{	
	GSList *li;
	GSList *mfl = NULL;
	gboolean add_separator = FALSE;
	int first_item = 0;
	GtkWidget *menuitem;
	MenuFinfo *mf = NULL;
	DirRec *dr = (DirRec *)fr;
	GtkWidget *pixmap;
	char *menu_name;
	IconSize size = global_config.use_large_icons
		? MEDIUM_ICON_SIZE : SMALL_ICON_SIZE;

	g_return_val_if_fail(!(fr&&fr->type!=FILE_REC_DIR),menu);
	
	if(!force && !fr)
		return menu;
	
	/*get this info ONLY if we haven't gotten it already*/
	if(!dir_name)
		dir_name = (fr&&fr->fullname)?fr->fullname:_("Menu");
	if(!pixmap_name)
		pixmap_name = (fr&&fr->icon)?fr->icon:gnome_folder;
	
	if(!menu) {
		menu = gtk_menu_new ();
		gtk_signal_connect(GTK_OBJECT(menu), "show",
				   setup_menu_panel, NULL);
#ifdef TEAROFF_MENUS
		if (gnome_preferences_get_menus_have_tearoff ()) {
			add_tearoff(GTK_MENU(menu));
			first_item++;
		}
#endif
		gtk_signal_connect(GTK_OBJECT(menu),"destroy",
				   GTK_SIGNAL_FUNC(menu_destroy),NULL);
	} else {
		first_item = g_list_length(GTK_MENU_SHELL(menu)->children);
		mfl = gtk_object_get_data(GTK_OBJECT(menu), "mf");
		if(GTK_MENU_SHELL(menu)->children &&
		   !(GTK_MENU_SHELL(menu)->children->next == NULL &&
		     IS_TEAROFF_ITEM(GTK_MENU_SHELL(menu)->children->data)))
			add_separator = TRUE;
	}
	
	if(fr) {
		for(li = dr->recs; li!=NULL; li=g_slist_next(li)) {
			create_menuitem(menu,li->data,
					applets,fake_submenus,
					&add_separator,
					&first_item);
		}
	}

	mf = g_new0(MenuFinfo,1);
	mf->menudir = g_strdup(fr->name);
	mf->applets = applets;
	mf->dir_name = dir_name?g_strdup(dir_name):NULL;
	mf->pixmap_name = pixmap_name?g_strdup(pixmap_name):NULL;
	mf->fake_menu = FALSE;
	mf->fr = fr;
	if(fr) {
		DirRec *dr = (DirRec *)fr;
		dr->mfl = g_slist_prepend(dr->mfl,mf);
	}

	/*if we actually added anything*/
	if(first_item < g_list_length(GTK_MENU_SHELL(menu)->children)) {
		menuitem = gtk_menu_item_new();
		gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);
		gtk_widget_show(menuitem);
		gtk_widget_set_sensitive(menuitem,FALSE);
		menu_name = g_strdup(dir_name?dir_name:_("Menu"));
	} else {
		menu_name = g_strconcat(dir_name?dir_name:_("Menu"),_(" (empty)"),NULL);
	}


	pixmap = NULL;
	if (gnome_preferences_get_menus_have_icons ()) {
		if (pixmap_name) {
			pixmap = gnome_stock_pixmap_widget_at_size (NULL, pixmap_name,
								    size, size);
		}
		if (!pixmap && gnome_folder && g_file_exists (gnome_folder)) {
			pixmap = gnome_stock_pixmap_widget_at_size (NULL, gnome_folder,
								    size, size);
		}
	}

	if (pixmap)
		gtk_widget_show (pixmap);
	menuitem = gtk_menu_item_new();
	setup_title_menuitem(menuitem,pixmap,menu_name,mf);
	gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);

	g_free(menu_name);

	setup_directory_drag (menuitem, mf->menudir);

	/*add separator*/
	if(add_separator) {
		menuitem = gtk_menu_item_new();
		gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);
		gtk_widget_show(menuitem);
		gtk_widget_set_sensitive(menuitem,FALSE);
		add_separator = FALSE;
	}

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
	if(menu->menu)
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
	if(IS_BASEP_WIDGET(panel))
		BASEP_WIDGET(panel)->autohide_inhibit = FALSE;
	BUTTON_WIDGET(menu->button)->in_button = FALSE;
	BUTTON_WIDGET(menu->button)->ignore_leave = FALSE;
	button_widget_up(BUTTON_WIDGET(menu->button));
	menu->age = 0;
}

static GtkWidget *
create_applets_menu(GtkWidget *menu, gboolean fake_submenus)
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
	
	applet_menu = create_menu_at(menu, menudir, TRUE,
				     _("Applets"),
				     GNOME_STOCK_PIXMAP_EXEC,
				     fake_submenus, FALSE);
	g_free (menudir);
	return applet_menu;
}

static void
find_empty_pos_array (int posscore[3][3])
{
	GSList *li;
	int i,j;
	PanelData *pd;
	BasePWidget *basep;
	
	int tx, ty;
	int w, h;
	gfloat sw, sw2, sh, sh2;

	sw2 = 2 * (sw = gdk_screen_width () / 3);
	sh2 = 2 * (sh = gdk_screen_height () / 3);
	
	for(li=panel_list;li!=NULL;li=g_slist_next(li)) {
		pd = li->data;
		basep = BASEP_WIDGET (pd->panel);

		if(IS_DRAWER_WIDGET(basep))
			continue;

		basep_widget_get_pos (basep, &tx, &ty);
		basep_widget_get_size (basep, &w, &h);

		if (PANEL_WIDGET (basep->panel)->orient == PANEL_HORIZONTAL) {
			j = MIN (ty / sh, 2);
			ty = tx + w;
			if (tx < sw) posscore[0][j]++;
			if (tx < sw2 && ty > sw) posscore[1][j]++;
			if (ty > sw2) posscore[2][j]++;
		} else {
			i = MIN (tx / sw, 2);
			tx = ty + h;
			if (ty < sh) posscore[i][0]++;
			if (ty < sh2 && tx > sh) posscore[i][1]++;
			if (tx > sh2) posscore[i][2]++;
		}
	}
}

static void
find_empty_pos (gint16 *x, gint16 *y)
{
	int posscore[3][3] = { {0,0,0}, {0,512,0}, {0,0,0}};
	int i, j, lowi= 0, lowj = 0;

	find_empty_pos_array (posscore);

	for(j=2;j>=0;j--) {
		for (i=0;i<3;i++) {
			if(posscore[i][j]<posscore[lowi][lowj]) {
				lowi = i;
				lowj = j;
			}
		}
	}

	*x = ((float)lowi * gdk_screen_width ()) / 2.0;
	*y = ((float)lowj * gdk_screen_height ()) / 2.0;
}

static BorderEdge
find_empty_edge (void)
{
	int posscore[3][3] = { {0,0,0}, {0,512,0}, {0,0,0}};
	int escore [4] = { 0, 0, 0, 0};
	BorderEdge edge = BORDER_BOTTOM;
	int low=512, i;

	find_empty_pos_array (posscore);

	escore[BORDER_TOP] = posscore[0][0] + posscore[1][0] + posscore[2][0];
	escore[BORDER_RIGHT] = posscore[2][0] + posscore[2][1] + posscore[2][2];
	escore[BORDER_BOTTOM] = posscore[0][2] + posscore[1][2] + posscore[2][2];
	escore[BORDER_LEFT] = posscore[0][0] + posscore[0][1] + posscore[0][2];
	
	for (i=0; i<4; i++) {
		if (escore[i] < low) {
			edge = i;
			low = escore[i];
		}
	}
	return edge;
}

static void
create_new_panel(GtkWidget *w, gpointer data)
{
	PanelType type = GPOINTER_TO_INT(data);
	GdkColor bcolor = {0,0,0,1};
	gint16 x, y;
	GtkWidget *panel = NULL;
    
	g_return_if_fail (type != DRAWER_PANEL);

	switch(type) {
	case ALIGNED_PANEL: 
		find_empty_pos (&x, &y);
		panel = aligned_widget_new(ALIGNED_LEFT,
					   BORDER_TOP,
					   BASEP_EXPLICIT_HIDE,
					   BASEP_SHOWN,
					   SIZE_STANDARD,
					   TRUE,
					   TRUE,
					   PANEL_BACK_NONE,
					   NULL,
					   TRUE, FALSE, TRUE,
					   &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	case EDGE_PANEL: 
		panel = edge_widget_new(find_empty_edge (),
					BASEP_EXPLICIT_HIDE,
					BASEP_SHOWN,
					SIZE_STANDARD,
					TRUE,
					TRUE,
					PANEL_BACK_NONE,
					NULL,
					TRUE, FALSE, TRUE,
					&bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);	
		break;
	case SLIDING_PANEL:
		find_empty_pos (&x, &y);
		panel = sliding_widget_new (SLIDING_ANCHOR_LEFT, 0,
					    BORDER_TOP,
					    BASEP_EXPLICIT_HIDE,
					    BASEP_SHOWN,
					    SIZE_STANDARD,
					    TRUE, TRUE,
					    PANEL_BACK_NONE,
					    NULL, TRUE, FALSE, TRUE,
					    &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);	
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	case FLOATING_PANEL:
		find_empty_pos (&x, &y);
		panel = floating_widget_new (x, y,
					     PANEL_VERTICAL,
					     BASEP_EXPLICIT_HIDE,
					     BASEP_SHOWN,
					     SIZE_STANDARD,
					     TRUE, TRUE,
					     PANEL_BACK_NONE,
					     NULL, TRUE, FALSE, TRUE,
					     &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	default: break;
	}

	if (panel == NULL)
		return;
		
	panels_to_sync = TRUE;
}

static GtkWidget * create_add_panel_submenu (gboolean tearoff);

static void
add_panel_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	GtkWidget *menu = create_add_panel_submenu(FALSE);
	PanelWidget *menu_panel;

	menu_panel = get_panel_from_menu_data(w);

	/*set the panel to use as the data*/
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);
	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroy),
		      GTK_OBJECT(menu));

	show_tearoff_menu(menu, _("Create panel"),TRUE,0,0,wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Create panel"));
	tm->special = g_strdup("ADD_PANEL");
	tm->wmclass = g_strdup(wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

static GtkWidget *
create_add_panel_submenu (gboolean tearoff)
{
	GtkWidget *menu, *menuitem;

	menu = gtk_menu_new ();
	gtk_signal_connect(GTK_OBJECT(menu), "show",
			   setup_menu_panel, NULL);
	
	if(tearoff && gnome_preferences_get_menus_have_tearoff ()) {
		menuitem = tearoff_item_new();
		gtk_widget_show(menuitem);
		gtk_menu_prepend(GTK_MENU(menu),menuitem);
	
		gtk_signal_connect(GTK_OBJECT(menuitem),"activate",
				   GTK_SIGNAL_FUNC(add_panel_tearoff_new_menu),
				   NULL);
	}
 	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Edge panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(EDGE_PANEL));

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Aligned panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(ALIGNED_PANEL));

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Sliding panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(SLIDING_PANEL));
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Floating panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(FLOATING_PANEL));

	return menu;
}

static void
setup_menuitem_try_pixmap (GtkWidget *menuitem, char *try_file,
			   char *title, int icon_size)
{
	char *file;

	if (!gnome_preferences_get_menus_have_icons ()) {
		setup_menuitem (menuitem, NULL, title);
		return;
	}
	
	file = gnome_pixmap_file (try_file);
	if (!file) {
		g_warning (_("Cannot find pixmap file %s"), try_file);
		setup_menuitem (menuitem, NULL, title);
	} else
		setup_menuitem_with_size (menuitem,
					  gnome_stock_pixmap_widget_at_size(
						  NULL, file, 
						  icon_size, icon_size),
					  title, icon_size);
	g_free (file);
}
	  

static GtkWidget *
create_system_menu(GtkWidget *menu, gboolean fake_submenus, gboolean fake)
{
	char *menu_base = gnome_unconditional_datadir_file ("gnome/apps");
	char *menudir;

	menudir = g_concat_dir_and_file (menu_base, ".");
	g_free (menu_base);
	if (g_file_exists (menudir)) {
		if(!fake || menu) {
			menu = create_menu_at (menu, menudir, FALSE, _("Programs"),
					       "gnome-logo-icon-transparent.png",
					       fake_submenus, FALSE);
		} else {
			menu = create_fake_menu_at (menudir, FALSE,
						    _("Programs"),
						    "gnome-logo-icon-transparent.png");
		}
	} else {
		/* show an error dialog for this only once, then just
		   use g_warning */
		static int dialogs = 0;
		if(dialogs==0) {
			gnome_error_dialog(_("No system menus found!"));
			dialogs++;
		} else
			g_warning(_("No system menus found!"));
	}
	g_free (menudir); 	
	return menu;
}

static GtkWidget *
create_user_menu(char *title, char *dir, GtkWidget *menu, char *pixmap,
		 gboolean fake_submenus, gboolean force, gboolean fake)
{
	char *menu_base = gnome_util_home_file (dir);
	char *menudir = g_concat_dir_and_file (menu_base, ".");
	if (!g_file_exists (menudir))
		mkdir (menu_base, 0755);
	g_free (menu_base);
	
	if(!fake || menu) {
		menu = create_menu_at (menu, menudir, FALSE,
				       title, pixmap,
				       fake_submenus,
				       force);
	} else {
		menu = create_fake_menu_at (menudir, FALSE,
					    title, pixmap);
	}
	g_free (menudir); 
	return menu;
}

static GtkWidget *
create_debian_menu(GtkWidget *menu, gboolean fake_submenus, gboolean fake)
{
	if (!fake || menu) {
		menu = create_menu_at (menu, DEBIAN_MENUDIR, FALSE,
				       _("Debian menus"), NULL,
				       fake_submenus, FALSE);
	} else {
		menu = create_fake_menu_at (DEBIAN_MENUDIR, FALSE,
					    _("Debian menus"), NULL);
	}

	return menu;
}

static GtkWidget *
create_kde_menu(GtkWidget *menu, gboolean fake_submenus,
		gboolean force, gboolean fake)
{
	char *pixmap_name;

	pixmap_name = g_concat_dir_and_file (kde_icondir, "exec.xpm");

	if(!fake || menu) {
		menu = create_menu_at (menu, 
				       kde_menudir, FALSE,
				       _("KDE menus"), 
				       pixmap_name,
				       fake_submenus,
				       force);
	} else {
		menu = create_fake_menu_at (kde_menudir, FALSE,
					    _("KDE menus"),
					    pixmap_name);
	}
	g_free (pixmap_name);
	return menu;
}

static void
status_unparent(GtkWidget *widget)
{
	GList *li;
	PanelWidget *panel = PANEL_WIDGET(BASEP_WIDGET(widget)->panel);
	for(li=panel->applet_list;li;li=li->next) {
		AppletData *ad = li->data;
		AppletInfo *info = gtk_object_get_data(GTK_OBJECT(ad->applet),
						       "applet_info");
		if(info->type == APPLET_STATUS) {
			status_applet_put_offscreen(info->data);
		} else if(info->type == APPLET_DRAWER) {
			Drawer *dr = info->data;
			status_unparent(dr->drawer);
		}
	}
}

static void
remove_panel (GtkWidget *w, gpointer data)
{
	BasePWidget *basep;
	PanelWidget *panel;
	g_return_if_fail (IS_BASEP_WIDGET (data));

	panel = get_panel_from_menu_data(w);
	
	basep = data 
		? BASEP_WIDGET (data)
		: BASEP_WIDGET (panel->panel_parent);
	
	if (base_panels == 1 && !IS_DRAWER_WIDGET (basep)) {
		gnome_error_dialog (_("You cannot remove your last panel."));
		return;
	}

	status_unparent (GTK_WIDGET (basep));
	gtk_widget_destroy (GTK_WIDGET (basep));
}

static GtkWidget *
create_root_menu(gboolean fake_submenus, int flags, gboolean tearoff);

static void
panel_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	GtkWidget *menu;
	PanelWidget *menu_panel;

	menu = create_root_menu (TRUE, global_config.menu_flags, FALSE);

	menu_panel = get_panel_from_menu_data(w);
	gtk_object_set_data (GTK_OBJECT(menu), "menu_panel", menu_panel);
	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroy),
		      GTK_OBJECT(menu));

	show_tearoff_menu(menu, _("Panel"),TRUE,0,0,wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Panel"));
	tm->special = g_strdup("PANEL");
	tm->wmclass = g_strdup(wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

GtkWidget *
create_panel_root_menu(PanelWidget *panel, gboolean tearoff)
{
	GtkWidget *menu;

	menu = create_root_menu (TRUE, global_config.menu_flags, tearoff);

	gtk_object_set_data (GTK_OBJECT(menu), "menu_panel", panel);
	gtk_signal_connect_object_while_alive(GTK_OBJECT(panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroy),
		      GTK_OBJECT(menu));

	return menu;
}

static void
current_panel_config(GtkWidget *w, gpointer data)
{
	PanelWidget *panel = get_panel_from_menu_data(w);
	GtkWidget *parent = panel->panel_parent;
	panel_config(parent);
}

static void
ask_about_launcher_cb(GtkWidget *w, gpointer data)
{
	ask_about_launcher(NULL, get_panel_from_menu_data(w), 0, FALSE);
}

static void
ask_about_swallowing_cb(GtkWidget *w, gpointer data)
{
	ask_about_swallowing(get_panel_from_menu_data(w), 0, FALSE);
}

static void
convert_setup (BasePWidget *basep, GtkType type)
{
	basep->pos = gtk_type_new (type);
	basep->pos->basep = basep;
	basep_widget_pre_convert_hook (basep);
	basep_pos_connect_signals (basep);
	update_config_type (basep);
}

static void
convert_to_panel(GtkWidget *widget, gpointer data)
{
	PanelType type = GPOINTER_TO_INT(data);
	PanelData *pd;
	int x, y;
	int w, h;
	BasePWidget *basep;
	BasePPos *old_pos;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget);

	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (IS_BASEP_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);

	pd = gtk_object_get_user_data (GTK_OBJECT (basep));
	if (pd->type == type)
		return;

	basep_widget_get_pos (basep, &x, &y);
	basep_widget_get_size (basep, &w, &h);

	old_pos = basep->pos;
	old_pos->basep = NULL;
	pd->type = type;

	/* for now, just ignore non-border types */
	switch (type) {
	case EDGE_PANEL: 
	{
		BorderEdge edge = BORDER_BOTTOM;
		convert_setup (basep, EDGE_POS_TYPE);

		if (IS_BORDER_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		else if (PANEL_WIDGET (cur_panel)->orient == PANEL_HORIZONTAL)
			edge = (y > gdk_screen_height () / 2)
				? BORDER_BOTTOM : BORDER_TOP;
		else
			edge = (x > gdk_screen_width () / 2)
				? BORDER_RIGHT : BORDER_LEFT;

		border_widget_change_edge (BORDER_WIDGET (basep), edge);
		break;
	}
	case ALIGNED_PANEL: 
	{
		gint mid, max;
		BorderEdge edge = BORDER_BOTTOM;
		AlignedAlignment align;

		convert_setup (basep, ALIGNED_POS_TYPE);

		if (IS_BORDER_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		else if (PANEL_WIDGET (cur_panel)->orient == PANEL_HORIZONTAL)
			edge = (y > gdk_screen_height () / 2)
				? BORDER_BOTTOM : BORDER_TOP;
		else
			edge = (x > gdk_screen_width () / 2)
				? BORDER_RIGHT : BORDER_LEFT;

		if (PANEL_WIDGET (cur_panel)->orient == PANEL_HORIZONTAL) {
			mid = x + w / 2;
			max = gdk_screen_width ();
		} else {
			mid = y + h / 2;
			max = gdk_screen_height ();
		}
	
		if (mid < max / 3)
			align = ALIGNED_LEFT;
		else if (mid < 2 * (max / 3))
			align = ALIGNED_CENTER;
		else
			align = ALIGNED_RIGHT;
		aligned_widget_change_align_edge (
			ALIGNED_WIDGET (basep), align, edge);
		break;
	}
	case SLIDING_PANEL:
	{
		gint val, max;
		BorderEdge edge = BORDER_BOTTOM;
		SlidingAnchor anchor;
		gint16 offset;
		
		convert_setup (basep, SLIDING_POS_TYPE);
		
		if (IS_BORDER_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		else if (PANEL_WIDGET (cur_panel)->orient == PANEL_HORIZONTAL)
			edge = (y > gdk_screen_height () / 2)
				? BORDER_BOTTOM : BORDER_TOP;
		else
			edge = (x > gdk_screen_width () / 2)
				? BORDER_RIGHT : BORDER_LEFT;
		
		if (PANEL_WIDGET (cur_panel)->orient == PANEL_HORIZONTAL) {
			val = x;
			max = gdk_screen_width ();
		} else {
			val = y;
			max = gdk_screen_height ();
		}
		
		if (val > 0.9 * max) {
			offset = max - val;
			anchor = SLIDING_ANCHOR_RIGHT;
		} else {
			offset = val;
			anchor = SLIDING_ANCHOR_LEFT;
		}

		sliding_widget_change_anchor_offset_edge (
			SLIDING_WIDGET (basep), anchor, offset, edge);
		
		break;
	}
	case FLOATING_PANEL:
	{
		convert_setup (basep, FLOATING_POS_TYPE);
		floating_widget_change_coords (FLOATING_WIDGET (basep),
					       x, y);
		break;
	}
	default:
		g_assert_not_reached ();
		break;
	}

	gtk_object_unref (GTK_OBJECT (old_pos));
	gtk_widget_queue_resize (GTK_WIDGET (basep));
}

static void
change_hiding_mode (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget);

	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (IS_BASEP_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);
	
	basep_widget_change_params (basep,
				    cur_panel->orient,
				    cur_panel->sz,
				    GPOINTER_TO_INT (data),
				    basep->state,
				    basep->hidebuttons_enabled,
				    basep->hidebutton_pixmaps_enabled,
				    cur_panel->back_type,
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    &cur_panel->back_color);
}

static void
change_size (GtkWidget *widget, gpointer data)
{
	PanelWidget *cur_panel = get_panel_from_menu_data(widget);
	g_return_if_fail(cur_panel != NULL);
	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	panel_widget_change_params (cur_panel,
				    cur_panel->orient,
				    GPOINTER_TO_INT (data),
				    cur_panel->back_type,
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    &cur_panel->back_color);
}

static void
change_orient (GtkWidget *widget, gpointer data)
{

	BasePWidget *basep;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget);
	
	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (IS_BASEP_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);
	
	basep_widget_change_params (basep,
				    GPOINTER_TO_INT (data),
				    cur_panel->sz,
				    basep->mode,
				    basep->state,
				    basep->hidebuttons_enabled,
				    basep->hidebutton_pixmaps_enabled,
				    cur_panel->back_type,
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    &cur_panel->back_color);
}

static void
change_background (GtkWidget *widget, gpointer data)
{
	PanelWidget *cur_panel = get_panel_from_menu_data(widget);
	g_return_if_fail(cur_panel != NULL);

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	panel_widget_change_params (cur_panel,
				    cur_panel->orient,
				    cur_panel->sz,
				    GPOINTER_TO_INT (data),
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    &cur_panel->back_color);
}

static void
change_hidebuttons (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep;
	gboolean hidebutton_pixmaps_enabled, hidebuttons_enabled;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget);

	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (IS_BASEP_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);

	hidebuttons_enabled = basep->hidebuttons_enabled;
	hidebutton_pixmaps_enabled = basep->hidebutton_pixmaps_enabled;

	switch (GPOINTER_TO_INT (data)) {
	case HIDEBUTTONS_NONE:
		hidebuttons_enabled = FALSE;
		break;
	case HIDEBUTTONS_PLAIN:
		hidebutton_pixmaps_enabled = FALSE;
		hidebuttons_enabled = TRUE;
		break;
	case HIDEBUTTONS_PIXMAP:
		hidebutton_pixmaps_enabled = TRUE;
		hidebuttons_enabled = TRUE;
		break;
	}

	basep_widget_change_params (basep,
				    cur_panel->orient,
				    cur_panel->sz,
				    basep->mode,
				    basep->state,
				    hidebuttons_enabled,
				    hidebutton_pixmaps_enabled,
				    cur_panel->back_type,
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    &cur_panel->back_color);
}

static void
show_x_on_panels(GtkWidget *menu, gpointer data)
{
	GtkWidget *pw;
	GtkWidget *types = gtk_object_get_data(GTK_OBJECT(menu), MENU_TYPES);
	GtkWidget *modes = gtk_object_get_data(GTK_OBJECT(menu), MENU_MODES);
	GtkWidget *orient = gtk_object_get_data (GTK_OBJECT (menu), MENU_ORIENTS);
	PanelWidget *cur_panel = get_panel_from_menu_data(menu);
	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));
	g_return_if_fail(types != NULL);
	g_return_if_fail(modes != NULL);
	
	pw = cur_panel->panel_parent;
	g_return_if_fail(pw != NULL);
	
	if(IS_DRAWER_WIDGET(pw)) {
		gtk_widget_hide(modes);
		gtk_widget_hide(types);
	} else {
		gtk_widget_show(modes);
		gtk_widget_show(types);
	}

	if (IS_FLOATING_WIDGET (pw))
		gtk_widget_show (orient);
	else
		gtk_widget_hide (orient);
}

static void
update_type_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu);
	GtkWidget *basep = cur_panel->panel_parent;
	if (IS_EDGE_WIDGET (basep))
		s = MENU_TYPE_EDGE;
	else if (IS_ALIGNED_WIDGET (basep))
		s = MENU_TYPE_ALIGNED;
	else if (IS_SLIDING_WIDGET (basep))
		s = MENU_TYPE_SLIDING;
	else if (IS_FLOATING_WIDGET (basep))
		s = MENU_TYPE_FLOATING;
	else
		return;
	
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);				 
	
	if (menuitem)
		gtk_check_menu_item_set_active (
			GTK_CHECK_MENU_ITEM (menuitem), TRUE);
}

static void
update_size_menu (GtkWidget *menu, gpointer data)
{
	GtkWidget *menuitem = NULL;
	char *s = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu);
	switch (cur_panel->sz) {
	case SIZE_TINY:
		s = MENU_SIZE_TINY;
		break;
	case SIZE_SMALL:
		s = MENU_SIZE_SMALL;
		break;
	case SIZE_STANDARD:
		s = MENU_SIZE_STANDARD;
		break;
	case SIZE_LARGE:
		s = MENU_SIZE_LARGE;
		break;
	case SIZE_HUGE:
		s = MENU_SIZE_HUGE;
		break;
	default:
		return;
	}

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}


static void
update_back_menu (GtkWidget *menu, gpointer data)
{
	GtkWidget *menuitem = NULL;
	char *s = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu);
	switch (cur_panel->back_type) {
	case PANEL_BACK_NONE:
		s = MENU_BACK_NONE;
		break;
	case PANEL_BACK_COLOR:
		s = MENU_BACK_COLOR;
		break;
	case PANEL_BACK_PIXMAP:
		s = MENU_BACK_PIXMAP;
		break;
	default:
		return;
	}

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), MENU_BACK_PIXMAP);
	gtk_widget_set_sensitive (menuitem, cur_panel->back_pixmap != NULL);
}

static void
update_hidebutton_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu);
	BasePWidget *basep = BASEP_WIDGET(cur_panel->panel_parent);

	if (!basep->hidebuttons_enabled)
		s = MENU_HIDEBUTTONS_NONE;
	else if (basep->hidebutton_pixmaps_enabled)
		s = MENU_HIDEBUTTONS_PIXMAP;
	else 
		s = MENU_HIDEBUTTONS_PLAIN;
	
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}

static void
update_hiding_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu);
	BasePWidget *basep = BASEP_WIDGET(cur_panel->panel_parent);
	s =  (basep->mode == BASEP_EXPLICIT_HIDE)
		? MENU_MODE_EXPLICIT_HIDE
		: MENU_MODE_AUTO_HIDE;

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}

static void
update_orient_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data (menu);
	BasePWidget *basep = BASEP_WIDGET (cur_panel->panel_parent);
	s = (PANEL_WIDGET (basep->panel)->orient == PANEL_HORIZONTAL)
		? MENU_ORIENT_HORIZONTAL
		: MENU_ORIENT_VERTICAL;

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}

typedef struct {
	char *name;
	char *id;
	int i;
} NameIdEnum;

static void
add_radios_to_menu (GtkWidget *menu, NameIdEnum *items,
		    GtkSignalFunc func)
{
	int i;
	GSList *radio_group = NULL;
	GtkWidget *menuitem;

	for (i=0; items[i].name; i++) {
		menuitem = gtk_radio_menu_item_new (radio_group);
		radio_group = gtk_radio_menu_item_group (
			GTK_RADIO_MENU_ITEM (menuitem));
		setup_menuitem (menuitem, NULL, _(items[i].name));
		gtk_menu_append (GTK_MENU (menu), 
				 menuitem);
		gtk_object_set_data (GTK_OBJECT (menu),
				     items[i].id, menuitem);
		gtk_check_menu_item_set_show_toggle (
			GTK_CHECK_MENU_ITEM (menuitem), TRUE);
		gtk_check_menu_item_set_active (
			GTK_CHECK_MENU_ITEM (menuitem), FALSE);
		gtk_signal_connect (GTK_OBJECT (menuitem), "toggled",
				    GTK_SIGNAL_FUNC (func),
				    GINT_TO_POINTER (items[i].i));
	}
}

static void
add_radio_menu (GtkWidget *menu, char *menutext, 
		NameIdEnum *items, char *menu_key,
		GtkSignalFunc change_func,
		GtkSignalFunc update_func)
{
	GtkWidget *menuitem;
	GtkWidget *submenu;

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, NULL, menutext);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_object_set_data (GTK_OBJECT (menu), menu_key, menuitem);

	submenu = gtk_menu_new ();
	gtk_signal_connect(GTK_OBJECT(submenu), "show",
			   setup_menu_panel, NULL);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
	add_radios_to_menu (submenu, items, change_func);
	gtk_signal_connect (GTK_OBJECT (submenu), "show",
			    GTK_SIGNAL_FUNC (update_func),
			    NULL);
	
}

static void
make_properties_submenu (GtkWidget *menu)
{
	NameIdEnum types[] = {
		{ N_("Edge panel"), MENU_TYPE_EDGE, EDGE_PANEL },
		{ N_("Aligned panel"), MENU_TYPE_ALIGNED, ALIGNED_PANEL },
		{ N_("Sliding panel"), MENU_TYPE_SLIDING, SLIDING_PANEL },
		{ N_("Floating panel"), MENU_TYPE_FLOATING, FLOATING_PANEL },
		{ NULL, NULL, -1 }
	};
	
	NameIdEnum modes[] = {
		{ N_("Explicit hide"), MENU_MODE_EXPLICIT_HIDE, BASEP_EXPLICIT_HIDE },
		{ N_("Auto hide"), MENU_MODE_AUTO_HIDE, BASEP_AUTO_HIDE },
		{ NULL, NULL, -1 }
	};

	NameIdEnum hidebuttons[] = {
		{ N_("With pixmap arrow"), MENU_HIDEBUTTONS_PIXMAP, HIDEBUTTONS_PIXMAP },
		{ N_("Without pixmap"), MENU_HIDEBUTTONS_PLAIN, HIDEBUTTONS_PLAIN },
		{ N_("None"), MENU_HIDEBUTTONS_NONE, HIDEBUTTONS_NONE },
		{ NULL, NULL, -1 }
	};

	NameIdEnum orients[] = {
		{ N_("Horizontal"), MENU_ORIENT_HORIZONTAL, PANEL_HORIZONTAL },
		{ N_("Vertical"), MENU_ORIENT_VERTICAL, PANEL_VERTICAL },
		{ NULL, NULL, -1 }
	};

	NameIdEnum sizes[] = {
		{ N_("Tiny (24 pixels)"), MENU_SIZE_TINY, SIZE_TINY },
		{ N_("Small (36 pixels)"), MENU_SIZE_SMALL, SIZE_SMALL },
		{ N_("Standard (48 pixels)"), MENU_SIZE_STANDARD, SIZE_STANDARD },
		{ N_("Large (64 pixels)"), MENU_SIZE_LARGE, SIZE_LARGE },
		{ N_("Huge (80 pixels)"), MENU_SIZE_HUGE, SIZE_HUGE },
		{ NULL, NULL, -1 }
	};

	NameIdEnum backgrounds[] = {
		{ N_("Standard"), MENU_BACK_NONE, PANEL_BACK_NONE },
		{ N_("Pixmap"), MENU_BACK_COLOR, PANEL_BACK_COLOR },
		{ N_("Color"), MENU_BACK_PIXMAP, PANEL_BACK_PIXMAP },
		{ NULL, NULL, -1 }
	};

	add_radio_menu (menu, _("Type"), types, MENU_TYPES,
			convert_to_panel, update_type_menu);

	add_radio_menu (menu, _("Hiding policy"), modes, MENU_MODES,
			change_hiding_mode, update_hiding_menu);

	add_radio_menu (menu, _("Hide buttons"), hidebuttons, MENU_HIDEBUTTONS,
			change_hidebuttons, update_hidebutton_menu);

	add_radio_menu (menu, _("Size"), sizes, MENU_SIZES,
			change_size, update_size_menu);

	add_radio_menu (menu, _("Orientation"), orients, MENU_ORIENTS,
			change_orient, update_orient_menu);

	add_radio_menu (menu, _("Background type"), backgrounds, MENU_BACKS,
			change_background, update_back_menu);
	
	gtk_signal_connect (GTK_OBJECT (menu), "show",
			    GTK_SIGNAL_FUNC (show_x_on_panels),
			    NULL);
}

static void
make_add_submenu (GtkWidget *menu, int fake_submenus)
{
	GtkWidget *menuitem, *submenu, *submenuitem, *m;

	/* Add Menu */

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_stock_pixmap_widget (menu,
						   GNOME_STOCK_MENU_EXEC),
			_("Applet"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	m = create_applets_menu(NULL,fake_submenus);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),m);
	gtk_signal_connect(GTK_OBJECT(m),"show",
			   GTK_SIGNAL_FUNC(submenu_to_display),
			   menuitem);

	menuitem = gtk_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem, 
				   "gnome-gmenu.png",
				   _("Menu"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);

	submenu = gtk_menu_new ();
	gtk_signal_connect(GTK_OBJECT(submenu), "show",
			   setup_menu_panel, NULL);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);

	submenuitem = gtk_menu_item_new ();
	setup_menuitem_try_pixmap (submenuitem,
				   "gnome-logo-icon-transparent.png",
				   _("Main menu"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (submenu), submenuitem);
	gtk_signal_connect(GTK_OBJECT(submenuitem), "activate",
			   GTK_SIGNAL_FUNC(add_menu_to_panel),
			   NULL);
	setup_internal_applet_drag(submenuitem, "MENU:MAIN");

	submenuitem = gtk_menu_item_new ();
	setup_menuitem_try_pixmap (submenuitem, 
				   "gnome-logo-icon-transparent.png",
				   _("Programs menu"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (submenu), submenuitem);
	gtk_signal_connect(GTK_OBJECT(submenuitem), "activate",
			   GTK_SIGNAL_FUNC(add_menu_to_panel),
			   "gnome/apps/");
	setup_internal_applet_drag(submenuitem, "MENU:gnome/apps/");

	submenuitem = gtk_menu_item_new ();
	setup_menuitem_try_pixmap (submenuitem, "gnome-favorites.png",
				   _("Favorites menu"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (submenu), submenuitem);
	gtk_signal_connect(GTK_OBJECT(submenuitem), "activate",
			   GTK_SIGNAL_FUNC(add_menu_to_panel),
			   "~/.gnome/apps/");
	setup_internal_applet_drag(submenuitem, "MENU:~/.gnome/apps/");

	menuitem = gtk_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem, 
				   "launcher-program.png",
				   _("Launcher"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(ask_about_launcher_cb),NULL);
	setup_internal_applet_drag(menuitem, "LAUNCHER:ASK");

	menuitem = gtk_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem, 
				   "panel-drawer.png",
				   _("Drawer"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_drawer_to_panel,
			   NULL);
	setup_internal_applet_drag(menuitem, "DRAWER:NEW");

	menuitem = gtk_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem,
				   "gnome-term-night.png",
				   _("Log out button"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(add_logout_to_panel),
			   NULL);
	setup_internal_applet_drag(menuitem, "LOGOUT:NEW");
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem, 
				   "gnome-lockscreen.png",
				   _("Lock button"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(add_lock_to_panel),
			   NULL);
	setup_internal_applet_drag(menuitem, "LOCK:NEW");

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_stock_pixmap_widget (menu,
						   GNOME_STOCK_PIXMAP_ADD),
			_("Swallowed app"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(ask_about_swallowing_cb),NULL);
	setup_internal_applet_drag(menuitem, "SWALLOW:ASK");

	menuitem = gtk_menu_item_new ();
	setup_menuitem(menuitem, 0, _("Status dock"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(try_add_status_to_panel),NULL);
	setup_internal_applet_drag(menuitem, "STATUS:TRY");
}

static void
add_to_panel_menu_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	GtkWidget *menu;
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	PanelWidget *menu_panel;

	menu = gtk_menu_new();
	gtk_signal_connect(GTK_OBJECT(menu), "show",
			   setup_menu_panel, NULL);
	make_add_submenu(menu, TRUE);
	
	/*set the panel to use as the data*/
	menu_panel = get_panel_from_menu_data(w);
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);
	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroy),
		      GTK_OBJECT(menu));
	show_tearoff_menu(menu, _("Add to panel"),TRUE,0,0, wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Add to panel"));
	tm->special = g_strdup("ADD_TO_PANEL");
	tm->wmclass = g_strdup(wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

/* just run the gnome-panel-properties */
static void
panel_config_global(void)
{
	char *argv[2] = {"gnome-panel-properties-capplet", NULL};
	if(gnome_execute_async(NULL,1,argv)<0)
		gnome_error_dialog(_("Cannot execute panel global properties"));
}

void
make_panel_submenu (GtkWidget *menu, gboolean fake_submenus)
{
	GtkWidget *menuitem, *submenu, *submenuitem;

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget (menu,
						   GNOME_STOCK_PIXMAP_ADD),
			_("Add to panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);

	submenu = gtk_menu_new ();
	gtk_signal_connect(GTK_OBJECT(submenu), "show",
			   setup_menu_panel, NULL);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   submenu);
	
	if (gnome_preferences_get_menus_have_tearoff ()) {
		menuitem = tearoff_item_new();
		gtk_widget_show(menuitem);
		gtk_menu_prepend(GTK_MENU(submenu),menuitem);
	
		gtk_signal_connect(GTK_OBJECT(menuitem),"activate",
				   GTK_SIGNAL_FUNC(add_to_panel_menu_tearoff_new_menu),
				   NULL);
	}

	make_add_submenu (submenu, fake_submenus);


        menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_stock_pixmap_widget (menu,
						   GNOME_STOCK_MENU_NEW),
			_("Create panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   create_add_panel_submenu(TRUE));

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_stock_pixmap_widget (NULL,
						   GNOME_STOCK_PIXMAP_REMOVE),
			_("Remove this panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (remove_panel),
			    NULL);

	add_menu_separator(menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 
			gnome_stock_pixmap_widget (menu,
						   GNOME_STOCK_MENU_PROP),
			_("Properties"));
	gtk_menu_append (GTK_MENU (menu), menuitem);

	submenu = gtk_menu_new ();
	gtk_signal_connect(GTK_OBJECT(submenu), "show",
			   setup_menu_panel, NULL);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);

	submenuitem = gtk_menu_item_new ();
	setup_menuitem (submenuitem,
			gnome_stock_pixmap_widget(submenu,
						  GNOME_STOCK_MENU_PROP),
			_("All properties..."));

	gtk_menu_append (GTK_MENU (submenu), submenuitem);
	gtk_signal_connect (GTK_OBJECT (submenuitem), "activate",
			    GTK_SIGNAL_FUNC(current_panel_config), 
			    NULL);

	/*add_menu_separator (submenu);*/
	make_properties_submenu (submenu);
	
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_MENU_PREF),
			_("Global Preferences..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_config_global), 
			    NULL);
}

void
panel_lock (GtkWidget *widget, gpointer data)
{
	char *argv[3] = {"xscreensaver-command", NULL, NULL};
	gboolean lock;
	lock = gnome_config_get_bool ("Screensaver/Default/password=false");
	/* we want to default to something safe.
	 * Have we started xscreensaver in a locked state or not?*/
	if (lock)
		argv[1] = "-activate";
	else
		argv[1] = "-lock";
	if(gnome_execute_async(NULL,2,argv)<0)
		gnome_error_dialog(_("Cannot execute xscreensaver"));
}

static GtkWidget *create_panel_submenu (GtkWidget *m, gboolean fake_sub,
					gboolean tearoff);
static GtkWidget *create_desktop_menu (GtkWidget *m, gboolean fake_sub,
				       gboolean tearoff);

static void
panel_menu_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	GtkWidget *menu = create_panel_submenu (NULL, TRUE, FALSE);
	PanelWidget *menu_panel = get_panel_from_menu_data(w);

	/*set the panel to use as the data*/
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);
	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroy),
		      GTK_OBJECT(menu));

	show_tearoff_menu(menu, _("Panel"),TRUE,0,0,wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Panel"));
	tm->special = g_strdup("PANEL_SUBMENU");
	tm->wmclass = g_strdup(wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

static void
desktop_menu_tearoff_new_menu (GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	PanelWidget *menu_panel;
	GtkWidget *menu = create_desktop_menu (NULL, TRUE, FALSE);

	/*set the panel to use as the data*/
	menu_panel = get_panel_from_menu_data(w);
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);
	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroy),
		      GTK_OBJECT(menu));

	show_tearoff_menu (menu, _("Desktop"),TRUE,0,0, wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Desktop"));
	tm->special = g_strdup("DESKTOP");
	tm->wmclass = g_strdup(wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

static GtkWidget *
create_panel_submenu(GtkWidget *menu, gboolean fake_submenus, gboolean tearoff)
{
	GtkWidget *menuitem;
	char *char_tmp;

	if (!menu) {
		menu = gtk_menu_new();
		gtk_signal_connect(GTK_OBJECT(menu), "show",
				   setup_menu_panel, NULL);
	}

	if(tearoff && gnome_preferences_get_menus_have_tearoff ()) {
		menuitem = tearoff_item_new();
		gtk_widget_show(menuitem);
		gtk_menu_prepend (GTK_MENU (menu), menuitem);

		gtk_signal_connect(GTK_OBJECT(menuitem),"activate",
				   GTK_SIGNAL_FUNC(panel_menu_tearoff_new_menu),
				   NULL);
	}

	make_panel_submenu (menu, fake_submenus);

	add_menu_separator (menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_PIXMAP_ABOUT),
			_("About the panel..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(about_cb),
			    NULL);
	
	char_tmp = gnome_is_program_in_path("gnome-about");
	if(!char_tmp)
		char_tmp = gnome_is_program_in_path ("guname");

	if (char_tmp) {
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
	return menu;
}

static void
run_cb (GtkWidget *w, gpointer data)
{
	show_run_dialog ();
}

static GtkWidget *
create_desktop_menu (GtkWidget *menu, gboolean fake_submenus, gboolean tearoff)
{
	GtkWidget *menuitem;
	char *char_tmp;
	/* Panel entry */

	if (!menu) {
		menu = gtk_menu_new ();
		gtk_signal_connect(GTK_OBJECT(menu), "show",
				   setup_menu_panel, NULL);
	}

	if(tearoff && gnome_preferences_get_menus_have_tearoff ()) {
		menuitem = tearoff_item_new();
		gtk_widget_show(menuitem);
		gtk_menu_prepend (GTK_MENU (menu), menuitem);

		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC(desktop_menu_tearoff_new_menu),
				    NULL);
	}

	char_tmp = gnome_is_program_in_path ("xscreensaver");
	if (char_tmp) {	
		menuitem = gtk_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem,
					   "gnome-lockscreen.png",
					   _("Lock screen"), SMALL_ICON_SIZE);
		gtk_menu_append (GTK_MENU (menu), menuitem);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC(panel_lock), 0);
		setup_internal_applet_drag(menuitem, "LOCK:NEW");
	}
	g_free (char_tmp);

	menuitem = gtk_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem,
				   "gnome-term-night.png",
				   _("Log out"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_quit), 0);
	setup_internal_applet_drag(menuitem, "LOGOUT:NEW");

	return menu;
}

static GtkWidget *
create_root_menu(gboolean fake_submenus, int flags, gboolean tearoff)
{
	GtkWidget *root_menu;
	GtkWidget *menu;
	GtkWidget *menuitem;

	gboolean has_inline = (flags & (MAIN_MENU_SYSTEM |
					MAIN_MENU_USER |
					MAIN_MENU_APPLETS |
					MAIN_MENU_KDE |
					MAIN_MENU_REDHAT |
					MAIN_MENU_DEBIAN));

	gboolean has_subs = (flags & (MAIN_MENU_SYSTEM_SUB |
				      MAIN_MENU_USER_SUB |
				      MAIN_MENU_APPLETS_SUB |
				      MAIN_MENU_KDE_SUB |
				      MAIN_MENU_REDHAT_SUB |
				      MAIN_MENU_DEBIAN_SUB));

	gboolean has_inline2 = (flags & (MAIN_MENU_DESKTOP |
					 MAIN_MENU_PANEL));
	gboolean has_subs2 = (flags & (MAIN_MENU_DESKTOP_SUB |
				       MAIN_MENU_PANEL_SUB));

	IconSize size = global_config.use_large_icons 
		? MEDIUM_ICON_SIZE : SMALL_ICON_SIZE;

	root_menu = gtk_menu_new ();
	gtk_signal_connect(GTK_OBJECT(root_menu), "show",
			   setup_menu_panel, NULL);
	if (tearoff && gnome_preferences_get_menus_have_tearoff ()) {
		GtkWidget *menuitem = tearoff_item_new ();
		gtk_widget_show (menuitem);
		gtk_menu_prepend (GTK_MENU (root_menu), menuitem);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC (panel_tearoff_new_menu),
				    NULL);
	}
	
	if (flags & MAIN_MENU_SYSTEM)
		create_system_menu(root_menu,fake_submenus,FALSE);
	if (flags & MAIN_MENU_USER)
		create_user_menu(_("Favorites"), "apps",
				 root_menu, "gnome-favorites.png",
				 fake_submenus, FALSE, FALSE);
	if (flags & MAIN_MENU_APPLETS)
		create_applets_menu(root_menu,fake_submenus);

	if (flags & MAIN_MENU_REDHAT) {
		rh_submenu_to_display(NULL,NULL);
		create_user_menu(_("AnotherLevel menus"), "apps-redhat",
				 root_menu, NULL, fake_submenus, FALSE,
				 FALSE);
	}

	if (flags & MAIN_MENU_DEBIAN)
		create_debian_menu(root_menu, fake_submenus, FALSE);
	if (flags & MAIN_MENU_KDE)
		create_kde_menu(root_menu,fake_submenus,FALSE, FALSE);

	/*others here*/

	if (has_subs && has_inline)
		add_menu_separator (root_menu);

	
	if (flags & MAIN_MENU_SYSTEM_SUB) {
		menu = create_system_menu(NULL,fake_submenus, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem,
					   "gnome-logo-icon-transparent.png",
					   _("Programs"), size);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		if(menu) {
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
						   menu);
			gtk_signal_connect(GTK_OBJECT(menu),"show",
					   GTK_SIGNAL_FUNC(submenu_to_display),
					   menuitem);
		}
	}

	if (flags & MAIN_MENU_USER_SUB) {
		menu = create_user_menu(_("Favorites"), "apps", NULL,
					"gnome-favorites.png",
					fake_submenus, TRUE, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem, "gnome-favorites.png",
					   _("Favorites"), size);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}
	if (flags & MAIN_MENU_APPLETS_SUB) {
		menu = create_applets_menu(NULL,fake_submenus);
		menuitem = gtk_menu_item_new ();
		setup_menuitem_with_size (menuitem,
					  gnome_stock_pixmap_widget_at_size (
						  NULL,
						  GNOME_STOCK_PIXMAP_EXEC,
						  size, size),
					  _("Applets"), size);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}
	if (flags & MAIN_MENU_REDHAT_SUB) {
		menu = create_user_menu(_("AnotherLevel menus"), "apps-redhat",
					NULL, NULL, fake_submenus, TRUE, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem_with_size (menuitem, NULL, 
					  _("AnotherLevel menus"),
					  size);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(rh_submenu_to_display),
				   menuitem);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}
	if (flags & MAIN_MENU_DEBIAN_SUB) {
		menu = create_debian_menu(NULL,fake_submenus, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem_with_size (menuitem, 0, _("Debian menus"),
					  size);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		if(menu) {
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
						   menu);
			gtk_signal_connect(GTK_OBJECT(menu),"show",
					   GTK_SIGNAL_FUNC(submenu_to_display),
					   menuitem);
		}
	}
	if (flags & MAIN_MENU_KDE_SUB) {
		GtkWidget *pixmap = NULL;
		char *pixmap_path;
		menu = create_kde_menu(NULL, fake_submenus, TRUE, TRUE);
		pixmap_path = g_concat_dir_and_file (kde_icondir, "exec.xpm");

		if (g_file_exists(pixmap_path)) {
			pixmap = gnome_stock_pixmap_widget_at_size (
				NULL, pixmap_path, size,
				size);
			if (pixmap)
				gtk_widget_show (pixmap);
		}
		g_free (pixmap_path);
		menuitem = gtk_menu_item_new ();
		setup_menuitem_with_size (menuitem, pixmap, _("KDE menus"),
					  size);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   menuitem);
	}

	menuitem = gtk_menu_item_new ();
	setup_menuitem_try_pixmap (menuitem, "gnome-tigert.png", 
				   _("Run..."), size);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (run_cb), NULL);
	gtk_menu_append (GTK_MENU (root_menu), menuitem);


	if (((has_inline && !has_subs) || has_subs) && has_subs2)
		add_menu_separator (root_menu);

	if (flags & MAIN_MENU_PANEL_SUB) {
		menu = create_panel_submenu (NULL, fake_submenus, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem, "gnome-panel.png", 
					   _("Panel"), SMALL_ICON_SIZE);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	}
	if (flags & MAIN_MENU_DESKTOP_SUB) {
		menu = create_desktop_menu (NULL, fake_submenus, TRUE);
		menuitem = gtk_menu_item_new ();
		setup_menuitem_try_pixmap (menuitem, "gnome-ccdesktop.png",
					   _("Desktop"), SMALL_ICON_SIZE);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	}

	if (!has_inline2)
		return root_menu;

	if (has_subs2 || has_subs || has_inline)
		add_menu_separator (root_menu);

	if (flags & MAIN_MENU_PANEL) {
		make_panel_submenu (root_menu, fake_submenus);
	}
	
	if (flags & MAIN_MENU_DESKTOP) {
		if (flags & MAIN_MENU_PANEL)
			add_menu_separator (root_menu);
		create_desktop_menu (root_menu, fake_submenus, FALSE);
	}
	
	return root_menu;
}
	
void
add_menu_widget (Menu *menu, PanelWidget *panel, GSList *menudirl,
		 gboolean main_menu, gboolean fake_subs)
{
	GSList *li;

	/* one of these has to be there in order to get the panel of the
	   applet */
	g_return_if_fail (menu->menu || panel);

	if(menu->menu) {
		panel = get_panel_from_menu_data(menu->menu);
		gtk_widget_destroy(menu->menu);
	}

	if(!panel) {
		g_warning ("Menu is seriously weird");
		return;
	}

	if (main_menu)
		menu->menu = create_root_menu(fake_subs, 
					      menu->main_menu_flags,
					      FALSE);
	else {
		menu->menu = NULL;
		for(li=menudirl;li!=NULL;li=g_slist_next(li))
			menu->menu = create_menu_at (menu->menu,li->data,
						     FALSE, NULL, NULL,
						     fake_subs, FALSE);
		if(!menu->menu) {
			g_warning(_("Can't create menu, using main menu!"));
			menu->menu = create_root_menu(fake_subs,
						      menu->main_menu_flags,
						      FALSE);
		}
	}
	gtk_signal_connect (GTK_OBJECT (menu->menu), "deactivate",
			    GTK_SIGNAL_FUNC (menu_deactivate), menu);

	gtk_object_set_data(GTK_OBJECT(menu->menu),"menu_panel", panel);
	gtk_signal_connect_object_while_alive(
	      GTK_OBJECT(panel),
	      "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroy),
	      GTK_OBJECT(menu->menu));
}

static void
menu_button_pressed(GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GdkEventButton *bevent = (GdkEventButton*)gtk_get_current_event();
	GtkWidget *wpanel = get_panel_parent(menu->button);
	int main_menu = (strcmp (menu->path, ".") == 0);

	if(!menu->menu) {
		char *this_menu = get_real_menu_path(menu->path);
		GSList *list = g_slist_append(NULL,this_menu);
		
		add_menu_widget(menu, PANEL_WIDGET(menu->button->parent),
				list, strcmp(menu->path,".")==0, TRUE);
		
		g_free(this_menu);

		g_slist_free(list);
	} else {
		if(menu->main_menu_flags&MAIN_MENU_REDHAT &&
		   !(menu->main_menu_flags&MAIN_MENU_REDHAT_SUB))
			rh_submenu_to_display(NULL,NULL);

		check_and_reread_applet(menu, main_menu);
	}

	/*so that the panel doesn't pop down until we're
	  done with the menu */
	if(IS_BASEP_WIDGET(wpanel)) {
		BASEP_WIDGET(wpanel)->autohide_inhibit = TRUE;
		basep_widget_autohide(BASEP_WIDGET(wpanel));
	}

	BUTTON_WIDGET(menu->button)->ignore_leave = TRUE;
	gtk_grab_remove(menu->button);

	menu->age = 0;
	gtk_menu_popup(GTK_MENU(menu->menu), 0,0, 
		       applet_menu_position,
		       menu->info, bevent->button, bevent->time);
	gdk_event_free((GdkEvent *)bevent);
}

static Menu *
create_panel_menu (PanelWidget *panel, char *menudir, gboolean main_menu,
		   PanelOrientType orient, int main_menu_flags)
{
	Menu *menu;
	
	char *pixmap_name;

	menu = g_new0(Menu,1);

	pixmap_name = get_pixmap(menudir,main_menu);

	menu->main_menu_flags = main_menu_flags;

	/*make the pixmap*/
	menu->button = button_widget_new (pixmap_name,-1, MENU_TILE,
					  TRUE,orient, _("Menu"));
	gtk_signal_connect_after (GTK_OBJECT (menu->button), "pressed",
				  GTK_SIGNAL_FUNC (menu_button_pressed), menu);
	gtk_signal_connect (GTK_OBJECT (menu->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_menu), menu);
	gtk_widget_show(menu->button);

	/*if we are allowed to be pigs and load all the menus to increase
	  speed, load them*/
	if(global_config.hungry_menus) {
		GSList *list = g_slist_append(NULL,menudir);
		add_menu_widget(menu,panel,list,main_menu,TRUE);
		g_slist_free(list);
	}

	g_free (pixmap_name);

	return menu;
}

static Menu *
create_menu_applet(PanelWidget *panel, char *arguments,
		   PanelOrientType orient, int main_menu_flags)
{
	Menu *menu;
	gboolean main_menu;

	char *this_menu = get_real_menu_path(arguments);

	if (!this_menu)
		return NULL;

	if(!gnome_folder) {
		gnome_folder =
			gnome_unconditional_pixmap_file("gnome-folder.png");
		if (!g_file_exists (gnome_folder)) {
			g_free (gnome_folder);
			gnome_folder = NULL;
		}
	}

	main_menu = (!arguments ||
		     !*arguments ||
		     (strcmp (arguments, ".") == 0));

	menu = create_panel_menu (panel, this_menu, main_menu,
				  orient, main_menu_flags);
	if (arguments && *arguments)
		menu->path = g_strdup(arguments);
	else
		menu->path = g_strdup(".");

	gtk_object_set_user_data(GTK_OBJECT(menu->button), menu);

	g_free (this_menu);
	return menu;
}

void
set_menu_applet_orient(Menu *menu, PanelOrientType orient)
{
	g_return_if_fail(menu!=NULL);

	button_widget_set_params(BUTTON_WIDGET(menu->button),
				 MENU_TILE, TRUE, orient);
}

void
load_menu_applet(char *params, int main_menu_flags,
		 PanelWidget *panel, int pos, gboolean exactpos)
{
	Menu *menu;

	menu = create_menu_applet(panel, params, ORIENT_UP, main_menu_flags);

	if(menu) {
		char *tmp;
		if(!register_toy(menu->button, menu,
				 panel, pos, exactpos, APPLET_MENU))
			return;

		menu->info = applets_last->data;

		applet_add_callback(menu->info, "properties",
				    GNOME_STOCK_MENU_PROP,
				    _("Properties..."));
		if(params && strcmp(params, ".")==0 &&
		   (tmp = gnome_is_program_in_path("gmenu")))  {
			g_free(tmp);
			applet_add_callback(menu->info, "edit_menus",
					    NULL, _("Edit menus..."));
		}
	}
}

void
save_tornoff(void)
{
	GSList *li;
	int i;

	gnome_config_push_prefix (PANEL_CONFIG_PATH "panel/Config/");

	gnome_config_set_int("tearoffs_count",g_slist_length(tearoffs));

	gnome_config_pop_prefix ();

	for(i=0,li=tearoffs;li;i++,li=li->next) {
		TearoffMenu *tm = li->data;
		int x = 0,y = 0;
		GtkWidget *tw;
		int menu_panel = 0;
		PanelWidget *menu_panel_widget = NULL;
		GSList *l;
		char *s;
		int j;

		s = g_strdup_printf("%spanel/TornoffMenu_%d/",
				    PANEL_CONFIG_PATH,i);
		gnome_config_push_prefix (s);
		g_free(s);

		tw = GTK_MENU(tm->menu)->tearoff_window;

		if(tw && tw->window) {
			gdk_window_get_root_origin(tw->window, &x, &y);
			/* unfortunately we must do this or set_uposition
			   will crap out */
			if(x<0) x=0;
			if(y<0) y=0;
		}

		gnome_config_set_string("title",tm->title);
		gnome_config_set_string("wmclass",tm->wmclass);
		gnome_config_set_int("x", x);
		gnome_config_set_int("y", y);

		menu_panel_widget = gtk_object_get_data(GTK_OBJECT(tm->menu),
							"menu_panel");
		menu_panel = g_slist_index(panels,menu_panel_widget);
		if(menu_panel<0) menu_panel = 0;

		gnome_config_set_int("menu_panel", menu_panel);

		gnome_config_set_int("workspace",
				     gnome_win_hints_get_workspace(tw));
		gnome_config_set_int("hints",
				     gnome_win_hints_get_hints(tw));
		gnome_config_set_int("state",
				     gnome_win_hints_get_state(tw));

		gnome_config_set_string("special",
					tm->special?tm->special:"");

		gnome_config_set_int("mfl_count", g_slist_length(tm->mfl));

		for(j=0,l=tm->mfl;l;j++,l=l->next) {
			MenuFinfo *mf = l->data;
			char name[256];
			g_snprintf(name, 256, "name_%d", j);
			gnome_config_set_string(name, mf->fr->name);
			g_snprintf(name, 256, "dir_name_%d", j);
			gnome_config_set_string(name, mf->dir_name);
			g_snprintf(name, 256, "pixmap_name_%d", j);
			gnome_config_set_string(name, mf->pixmap_name);
			g_snprintf(name, 256, "applets_%d", j);
			gnome_config_set_bool(name, mf->applets);

		}

		gnome_config_pop_prefix();
	}
}

static GtkWidget *
create_special_menu(char *special, PanelWidget *menu_panel_widget)
{
	GtkWidget *menu = NULL;

	if(strcmp(special,"ADD_PANEL")==0) {
		menu = create_add_panel_submenu(FALSE);
	} else if(strcmp(special,"PANEL")==0) {
		menu = create_root_menu (TRUE, global_config.menu_flags,
					 FALSE);
	} else if(strcmp(special,"DESKTOP")==0) {
		menu = create_desktop_menu (NULL, TRUE, FALSE);
	} else if(strcmp(special,"ADD_TO_PANEL")==0) {
		menu = gtk_menu_new();
		gtk_signal_connect(GTK_OBJECT(menu), "show",
				   GTK_SIGNAL_FUNC(setup_menu_panel),
				   NULL);
		make_add_submenu(menu, TRUE);
	} else if(strcmp(special,"PANEL_SUBMENU")==0) {
		menu = create_panel_submenu (NULL, TRUE, FALSE);

	}

	return menu;
}

static void
load_tearoff_menu(void)
{
	GtkWidget *menu;
	char *title, *wmclass, *special;
	int x, y, i;
	int workspace, hints, state;
	int mfl_count;
	TearoffMenu *tm;
	gulong wmclass_num;
	PanelWidget *menu_panel_widget = NULL;

	title = gnome_config_get_string("title=");
	wmclass = gnome_config_get_string("wmclass=");

	if(!*title || !*wmclass) {
		g_free(title);
		g_free(wmclass);
		return;
	}

	x = gnome_config_get_int("x=0");
	y = gnome_config_get_int("y=0");
	workspace = gnome_config_get_int("workspace=0");
	hints = gnome_config_get_int("hints=0");
	state = gnome_config_get_int("state=0");

	i = gnome_config_get_int("menu_panel=0");
	if(i<0) i = 0;
	menu_panel_widget = g_slist_nth_data(panels, i);
	if(!menu_panel_widget)
		menu_panel_widget = panels->data;
	if(!IS_PANEL_WIDGET(menu_panel_widget))
		g_warning("panels list is on crack");

	mfl_count = gnome_config_get_int("mfl_count=0");

	special = gnome_config_get_string("special=");
	if(!*special) {
		g_free(special);
		special = NULL;
	}

	/* find out the wmclass_number that was used
	   for this wmclass and make our default one 1 higher
	   so that we will always get unique wmclasses */
	wmclass_num = 0;
	sscanf(wmclass,"panel_tearoff_%lu",&wmclass_num);
	if(wmclass_num>=wmclass_number)
		wmclass_number = wmclass_num+1;

	menu = NULL;

	if(special) {
		menu = create_special_menu(special, menu_panel_widget);
	} else {
		for(i = 0; i < mfl_count; i++) {
			char propname[256];
			char *name;
			gboolean applets;
			char *dir_name;
			char *pixmap_name;

			g_snprintf(propname, 256, "name_%d=", i);
			name = gnome_config_get_string(propname);
			g_snprintf(propname, 256, "applets_%d=", i);
			applets = gnome_config_get_bool(propname);
			g_snprintf(propname, 256, "dir_name_%d=", i);
			dir_name = gnome_config_get_string(propname);
			g_snprintf(propname, 256, "pixmap_name_%d=", i);
			pixmap_name = gnome_config_get_string(propname);

			if(!menu) {
				menu = gtk_menu_new ();
				gtk_signal_connect(GTK_OBJECT(menu), "show",
						   GTK_SIGNAL_FUNC(setup_menu_panel),
						   NULL);
			}

			menu = create_menu_at(menu, name, applets, dir_name,
					      pixmap_name, TRUE, FALSE);

			g_free(name);
			g_free(dir_name);
			g_free(pixmap_name);
		}

		if(menu && !gtk_object_get_data(GTK_OBJECT(menu), "mf")) {
			gtk_widget_destroy(menu);
			menu = NULL;
		}
	}

	if(!menu) {
		g_free(special);
		g_free(title);
		g_free(wmclass);
		return;
	}

	/*set the panel to use as the data, or we will use current_panel*/
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel",
			    menu_panel_widget);
	gtk_signal_connect_object_while_alive(
	      GTK_OBJECT(menu_panel_widget),
	      "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroy),
	      GTK_OBJECT(menu));
	
	/* This is so that we get size of the menu right */
	show_tearoff_menu(menu,title,FALSE,x,y,wmclass);

	{
		GtkWidget *window = GTK_MENU(menu)->tearoff_window;
		gnome_win_hints_set_workspace(window,workspace);
		gnome_win_hints_set_hints(window,hints);
		gnome_win_hints_set_state(window,state);
	}

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = gtk_object_get_data(GTK_OBJECT(menu), "mf");
	tm->title = title;
	tm->special = special;
	tm->wmclass = wmclass;
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

void
load_tornoff(void)
{
	char *s;
	int i,length;

	gnome_config_push_prefix(PANEL_CONFIG_PATH "panel/Config/");
	length = gnome_config_get_int("tearoffs_count=0");
	gnome_config_pop_prefix();

	if(length==0) return;

	for(i=0;i<length;i++) {
		s = g_strdup_printf("%spanel/TornoffMenu_%d/",
				    PANEL_CONFIG_PATH, i);
		gnome_config_push_prefix(s);
		g_free(s);

		load_tearoff_menu();

		gnome_config_pop_prefix();
	}
}
