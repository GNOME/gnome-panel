/*
 * GNOME panel menu module.
 * (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <stdio.h>
#ifdef HAVE_LIBINTL
#include <libintl.h>
#define _(String) gettext(String)
#else
#define _(String) (String)
#endif
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include "gnome.h"
#include "../panel_cmds.h"
#include "../applet_cmds.h"
#include "../panel.h"

#include "libgnomeui/gnome-session.h"


#define APPLET_ID "Menu"

#define MENU_PATH "menu_path"

typedef struct {
	GtkWidget *button;
	char *path;
	GList *small_icons;
	int show_small_icons;
} Menu;

typedef struct {
	GtkWidget *dialog;
	Menu *menu;
} Properties;


static char *gnome_folder;

static gint panel_pos=PANEL_POS_BOTTOM;

static PanelCmdFunc panel_cmd_func;

gpointer applet_cmd_func(AppletCommand *cmd);


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

void
setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title,
	GList ** small_icons)
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

	if (pixmap){
		gtk_container_add (GTK_CONTAINER (align), pixmap);
		gtk_widget_set_usize (align, 22, 16);
	} else
		gtk_widget_set_usize (align, 22, 16);


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
	PanelCommand cmd;
	
	cmd.cmd = PANEL_CMD_CREATE_APPLET;
	cmd.params.create_applet.id     = applet;
	cmd.params.create_applet.params = arg;
	cmd.params.create_applet.xpos   = PANEL_UNKNOWN_APPLET_POSITION;
	cmd.params.create_applet.ypos   = PANEL_UNKNOWN_APPLET_POSITION;

	(*panel_cmd_func) (&cmd);
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
	add_to_panel (APPLET_ID, data);
}

GtkWidget *
create_menu_at (GtkWidget *window, char *menudir, int create_app_menu,
	GList ** small_icons)
{	
	GnomeDesktopEntry *item_info;
	GtkWidget *menu;
	struct dirent *dent;
	struct stat s;
	char *filename;
	DIR *dir;
	int  items = 0;
	
	dir = opendir (menudir);
	if (dir == NULL)
		return NULL;

	menu = gtk_menu_new ();
	
	while ((dent = readdir (dir)) != NULL){
		GtkWidget     *menuitem, *sub, *pixmap;
		GtkSignalFunc activate_func;
		char          *thisfile, *pixmap_name;
		char          *p;

		thisfile = dent->d_name;
		/* Skip over . and .. */
		if ((thisfile [0] == '.' && thisfile [1] == 0) ||
		    (thisfile [0] == '.' && thisfile [1] == '.' && thisfile [2] == 0))
			continue;

		filename = g_concat_dir_and_file (menudir, thisfile);
		if (stat (filename, &s) == -1){
			g_free (filename);
			continue;
		}

		sub = 0;
		item_info = 0;
		if (S_ISDIR (s.st_mode)){
			sub = create_menu_at (window, filename,
					      create_app_menu, small_icons);
			if (!sub){
				g_free (filename);
				continue;
			}
			/* just for now */
			pixmap_name = NULL;

			if (create_app_menu){
				GtkWidget *pixmap = NULL;
				char *text;

				text = g_copy_strings ("Menu: ", thisfile, NULL);

				menuitem = gtk_menu_item_new ();
				gtk_menu_prepend (GTK_MENU (sub), menuitem);
				gtk_widget_show (menuitem);
				
				menuitem = gtk_menu_item_new ();
				if (gnome_folder){
					pixmap =gnome_create_pixmap_widget (window, menuitem, gnome_folder);
					gtk_widget_show (pixmap);
				}
				setup_menuitem (menuitem, pixmap, text,
					small_icons);
				g_free (text);
				text = g_strdup (filename);
				gtk_menu_prepend (GTK_MENU (sub), menuitem);
				gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
						    (GtkSignalFunc) add_dir_to_panel,
						    text);
				gtk_signal_connect (GTK_OBJECT (menuitem), "destroy",
						    (GtkSignalFunc) free_string,
						    text);
			}
		} else {
			if (strstr (filename, ".desktop") == 0){
				g_free (filename);
				continue;
			}
			item_info = gnome_desktop_entry_load (filename);
			if (!item_info){
				g_free (filename);
				continue;
			}
			pixmap_name = item_info->small_icon;

		}
		items++;
		menuitem = gtk_menu_item_new ();
		if (sub)
			gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), sub);

		pixmap = NULL;
		if (pixmap_name && g_file_exists (pixmap_name)){
			pixmap = gnome_create_pixmap_widget (window, menuitem,
							     pixmap_name);
			if (pixmap)
				gtk_widget_show (pixmap);
		}

		p = strstr(thisfile, ".desktop");
		if (p)
			*p = '\0';  /* Remove the .desktop part */
		
		setup_menuitem (menuitem, pixmap, thisfile, small_icons);

		gtk_menu_append (GTK_MENU (menu), menuitem);

		gtk_signal_connect (GTK_OBJECT (menuitem), "destroy",
				    (GtkSignalFunc) free_app_def, item_info);

		activate_func = create_app_menu ? (GtkSignalFunc) add_app_to_panel : (GtkSignalFunc) activate_app_def;
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate", activate_func, item_info);

		g_free (filename);
	}
	closedir (dir);

	if (items == 0){
		gtk_widget_destroy (menu);
		return 0;
	}
	return menu;
}

