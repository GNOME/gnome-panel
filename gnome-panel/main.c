/* Gnome panel: Initialization routines
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 */

#include <config.h>
#include <string.h>
#include <gnome.h>
#include "panel-widget.h"
#include "panel.h"
#include "panel_config_global.h"
#include "menu.h"
#include "drawer.h"
#include "mico-glue.h"
#include "mico-parse.h"

GList *panels = NULL;
GList *applets = NULL;

extern GtkWidget * root_menu;

GtkTooltips *panel_tooltips = NULL;

GnomeClient *client = NULL;

GlobalConfig global_config = {
		DEFAULT_AUTO_HIDE_STEP_SIZE,
		DEFAULT_EXPLICIT_HIDE_STEP_SIZE,
		DEFAULT_MINIMIZED_SIZE,
		DEFAULT_MINIMIZE_DELAY,
		TRUE, /*tooltips*/
		TRUE /*show small icons*/
	};


void
load_applet(char *id, char *params, int pos, int panel, char *cfgpath)
{
	/*FIXME: somehow load the applet and call register_toy ... this
	  thing has to exec the applet or if it is a local applet then it
	  just calls a function to create the appropriate widget and use
	  register_toy*/
	if(strcmp(id,EXTERN_ID) == 0) {
		gchar *command;
		AppletInfo * info;

		g_return_if_fail (params != NULL);

		reserve_applet_spot (cfgpath, params, panel, pos);
		
		puts("-------------------------------------------");
		puts("          LOADING EXTERN APPLET");
		puts("-------------------------------------------");
		command = g_copy_strings ("(true;", params, ") &", NULL);

		system (command);
		g_free (command);

	} else if(strcmp(id,MENU_ID) == 0) {
		Menu *menu;

		menu = create_menu_applet(GTK_WIDGET(panels->data),
					  params, MENU_UP);

		
		register_toy(menu->button,menu->menu,menu,MENU_ID,params,pos,
			     panel,APPLET_HAS_PROPERTIES,APPLET_MENU);
		printf("[load:menu:%ld]\n",(long)menu);
	} else if(strcmp(id,DRAWER_ID) == 0) {
		Drawer *drawer;
		PanelWidget *parent;
		DrawerOrient orient;

		parent = PANEL_WIDGET(g_list_nth(panels,panel)->data);

		switch(parent->snapped) {
			case PANEL_FREE:
			case PANEL_DRAWER:
				orient = (parent->orient==PANEL_VERTICAL)?
					 DRAWER_RIGHT:DRAWER_UP;
				break;
			case PANEL_TOP:
				orient = DRAWER_DOWN;
				break;
			case PANEL_BOTTOM:
				orient = DRAWER_UP;
				break;
			case PANEL_LEFT:
				orient = DRAWER_RIGHT;
				break;
			case PANEL_RIGHT:
				orient = DRAWER_LEFT;
				break;
		}

		if(!params) {
			drawer = create_empty_drawer_applet(GTK_WIDGET(parent),
							    orient);
			panels = g_list_append(panels,drawer->drawer);
		} else {
			int i;

			sscanf(params,"%d",&i);
			drawer = create_drawer_applet(GTK_WIDGET(parent),
						     g_list_nth(panels,i)->data,
						     orient);
		}

		g_return_if_fail(drawer != NULL);

		register_toy(drawer->button,drawer->drawer,drawer,DRAWER_ID,
			     params, pos, panel,
			     APPLET_HAS_PROPERTIES,APPLET_DRAWER);
	}
}

static void
load_default_applets(void)
{
	/* XXX: the IDs for these applets are hardcoded here. */

	load_applet("Menu", ".", PANEL_UNKNOWN_APPLET_POSITION,0,NULL);
/*
	load_applet("Clock", NULL, PANEL_UNKNOWN_APPLET_POSITION,0);
	load_applet("Mail check", NULL, PANEL_UNKNOWN_APPLET_POSITION,0);
*/
/*FIXME: fix applet loading with corba*/
}

