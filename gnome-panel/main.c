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
#include "panel-util.h"

/*GList *panels = NULL;*/
GList *applets = NULL;

extern GtkWidget * root_menu;

char *panel_cfg_path=NULL;

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

/*needed for drawers*/
static void panel_setup(PanelWidget *panel);

void
load_applet(char *id, char *params, int pos, int panel, char *cfgpath)
{
	if(strcmp(id,EXTERN_ID) == 0) {
		gchar *command;
		gchar *fullparams;

		/*make it an absolute path, same as the applets will
		  interpret it and the applets will sign themselves as
		  this, so it has to be exactly the same*/
		if(params[0]!='#')
			fullparams = get_full_path(params);
		else
			fullparams = g_strdup(params);
	

		/*start nothing, applet is taking care of everything*/
		if(params == NULL ||
		   params[0] == '\0')
		   	return;

		reserve_applet_spot (id, fullparams, panel, pos, cfgpath,
				     APPLET_EXTERN_PENDING);
		
		/*'#' marks an applet that will take care of starting
		  itself but wants us to reserve a spot for it*/
		if(params[0]!='#') {
			/*this applet is dumb and wants us to start it :)*/
			command = g_copy_strings ("(true;", fullparams, ") &",
						  NULL);

			system (command);
			g_free (command);
		}

		g_free(fullparams);
	} else if(strcmp(id,MENU_ID) == 0) {
		Menu *menu;

		menu = create_menu_applet(params, MENU_UP);

		
		register_toy(menu->button,menu->menu,menu,MENU_ID,params,pos,
			     panel,NULL,APPLET_MENU);
	} else if(strcmp(id,DRAWER_ID) == 0) {
		Drawer *drawer;
		PanelWidget *parent;
		DrawerOrient orient=DRAWER_UP;

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
			drawer = create_empty_drawer_applet(orient);
			panel_setup(PANEL_WIDGET(drawer->drawer));
			panels = g_list_append(panels,drawer->drawer);
		} else {
			int i;

			sscanf(params,"%d",&i);
			drawer=create_drawer_applet(g_list_nth(panels,i)->data,
						    orient);
		}

		g_return_if_fail(drawer != NULL);

		register_toy(drawer->button,drawer->drawer,drawer,DRAWER_ID,
			     params, pos, panel, NULL, APPLET_DRAWER);
	}
}