void
menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	GtkWidget *widget = (GtkWidget *) data;
	int wx, wy;
	
	gdk_window_get_origin (widget->window, &wx, &wy);

	switch(panel_pos) {
		case PANEL_POS_TOP:
			*x = wx;
			*y = wy + widget->allocation.height;
			break;
		case PANEL_POS_BOTTOM:
			*x = wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
			break;
		case PANEL_POS_LEFT:
			*x = wx + widget->allocation.width;
			*y = wy;
			break;
		case PANEL_POS_RIGHT:
			*x = wx - GTK_WIDGET (menu)->allocation.width;
			*y = wy;
			break;
	}
	if(*x + GTK_WIDGET (menu)->allocation.width > gdk_screen_width())
		*x=gdk_screen_width() - GTK_WIDGET (menu)->allocation.width;
	if(*y + GTK_WIDGET (menu)->allocation.height > gdk_screen_height())
		*y=gdk_screen_height() - GTK_WIDGET (menu)->allocation.height;
}

void
activate_menu (GtkWidget *widget, void *closure)
{
	GtkMenu *menu = closure;

	gtk_menu_popup (GTK_MENU (menu), 0, 0, menu_position, widget,
			1, 0);
}

void
panel_configure (GtkWidget *widget, void *data)
{
	PanelCommand cmd;

	cmd.cmd = PANEL_CMD_PROPERTIES;

	(*panel_cmd_func) (&cmd);
}

/* FIXME: panel is dynamicly configured! so we shouldn't need this*/
/*
void
panel_reload (GtkWidget *widget, void *data)
{
	fprintf(stderr, "Panel reload not yet implemented\n");
}*/

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
add_applet_to_panel(GtkWidget *widget, gpointer data)
{
	add_to_panel(gtk_object_get_user_data(GTK_OBJECT(widget)),
		     NULL); /* NULL means request default params */
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
	GList     **small_icons;

	ai = data;
	menu = GTK_MENU(user_data);
	small_icons = gtk_object_get_user_data(GTK_OBJECT(menu));

	oid = g_strdup(ai->original_id);

	menuitem = gtk_menu_item_new();
	setup_menuitem(menuitem, NULL, ai->translated,small_icons);
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
create_applets_menu(GList ** small_icons)
{
	GtkWidget    *menu;
	GList        *list;
	GList        *applets_list;
	PanelCommand  cmd;

	/* Get list of applet types */

	cmd.cmd = PANEL_CMD_GET_APPLET_TYPES;
	list = (*panel_cmd_func) (&cmd);

	/* Now translate and sort them */

	applets_list = NULL;
	g_list_foreach(list, munge_applet_item, &applets_list);

	/* Create a menu of the translated and sorted ones */

	g_list_free(list);

	menu = gtk_menu_new();
	gtk_object_set_user_data(GTK_OBJECT(menu),(gpointer)small_icons);

	g_list_foreach(applets_list, append_applet_item_to_menu, menu);

	/* Destroy the list (the list items have already been freed by
	 * append_applet_item_to_menu()), and return the finished menu.
	 */

	g_list_free(applets_list);
	return menu;
}

static GtkWidget *
create_panel_submenu (GtkWidget *app_menu, GList ** small_icons)
{
	GtkWidget *menu, *menuitem;

	menu = gtk_menu_new ();
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add to panel"),small_icons);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), app_menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Add applet"),small_icons);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
		create_applets_menu(small_icons));

	add_menu_separator(menu);
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Configure"),small_icons);
	gtk_menu_append (GTK_MENU (menu), menuitem);
        gtk_signal_connect (GTK_OBJECT (menuitem), "activate", (GtkSignalFunc) panel_configure, 0);

	/*FIXME: this is not needed, or is it?, so take it out unless we
	  do need it!
	*/
	/*menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Reload configuration"),small_icons);
	gtk_menu_append (GTK_MENU (menu), menuitem);
        gtk_signal_connect (GTK_OBJECT (menuitem), "activate", (GtkSignalFunc) panel_reload, 0);*/

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
	PanelCommand cmd;

	cmd.cmd = PANEL_CMD_QUIT;
	(*panel_cmd_func) (&cmd);
}

