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
#include "panel_config_global.h"
#include <gdk/gdkx.h>

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

/*FIXME: THESE CANNOT REALLY BE LINKED LISTS!! THEY SHOULD BE ARRAYS*/
extern GList *panels;
extern GList *applets;

extern GtkTooltips *panel_tooltips;
extern gint tooltips_enabled;

extern GnomeClient *client;

extern GtkWidget *root_menu;
extern GList *small_icons;

extern GlobalConfig global_config;

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

static void
apply_global_config_to_panel(gpointer data, gpointer user_data)
{
	PanelWidget *panel = data;

	if(panel->mode == PANEL_AUTO_HIDE)
		panel->step_size = global_config.auto_hide_step_size;
	else
		panel->step_size = global_config.explicit_hide_step_size;
	panel->minimize_delay = global_config.minimize_delay;
	panel->minimized_size = global_config.minimized_size;
}

void
apply_global_config(void)
{
	g_list_foreach(panels,apply_global_config_to_panel,NULL);
	if(global_config.tooltips_enabled)
		gtk_tooltips_enable(panel_tooltips);
	else
		gtk_tooltips_disable(panel_tooltips);
	g_list_foreach(small_icons,set_show_small_icons,NULL);
	
}

static gint
find_panel(PanelWidget *panel)
{
	gint i;
	GList *list;

	for(i=0,list=panels;list!=NULL;list=g_list_next(list),i++)
		if(list->data == panel)
			return i; 
	return -1;
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

	/*obviously no need for saving*/
	if(info->type==APPLET_EXTERN_PENDING && info->type==APPLET_EMPTY)
		return;

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

	if(info->type==APPLET_EXTERN) {
		fullpath = g_copy_strings(path,"id",NULL);
		gnome_config_set_string(fullpath, EXTERN_ID);
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

		/*have the applet do it's own session saving*/
		send_applet_session_save(info->id,(*num)-2,path);
	} else {
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
		if(strcmp(info->id,DRAWER_ID) == 0) {
			int i;

			i = find_panel(PANEL_WIDGET(info->assoc));
			if(i>=0)
				gnome_config_set_int(fullpath,i);
			else
				g_warning("Drawer not associated with applet!");
		} else {
			if(info->params)
				gnome_config_set_string(fullpath, info->params);
		}
		g_free(fullpath);
	}

	g_free(path);
}

static void
save_panel_configuration(gpointer data, gpointer user_data)
{
	char          *path;
	char          *fullpath;
	char           buf[256];
	int            x,y;
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

	fullpath = g_copy_strings(path,"minimized_size",NULL);
	gnome_config_set_int(fullpath,panel->minimized_size);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"minimize_delay",NULL);
	gnome_config_set_int(fullpath,panel->minimize_delay);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"size",NULL);
	gnome_config_set_int(fullpath,panel->size);
	g_free(fullpath);

	/*FIXME: this should be allocation.[xy] but those don't work!!!
	  probably a gtk bug*/
	gdk_window_get_origin(GTK_WIDGET(panel)->window,&x,&y);
	fullpath = g_copy_strings(path,"position_x",NULL);
	gnome_config_set_int(fullpath,x);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"position_y",NULL);
	gnome_config_set_int(fullpath,y);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"drawer_drop_zone_pos",NULL);
	gnome_config_set_int(fullpath,panel->drawer_drop_zone_pos);
	g_free(fullpath);

	g_free(path);
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
	g_list_foreach(panels,save_panel_configuration,&num);
	gnome_config_set_int("/panel/Config/panel_count",num-1);

	/*global options*/
	gnome_config_set_int("/panel/Config/auto_hide_step_size",
			     global_config.auto_hide_step_size);
	gnome_config_set_int("/panel/Config/explicit_hide_step_size",
			     global_config.explicit_hide_step_size);
	gnome_config_set_bool("/panel/Config/tooltips_enabled",
			      global_config.tooltips_enabled);
	gnome_config_set_bool("/panel/Config/show_small_icons",
			      global_config.show_small_icons);

	gnome_config_sync();

	if(is_shutdown) {
		GList *list;
		int i;
		AppletInfo *info;

		for(i=0,list=applets;list!=NULL;list = g_list_next(list),i++) {
			info = list->data;
			if(info->type == APPLET_EXTERN) {
				printf("SHUTTING DOWN EXTERN (%d)\n",i);
				send_applet_shutdown_applet(info->id,i);
				puts("DONE");
			}
			if(info->menu)
				gtk_widget_unref(info->menu);
		}

		g_list_foreach(panels,destroy_widget_list,NULL);

		gtk_object_unref(GTK_OBJECT (panel_tooltips));

		small_icons = NULL;
			/*prevent searches through the g_list to speed
					up this thing*/

		puts("unreffing root menu");
		gtk_widget_unref(root_menu);
		puts("done");

		/*FIXME: unref all menus here */
	}

	puts("AFTER_SESSION_SAVE");
	

	/* Always successful.  */
	return TRUE;
}

