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

#include "applet_files.h"
#include "gdkextra.h"
#include "panel_cmds.h"
#include "applet_cmds.h"
#include "panel.h"
#include "panel_config.h"


#define APPLET_CMD_FUNC "panel_applet_cmd_func"
#define APPLET_FLAGS    "panel_applet_flags"

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

/*FIXME: a hack for current code to work*/
#define the_panel (PANEL_WIDGET(panels->data))


/* some prototypes */
static void properties(PanelWidget *panel);


static AppletCmdFunc
applet_cmd_func(GtkWidget *applet)
{
	return gtk_object_get_data(GTK_OBJECT(applet), APPLET_CMD_FUNC);
}


static long
applet_flags(GtkWidget *applet)
{
	return (long) gtk_object_get_data(GTK_OBJECT(applet), APPLET_FLAGS);
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

static gpointer
call_applet(GtkWidget *applet, AppletCommand *cmd)
{
	AppletCmdFunc cmd_func;
	
	cmd->panel  = the_panel;
	cmd->applet = GTK_BIN(applet)->child;

	cmd_func = applet_cmd_func(applet);

	return (*cmd_func) (cmd);
}

/*FIXME this should be somehow done through signals and panel-widget*/
static void
applet_orientation_notify(GtkWidget *widget, gpointer data)
{
	AppletCommand  cmd;

	cmd.cmd = APPLET_CMD_ORIENTATION_CHANGE_NOTIFY;
	cmd.params.orientation_change_notify.snapped = the_panel->snapped;
	cmd.params.orientation_change_notify.orient = the_panel->orient;

	call_applet(widget, &cmd);
}

static void
save_applet_configuration(gpointer data, gpointer user_data)
{
	char          *id;
	char          *params;
	char          *path;
	char          *fullpath;
	char           buf[256];
	GtkWidget     *widget = data;
	int           *num = user_data;
	int            pos;
	int            panel;
	GList         *list;
	AppletCommand  cmd;

	pos = -1;
	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
	    	if((pos=panel_widget_get_pos(PANEL_WIDGET(list->data),
	    				     widget))!=-1)
			break; 

	/*not found*/
	if(pos == -1)
		return;

	cmd.cmd = APPLET_CMD_QUERY;
	id      = call_applet(widget, &cmd);

	cmd.cmd = APPLET_CMD_GET_INSTANCE_PARAMS;
	params  = call_applet(widget, &cmd);

	sprintf(buf, "_%d/", (*num)++);
	path = g_copy_strings("/panel/Applet", buf, NULL);

	fullpath = g_copy_strings(path,"id",NULL);
	gnome_config_set_string(fullpath, id);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"position",NULL);
	gnome_config_set_int(fullpath, pos);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"panel",NULL);
	gnome_config_set_int(fullpath, panel);
	g_free(fullpath);

	fullpath = g_copy_strings(path,"parameters",NULL);
	gnome_config_set_string(fullpath, params);
	g_free(fullpath);


	g_free(params);
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
destroy_applet_module(gpointer key, gpointer value, gpointer user_data)
{
	AppletCommand  cmd;
	AppletFile    *af;

	cmd.cmd    = APPLET_CMD_DESTROY_MODULE;
	cmd.panel  = NULL; /*the_panel;*/
	cmd.applet = NULL;

	af = value;

	(*af->cmd_func) (&cmd);
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
int
panel_session_save (gpointer client_data,
		    GnomeSaveStyle save_style,
		    int is_shutdown,
		    GnomeInteractStyle interact_style,
		    int is_fast)
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

	g_list_foreach(drawers,destroy_widget_list,NULL);
	g_list_foreach(panels,destroy_widget_list,NULL);

	g_hash_table_foreach(applet_files_ht, destroy_applet_module, NULL);
	applet_files_destroy();

	/* Always successful.  */
	return 1;
}

static void
panel_quit(void)
{
  if (! gnome_session_connected_p ())
    {
      panel_session_save (NULL, GNOME_SAVE_BOTH, 1, GNOME_INTERACT_NONE, 0);
      gtk_main_quit ();
      /* We don't want to return, because we've probably been called from an
       * applet which has since been dlclose()'d, and we'd end up with a SEGV
       * when we tried to return to the now-nonexistent code page. */
      exit(0);
    }
  else
    {
      /* We request a completely interactive, full, slow shutdown.  */
      gnome_session_request_save (GNOME_SAVE_BOTH, 1, GNOME_INTERACT_ANY,
				  0, 1);
    }
}