static void
init_user_applets(void)
{
	/*FIXME: change fields so that we can start corba applets*/
	char *applet_name;
	char *applet_params;
	int   pos=0,panel;
	char  buf[256];
	int   count,num;	

	count=gnome_config_get_int("/panel/Config/applet_count=0");
	if(count<=0)
		load_default_applets();
	for(num=1;num<=count;num++) {
		sprintf(buf,"/panel/Applet_%d/id=Unknown",num);
		applet_name = gnome_config_get_string(buf);
		sprintf(buf,"/panel/Applet_%d/parameters=",num);
		applet_params = gnome_config_get_string(buf);
		sprintf(buf,"/panel/Applet_%d/position=%d",num,
			PANEL_UNKNOWN_APPLET_POSITION);
		pos = gnome_config_get_int(buf);
		sprintf(buf,"/panel/Applet_%d/panel=0",num);
		panel = gnome_config_get_int(buf);

		/*this is the config path to be passed to the applet when it
		  loads*/
		sprintf(buf,"/panel/Applet_%d/",num);
		load_applet(applet_name, applet_params, pos, panel, buf);
		g_free(applet_name);
		g_free(applet_params);
	}
}

static void
change_window_cursor(GdkWindow *window, GdkCursorType cursor_type)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new(cursor_type);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
}

static void
panel_realize(GtkWidget *widget, gpointer data)
{
	change_window_cursor(widget->window, GDK_ARROW);
}

/*we call this recursively*/
static void orient_change_foreach(gpointer data, gpointer user_data);

void
orientation_change(AppletInfo *info, PanelWidget *panel)
{
	if(info->type == APPLET_EXTERN) {
		/*FIXME: call corba*/
	} else if(info->type == APPLET_MENU) {
		Menu *menu = info->data;
		MenuOrient orient;

		printf("[orient:menu:%ld]\n",(long)menu);

		switch(panel->snapped) {
			case PANEL_FREE:
			case PANEL_DRAWER:
				orient = (panel->orient==PANEL_VERTICAL)?
					 MENU_RIGHT:MENU_UP;
				break;
			case PANEL_TOP:
				orient = MENU_DOWN;
				break;
			case PANEL_BOTTOM:
				orient = MENU_UP;
				break;
			case PANEL_LEFT:
				orient = MENU_RIGHT;
				break;
			case PANEL_RIGHT:
				orient = MENU_LEFT;
				break;
		}
		set_menu_applet_orient(menu,orient);
	} else if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		DrawerOrient orient;

		switch(panel->snapped) {
			case PANEL_FREE:
			case PANEL_DRAWER:
				orient = (panel->orient==PANEL_VERTICAL)?
					 DRAWER_RIGHT:DRAWER_UP;
				break;
			case PANEL_TOP:
				orient = DRAWER_DOWN;
				break;
			case PANEL_BOTTOM:
				orient = DRAWER_UP;
				break;
			case PANEL_LEFT:
				orient = DRAWER_RIGHT;
				break;
			case PANEL_RIGHT:
				orient = DRAWER_LEFT;
				break;
		}
		reposition_drawer(drawer);
		set_drawer_applet_orient(drawer,orient);
		panel_widget_foreach(PANEL_WIDGET(info->assoc),
				     orient_change_foreach,
				     (gpointer)info->assoc);
	}
}

static void
orient_change_foreach(gpointer data, gpointer user_data)
{
	AppletInfo *info = gtk_object_get_user_data(GTK_OBJECT(data));
	PanelWidget *panel = user_data;

	if(!info) return;

	orientation_change(info,panel);
}


static int
panel_orient_change(GtkWidget *widget,
		    PanelOrientation orient,
		    PanelSnapped snapped,
		    gpointer data)
{
	puts("PANEL_ORIENT_CHANGE");

	panel_widget_foreach(PANEL_WIDGET(widget),orient_change_foreach,
			     (gpointer)widget);
}

