/* Gnome panel: panel functionality
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 */

#include <config.h>
#include <string.h>
#include "gnome.h"

#include "panel-widget.h"

#include "gdkextra.h"
#include "panel.h"
#include "menu.h"
#include "panel_config.h"

#include <gdk/gdkx.h>

static GtkWidget *applet_menu;
static GtkWidget *applet_menu_remove_item;
static GtkWidget *applet_menu_prop_separator;
static GtkWidget *applet_menu_prop_item;

/*FIXME: get rid of this, menu will be part of panel not an applet*/
static menu_count=0; /*how many "menu" applets we have ....*/
			/*FIXME: this should only count "main" menus!*/

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

extern GList *panels;
extern GList *drawers;
extern GList *applets;

extern GtkTooltips *panel_tooltips;
extern gint tooltips_enabled;

extern GnomeClient *client;

/*FIXME: integrate with menu.[ch]*/
extern GtkWidget *root_menu;
extern GList *small_icons;

static void
properties(PanelWidget *panel)
{
	panel_config(panel);
}



static void
get_applet_geometry(GtkWidget *applet, int *x, int *y, int *width, int *height)
{
	if (x)
		*x = applet->allocation.x;

	if (y)
		*y = applet->allocation.y;

	if (width)
		*width = applet->allocation.width;

	if (height)
		*height = applet->allocation.height;
}

/*FIXME this should be somehow done through signals and panel-widget*/
static void
applet_orientation_notify(GtkWidget *widget, gpointer data)
{
}

static void
save_applet_configuration(gpointer data, gpointer user_data)
{
	char          *path;
	char          *fullpath;
	char           buf[256];
	AppletInfo    *info = data;
	int           *num = user_data;
	int            pos;
	int            panel;
	GList         *list;

	pos = -1;
	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
	    	if((pos=panel_widget_get_pos(PANEL_WIDGET(list->data),
	    				     info->widget))!=-1)
			break; 

	/*not found*/
	if(pos == -1)
		return;

	sprintf(buf, "_%d/", (*num)++);
	path = g_copy_strings("/panel/Applet", buf, NULL);

	fullpath = g_copy_strings(path,"id",NULL);
	gnome_config_set_string(fullpath, info->id);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"position",NULL);
	gnome_config_set_int(fullpath, pos);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"panel",NULL);
	gnome_config_set_int(fullpath, panel);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"parameters",NULL);
	gnome_config_set_string(fullpath, info->params);
	g_free(fullpath);

	g_free(path);
}

static void
save_panel_configuration(gpointer data, gpointer user_data)
{
	char          *path;
	char          *fullpath;
	char           buf[256];
	int           *num = user_data;
	PanelWidget   *panel = data;

	sprintf(buf, "_%d/", (*num)++);
	path = g_copy_strings("/panel/Panel", buf, NULL);

	fullpath = g_copy_strings(path,"orient",NULL);
	gnome_config_set_int(fullpath,panel->orient);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"snapped",NULL);
	gnome_config_set_int(fullpath,panel->snapped);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"mode",NULL);
	gnome_config_set_int(fullpath,panel->mode);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"state",NULL);
	gnome_config_set_int(fullpath,panel->state);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"step_size",NULL);
	gnome_config_set_int(fullpath,panel->step_size);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"minimized_size",NULL);
	gnome_config_set_int(fullpath,panel->minimized_size);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"minimize_delay",NULL);
	gnome_config_set_int(fullpath,panel->minimize_delay);
	g_free(fullpath);

	g_free(path);
}

static void
save_drawer_configuration(gpointer data, gpointer user_data)
{
	/*FIXME: drawers*/
	/*char          *path;
	char          *fullpath;
	char           buf[256];
	GtkWidget     *widget = data;
	int           *num = user_data;
	
	sprintf(buf, "_%d/", (*num)++);
	path = g_copy_strings("/panel/Drawer", buf, NULL);

	fullpath = g_copy_strings(path,"name",NULL);
	gnome_config_set_string(fullpath, drawer->name);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"iconopen",NULL);
	gnome_config_set_string(fullpath, drawer->iconopen);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"iconclosed",NULL);
	gnome_config_set_string(fullpath, drawer->iconclosed);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"geometry",NULL);
	gnome_config_set_int(fullpath, pos);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"step_size",NULL);
	gnome_config_set_int(fullpath, drawer->panel->step_size);
	g_free(fullpath);

	g_free(path);*/
}


static void
destroy_widget_list(gpointer data, gpointer user_data)
{
	gtk_widget_destroy(GTK_WIDGET(data));
}

/* This is called when the session manager requests a shutdown.  It
   can also be run directly when we don't detect a session manager.
   We assume no interaction is done by the applets.  And we ignore the
   other arguments for now.  Yes, this is lame.  */