static void
create_applet(char *id, char *params, int pos, int panel)
{
	AppletCommand cmd;
	AppletCmdFunc cmd_func;
	int           requested;
	
	g_assert(id != NULL);
	
	cmd_func = get_applet_cmd_func(id);
	if (!cmd_func) {
		fprintf(stderr, "create_applet: could not find applet \"%s\"\n", id);
		return;
	}

	requested = FALSE;
	
	if (!params) {
		cmd.cmd    = APPLET_CMD_GET_DEFAULT_PARAMS;
		cmd.panel  = the_panel;
		cmd.applet = NULL;
		
		params = (*cmd_func) (&cmd);

		if (!params) {
			fprintf(stderr,
				"create_applet: warning: applet \"%s\" returned NULL default parameters\n"
				"               using empty parameter string \"\"\n",
				id);
			params = g_strdup("");
		}

		requested = TRUE;
	}

	cmd.cmd    = APPLET_CMD_CREATE_INSTANCE;
	cmd.panel  = PANEL_WIDGET(g_list_nth(panels,panel)->data);
	cmd.applet = NULL;
	cmd.params.create_instance.params = params;
	cmd.params.create_instance.pos    = pos;
	cmd.params.create_instance.panel  = panel;

	(*cmd_func) (&cmd);

	if (requested)
		g_free(params);
}

/*FIXME:applet menu!*/

#if 0
static void
move_applet_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget      *applet;

	applet = gtk_object_get_user_data(GTK_OBJECT(applet_menu));
	applet_drag_start(applet, TRUE);
}


static void
remove_applet_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget *applet;
	AppletCommand  cmd;
	gchar *id;
	gint pos;

	applet = gtk_object_get_user_data(GTK_OBJECT(applet_menu));

	cmd.cmd = APPLET_CMD_QUERY;
	id      = call_applet(applet, &cmd);

	if(strcmp(id,"Menu")==0) {
		if(menu_count<=1)
			return;
		/*FIXME: do something to make the user aware that this was
		  wrong ... a message box maybe ... or a beep*/
		menu_count--;
	}

	pos = find_applet(the_panel->panel,applet);
	gtk_widget_destroy(applet);

	/*FIMXE: no placeholders in drawers!*/
	the_panel->panel->applets[pos]->applet = gtk_event_box_new();
	gtk_widget_show(the_panel->panel->applets[pos]->applet);
	the_panel->panel->applets[pos]->type = APPLET_PLACEHOLDER;

	put_applet_in_slot(the_panel->panel,
			   the_panel->panel->applets[pos],pos);
}


static void
applet_properties_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget     *applet;
	AppletCommand  cmd;

	applet = gtk_object_get_user_data(GTK_OBJECT(applet_menu));

	cmd.cmd = APPLET_CMD_PROPERTIES;

	call_applet(applet, &cmd);
}

#endif

static void
create_applet_menu(void)
{
	/*GtkWidget *menuitem;

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
	applet_menu_prop_item = menuitem;*/
}