void
panel_quit(void)
{
	if (! GNOME_CLIENT_CONNECTED (client)) {
		panel_session_save (client, 1, GNOME_SAVE_BOTH, 1,
				    GNOME_INTERACT_NONE, 0, NULL);
		puts("BEFORE_GTK_MAIN_QUIT");
		gtk_main_quit ();
		puts("AFTER_GTK_MAIN_QUIT");
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

	info = data;

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

	info = data;

	if(info->type == APPLET_EXTERN)
		send_applet_shutdown_applet(info->id,info->applet_id);
	info->type = APPLET_EMPTY;

	if(!(panel = find_applet_panel(info->widget)))
		return;

	panel_widget_remove(panel,info->widget);
	gtk_widget_unref(info->widget);
	/* this should be handeled by the applet itself (hopefully)
	if(info->assoc)
		gtk_widget_unref(info->assoc);
	*/
	if(info->menu)
		gtk_widget_unref(info->menu);

	g_free(info->id);
	info->id=NULL;
	if(info->params) g_free(info->params);
	info->params=NULL;
}

static void
applet_callback_callback(GtkWidget *widget, gpointer data)
{
	AppletUserMenu *menu = data;

	if(menu->info->type == APPLET_EXTERN) {
		send_applet_do_callback(menu->info->id,
					menu->info->applet_id,
					menu->name);
	} else if(menu->info->type != APPLET_EXTERN_PENDING &&
	   menu->info->type != APPLET_EMPTY) {
		/*handle internal applet callbacks here*/
		;
	}
}

/*the menu is not used for corba applets right now, but it might be used
  if invoked from some other place*/
static GtkWidget *
create_applet_menu(AppletInfo *info, GList *user_menu)
{
	GtkWidget *menuitem;
	GtkWidget *applet_menu;

	applet_menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("Remove from panel"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) remove_applet_callback,
			   info);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	
	menuitem = gtk_menu_item_new_with_label(_("Move applet"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) move_applet_callback,
			   info);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	
	if(user_menu) {
		menuitem = gtk_menu_item_new();
		gtk_menu_append(GTK_MENU(applet_menu), menuitem);
		gtk_widget_show(menuitem);
	}

	for(;user_menu!=NULL;user_menu = g_list_next(user_menu)) {
		AppletUserMenu *menu=user_menu->data;
		menuitem = gtk_menu_item_new_with_label(menu->text);
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				   (GtkSignalFunc) applet_callback_callback,
				   menu);
		gtk_menu_append(GTK_MENU(applet_menu), menuitem);
		gtk_widget_show(menuitem);
	}

	return applet_menu;
}

static void
show_applet_menu(AppletInfo *info)
{
	if (!info->menu)
		info->menu = create_applet_menu(info,info->user_menu);

	gtk_menu_popup(GTK_MENU(info->menu), NULL, NULL, NULL, NULL, 0/*3*/, time(NULL));
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
	panel_config(PANEL_WIDGET(data));
}

static void
panel_global_properties_callback(GtkWidget *widget, gpointer data)
{
	panel_config_global();
}

static void
panel_log_out_callback(GtkWidget *widget, gpointer data)
{
	panel_quit();
}

void
applet_show_menu(int id)
{
	AppletInfo *info = g_list_nth(applets,id)->data;
	show_applet_menu(info);
}


int
applet_get_panel(int id)
{
	return 0; /*FIXME*/
}

int
applet_get_pos(int id)
{
	return 0; /*FIXME*/
}

void
applet_drag_start(int id)
{
	AppletInfo *info = g_list_nth(applets,id)->data;
	PanelWidget *panel = find_applet_panel(info->widget);

	panel_widget_applet_drag_start_no_grab(panel,info->widget);
	panel_widget_applet_move_use_idle(panel);
}

void
applet_drag_stop(int id)
{
	AppletInfo *info = g_list_nth(applets,id)->data;
	PanelWidget *panel = find_applet_panel(info->widget);

	panel_widget_applet_drag_end_no_grab(panel);
}

void
applet_add_callback(short id, char *callback_name, char *menuitem_text)
{
	AppletUserMenu *menu = g_new(AppletUserMenu,1);
	AppletInfo *info = g_list_nth(applets,id)->data;

	menu->name = g_strdup(callback_name);
	menu->text = g_strdup(menuitem_text);
	menu->info = info;

	/*make sure the menu is rebuilt*/
	if(info->menu) {
		gtk_widget_unref(info->menu);
		info->menu=NULL;
	}

	info->user_menu = g_list_append(info->user_menu,menu);
}