gint
panel_session_save (GnomeClient *client,
		    gint phase,
		    GnomeSaveStyle save_style,
		    gint is_shutdown,
		    GnomeInteractStyle interact_style,
		    gint is_fast,
		    gpointer client_data)
{
	gint num;
	gint drawernum;
	char buf[256];

	for(num=gnome_config_get_int("/panel/Config/applet_count=0");
		num>0;num--) {
		sprintf(buf,"/panel/Applet_%d",num);
		gnome_config_clean_section(buf);
	}
	for(num=gnome_config_get_int("/panel/Config/drawer_count=0");
		num>0;num--) {
		sprintf(buf,"/panel/Drawer_%d",num);
		gnome_config_clean_section(buf);
	}
	for(num=gnome_config_get_int("/panel/Config/panel_count=0");
		num>0;num--) {
		sprintf(buf,"/panel/Panel_%d",num);
		gnome_config_clean_section(buf);
	}

	num = 1;
	g_list_foreach(applets,save_applet_configuration,&num);
	gnome_config_set_int("/panel/Config/applet_count",num-1);
	num = 1;
	/* g_list_foreach(drawers,save_drawer_configuration,&num); */
	gnome_config_set_int("/panel/Config/drawer_count",num-1);
	num = 1;
	g_list_foreach(panels,save_panel_configuration,&num);
	gnome_config_set_int("/panel/Config/panel_count",num-1);

	/*global options*/
	gnome_config_set_bool("/panel/Config/tooltips_enabled",
			      tooltips_enabled);

	gnome_config_sync();

	/*FIXME: tell applets to go kill themselves*/

	g_list_foreach(drawers,destroy_widget_list,NULL);
	g_list_foreach(panels,destroy_widget_list,NULL);

	gtk_widget_unref(applet_menu);
	gtk_widget_unref(panel_tooltips);

	/*FIXME: integrate with menu.[ch]*/
	small_icons = NULL; /*prevent searches through the g_list to speed
				up this thing*/

	/*FIXME: integrate with menu.[ch]*/
	gtk_widget_unref(root_menu);

	/*FIXME: unref all menus here */
	

	/* Always successful.  */
	return TRUE;
}

static void
panel_quit(void)
{
	if (! GNOME_CLIENT_CONNECTED (client)) {
		panel_session_save (client, 1, GNOME_SAVE_BOTH, 1,
				    GNOME_INTERACT_NONE, 0, NULL);
		gtk_main_quit ();
		/* We don't want to return, because we've probably been
		   called from an applet which has since been dlclose()'d,
		   and we'd end up with a SEGV when we tried to return to
		   the now-nonexistent code page. */
		exit(0);
	} else {
		/* We request a completely interactive, full, slow shutdown.  */
		gnome_client_request_save (client, GNOME_SAVE_BOTH, 1,
					   GNOME_INTERACT_ANY, 0, 1);
	}
}

/*FIXME: how will we handle adding of applets????*/
static void
create_applet(char *id, char *params, int pos, int panel)
{
}

static PanelWidget *
find_applet_panel(GtkWidget *applet)
{
	GList *list;

	for(list=panels;list!=NULL;list=g_list_next(list))
		if(panel_widget_get_pos(PANEL_WIDGET(list->data),applet)!=-1)
			break;
	if(!list)
		return NULL;
	return PANEL_WIDGET(list->data);
}

static void
move_applet_callback(GtkWidget *widget, gpointer data)
{
	AppletInfo     *info;
	PanelWidget    *panel;

	info = gtk_object_get_user_data(GTK_OBJECT(applet_menu));

	if(!(panel = find_applet_panel(info->widget)))
		return;

	panel_widget_applet_drag_start(panel,info->widget);
}


static void
remove_applet_callback(GtkWidget *widget, gpointer data)
{
	AppletInfo *info;
	gchar *id;
	gint pos;
	PanelWidget *panel;

	info = gtk_object_get_user_data(GTK_OBJECT(applet_menu));

	/*FIXME: this will go*/
	if(strcmp(info->id,"Menu")==0) {
		if(menu_count<=1)
			return;
		/*FIXME: do something to make the user aware that this was
		  wrong ... a message box maybe ... or a beep*/
		menu_count--;
	}
	applets=g_list_remove(applets,info);

	if(!(panel = find_applet_panel(info->widget)))
		return;

	panel_widget_remove(panel,info->widget);
	gtk_widget_unref(info->widget);
	if(info->assoc)
		gtk_widget_unref(info->assoc);

	g_free(info->id);
	if(info->params) g_free(info->params);
	g_free(info);
}


/*tell applet to do properties*/
static void
applet_properties_callback(GtkWidget *widget, gpointer data)
{
}

void
create_applet_menu(void)
{
	GtkWidget *menuitem;

	applet_menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("Remove from panel"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) remove_applet_callback,
			   NULL);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	applet_menu_remove_item = menuitem;

	
	menuitem = gtk_menu_item_new_with_label(_("Move applet"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) move_applet_callback,
			   NULL);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	applet_menu_prop_separator = menuitem;
	
	menuitem = gtk_menu_item_new_with_label(_("Applet properties..."));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) applet_properties_callback,
			   NULL);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	applet_menu_prop_item = menuitem;
}