static void
show_applet_menu(GtkWidget *applet)
{
	long flags;
	AppletCommand  cmd;
	gchar *id;

	/*FIXME: DRAWERS crash on this, fix that */

	flags = applet_flags(applet);

	if (flags & APPLET_HAS_PROPERTIES) {
		gtk_widget_show(applet_menu_prop_separator);
		gtk_widget_show(applet_menu_prop_item);
	} else {
		gtk_widget_hide(applet_menu_prop_separator);
		gtk_widget_hide(applet_menu_prop_item);
	}

	cmd.cmd = APPLET_CMD_QUERY;
	id      = call_applet(applet, &cmd);

	if(strcmp(id,"Menu")!=0 || menu_count>1)
		gtk_widget_show(applet_menu_remove_item);
	else
		gtk_widget_hide(applet_menu_remove_item);
	gtk_object_set_user_data(GTK_OBJECT(applet_menu), applet);

	gtk_menu_popup(GTK_MENU(applet_menu), NULL, NULL, NULL, NULL, 3, time(NULL));
	/*FIXME: make it pop-up on some title bar of the applet menu or
	  somehow avoid pressing remove applet being under the cursor!*/
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
get_applet_type(gpointer key, gpointer value, gpointer user_data)
{
	GList **list = user_data;

	*list = g_list_prepend(*list, g_strdup(key));
}


GList *
get_applet_types(void)
{
	GList *list = NULL;

	g_hash_table_foreach(applet_files_ht, get_applet_type, &list);
	return list;
}

static void
init_applet_module(gpointer key, gpointer value, gpointer user_data)
{
	AppletCommand  cmd;
	AppletFile    *af;

	cmd.cmd    = APPLET_CMD_INIT_MODULE;
	cmd.panel  = the_panel;
	cmd.applet = NULL;
	cmd.params.init_module.cmd_func = panel_command;

	af = value;

	(*af->cmd_func) (&cmd);
}


void
panel_init_applet_modules(void)
{
	g_hash_table_foreach(applet_files_ht, init_applet_module, NULL);
}

static void
set_tooltip(GtkWidget *applet, char *tooltip)
{
	if(!applet)
		return;
	gtk_tooltips_set_tips(panel_tooltips,applet,tooltip);
}


static void
register_toy(GtkWidget *applet, char *id, int pos, int panel, long flags)
{
	GtkWidget     *eventbox;
	
	g_assert(applet != NULL);
	g_assert(id != NULL);

	/* We wrap the applet in a GtkEventBox so that we can capture events over it */

	eventbox = gtk_event_box_new();
	gtk_widget_set_events(eventbox, gtk_widget_get_events(eventbox) |
			      APPLET_EVENT_MASK);
	gtk_container_add(GTK_CONTAINER(eventbox), applet);

	/* FIXME:get rid of this*/
	/* Attach our private data to the applet */
	gtk_object_set_data(GTK_OBJECT(eventbox), APPLET_CMD_FUNC,
			    get_applet_cmd_func(id));
	gtk_object_set_data(GTK_OBJECT(eventbox), APPLET_FLAGS,
			    (gpointer) flags);

	if(pos==PANEL_UNKNOWN_APPLET_POSITION)
		pos = 0;
	panel_widget_add(PANEL_WIDGET(g_list_nth(panels,panel)->data),
			 eventbox, pos);

	gtk_widget_show(applet);
	gtk_widget_show(eventbox);

	applets = g_list_append(applets,eventbox);

	/*notify the applet of the orientation of the panel!*/
	applet_orientation_notify(eventbox,NULL);

	if(strcmp(id,"Menu")==0)
		menu_count++;
}

static void
create_drawer(char *name, char *iconopen, char* iconclosed, int step_size,
	int pos, int drawer)
{
	/*FIXME: add drawers*/
	return;
}

static void
properties(PanelWidget *panel)
{
	panel_config(panel);
}

gpointer
panel_command(PanelCommand *cmd)
{
	g_assert(cmd != NULL);
	
	switch (cmd->cmd) {
		case PANEL_CMD_QUIT:
			panel_quit();
			return NULL;

		case PANEL_CMD_GET_APPLET_TYPES:
			return get_applet_types();

		case PANEL_CMD_GET_APPLET_CMD_FUNC:
			return get_applet_cmd_func(cmd->params.get_applet_cmd_func.id);

		case PANEL_CMD_CREATE_APPLET:
			create_applet(cmd->params.create_applet.id,
				      cmd->params.create_applet.params,
				      cmd->params.create_applet.pos,
				      cmd->params.create_applet.panel);
			break;
			
		case PANEL_CMD_REGISTER_TOY:
			/*FIXME: fix it so that applets pass panel*/
			cmd->params.register_toy.panel = 0;
			register_toy(cmd->params.register_toy.applet,
				     cmd->params.register_toy.id,
				     cmd->params.register_toy.pos,
				     cmd->params.register_toy.panel,
				     cmd->params.register_toy.flags);
			break;

		case PANEL_CMD_CREATE_DRAWER:
			create_drawer(cmd->params.create_drawer.name,
				      cmd->params.create_drawer.iconopen,
				      cmd->params.create_drawer.iconclosed,
				      cmd->params.create_drawer.step_size,
				      cmd->params.create_drawer.pos,
				      cmd->params.create_drawer.panel);
			break;

		case PANEL_CMD_SET_TOOLTIP:
			set_tooltip(cmd->params.set_tooltip.applet,
				    cmd->params.set_tooltip.tooltip);
			break;

		case PANEL_CMD_PROPERTIES:
			/*FIXME: this gotta be fixed, but we'll do it after
			  move dl -> CORBA*/
			properties(the_panel);
			break;

		default:
			fprintf(stderr, "panel_command: Oops, unknown command type %d\n",
				(int) cmd->cmd);
			break;
	}

	return NULL;
}