int
applet_request_id (const char * ior, const char *path, char **cfgpath)
{
	GtkWidget *eb;
	GdkWindow *win;
	GList *list;
	AppletInfo *info;
	int i;

	for(i=0,list=applets;list!=NULL;list=g_list_next(list),i++) {
		info = list->data;
		if(info && info->type == APPLET_EXTERN_PENDING &&
		   strcmp(info->params,path)==0) {
			*cfgpath = info->id;
			info->id = g_strdup(ior);
			/*we started this and already reserved a spot
			  for it, including the eventbox widget*/
			return i;
		}
	}

	reserve_applet_spot (ior, path, 0, 0);
	*cfgpath = g_strdup("");
	return i;
}

void
reparent_window_id (unsigned long winid, int id)
{
	GtkWidget *eb;
	GdkWindow *win;
	int w,h;
	AppletInfo *info;
	
	printf ("I got this window ID to reparent: %d\n", winid);
	/*FIXME: check for NULLS!*/
	info = (AppletInfo *)(g_list_nth(applets,id)->data);

	/*no longer pending*/
	info->type = APPLET_EXTERN;
	eb = info->widget;
	
	win = gdk_window_foreign_new(winid);
	gdk_window_get_size(win,&w,&h);
	printf ("setting window size to: %d %d\n", w, h);
	gtk_widget_set_usize(eb,w,h);

	gdk_window_reparent(win,eb->window,0,0);
	
	printf ("leaving reparent\n");
}

void
reserve_applet_spot (const char *id, const char *path, int panel, int pos)
{
	GtkWidget *eb;
	GdkWindow *win;
	GList *list;

	printf ("entering reserve spot\n");
	
	eb = gtk_event_box_new();
	gtk_widget_show (eb);

	/*we save the ior in the id field of the appletinfo and the 
	  path in the params field*/
	register_toy(eb,NULL,NULL,g_strdup(id),g_strdup(path),
		     pos,panel,0,APPLET_EXTERN_PENDING);

	printf ("leaving reserve spot\n");
}

/*FIXME: add a function that does this, so generalize register_toy for this*/
/*static void
add_reparent(GtkWidget *widget, gpointer data)
{
	int id;
	int appletid;

	puts("** warning this will probably crash when saving session **");
	puts("Enter window ID to reparent:");
	scanf("%d",&id);

	appletid = reserve_applet_spot("???","echo",0,0);

	reparent_window_id (id,appletid);
}*/

GtkWidget *
create_panel_root_menu(PanelWidget *panel)
{
	GtkWidget *menuitem;
	GtkWidget *panel_menu;

	panel_menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("This panel properties..."));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) panel_properties_callback,
			   panel);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Global properties..."));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) panel_global_properties_callback,
			   panel);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);

	/*menuitem = gtk_menu_item_new_with_label(_("Add reparent (testing)"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_reparent,
			   panel);
	gtk_menu_append(GTK_MENU(panel_menu), menuitem);
	gtk_widget_show(menuitem);*/

	menuitem = gtk_menu_item_new_with_label(_("Main menu"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), root_menu);
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
	gtk_tooltips_set_tip (panel_tooltips,applet,tooltip,NULL);
}

void
register_toy(GtkWidget *applet,
	     GtkWidget * assoc,
	     gpointer data,
	     char *id,
	     char *params,
	     int pos,
	     int panel,
	     long flags,
	     AppletType type)
{
	GtkWidget     *eventbox;
	AppletInfo    *info;
	PanelWidget   *panelw;
	GList         *list;
	int            i;
	
	g_assert(applet != NULL);
	g_assert(id != NULL);

	panelw = PANEL_WIDGET(g_list_nth(panels,panel)->data);

	g_assert(panelw != NULL);
	/* We wrap the applet in a GtkEventBox so that we can capture events over it */

	eventbox = gtk_event_box_new();
	gtk_widget_set_events(eventbox, gtk_widget_get_events(eventbox) |
			      APPLET_EVENT_MASK);
	gtk_container_add(GTK_CONTAINER(eventbox), applet);

	info = g_new(AppletInfo,1);

	for(i=0,list=applets;list!=NULL;list = g_list_next(list),i++)
		;
	info->applet_id = i;
	info->type = type;
	info->widget = eventbox;
	info->assoc = assoc;
	info->menu = NULL;
	info->data = data;
	info->id = g_strdup(id);
	info->params = g_strdup(params);
	info->user_menu = NULL;

	gtk_object_set_user_data(GTK_OBJECT(eventbox),info);

	if(pos==PANEL_UNKNOWN_APPLET_POSITION)
		pos = 0;
	panel_widget_add(panelw, eventbox, pos);

	gtk_widget_show(applet);
	gtk_widget_show(eventbox);

	applets = g_list_append(applets,info);

	gtk_signal_connect(GTK_OBJECT(eventbox),
			   "button_press_event",
			   GTK_SIGNAL_FUNC(applet_button_press),
			   info);

	orientation_change(info,panelw);

	printf ("The window id for %s is: %d\n",id, GDK_WINDOW_XWINDOW (eventbox->window));
}
