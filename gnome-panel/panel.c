/* Gnome panel: panel functionality
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 *          George Lebl
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <gnome.h>

#include "panel-widget.h"
#include "gdkextra.h"
#include "panel.h"
#include "menu.h"
#include "mico-glue.h"
#include "panel_config.h"
#include "panel_config_global.h"
#include <gdk/gdkx.h>

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

/*extern GList *panels;*/
/*FIXME: THIS CANNOT REALLY BE A LINKED LIST!! THIS SHOULD BE AN ARRAY*/
extern GList *applets;

extern GtkTooltips *panel_tooltips;
extern gint tooltips_enabled;

extern GnomeClient *client;

extern GtkWidget *root_menu;
extern GList *small_icons;

extern GlobalConfig global_config;

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

#ifdef USE_INTERNAL_LAUNCHER
extern int launcher_pid;
#endif

void
apply_global_config(void)
{
	panel_widget_change_global(global_config.explicit_hide_step_size,
				   global_config.auto_hide_step_size,
				   global_config.minimized_size,
				   global_config.minimize_delay);
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
	char          *fullpath;
	char           path[256];
	AppletInfo    *info = data;
	int           *num = user_data;
	int            pos;
	int            panel;
	GList         *list;

	/*obviously no need for saving*/
	if(info->type==APPLET_EXTERN_PENDING ||
	   info->type==APPLET_EXTERN_RESERVED ||
	   info->type==APPLET_EMPTY)
		return;

	pos = -1;
	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
	    	if((pos=panel_widget_get_pos(PANEL_WIDGET(list->data),
	    				     info->widget))!=-1)
			break; 

	/*not found*/
	if(pos == -1)
		return;

	sprintf(path, "%sApplet_%d/", panel_cfg_path, (*num)++);

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

		/*sync before the applet does it's stuff*/
		gnome_config_sync();
		/*I think this should be done at sync and also that there
		  should be some flocking ... but this works for now*/
		gnome_config_drop_all();

		/*have the applet do it's own session saving*/
		send_applet_session_save(info->id_str,info->applet_id,path,
					 panel_cfg_path);
	} else {
		fullpath = g_copy_strings(path,"id",NULL);
		gnome_config_set_string(fullpath, info->id_str);
		g_free(fullpath);

		fullpath = g_copy_strings(path,"position",NULL);
		gnome_config_set_int(fullpath, pos);
		g_free(fullpath);

		fullpath = g_copy_strings(path,"panel",NULL);
		gnome_config_set_int(fullpath, panel);
		g_free(fullpath);

		fullpath = g_copy_strings(path,"parameters",NULL);
		if(strcmp(info->id_str,DRAWER_ID) == 0) {
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
}

static void
save_panel_configuration(gpointer data, gpointer user_data)
{
	char          *fullpath;
	char           path[256];
	int            x,y;
	int           *num = user_data;
	PanelWidget   *panel = data;

	sprintf(path, "%sPanel_%d/", panel_cfg_path, (*num)++);

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
/* update: some SM stuff implemented but we still ignore most of the
   arguments now*/
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
	char buf[256];
	char *session_id;

	session_id = gnome_client_get_id (client);
	if(session_id) {
		g_free(panel_cfg_path);
		panel_cfg_path = g_copy_strings("/panel-Session-",session_id,
						"/",NULL);
	}

	gnome_config_clean_file(panel_cfg_path);

	num = 1;
	g_list_foreach(applets,save_applet_configuration,&num);
	sprintf(buf,"%sConfig/applet_count",panel_cfg_path);
	gnome_config_set_int(buf,num-1);
	num = 1;
	g_list_foreach(panels,save_panel_configuration,&num);
	sprintf(buf,"%sConfig/panel_count",panel_cfg_path);
	gnome_config_set_int(buf,num-1);

	/*global options*/
	sprintf(buf,"%sConfig/auto_hide_step_size",panel_cfg_path);
	gnome_config_set_int(buf, global_config.auto_hide_step_size);
	sprintf(buf,"%sConfig/explicit_hide_step_size",panel_cfg_path);
	gnome_config_set_int(buf, global_config.explicit_hide_step_size);
	sprintf(buf,"%sConfig/minimized_size",panel_cfg_path);
	gnome_config_set_int(buf, global_config.minimized_size);
	sprintf(buf,"%sConfig/minimize_delay",panel_cfg_path);
	gnome_config_set_int(buf, global_config.minimize_delay);
	sprintf(buf,"%sConfig/tooltips_enabled",panel_cfg_path);
	gnome_config_set_bool(buf, global_config.tooltips_enabled);
	sprintf(buf,"%sConfig/show_small_icons",panel_cfg_path);
	gnome_config_set_bool(buf, global_config.show_small_icons);

	gnome_config_sync();

	if(is_shutdown) {
		GList *list;
		int i;
		AppletInfo *info;

		g_list_foreach(panels,destroy_widget_list,NULL);

		for(i=0,list=applets;list!=NULL;list = g_list_next(list),i++) {
			info = list->data;
			if(info->menu)
				gtk_widget_unref(info->menu);
			g_free(info);
		}

		gtk_object_unref(GTK_OBJECT (panel_tooltips));

		/*prevent searches through the g_list to speed
		  up this thing*/
		small_icons = NULL;

		gtk_widget_unref(root_menu);

		/*clean up corba stuff*/
		panel_corba_clean_up();

#ifdef USE_INTERNAL_LAUNCHER
		puts("killing launcher");
		kill(launcher_pid,SIGTERM);
#endif
		gtk_exit (0);
	}

	/* Always successful.  */
	return TRUE;
}

void
panel_quit(void)
{
	if (! GNOME_CLIENT_CONNECTED (client)) {
		panel_session_save (client, 1, GNOME_SAVE_BOTH, 1,
				    GNOME_INTERACT_NONE, 0, NULL);
	} else {
		/* We request a completely interactive, full, slow shutdown.  */
		gnome_client_request_save (client, GNOME_SAVE_BOTH, 1,
					   GNOME_INTERACT_ANY, 0, 1);
	}
}

static void
move_applet_callback(GtkWidget *widget, gpointer data)
{
	AppletInfo     *info;
	PanelWidget    *panel;

	info = data;

	if(!(panel = gtk_object_get_data(GTK_OBJECT(info->widget),
					 PANEL_APPLET_PARENT_KEY)))
		return;

	panel_widget_applet_drag_start(panel,info->widget);
}

static void
panel_clean_applet(AppletInfo *info)
{
	PanelWidget *panel;

	g_return_if_fail(info != NULL);

	info->type = APPLET_EMPTY;

	if(!(panel = gtk_object_get_data(GTK_OBJECT(info->widget),
					 PANEL_APPLET_PARENT_KEY)))
		return;

	panel_widget_remove(panel,info->widget);
	//gtk_widget_unref(info->widget);
	info->widget = NULL;
	if(info->type == APPLET_DRAWER && info->assoc) {
		panels = g_list_remove(panels,info->assoc);
		gtk_widget_unref(info->assoc);
	}
	if(info->menu)
		gtk_widget_unref(info->menu);
	info->menu = NULL;

	if(info->id_str) g_free(info->id_str);
	info->id_str=NULL;
	if(info->params) g_free(info->params);
	info->params=NULL;
}

static void
remove_applet_callback(GtkWidget *widget, gpointer data)
{
	AppletInfo *info = data;
	panel_clean_applet(info);
}

static void
applet_callback_callback(GtkWidget *widget, gpointer data)
{
	AppletUserMenu *menu = data;

	if(menu->info->type == APPLET_EXTERN) {
		send_applet_do_callback(menu->info->id_str,
					menu->info->applet_id,
					menu->name);
	} else if(menu->info->type != APPLET_EXTERN_PENDING &&
	   menu->info->type==APPLET_EXTERN_RESERVED &&
	   menu->info->type != APPLET_EMPTY) {
		/*handle internal applet callbacks here*/
	}
}

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
applet_menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	AppletInfo *info = data;
	int wx, wy;
	PanelWidget *panel = gtk_object_get_data(GTK_OBJECT(info->widget),
					 	 PANEL_APPLET_PARENT_KEY);

	g_return_if_fail(panel != NULL);

	gdk_window_get_origin (info->widget->window, &wx, &wy);

	switch(panel->snapped) {
		case PANEL_DRAWER:
		case PANEL_FREE:
			if(panel->orient==PANEL_VERTICAL) {
				*x = wx + info->widget->allocation.width;
				*y = wy;
				break;
			}
			/*fall through for horizontal*/
		case PANEL_BOTTOM:
			*x = wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
			break;
		case PANEL_TOP:
			*x = wx;
			*y = wy + info->widget->allocation.height;
			break;
		case PANEL_LEFT:
			*x = wx + info->widget->allocation.width;
			*y = wy;
			break;
		case PANEL_RIGHT:
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
show_applet_menu(AppletInfo *info)
{
	g_return_if_fail(info!=NULL);

	if (!info->menu)
		info->menu = create_applet_menu(info,info->user_menu);

	gtk_menu_popup(GTK_MENU(info->menu), NULL, NULL, applet_menu_position,
		       info, 0/*3*/, time(NULL));
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

static AppletInfo*
get_applet_by_id(gint applet_id)
{
	GList *l = g_list_nth(applets,applet_id);

	g_return_val_if_fail(l != NULL,NULL);

	return (AppletInfo *)(l->data);
}

void
applet_show_menu(gint applet_id)
{
	static GdkCursor *arrow = NULL;
	AppletInfo *info = get_applet_by_id(applet_id);

	if(!info)
		return;

	if (!info->menu)
		info->menu = create_applet_menu(info,info->user_menu);

	if(!arrow)
		arrow = gdk_cursor_new(GDK_ARROW);

	gtk_menu_popup(GTK_MENU(info->menu), NULL, NULL, applet_menu_position,
		       info, 0/*3*/, time(NULL));
	gtk_grab_add(info->menu);
	gdk_pointer_grab(info->menu->window,
			 TRUE,
			 APPLET_EVENT_MASK,
			 NULL,
			 arrow,
			 GDK_CURRENT_TIME);
}


int
applet_get_panel(gint applet_id)
{
	int pos = -1;
	int panel;
	GList *list;
	AppletInfo *info = get_applet_by_id(applet_id);

	if(!info)
		return -1;

	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
	    	if((pos=panel_widget_get_pos(PANEL_WIDGET(list->data),
	    				     info->widget))!=-1)
			return panel;
	return -1;
}

void
applet_abort_id(gint applet_id)
{
	AppletInfo *info = get_applet_by_id(applet_id);

	/*only reserved spots can be canceled, if an applet
	  wants to chance a pending applet it needs to first
	  user reserve spot to obtain id and make it EXTERN_RESERVED*/
	if(info->type != APPLET_EXTERN_RESERVED)
		return;

	panel_clean_applet(info);
}


int
applet_get_pos(gint applet_id)
{
	int pos = -1;
	int panel;
	GList *list;
	AppletInfo *info = get_applet_by_id(applet_id);

	if(!info)
		return -1;

	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
	    	if((pos=panel_widget_get_pos(PANEL_WIDGET(list->data),
	    				     info->widget))!=-1)
			return pos;
	return -1;
}

void
applet_drag_start(gint applet_id)
{
	PanelWidget *panel;
	AppletInfo *info = get_applet_by_id(applet_id);

	if(!info)
		return;
	if(!(panel = gtk_object_get_data(GTK_OBJECT(info->widget),
					 PANEL_APPLET_PARENT_KEY)))
		return;

	panel_widget_applet_drag_start_no_grab(panel,info->widget);
	panel_widget_applet_move_use_idle(panel);
}

void
applet_drag_stop(gint applet_id)
{
	PanelWidget *panel;
	AppletInfo *info = get_applet_by_id(applet_id);

	if(!info)
		return;
	if(!(panel = gtk_object_get_data(GTK_OBJECT(info->widget),
					 PANEL_APPLET_PARENT_KEY)))
		return;

	panel_widget_applet_drag_end_no_grab(panel);
}

void
applet_add_callback(gint applet_id, char *callback_name, char *menuitem_text)
{
	AppletUserMenu *menu = g_new(AppletUserMenu,1);
	AppletInfo *info = get_applet_by_id(applet_id);

	if(!info)
		return;

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
applet_request_id (const char *path, char **cfgpath,
		   char **globcfgpath, guint32 * winid)
{
	GList *list;
	AppletInfo *info;
	int i;

	for(i=0,list=applets;list!=NULL;list=g_list_next(list),i++) {
		info = list->data;
		if(info && info->type == APPLET_EXTERN_PENDING &&
		   strcmp(info->params,path)==0) {
			/*we started this and already reserved a spot
			  for it, including the socket widget*/
			*cfgpath = info->cfg;
			info->cfg = NULL;
			*globcfgpath = g_strdup(old_panel_cfg_path);
			info->type = APPLET_EXTERN_RESERVED;
			*winid=GDK_WINDOW_XWINDOW(info->applet_widget->window);
			return i;
		}
	}

	*winid = reserve_applet_spot (EXTERN_ID, path, 0, 0, NULL,
				      APPLET_EXTERN_RESERVED);
	if(*winid == 0) {
		*globcfgpath = NULL;
		*cfgpath = NULL;
		return -1;
	}
	*cfgpath = NULL;
	*globcfgpath = g_strdup(old_panel_cfg_path);
	return i;
}

void
applet_request_glob_cfg (char **globcfgpath)
{
	*globcfgpath = g_strdup(old_panel_cfg_path);
}

void
applet_register (const char * ior, int applet_id)
{
	AppletInfo *info = get_applet_by_id(applet_id);
	PanelWidget *panel;

	if(!info)
		return;

 	panel = gtk_object_get_data(GTK_OBJECT(info->widget),
				    PANEL_APPLET_PARENT_KEY);

	/*no longer pending*/
	info->type = APPLET_EXTERN;

	/*set the ior*/
	g_free(info->id_str);
	info->id_str = g_strdup(ior);

	orientation_change(info,panel);
}

/*note that type should be APPLET_EXTERN_RESERVED or APPLET_EXTERN_PENDING
  only*/
guint32
reserve_applet_spot (const char *id_str, const char *path, int panel,
		     int pos, char *cfgpath, AppletType type)
{
	GtkWidget *socket;

	socket = gtk_socket_new();

	g_return_val_if_fail(socket!=NULL,0);

	gtk_widget_show (socket);
	
	/*we save the ior in the id field of the appletinfo and the 
	  path in the params field*/
	if(!register_toy(socket,NULL,NULL,g_strdup(id_str),g_strdup(path),
		         pos,panel,cfgpath, type))
		return 0;

	return GDK_WINDOW_XWINDOW(socket->window);
}

void
applet_remove_from_panel(gint applet_id)
{
	AppletInfo *info = get_applet_by_id(applet_id);

	panel_clean_applet(info);
}


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


void
applet_set_tooltip(gint applet_id, const char *tooltip)
{
	AppletInfo *info = get_applet_by_id(applet_id);
	if(!info)
		return;
	gtk_tooltips_set_tip (panel_tooltips,info->widget,tooltip,NULL);
}

#if 0
static gint
panel_dnd_drag_request(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	AppletInfo *info = data;
	PanelWidget *panel;

	if(!(panel = gtk_object_get_data(GTK_OBJECT(info->widget),
					 PANEL_APPLET_PARENT_KEY)))
		return;

	gtk_widget_dnd_data_set (widget, event, &info->widget,
				 sizeof(GtkWidget *));
	/*gtk_widget_ref(info->widget);
	panel_widget_remove(panel,info->widget);*/

	return TRUE;
}

static char *applet_drag_types[]={"internal/applet-widget-pointer"};
#endif

gint
register_toy(GtkWidget *applet,
	     GtkWidget * assoc,
	     gpointer data,
	     char *id_str,
	     char *params,
	     int pos,
	     int panel,
	     char *cfgpath,
	     AppletType type)
{
	GtkWidget     *eventbox;
	AppletInfo    *info;
	PanelWidget   *panelw;
	GList         *list;
	int            i;
	
	g_return_if_fail(applet != NULL);
	g_return_if_fail(id_str != NULL);

	list = g_list_nth(panels,panel);

	g_return_if_fail(list != NULL);

	panelw = PANEL_WIDGET(list->data);

	g_return_if_fail(panelw != NULL);
	/* We wrap the applet in a GtkEventBox so that we can capture events over it */

	eventbox = gtk_event_box_new();
	gtk_widget_set_events(eventbox, gtk_widget_get_events(eventbox) |
			      APPLET_EVENT_MASK);
	gtk_container_add(GTK_CONTAINER(eventbox), applet);

	info = g_new(AppletInfo,1);

	info->applet_id = g_list_length(applets);
	info->type = type;
	info->widget = eventbox;
	info->applet_widget = applet;
	info->assoc = assoc;
	info->menu = NULL;
	info->data = data;
	info->id_str = g_strdup(id_str);
	if(params)
		info->params = g_strdup(params);
	else
		info->params = NULL;
	if(cfgpath)
		info->cfg = g_strdup(cfgpath);
	else
		info->cfg = NULL;
	info->user_menu = NULL;

	gtk_object_set_user_data(GTK_OBJECT(eventbox),info);

	if(type == APPLET_DRAWER)
		gtk_object_set_data(GTK_OBJECT(eventbox),
				    PANEL_APPLET_ASSOC_PANEL_KEY,assoc);
		

	if(pos==PANEL_UNKNOWN_APPLET_POSITION)
		pos = 0;
	while(panel_widget_add(panelw, eventbox, pos)==-1) {
		list = g_list_next(list);
		if(!list) {
			gtk_widget_unref(eventbox);
			if(info->cfg)
				g_free(info->cfg);
			if(info->params)
				g_free(info->params);
			g_free(info->id_str);
			g_free(info);
			return FALSE;
		}
		panelw = PANEL_WIDGET(list->data);
	}

	gtk_widget_show(applet);
	gtk_widget_show(eventbox);

	applets = g_list_append(applets,info);

	/*gtk_signal_connect (GTK_OBJECT (eventbox), 
			    "drag_request_event",
			    GTK_SIGNAL_FUNC(panel_dnd_drag_request),
			    info);

	gtk_widget_dnd_drag_set (GTK_WIDGET(eventbox), TRUE,
				 applet_drag_types, 1);*/


	gtk_signal_connect(GTK_OBJECT(eventbox),
			   "button_press_event",
			   GTK_SIGNAL_FUNC(applet_button_press),
			   info);

	orientation_change(info,panelw);

	return TRUE;
}