static void
show_applet_menu(AppletInfo *info)
{
	if (info->flags & APPLET_HAS_PROPERTIES) {
		gtk_widget_show(applet_menu_prop_separator);
		gtk_widget_show(applet_menu_prop_item);
	} else {
		gtk_widget_hide(applet_menu_prop_separator);
		gtk_widget_hide(applet_menu_prop_item);
	}

	/*FIMXE: this should go*/
	if(strcmp(info->id,"Menu")!=0 || menu_count>1)
		gtk_widget_show(applet_menu_remove_item);
	else
		gtk_widget_hide(applet_menu_remove_item);
	gtk_object_set_user_data(GTK_OBJECT(applet_menu), info);

	gtk_menu_popup(GTK_MENU(applet_menu), NULL, NULL, NULL, NULL, 3, time(NULL));
	/*FIXME: make it pop-up on some title bar of the applet menu or
	  somehow avoid pressing remove applet being under the cursor!*/
}

static gint
applet_button_press(GtkWidget *widget,GdkEventButton *event, gpointer data)
{
	if(event->button==3) {
		show_applet_menu((AppletInfo *)data);
		return TRUE;
	}
	return FALSE;
}

static void
panel_properties_callback(GtkWidget *widget, gpointer data)
{
	properties(PANEL_WIDGET(data));
}

static void
panel_log_out_callback(GtkWidget *widget, gpointer data)
{
	panel_quit();
}

static void
add_main_menu(GtkWidget *widget, gpointer data)
{
	PanelWidget *panel = data;
	/*FIXME: 1) doesn't work at all, 2)should add to current panel*/
	create_applet("Menu",".:1",PANEL_UNKNOWN_APPLET_POSITION,1);
}

/*FIXME: add a function that does this, so generalize register_toy for this*/
static void
add_reparent(GtkWidget *widget, gpointer data)
{
	int id;
	GtkWidget *eb;
	GdkWindow *win;
	int w,h;

	puts("Enter window ID to reparent:");
	scanf("%d",&id);

	eb = gtk_event_box_new();
	gtk_widget_show(eb);

	win = gdk_window_foreign_new(id);
	gdk_window_get_size(win,&w,&h);
	gtk_widget_set_usize(eb,w,h);

	panel_widget_add(PANEL_WIDGET(panels->data), eb, 0);

	gdk_window_reparent(win,eb->window,0,0);
}

GtkWidget *
create_panel_root_menu(PanelWidget *panel)
{
	GtkWidget *menuitem;
	GtkWidget *panel_menu;

	panel_menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("Panel properties..."));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) panel_properties_callback,
			   panel);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Add reparent"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_reparent,
			   panel);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Add main menu applet"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_main_menu,
			   panel);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);
	
	menuitem = gtk_menu_item_new_with_label(_("Log out"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) panel_log_out_callback,
			   NULL);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	return panel_menu;
}


static void
set_tooltip(GtkWidget *applet, char *tooltip)
{
	if(!applet)
		return;
	gtk_tooltips_set_tips(panel_tooltips,applet,tooltip);
}


void
register_toy(GtkWidget *applet,
	     GtkWidget * assoc,
	     char *id,
	     char *params,
	     int pos,
	     int panel,
	     long flags,
	     AppletType type)
{
	GtkWidget     *eventbox;
	AppletInfo    *info;
	
	g_assert(applet != NULL);
	g_assert(id != NULL);

	/* We wrap the applet in a GtkEventBox so that we can capture events over it */

	eventbox = gtk_event_box_new();
	gtk_widget_set_events(eventbox, gtk_widget_get_events(eventbox) |
			      APPLET_EVENT_MASK);
	gtk_container_add(GTK_CONTAINER(eventbox), applet);

	info = g_new(AppletInfo,1);

	info->flags = flags;
	info->widget = eventbox;
	info->type = type;
	info->assoc = assoc;
	info->id = g_strdup(id);
	info->params = g_strdup(params);

	gtk_object_set_user_data(GTK_OBJECT(eventbox),info);

	if(pos==PANEL_UNKNOWN_APPLET_POSITION)
		pos = 0;
	panel_widget_add(PANEL_WIDGET(g_list_nth(panels,panel)->data),
			 eventbox, pos);

	gtk_widget_show(applet);
	gtk_widget_show(eventbox);

	applets = g_list_append(applets,info);

	gtk_signal_connect(GTK_OBJECT(eventbox),
			   "button_press_event",
			   GTK_SIGNAL_FUNC(applet_button_press),
			   info);

	/*notify the applet of the orientation of the panel!*/
	/*applet_orientation_notify(eventbox,NULL);*/

	if(strcmp(id,"Menu")==0)
		menu_count++;

	printf ("The window id for %s is: %d\n",id, GDK_WINDOW_XWINDOW (eventbox->window));
}

static void
create_drawer(char *name, char *iconopen, char* iconclosed, int step_size,
	int pos, int drawer)
{
	/*FIXME: add drawers*/
	return;
}