static void
add_special_entries (GtkWidget *menu, GtkWidget *app_menu, GList ** small_icons)
{
	GtkWidget *menuitem;
	
	/* Panel entry */

	add_menu_separator (menu);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Panel"),small_icons);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_panel_submenu (app_menu,small_icons));

	add_menu_separator (menu);
	
	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Lock screen"),small_icons);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate", (GtkSignalFunc) panel_lock, 0);

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Log out"),small_icons);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate", (GtkSignalFunc) panel_logout, 0);
}

static GtkWidget *
create_panel_menu (GtkWidget *window, char *menudir, int main_menu,
	GList ** small_icons)
{
	GtkWidget *button;
	GtkWidget *pixmap;
	GtkWidget *menu;
	GtkWidget *app_menu;
	
	char *pixmap_name;

	if (main_menu)
		switch(panel_pos) {
			case PANEL_POS_TOP:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-down.xpm");
				break;
			case PANEL_POS_BOTTOM:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-up.xpm");
				break;
			case PANEL_POS_LEFT:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-right.xpm");
				break;
			case PANEL_POS_RIGHT:
				pixmap_name = gnome_unconditional_pixmap_file ("gnome-menu-left.xpm");
				break;
		}
	else
		/*FIXME: these guys need arrows as well*/
		pixmap_name = gnome_unconditional_pixmap_file ("panel-folder.xpm");
		
	/* main button */
	button = gtk_button_new ();

	/*make the pixmap*/
	pixmap = gnome_create_pixmap_widget (window, button, pixmap_name);
	gtk_widget_show(pixmap);

	/* put pixmap in button */
	gtk_container_add (GTK_CONTAINER(button), pixmap);
	gtk_widget_show (button);

	menu = create_menu_at (window, menudir, 0, small_icons);
	if (main_menu) {
		app_menu = create_menu_at (window, menudir, 1, small_icons);
		add_special_entries (menu, app_menu, small_icons);
	}
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (activate_menu), menu);

	g_free (pixmap_name);
	return button;
}

static GtkWidget *
create_menu_widget (GtkWidget *window, char *arguments, char *menudir,
	GList ** small_icons)
{
	GtkWidget *menu;
	int main_menu;
	
	main_menu = (strcmp (arguments, ".") == 0);
	menu = create_panel_menu (window, menudir, main_menu, small_icons);
	return menu;
}

static void
set_show_small_icons(gpointer data, gpointer user_data)
{
	GtkWidget *w = data;
	if(!w) {
		g_warning("Internal error in set_show_small_icons (!w)");
		return;
	}
	if(*(int *)user_data)
		gtk_widget_show(w);
	else
		gtk_widget_hide(w);
}