static void
state_restore_foreach(gpointer data, gpointer user_data)
{
	AppletInfo *info = gtk_object_get_user_data(GTK_OBJECT(data));
	PanelWidget *panel = user_data;

	if(!info) return;

	if(info->type == APPLET_DRAWER) {
		if(PANEL_WIDGET(info->assoc)->state == PANEL_SHOWN) {
			panel_widget_restore_state(PANEL_WIDGET(info->assoc));
			panel_widget_foreach(PANEL_WIDGET(info->assoc),
					     state_restore_foreach,
					     (gpointer)info->assoc);
		}
	}
}

static void
state_hide_foreach(gpointer data, gpointer user_data)
{
	AppletInfo *info = gtk_object_get_user_data(GTK_OBJECT(data));
	PanelWidget *panel = user_data;

	if(!info) return;

	if(info->type == APPLET_DRAWER) {
		if(PANEL_WIDGET(info->assoc)->state == PANEL_SHOWN) {
			gtk_widget_hide(info->assoc);
			panel_widget_foreach(PANEL_WIDGET(info->assoc),
					     state_restore_foreach,
					     (gpointer)info->assoc);
		}
	}
}

static int
panel_state_change(GtkWidget *widget,
		    PanelState state,
		    gpointer data)
{
	puts("PANEL_STATE_CHANGE");

	if(state==PANEL_SHOWN) {
		puts("PANEL_SHOW");
		panel_widget_foreach(PANEL_WIDGET(widget),state_restore_foreach,
				     (gpointer)widget);
	} else {
		puts("PANEL_HIDE");
		panel_widget_foreach(PANEL_WIDGET(widget),state_hide_foreach,
				     (gpointer)widget);
	}
}

static void
applet_move_foreach(gpointer data, gpointer user_data)
{
	AppletInfo *info = gtk_object_get_user_data(GTK_OBJECT(data));
	PanelWidget *panel = user_data;

	if(!info) return;

	if(info->type == APPLET_DRAWER) {
		if(PANEL_WIDGET(info->assoc)->state == PANEL_SHOWN) {
			Drawer *drawer = info->data;
			reposition_drawer(drawer);
			panel_widget_foreach(PANEL_WIDGET(info->assoc),
					     state_restore_foreach,
					     (gpointer)info->assoc);
		}
	}
}

static void
panel_applet_move(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	applet_move_foreach(applet,widget);
}



static int
panel_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	/* FIXME: display main menu instead! */
	if(event->button==3 || event->button==1) {
		gtk_menu_popup(GTK_MENU(data), NULL, NULL, NULL,
			NULL, event->button, time(NULL));
		return TRUE;
	}
	return FALSE;
}


