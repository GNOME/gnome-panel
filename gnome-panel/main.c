/* Gnome panel: Initialization routines
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 */

#include <config.h>
#include <string.h>
#include "gnome.h"
#include "panel-widget.h"
#include "panel.h"
#include "panel_config_global.h"
#include "menu.h"

GList *panels = NULL;
GList *drawers = NULL;
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
load_applet(char *id, char *params, int pos, int panel)
{
	/*FIXME: somehow load the applet and call register_toy ... this
	  thing has to exec the applet or if it is a local applet then it
	  just calls a function to create the appropriate widget and use
	  register_toy*/
	if(strcmp(id,MENU_ID) == 0) {
		Menu *menu;

		menu = create_menu_applet(GTK_WIDGET(panels->data),
					  params,MENU_UP);

		
		register_toy(menu->button,menu->menu,MENU_ID,params,pos,
			     panel,APPLET_HAS_PROPERTIES,APPLET_MENU);
	}
}

void
load_drawer(char *name, char *iconopen, char *iconclosed, int step_size,
	int pos, int panel)
{
	/*FIXME: drawers*/
}

static void
load_default_applets(void)
{
	/* XXX: the IDs for these applets are hardcoded here. */

	load_applet("Menu", ".", PANEL_UNKNOWN_APPLET_POSITION,0);
/*
	load_applet("Clock", NULL, PANEL_UNKNOWN_APPLET_POSITION,0);
	load_applet("Mail check", NULL, PANEL_UNKNOWN_APPLET_POSITION,0);
*/
/*FIXME: fix applet loading with corba*/
}

static void
init_user_drawers(void)
{
	/*FIXME: DRAWERS*/

	/*
	char *drawer_name;
	char *drawer_iconopen;
	char *drawer_iconclosed;
	int   pos,panel;
	int   step_size;
	char  buf[256];
	int   count,num;	


	count=gnome_config_get_int("/panel/Config/drawer_count=0");
	for(num=1;num<=count;num++) {
		sprintf(buf,"/panel/Drawer_%d/name=Drawer",num);
		drawer_name = gnome_config_get_string(buf);
		sprintf(buf,"/panel/Drawer_%d/iconopen=",num);
		drawer_iconopen = gnome_config_get_string(buf);
		sprintf(buf,"/panel/Drawer_%d/iconclosed=",num);
		drawer_iconclosed = gnome_config_get_string(buf);

		sprintf(buf,"/panel/Drawer_%d/step_size=%d",num,
			DEFAULT_STEP_SIZE);
		step_size = gnome_config_get_int(buf);
		sprintf(buf,"/panel/Drawer_%d/geometry=%d",num,
			PANEL_UNKNOWN_APPLET_POSITION);
		pos = gnome_config_get_int(buf);
		sprintf(buf,"/panel/Drawer_%d/panel=0",num);
		panel = gnome_config_get_int(buf);
		load_drawer(drawer_name,drawer_iconopen,drawer_iconclosed,
			    step_size,pos,panel);
		g_free(drawer_name);
		g_free(drawer_iconopen);
		g_free(drawer_iconclosed);
	}
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
		load_applet(applet_name, applet_params, pos, panel);
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
	

/* FIXME: session management not complete.  In particular, we should:
   1. Actually save state in a useful way.  */
static void
init_session_management (int argc, char *argv[])
{
  client = gnome_client_new (argc, argv);
  
  gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
		      GTK_SIGNAL_FUNC (panel_session_save), NULL);
}

int
main(int argc, char **argv)
{
	GtkWidget *base_panel;
	char buf[256];
	
	gnome_init(&argc, &argv);
	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	init_session_management (argc, argv);

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
	init_user_drawers();
	init_user_applets();

	panel_tooltips = gtk_tooltips_new();

	apply_global_config();

	gtk_main();
	return 0;
}