static void
load_default_applets(void)
{
	load_applet(MENU_ID, ".", PANEL_UNKNOWN_APPLET_POSITION,0,NULL);
	/* FIXME: this for some reason segfaulst rather frequently
	load_applet(EXTERN_ID, "clock_applet", PANEL_UNKNOWN_APPLET_POSITION,0,
		    NULL);
	*/
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
		PanelOrientType orient;
		switch(panel->snapped) {
			case PANEL_FREE:
			case PANEL_DRAWER:
				orient = (panel->orient==PANEL_VERTICAL)?
					 ORIENT_RIGHT:ORIENT_UP;
				break;
			case PANEL_TOP:
				orient = ORIENT_DOWN;
				break;
			case PANEL_BOTTOM:
				orient = ORIENT_UP;
				break;
			case PANEL_LEFT:
				orient = ORIENT_RIGHT;
				break;
			case PANEL_RIGHT:
				orient = ORIENT_LEFT;
				break;
		}
		send_applet_change_orient(info->id,info->applet_id,orient);
	} else if(info->type == APPLET_MENU) {
		Menu *menu = info->data;
		MenuOrient orient=MENU_UP;

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
		DrawerOrient orient = DRAWER_UP;

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


static gint
panel_orient_change(GtkWidget *widget,
		    PanelOrientation orient,
		    PanelSnapped snapped,
		    gpointer data)
{
	panel_widget_foreach(PANEL_WIDGET(widget),orient_change_foreach,
			     (gpointer)widget);
	return TRUE;
}

static void
state_restore_foreach(gpointer data, gpointer user_data)
{
	AppletInfo *info = gtk_object_get_user_data(GTK_OBJECT(data));

	if(!info) return;

	if(info->type == APPLET_DRAWER) {
		if(PANEL_WIDGET(info->assoc)->state == PANEL_SHOWN) {
			panel_widget_restore_state(PANEL_WIDGET(info->assoc));
			panel_widget_foreach(PANEL_WIDGET(info->assoc),
					     state_restore_foreach,
					     NULL);
		}
	}
}

static void
state_hide_foreach(gpointer data, gpointer user_data)
{
	AppletInfo *info = gtk_object_get_user_data(GTK_OBJECT(data));

	if(!info) return;

	if(info->type == APPLET_DRAWER) {
		if(PANEL_WIDGET(info->assoc)->state == PANEL_SHOWN) {
			gtk_widget_hide(info->assoc);
			panel_widget_foreach(PANEL_WIDGET(info->assoc),
					     state_hide_foreach,
					     NULL);
		}
	}
}

static gint
panel_state_change(GtkWidget *widget,
		    PanelState state,
		    gpointer data)
{
	if(state==PANEL_SHOWN)
		panel_widget_foreach(PANEL_WIDGET(widget),state_restore_foreach,
				     (gpointer)widget);
	else
		panel_widget_foreach(PANEL_WIDGET(widget),state_hide_foreach,
				     (gpointer)widget);

	return TRUE;
}

static gint
applet_move_foreach(gpointer data, gpointer user_data)
{
	AppletInfo *info = gtk_object_get_user_data(GTK_OBJECT(data));

	if(!info) return FALSE;

	if(info->type == APPLET_DRAWER) {
		if(PANEL_WIDGET(info->assoc)->state == PANEL_SHOWN) {
			Drawer *drawer = info->data;
			reposition_drawer(drawer);
			panel_widget_foreach(PANEL_WIDGET(info->assoc),
					     state_restore_foreach,
					     NULL);
		}
	}
	return TRUE;
}

static gint
panel_applet_move(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	applet_move_foreach(applet,NULL);
	return TRUE;
}

static gint
panel_size_allocate(GtkWidget *widget, GtkAllocation *alloc, gpointer data)
{
	Drawer *drawer = gtk_object_get_data(GTK_OBJECT(widget),DRAWER_PANEL);
	PanelWidget *panel = PANEL_WIDGET(widget);

	if(drawer)
		if(panel->state == PANEL_SHOWN)
			reposition_drawer(drawer);
	return TRUE;
}

static gint
panel_applet_added(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	AppletInfo *info = gtk_object_get_user_data(GTK_OBJECT(applet));
	PanelWidget *panel = PANEL_WIDGET(widget);

	if(!info) return FALSE;

	orientation_change(info,panel);

	return TRUE;
}

static gint
panel_applet_removed(GtkWidget *widget, gpointer data)
{
	return TRUE;
}

static void
panel_menu_position (GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	int wx, wy;
	PanelWidget *panel = data;

	g_return_if_fail(panel != NULL);

	gdk_window_get_origin (GTK_WIDGET(panel)->window, &wx, &wy);

	switch(panel->snapped) {
		case PANEL_DRAWER:
		case PANEL_FREE:
			if(panel->orient==PANEL_VERTICAL) {
				gtk_widget_get_pointer(GTK_WIDGET(panel),
						       NULL, &wy);
				*x = wx + GTK_WIDGET(panel)->allocation.width;
				*y = wy;
				break;
			}
			/*fall through for horizontal*/
		case PANEL_BOTTOM:
			gtk_widget_get_pointer(GTK_WIDGET(panel),
					       &wx, NULL);
			*x = wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
			break;
		case PANEL_TOP:
			gtk_widget_get_pointer(GTK_WIDGET(panel),
					       &wx, NULL);
			*x = wx;
			*y = wy + GTK_WIDGET(panel)->allocation.height;
			break;
		case PANEL_LEFT:
			gtk_widget_get_pointer(GTK_WIDGET(panel),
					       NULL, &wy);
			*x = wx + GTK_WIDGET(panel)->allocation.width;
			*y = wy;
			break;
		case PANEL_RIGHT:
			gtk_widget_get_pointer(GTK_WIDGET(panel),
					       NULL, &wy);
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


static int
panel_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	/* FIXME: display main menu instead! */
	if(event->button==3 || event->button==1) {
		gtk_menu_popup(GTK_MENU(data), NULL, NULL, panel_menu_position,
			widget, event->button, time(NULL));
		return TRUE;
	}
	return FALSE;
}

static gint
panel_destroy(GtkWidget *widget, gpointer data)
{
	GtkWidget *panel_menu = data;

	if(panel_menu)
		gtk_widget_unref(panel_menu);
}

static void
panel_setup(PanelWidget *panel)
{
	GtkWidget *panel_menu;

	panel_menu = create_panel_root_menu(panel);
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
			   "size_allocate",
			   GTK_SIGNAL_FUNC(panel_size_allocate),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "applet_added",
			   GTK_SIGNAL_FUNC(panel_applet_added),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "applet_removed",
			   GTK_SIGNAL_FUNC(panel_applet_removed),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "button_press_event",
			   GTK_SIGNAL_FUNC(panel_button_press),
			   panel_menu);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "destroy",
			   GTK_SIGNAL_FUNC(panel_destroy),
			   panel_menu);

	gtk_signal_connect_after(GTK_OBJECT(panel), "realize",
				 GTK_SIGNAL_FUNC(panel_realize),
				 NULL);
}


static void
init_user_panels(void)
{
	char  buf[256];
	int   count,num;	
	int   size,x,y;
	PanelConfig config;
	GtkWidget *panel;
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
					/*the previous three will get changed
					  anyway, they are globals*/
					 x,
					 y,
					 drop_pos);

		panel_setup(PANEL_WIDGET(panel));

		gtk_widget_show(panel);


		panels = g_list_append(panels,panel);
	}
}

gint
call_launcher_timeout(gpointer data)
{
	puts("Waiting for launcher ...");
	return !(panel_corba_restart_launchers());
}
	

int
main(int argc, char **argv)
{
	char buf[256];

	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	client = gnome_client_new_default ();
	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
			    GTK_SIGNAL_FUNC (panel_session_save), NULL);

	panel_corba_register_arguments ();

	gnome_init("panel", NULL, argc, argv, 0, NULL);

	/*FIXME: this should be session specific*/
	panel_cfg_path = g_strdup("/panel/");

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

	init_main_menu();
	init_user_panels();
	init_user_applets();

	panel_tooltips = gtk_tooltips_new();

	apply_global_config();

	gtk_timeout_add(300,call_launcher_timeout,NULL);

	/* I use the glue code to avoid making this a C++ file */
	panel_corba_gtk_main ("IDL:GNOME/Panel:1.0");

	return 0;
}