static void
create_instance (Panel *panel, char *params, int xpos, int ypos)
{
	char *menu_base = gnome_unconditional_datadir_file ("apps");
	char *this_menu;
	char *p;
	Menu *menu;
	PanelCommand cmd;
	int show_small_icons;

	if (!getenv ("PATH"))
		return;

	if(!params)
		return;

	/*parse up the params*/
	p = strchr(params,':');
	show_small_icons = TRUE;
	if(p) {
		*(p++)='\0';
		if(*(p++)=='0')
			show_small_icons = FALSE;
	}

	if (*params == '/')
		this_menu = strdup (params);
	else 
		this_menu = g_concat_dir_and_file (menu_base, params);

	if (!g_file_exists (this_menu)){
		g_free (menu_base);
		g_free (this_menu);
		return;
	}

	gnome_folder = gnome_unconditional_pixmap_file ("gnome-folder-small.xpm");
	if (!g_file_exists (gnome_folder)){
		free (gnome_folder);
		gnome_folder = NULL;
	}

	menu = g_new(Menu,1);
	menu->small_icons = NULL;
	menu->button = create_menu_widget (panel->window, params, this_menu,
					   &(menu->small_icons));
	menu->path = g_strdup(params);
	menu->show_small_icons = show_small_icons;

	g_list_foreach(menu->small_icons,set_show_small_icons,
		       &show_small_icons);
	
	gtk_object_set_user_data(GTK_OBJECT(menu->button),menu);
	
	cmd.cmd = PANEL_CMD_REGISTER_TOY;
	cmd.params.register_toy.applet = menu->button;
	cmd.params.register_toy.id     = APPLET_ID;
	cmd.params.register_toy.xpos   = xpos;
	cmd.params.register_toy.ypos   = ypos;
	cmd.params.register_toy.flags  = APPLET_HAS_PROPERTIES;

	(*panel_cmd_func) (&cmd);
}

static void
set_orientation(GtkWidget *applet, Panel *panel)
{
	GtkWidget *pixmap;
	char *pixmap_name;
	Menu *menu;

	if(panel_pos==panel->pos)
		return;
	panel_pos=panel->pos;


	menu = gtk_object_get_user_data(GTK_OBJECT(applet));
	if(!menu || !menu->path)
		return;

	if (strcmp(menu->path,".")==0)
		switch(panel_pos) {
			case PANEL_POS_TOP:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-down.xpm");
				break;
			case PANEL_POS_BOTTOM:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-up.xpm");
				break;
			case PANEL_POS_LEFT:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-right.xpm");
				break;
			case PANEL_POS_RIGHT:
				pixmap_name = gnome_unconditional_pixmap_file(
					"gnome-menu-left.xpm");
				break;
		}
	else
		/*FIXME: these guys need arrows as well*/
		pixmap_name = gnome_unconditional_pixmap_file ("panel-folder.xpm");
		
	pixmap=GTK_BUTTON(applet)->child;
	gtk_container_remove(GTK_CONTAINER(applet),pixmap);
	gtk_widget_destroy(pixmap);

	/*make the pixmap*/
	pixmap = gnome_create_pixmap_widget (panel->fixed, applet, pixmap_name);

	gtk_container_add (GTK_CONTAINER(applet), pixmap);
	gtk_widget_show (pixmap);
	
	g_free(pixmap_name);
}

static void
properties_apply_callback(GtkWidget *widget, gpointer data)
{
	Properties *prop = data;
	Menu *menu;

	menu = gtk_object_get_user_data(GTK_OBJECT(prop->menu->button));
	if(!menu)
		return;

	menu->show_small_icons=prop->menu->show_small_icons;
	g_list_foreach(menu->small_icons,set_show_small_icons,
		       &(menu->show_small_icons));
}

static void
properties_close_callback(GtkWidget *widget, gpointer data)
{
	Properties *prop = data;

	gtk_widget_destroy(prop->dialog);
	g_free(prop->menu);
	g_free(prop);
}

static void 
set_toggle_button_value (GtkWidget *widget, gpointer data)
{
	if(GTK_TOGGLE_BUTTON(widget)->active)
		*(int *)data=TRUE;
	else
		*(int *)data=FALSE;
}