static void
init_user_panels(void)
{
	char  buf[256];
	int   count,num;	
	int   size,x,y;
	PanelConfig config;
	GtkWidget *panel;
	GtkWidget *panel_menu;
	PanelState state;
	DrawerDropZonePos drop_pos;

	count=gnome_config_get_int("/panel/Config/panel_count=0");
	if(count<=0) count++; /*this will load up a single panel with
				default settings*/
	for(num=1;num<=count;num++) {
		/*these are only for free floating non-drawer like panels */
		sprintf(buf,"/panel/Panel_%d/size=%d",num, 50);
		size=gnome_config_get_int(buf);
		sprintf(buf,"/panel/Panel_%d/position_x=0",num);
		x=gnome_config_get_int(buf);
		sprintf(buf,"/panel/Panel_%d/position_y=0",num);
		y=gnome_config_get_int(buf);

		sprintf(buf,"/panel/Panel_%d/snapped=%d",num,
			PANEL_BOTTOM);
		config.snapped=gnome_config_get_int(buf);

		sprintf(buf,"/panel/Panel_%d/orient=%d",num,
			PANEL_HORIZONTAL);
		config.orient=gnome_config_get_int(buf);

		sprintf(buf,"/panel/Panel_%d/mode=%d",num,
			PANEL_EXPLICIT_HIDE);
		config.mode=gnome_config_get_int(buf);

		sprintf(buf,"/panel/Panel_%d/state=%d",num,
			PANEL_SHOWN);
		state=gnome_config_get_int(buf);

		sprintf(buf,"/panel/Panel_%d/drawer_drop_zone_pos=%d",num,
			DRAWER_LEFT);
		drop_pos=gnome_config_get_int(buf);


		panel = panel_widget_new(size,
					 config.orient,
					 config.snapped,
					 config.mode,
					 state,
					 DEFAULT_EXPLICIT_HIDE_STEP_SIZE,
					 DEFAULT_MINIMIZED_SIZE,
					 DEFAULT_MINIMIZE_DELAY,
					/*the last three will get changed
					  anyway, they are globals*/
					 x,
					 y,
					 drop_pos);


		/*FIXME: this should be made cleaner I guess*/
		if(!root_menu) init_main_menu(panel);

		panel_menu = create_panel_root_menu(PANEL_WIDGET(panel));
		gtk_signal_connect(GTK_OBJECT(panel),
				   "orient_change",
				   GTK_SIGNAL_FUNC(panel_orient_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panel),
				   "state_change",
				   GTK_SIGNAL_FUNC(panel_state_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panel),
				   "applet_move",
				   GTK_SIGNAL_FUNC(panel_applet_move),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panel),
				   "button_press_event",
				   GTK_SIGNAL_FUNC(panel_button_press),
				   panel_menu);
		gtk_signal_connect_object(GTK_OBJECT(panel),
					  "destroy",
					  GTK_SIGNAL_FUNC(gtk_widget_unref),
					  GTK_OBJECT(panel_menu));

		gtk_signal_connect_after(GTK_OBJECT(panel), "realize",
					 GTK_SIGNAL_FUNC(panel_realize),
					 NULL);

		gtk_widget_show(panel);

		panels = g_list_append(panels,panel);
	}
}
	

int
main(int argc, char **argv)
{
	GtkWidget *base_panel;
	char buf[256];

	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	client = gnome_client_new_default ();
	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
			    GTK_SIGNAL_FUNC (panel_session_save), NULL);

	panel_corba_register_arguments ();

	gnome_init("panel", NULL, argc, argv, 0, NULL);

	create_applet_menu();

	/*set up global options*/
	global_config.tooltips_enabled =
		gnome_config_get_bool("/panel/Config/tooltips_enabled=TRUE");
	global_config.show_small_icons =
		gnome_config_get_bool("/panel/Config/show_small_icons=TRUE");
	sprintf(buf,"/panel/Config/auto_hide_step_size=%d",
		DEFAULT_AUTO_HIDE_STEP_SIZE);
	global_config.auto_hide_step_size=gnome_config_get_int(buf);
	sprintf(buf,"/panel/Config/explicit_hide_step_size=%d",
		DEFAULT_EXPLICIT_HIDE_STEP_SIZE);
	global_config.explicit_hide_step_size=gnome_config_get_int(buf);
	sprintf(buf,"/panel/Config/minimize_delay=%d",
		DEFAULT_MINIMIZE_DELAY);
	global_config.minimize_delay=gnome_config_get_int(buf);
	sprintf(buf,"/panel/Config/minimized_size=%d",
		DEFAULT_MINIMIZED_SIZE);
	global_config.minimized_size=gnome_config_get_int(buf);

	init_user_panels();
	init_user_applets();

	panel_tooltips = gtk_tooltips_new();

	apply_global_config();

	printf ("starting corba looop\n");
	/* I use the glue code to avoid making this a C++ file */
	panel_corba_gtk_main ("IDL:GNOME/Panel:1.0");

	return 0;
}