static void
properties(GtkWidget *applet)
{
	GtkWidget *table;
	GtkWidget *w;
	Menu *menu;
	Properties *prop;

	menu = gtk_object_get_user_data(GTK_OBJECT(applet));
	if(!menu)
		return;

	prop = g_new(Properties,1);
	prop->menu = g_new(Menu,1);

	prop->menu->button=menu->button;
	prop->menu->show_small_icons=menu->show_small_icons;

	/*make us a dialog*/
	prop->dialog = gtk_dialog_new();

	gtk_window_set_title(GTK_WINDOW(prop->dialog), _("Menu properties"));
	gtk_window_position(GTK_WINDOW(prop->dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_policy(GTK_WINDOW(prop->dialog), FALSE, FALSE, TRUE);
	
	table = gtk_table_new(1, 2, FALSE);
	gtk_container_border_width(GTK_CONTAINER(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(prop->dialog)->vbox), table,
		FALSE, FALSE, 0);
	gtk_widget_show(table);

	/*the different properties go here*/
	w = gtk_check_button_new_with_label(_("Enable small icons in menu"));
	gtk_signal_connect (GTK_OBJECT (w), "clicked", 
			    GTK_SIGNAL_FUNC (set_toggle_button_value), 
			    &(prop->menu->show_small_icons));
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w),
		prop->menu->show_small_icons ? TRUE : FALSE);
	gtk_table_attach(GTK_TABLE(table), w,
			 0, 2, 0, 1,
			 GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			 GTK_FILL | GTK_SHRINK,
			 0, 0);
	gtk_widget_show(w);

	/*close and apply buttons*/
	gtk_container_border_width(
		GTK_CONTAINER(GTK_DIALOG(prop->dialog)->action_area), 4);
	
	w = gtk_button_new_with_label(_("Close"));
	gtk_signal_connect(GTK_OBJECT(w), "clicked",
			   GTK_SIGNAL_FUNC (properties_close_callback),
			   prop);
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(prop->dialog)->action_area),
			 w,TRUE,TRUE,0);
	gtk_widget_show(w);

	w = gtk_button_new_with_label(_("Apply"));
	gtk_signal_connect(GTK_OBJECT(w), "clicked",
			   GTK_SIGNAL_FUNC(properties_apply_callback),
			   prop);
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(prop->dialog)->action_area),
			 w, TRUE, TRUE, 0);
	gtk_widget_show(w);

	gtk_signal_connect(GTK_OBJECT(prop->dialog), "delete_event",
			   GTK_SIGNAL_FUNC(properties_close_callback),
			   prop);
	gtk_widget_show(prop->dialog);
}

gpointer
applet_cmd_func(AppletCommand *cmd)
{
	Menu *menu;
	g_assert(cmd != NULL);


	switch (cmd->cmd) {
		case APPLET_CMD_QUERY:
			return APPLET_ID;

		case APPLET_CMD_INIT_MODULE:
			panel_cmd_func = cmd->params.init_module.cmd_func;
			break;

		case APPLET_CMD_DESTROY_MODULE:
			break;

		case APPLET_CMD_GET_DEFAULT_PARAMS:
			return g_strdup(".:1");

		case APPLET_CMD_CREATE_INSTANCE:
			create_instance(cmd->panel,
					cmd->params.create_instance.params,
					cmd->params.create_instance.xpos,
					cmd->params.create_instance.ypos);
			break;

		case APPLET_CMD_GET_INSTANCE_PARAMS:
			menu = gtk_object_get_user_data(
				GTK_OBJECT(cmd->applet));
			if(!menu) return NULL;
			return g_copy_strings(menu->path,":",
					      menu->show_small_icons?"1":"0",
					      NULL);

		case APPLET_CMD_ORIENTATION_CHANGE_NOTIFY:
			set_orientation(cmd->applet,cmd->panel);
			break;

		case APPLET_CMD_PROPERTIES:
			properties(cmd->applet);
			break;

		default:
			fprintf(stderr,
				APPLET_ID " applet_cmd_func: Oops, unknown command type %d\n",
				(int) cmd->cmd);
			break;
	}
	return NULL;
}


#if 0
main (int argc, char *argv [])
{
	GtkWidget *window;
	GtkWidget *thing;
	void *lib_handle;
	char *f;
	void *(*init_ptr)(GtkWidget *, char *);

	gtk_init (&argc, &argv);
	gnome_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	thing = init (window, ".");
	if (!thing)
		return printf ("Module was not initialized\n");
	
	gtk_container_add (GTK_CONTAINER (window), (GtkWidget *) thing);
	gtk_widget_set_usize (window, 48, 48);
	gtk_window_set_policy (window, 0, 0, 1);
	gtk_widget_show (window);
	gtk_widget_realize (window);
	gtk_main ();
	
	return 0;
}
#endif
