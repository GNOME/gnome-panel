/* Gnome panel: Initialization routines
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 *          George Lebl
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <gnome.h>
#include "panel-widget.h"
#include "snapped-widget.h"
#include "drawer-widget.h"
#include "corner-widget.h"
#include "panel.h"
#include "panel_config.h"
#include "panel_config_global.h"
#include "menu.h"
#include "drawer.h"
#include "swallow.h"
#include "logout.h"
#include "mulapp.h"
#include "mico-glue.h"
#include "mico-parse.h"
#include "panel-util.h"
#include "launcher.h"

#include "cookie.h"

#include "main.h"

#define PANEL_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

/*list of all panel widgets created*/
GList *panel_list = NULL;

/*the timeout handeler for panel dragging id,
  yes I am too lazy to get the events to work*/
static int panel_dragged = 0;

/*the number of base panels out there, never let it go below 1*/
int base_panels = 0;

int config_sync_timeout = 0;
GList *applets_to_sync = NULL;
int panels_to_sync = FALSE;
int globals_to_sync = FALSE;
int need_complete_save = FALSE;

GArray *applets;
int applet_count;

char *panel_cfg_path=NULL;
char *old_panel_cfg_path=NULL;

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

typedef struct _LoadApplet LoadApplet;
struct _LoadApplet {
	char *id_str;
	char *path;
	char *params;
	int width;
	int height;
	char *pixmap;
	char *tooltip;
	int pos;
	PanelWidget *panel;
	char *cfgpath;
};

GList * children = NULL;

GList *load_queue=NULL;
	
/* True if parsing determined that all the work is already done.  */
int just_exit = 0;

/* The security cookie */
char *cookie;




/*execution queue stuff, execute only one applet in a row, thereby getting
  rid of some problems with applet*/
int current_exec = -1;
guint cur_timeout=0;
typedef struct _ExecQueue ExecQueue;
struct _ExecQueue {
	int applet_id;
	char *path;
	char *param;
};
GList *exec_queue=NULL;


/* These are the arguments that our application supports.  */
static struct argp_option arguments[] =
{
#define DISCARD_KEY -1
  { "discard-session", DISCARD_KEY, N_("ID"), 0, N_("Discard session"), 1 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Forward declaration of the function that gets called when one of
   our arguments is recognized.  */
static error_t parse_an_arg (int key, char *arg, struct argp_state *state);

/* This structure defines our parser.  It can be used to specify some
   options for how our parsing function should be called.  */
static struct argp parser =
{
	arguments,			/* Options.  */
	parse_an_arg,			/* The parser function.  */
	NULL,				/* Some docs.  */
	NULL,				/* Some more docs.  */
	NULL,				/* Child arguments -- gnome_init fills
					   this in for us.  */
	NULL,				/* Help filter.  */
	NULL				/* Translation domain; for the app it
					   can always be NULL.  */
};


static int really_exec_prog(int applet_id, char *path, char *param);
static void exec_queue_start_next(void);

static int
exec_queue_timeout(gpointer data)
{
	g_warning("TIMED OUT waiting to applet ID: %d!",current_exec);
	cur_timeout = -1;
	exec_queue_start_next();
	return FALSE;
}

static int
really_exec_prog(int applet_id, char *path, char *param)
{
	/*check if this is an applet which is a multi applet and
	  has something already loaded*/
	if(mulapp_is_in_list(path)) {
		printf("multi applet running, contacting [%s]\n",path);
		mulapp_load_or_add_to_queue(path,param);
		return TRUE;
	}  else {
		AppletChild *child;


		child = g_new(AppletChild,1);

		child->pid = fork();
		if(child->pid < 0)
			g_error("Can't fork!");
		if(child->pid == 0) {
			if(strlen(param)>0)
				execl(path,path,param,NULL);
			else
				execl(path,path,NULL);
			g_error("Can't execl!");
		}

		printf("started applet, pid: %d\n",child->pid);
		
		child->applet_id = applet_id;
			
		children = g_list_prepend(children,child);

		current_exec = applet_id;

		/*wait 100 seconds before timing out*/
		cur_timeout = gtk_timeout_add(100*1000,exec_queue_timeout,NULL);

		return FALSE;
	}
}

/*start the next item in the exec queue*/
static void
exec_queue_start_next(void)
{
	ExecQueue *eq;
	int ret;

	current_exec = -1;
	if(cur_timeout>0)
		gtk_timeout_remove(cur_timeout);
	cur_timeout=0;

	do {
		if(!exec_queue)
			return;

		eq = exec_queue->data;

		ret = really_exec_prog(eq->applet_id, eq->path, eq->param);
		g_free(eq->path);
		if(eq->param) g_free(eq->param);
		g_free(eq);

		exec_queue = g_list_remove_link(exec_queue,exec_queue);
	/*repeat while we are doing applets that do not require a wait
	  (second invocations of multi applets)*/
	} while(ret);
}

/* this applet has finished loading, if it was the one we were waiting
   on, start the next applet */
void
exec_queue_done(int applet_id)
{
	if(applet_id>-1 && applet_id==current_exec)
		exec_queue_start_next();
}


static void
exec_prog(int applet_id, char *path, char *param)
{
	if(current_exec==-1) {
		really_exec_prog(applet_id,path,param);
	} else {
		ExecQueue *eq = g_new(ExecQueue,1);
		eq->applet_id = applet_id;
		eq->path = g_strdup(path);
		if(param)
			eq->param = g_strdup(param);
		else
			eq->param = NULL;
		exec_queue = g_list_append(exec_queue,eq);
	}
}

/*get the default panel widget if the panel has more then one or
  just get the that one*/
PanelWidget *
get_def_panel_widget(GtkWidget *panel)
{
	g_return_val_if_fail(panel!=NULL,NULL);
	if(IS_SNAPPED_WIDGET(panel)) {
		return PANEL_WIDGET(SNAPPED_WIDGET(panel)->panel);
	} else if(IS_CORNER_WIDGET(panel)) {
		return PANEL_WIDGET(CORNER_WIDGET(panel)->panel);
	} else if(IS_DRAWER_WIDGET(panel)) {
		return PANEL_WIDGET(DRAWER_WIDGET(panel)->panel);
	}
	g_warning("unknown panel type");
	return NULL;
}


static void
queue_load_applet(char *id_str, char *path, char *params, int width, int height,
		  char *pixmap, char *tooltip,
		  int pos, PanelWidget *panel, char *cfgpath)
{
	LoadApplet *l;
	l = g_new(LoadApplet,1);

	l->id_str=g_strdup(id_str);
	if(path) l->path=g_strdup(path);
	else l->path = NULL;
	l->params=g_strdup(params);
	l->width=width;
	l->height=height;
	l->pixmap=g_strdup(pixmap);
	l->tooltip=g_strdup(tooltip);
	l->pos=pos;
	l->panel=panel;
	l->cfgpath=g_strdup(cfgpath);

	load_queue = g_list_append(load_queue,l);
}

static void
monitor_drawers(GtkWidget *w, PanelWidget *panel)
{
	DrawerWidget *drawer = gtk_object_get_data(GTK_OBJECT(panel),
						   PANEL_PARENT);
	PanelWidget *parent =
		gtk_object_get_data(GTK_OBJECT(panel->master_widget),
				    PANEL_APPLET_PARENT_KEY);
	GtkWidget *panelw = gtk_object_get_data(GTK_OBJECT(parent),
						PANEL_PARENT);
	
	if(IS_SNAPPED_WIDGET(panelw)) {
		if(drawer->state==DRAWER_SHOWN)
			SNAPPED_WIDGET(panelw)->drawers_open++;
		else
			SNAPPED_WIDGET(panelw)->drawers_open--;
	}
}

static PanelData *
get_lowest_level_master_pd(PanelWidget *panel)
{
	GtkObject *parent;
	PanelData *pd;

	while(panel->master_widget)
		panel = gtk_object_get_data(GTK_OBJECT(panel->master_widget),
					    PANEL_APPLET_PARENT_KEY);
	parent = gtk_object_get_data(GTK_OBJECT(panel),PANEL_PARENT);
	g_return_val_if_fail(parent!=NULL,NULL);
	
	pd = gtk_object_get_user_data(parent);
	g_return_val_if_fail(pd!=NULL,NULL);
	
	return pd;
}

/*whoa ... what an ugly function!, it gets the right orient type
 for an applet on the panel for pd*/
PanelOrientType
get_applet_orient(PanelWidget *panel)
{
	PanelOrientType orient=ORIENT_UP;
	PanelOrientation porient;
	GtkWidget *panelw = gtk_object_get_data(GTK_OBJECT(panel),
						PANEL_PARENT);
	PanelData *pd = gtk_object_get_user_data(GTK_OBJECT(panelw));
	PanelData *tpd;
	switch(pd->type) {
	/*case FREE_PANEL:
		orient = (panel->orient==PANEL_VERTICAL)?
			ORIENT_RIGHT:ORIENT_UP;
		break;*/
	case DRAWER_PANEL:
		porient = PANEL_WIDGET(DRAWER_WIDGET(panelw)->panel)->orient;
		tpd = get_lowest_level_master_pd(
			PANEL_WIDGET(DRAWER_WIDGET(panelw)->panel));
		switch(tpd->type){
		case FREE_PANEL:
		case DRAWER_PANEL:
			orient=(porient==PANEL_VERTICAL)?
				ORIENT_RIGHT:ORIENT_UP;
			break;
		case SNAPPED_PANEL:
			switch(SNAPPED_WIDGET(tpd->panel)->pos) {
			case SNAPPED_TOP:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_RIGHT:ORIENT_DOWN;
				break;
			case SNAPPED_BOTTOM:
			case SNAPPED_LEFT:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_RIGHT:ORIENT_UP;
				break;
			case SNAPPED_RIGHT:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_LEFT:ORIENT_UP;
				break;
			}
			break;
		case CORNER_PANEL:
			switch(CORNER_WIDGET(tpd->panel)->pos) {
			case CORNER_NE:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_LEFT:ORIENT_DOWN;
				break;
			case CORNER_SE:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_LEFT:ORIENT_UP;
				break;
			case CORNER_SW:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_RIGHT:ORIENT_UP;
				break;
			case CORNER_NW:
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_RIGHT:ORIENT_DOWN;
				break;
			}
		default: break;
		}
		break;
	case SNAPPED_PANEL:
		switch(SNAPPED_WIDGET(panelw)->pos) {
		case SNAPPED_TOP: orient = ORIENT_DOWN; break;
		case SNAPPED_BOTTOM: orient = ORIENT_UP; break;
		case SNAPPED_LEFT: orient = ORIENT_RIGHT; break;
		case SNAPPED_RIGHT: orient = ORIENT_LEFT; break;
		}
		break;
	case CORNER_PANEL:
		if(PANEL_WIDGET(CORNER_WIDGET(panelw)->panel)->orient ==
		   PANEL_HORIZONTAL) {
			switch(CORNER_WIDGET(panelw)->pos) {
			case CORNER_SE: 
			case CORNER_SW:
				orient = ORIENT_UP;
				break;
			case CORNER_NE:
			case CORNER_NW:
				orient = ORIENT_DOWN;
				break;
			}
		} else { /*vertical*/
			switch(CORNER_WIDGET(panelw)->pos) {
			case CORNER_SE: 
			case CORNER_NE:
				orient = ORIENT_LEFT;
				break;
			case CORNER_SW:
			case CORNER_NW:
				orient = ORIENT_RIGHT;
				break;
			}
		}
	default: break;
	}
	return orient;
}

static void
drawer_realize_cb(GtkWidget *button, Drawer *drawer)
{
	reposition_drawer(drawer);
	if(DRAWER_WIDGET(drawer->drawer)->state == DRAWER_SHOWN)
		gtk_widget_show(drawer->drawer);
	else {
		if(!GTK_WIDGET_REALIZED(drawer->drawer))
			gtk_widget_realize(drawer->drawer);
		gtk_widget_hide(drawer->drawer);
	}
}



void
load_applet(char *id_str, char *path, char *params, int width, int height,
	    char *pixmap, char *tooltip,
	    int pos, PanelWidget *panel, char *cfgpath)
{
	if(strcmp(id_str,EXTERN_ID) == 0) {
		char *fullpath;
		char *param;

		/*start nothing, applet is taking care of everything*/
		if(path == NULL ||
		   path[0] == '\0')
		   	return;

		if(!params)
			param = "";
		else
			param = params;

		if(!cfgpath || !*cfgpath)
			cfgpath = g_copy_strings(old_panel_cfg_path,
						 "Applet_Dummy/",NULL);
		else
			/*we will free this lateer*/
			cfgpath = g_strdup(cfgpath);

		/*make it an absolute path, same as the applets will
		  interpret it and the applets will sign themselves as
		  this, so it has to be exactly the same*/
		if(path[0]!='#')
			fullpath = get_full_path(path);
		else
			fullpath = g_strdup(path);

		if(reserve_applet_spot (id_str, fullpath,params,
					panel, pos, cfgpath,
					APPLET_EXTERN_PENDING)==0) {
			g_warning("Whoops! for some reason we can't add "
				  "to the panel");
			g_free(fullpath);
			g_free(cfgpath);
			return;
		}
		
		/*'#' marks an applet that will take care of starting
		  itself but wants us to reserve a spot for it*/
		if(path[0]!='#')
			exec_prog(applet_count-1,fullpath,param);

		g_free(cfgpath);
		g_free(fullpath);
	} else if(strcmp(id_str,MENU_ID) == 0) {
		Menu *menu;

		menu = create_menu_applet(params, ORIENT_UP);

		if(menu)
			register_toy(menu->button,menu->menu,menu,MENU_ID,
				     NULL,params, pos,panel,NULL,APPLET_MENU);
	} else if(strcmp(id_str,LAUNCHER_ID) == 0) {
		Launcher *launcher;

		launcher = create_launcher(params);

		if(launcher) {
			register_toy(launcher->button,NULL,launcher,
				     LAUNCHER_ID,NULL,NULL,pos,panel,
				     NULL,APPLET_LAUNCHER);
			
			gtk_tooltips_set_tip (panel_tooltips,
					      launcher->button->parent,
					      launcher->dentry->comment,NULL);
			
			applet_add_callback(applet_count-1,"properties",
					    GNOME_STOCK_MENU_PROP,
					    _("Properties..."));
		}
	} else if(strcmp(id_str,DRAWER_ID) == 0) {
		Drawer *drawer;
		PanelWidget *dr_panel;

		if(!params) {
			drawer = create_empty_drawer_applet(
				tooltip,pixmap,
				get_applet_orient(panel));
			if(drawer) panel_setup(drawer->drawer);
		} else {
			int i;
			PanelData *dr_pd;

			sscanf(params,"%d",&i);
			dr_pd = g_list_nth(panel_list,i)->data;

			drawer=create_drawer_applet(dr_pd->panel,
						    tooltip,pixmap,
						    get_applet_orient(panel));
		}
		
		if(!drawer)
			return;

		g_return_if_fail(drawer != NULL);

		register_toy(drawer->button,drawer->drawer,drawer,DRAWER_ID,
			     NULL, params, pos, panel, NULL, APPLET_DRAWER);
		
		/*the panel of the drawer*/
		dr_panel = PANEL_WIDGET(DRAWER_WIDGET(drawer->drawer)->panel);

		gtk_signal_connect(GTK_OBJECT(drawer->button), "clicked",
				   GTK_SIGNAL_FUNC(monitor_drawers),
				   dr_panel);

		if(DRAWER_WIDGET(drawer->drawer)->state == DRAWER_SHOWN) {
			GtkWidget *wpanel;
			/*pop up, if popped down*/
			wpanel = gtk_object_get_data(GTK_OBJECT(panel),
						      PANEL_PARENT);
			if(IS_SNAPPED_WIDGET(wpanel)) {
				/*drawer is open so we track it*/
				SNAPPED_WIDGET(wpanel)->drawers_open++;
				snapped_widget_pop_up(SNAPPED_WIDGET(wpanel));
			}
		} 

		panel_widget_add_forbidden(
			PANEL_WIDGET(DRAWER_WIDGET(drawer->drawer)->panel));

		gtk_tooltips_set_tip (panel_tooltips,drawer->button->parent,
				      drawer->tooltip,NULL);
		if(GTK_WIDGET_REALIZED(drawer->button)) {
			reposition_drawer(drawer);
			if(DRAWER_WIDGET(drawer->drawer)->state == DRAWER_SHOWN)
				gtk_widget_show(drawer->drawer);
			else {
				/*hmm ... weird but it works*/
				gtk_widget_set_uposition(drawer->drawer,
							 -100,-100);
				gtk_widget_show(drawer->drawer);
				/*gtk_widget_realize(drawer->drawer);*/
				gtk_widget_hide(drawer->drawer);
			}
		} else
			gtk_signal_connect_after(GTK_OBJECT(drawer->button),
						 "realize",
						 GTK_SIGNAL_FUNC(
							drawer_realize_cb),
						 drawer);
		gtk_signal_connect_after(GTK_OBJECT(drawer->drawer),
					 "realize",
					 GTK_SIGNAL_FUNC(drawer_realize_cb),
					 drawer);

		applet_add_callback(applet_count-1,"properties",
				    GNOME_STOCK_MENU_PROP,
				    _("Properties..."));
	} else if(strcmp(id_str,SWALLOW_ID) == 0) {
		Swallow *swallow;

		swallow = create_swallow_applet(params, width, height,
						SWALLOW_HORIZONTAL);
		if(!swallow)
			return;

		register_toy(swallow->table,NULL,swallow,SWALLOW_ID,
			     path, params,pos, panel,NULL,
			     APPLET_SWALLOW);

		if(path && *path) {
			char *s = g_copy_strings("(true; ",path," &)",NULL);
			system(s);
			g_free(s);
		}
	} else if(strcmp(id_str,LOGOUT_ID) == 0) {
		GtkWidget *logout;

		logout = create_logout_widget();
		
		if(logout)
			register_toy(logout,NULL,NULL,LOGOUT_ID,NULL,params,
				     pos, panel,NULL,APPLET_LOGOUT);
	}
}

static void
load_queued_applets(void)
{
	GList *list;

	for(list = load_queue;list!=NULL;list=g_list_next(list)) {
		LoadApplet *l=list->data;
		load_applet(l->id_str,l->path,l->params,l->width,l->height,
			    l->pixmap,l->tooltip,
			    l->pos,l->panel,l->cfgpath);
		g_free(l->id_str);
		if(l->path) g_free(l->path);
		g_free(l->params);
		g_free(l->pixmap);
		g_free(l->tooltip);
		g_free(l->cfgpath); 
		g_free(l);
	}
	g_list_free(load_queue);
}

static void
load_default_applets(void)
{
	queue_load_applet(MENU_ID,NULL,".",0,0,NULL,NULL,
			  PANEL_UNKNOWN_APPLET_POSITION,panels->data,NULL);
	queue_load_applet(EXTERN_ID,"gen_util_applet","--clock",0,0,NULL,NULL,
			  PANEL_UNKNOWN_APPLET_POSITION,panels->data,NULL);
	/*we laoded default applets, so we didn't find the config or
	  something else was wrong, so do complete save when next syncing*/
	need_complete_save = TRUE;
}

static void
init_user_applets(void)
{
	char  buf[256];
	int   count,num;	

	g_snprintf(buf,256,"%spanel/Config/applet_count=0",old_panel_cfg_path);
	count=gnome_config_get_int(buf);
	for(num=1;num<=count;num++) {
		char *applet_name;
		char *applet_params;
		char *applet_pixmap;
		char *applet_tooltip;
		char *applet_path;
		int applet_width,applet_height;
		int   pos=0,panel_num;
		PanelWidget *panel;

		g_snprintf(buf,256,"%sApplet_%d/config/", old_panel_cfg_path, num);
		gnome_config_push_prefix(buf);
		applet_name = gnome_config_get_string("id=Unknown");
		
		if(strcmp(applet_name,EMPTY_ID)==0) {
			g_free(applet_name);
			continue;
		}

		applet_path = gnome_config_get_string("execpath=");
		applet_params = gnome_config_get_string("parameters=");
		applet_pixmap = gnome_config_get_string("pixmap=");
		applet_tooltip = gnome_config_get_string("tooltip=");
		applet_width = gnome_config_get_int("width=0");
		applet_height = gnome_config_get_int("height=0");

		g_snprintf(buf,256,"position=%d",
			   PANEL_UNKNOWN_APPLET_POSITION);
		pos = gnome_config_get_int(buf);
		panel_num = gnome_config_get_int("panel=0");
		{
			GList *list = g_list_nth(panels,panel_num);
			if(!list)  {
				g_warning("Can't find panel, "
					  "putting applet on the first one");
				panel = panels->data;
			} else
				panel = list->data;
		}
			
		
		/*if we are to right stick this, make the number large, 
		 INT_MAX/2 should allways be large enough */
		pos += gnome_config_get_bool("right_stick=false")?INT_MAX/2:0;

		gnome_config_pop_prefix();

		/*this is the config path to be passed to the applet when it
		  loads*/
		g_snprintf(buf,256,"%sApplet_%d/",old_panel_cfg_path,num);
		queue_load_applet(applet_name, applet_path, applet_params,
				  applet_width,applet_height,
				  applet_pixmap, applet_tooltip,
				  pos, panel, buf);

		g_free(applet_name);
		if(applet_path) g_free(applet_path);
		g_free(applet_params);
		g_free(applet_pixmap);
		g_free(applet_tooltip);
	}
}

void
change_window_cursor(GdkWindow *window, GdkCursorType cursor_type)
{
	GdkCursor *cursor = gdk_cursor_new(cursor_type);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
}

static void
panel_realize(GtkWidget *widget, gpointer data)
{
	change_window_cursor(widget->window, GDK_ARROW);
	
	if(IS_SNAPPED_WIDGET(widget))
		snapped_widget_enable_buttons(SNAPPED_WIDGET(widget));
	else if(IS_CORNER_WIDGET(widget))
		corner_widget_enable_buttons(CORNER_WIDGET(widget));
}

/*we call this recursively*/
static void orient_change_foreach(gpointer data, gpointer user_data);

void
orientation_change(int applet_id, PanelWidget *panel)
{
	AppletInfo *info = get_applet_info(applet_id);
	if(info->type == APPLET_EXTERN) {
		send_applet_change_orient(info->id_str,info->applet_id,
					  get_applet_orient(panel));
	} else if(info->type == APPLET_MENU) {
		Menu *menu = info->data;
		set_menu_applet_orient(menu,get_applet_orient(panel));
	} else if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		DrawerWidget *dw = DRAWER_WIDGET(info->assoc);
		reposition_drawer(drawer);
		set_drawer_applet_orient(drawer,get_applet_orient(panel));
		panel_widget_foreach(PANEL_WIDGET(dw->panel),
				     orient_change_foreach,
				     (gpointer)dw->panel);
	} else if(info->type == APPLET_SWALLOW) {
		Swallow *swallow = info->data;

		if(panel->orient == PANEL_VERTICAL)
			set_swallow_applet_orient(swallow,SWALLOW_VERTICAL);
		else
			set_swallow_applet_orient(swallow,SWALLOW_HORIZONTAL);
	}
}

static void
orient_change_foreach(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	PanelWidget *panel = user_data;
	
	orientation_change(applet_id,panel);
}


static void
panel_orient_change(GtkWidget *widget,
		    PanelOrientation orient,
		    gpointer data)
{
	panel_widget_foreach(PANEL_WIDGET(widget),orient_change_foreach,
			     widget);
	panels_to_sync = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_orient(gtk_object_get_data(GTK_OBJECT(widget),
						 PANEL_PARENT));
}

static void
snapped_pos_change(GtkWidget *widget,
		   SnappedPos pos,
		   gpointer data)
{
	panel_widget_foreach(PANEL_WIDGET(SNAPPED_WIDGET(widget)->panel),
			     orient_change_foreach,
			     SNAPPED_WIDGET(widget)->panel);
	panels_to_sync = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_orient(widget);
}

static void
corner_pos_change(GtkWidget *widget,
		  CornerPos pos,
		  gpointer data)
{
	panel_widget_foreach(PANEL_WIDGET(CORNER_WIDGET(widget)->panel),
			     orient_change_foreach,
			     CORNER_WIDGET(widget)->panel);
	panels_to_sync = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_orient(widget);
}

void
back_change(int applet_id,
	    PanelWidget *panel)
{
  
	AppletInfo *info = get_applet_info(applet_id);
	if(info->type == APPLET_EXTERN) {
		send_applet_change_back(info->id_str, info->applet_id,
					panel->back_type,panel->back_pixmap,
					&panel->back_color);
	}
	/*FIXME: probably set the launcher background as well*/
}


static void
back_change_foreach(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	PanelWidget *panel = user_data;

	back_change(applet_id,panel);
}

static void
panel_back_change(GtkWidget *widget,
		  PanelBackType type,
		  char *pixmap,
		  GdkColor *color)
{
	panel_widget_foreach(PANEL_WIDGET(widget),back_change_foreach,widget);
	panels_to_sync = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_back(widget);
}

static void state_hide_foreach(gpointer data, gpointer user_data);

static void
state_restore_foreach(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);
	
	if(info->type == APPLET_DRAWER) {
		DrawerWidget *drawer = DRAWER_WIDGET(info->assoc);
		if(drawer->state == DRAWER_SHOWN) {
			drawer_widget_restore_state(drawer);
			panel_widget_foreach(PANEL_WIDGET(drawer->panel),
					     state_restore_foreach,
					     NULL);
		} else { /*it's hidden*/
			gtk_widget_hide(info->assoc);
			panel_widget_foreach(PANEL_WIDGET(drawer->panel),
					     state_hide_foreach,
					     NULL);
		}
	}
}

static void
state_hide_foreach(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);

	if(info->type == APPLET_DRAWER) {
		DrawerWidget *drawer = DRAWER_WIDGET(info->assoc);
		/*if(drawer->state == DRAWER_SHOWN) {*/
		gtk_widget_hide(info->assoc);
		panel_widget_foreach(PANEL_WIDGET(drawer->panel),
				     state_hide_foreach,
				     NULL);
		/*}*/
	}
}

static int
snapped_state_change(GtkWidget *widget,
		     SnappedState state,
		     gpointer data)
{
	if(state==SNAPPED_SHOWN)
		panel_widget_foreach(PANEL_WIDGET(SNAPPED_WIDGET(widget)->panel),
				     state_restore_foreach,
				     (gpointer)widget);
	else
		panel_widget_foreach(PANEL_WIDGET(SNAPPED_WIDGET(widget)->panel),
				     state_hide_foreach,
				     (gpointer)widget);

	panels_to_sync = TRUE;

	return TRUE;
}
static int
corner_state_change(GtkWidget *widget,
		    CornerState state,
		    gpointer data)
{
	if(state==CORNER_SHOWN)
		panel_widget_foreach(PANEL_WIDGET(CORNER_WIDGET(widget)->panel),
				     state_restore_foreach,
				     (gpointer)widget);
	else
		panel_widget_foreach(PANEL_WIDGET(CORNER_WIDGET(widget)->panel),
				     state_hide_foreach,
				     (gpointer)widget);

	panels_to_sync = TRUE;

	return TRUE;
}
static int
drawer_state_change(GtkWidget *widget,
		    DrawerState state,
		    gpointer data)
{
	if(state==DRAWER_SHOWN)
		panel_widget_foreach(PANEL_WIDGET(DRAWER_WIDGET(widget)->panel),
				     state_restore_foreach,
				     (gpointer)widget);
	else
		panel_widget_foreach(PANEL_WIDGET(DRAWER_WIDGET(widget)->panel),
				     state_hide_foreach,
				     (gpointer)widget);

	panels_to_sync = TRUE;

	return TRUE;
}

static int
panel_size_allocate(GtkWidget *widget, GtkAllocation *alloc, gpointer data)
{
	if(IS_DRAWER_WIDGET(widget)) {
		Drawer *drawer = gtk_object_get_data(GTK_OBJECT(widget),
						     DRAWER_PANEL_KEY);
		if(!drawer)
			return FALSE;

		if(DRAWER_WIDGET(widget)->state == DRAWER_SHOWN)
			reposition_drawer(drawer);
		panels_to_sync = TRUE;
	}
	return FALSE;
}

/*the following is slightly ugly .... but it works, I need to send the
  orient change in an idle handeler*/
static int
panel_applet_added_idle(gpointer data)
{
	int applet_id = GPOINTER_TO_INT(data);
	AppletInfo *info = get_applet_info(applet_id);
	PanelWidget *panel = gtk_object_get_data(GTK_OBJECT(info->widget),
						 PANEL_APPLET_PARENT_KEY);

	orientation_change(applet_id,panel);

	return FALSE;
}

static void
panel_applet_added(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(applet)));
	AppletInfo *info = get_applet_info(applet_id);
	GtkWidget *panelw = gtk_object_get_data(GTK_OBJECT(widget),
						PANEL_PARENT);
	
	/*on a real add the info will be NULL as the only adding
	  is done in register_toy and that doesn't add the info to the
	  array until after the add, so we can be sure this was
	  generated on a reparent*/
	if(IS_SNAPPED_WIDGET(panelw) &&
	   info && info->type == APPLET_DRAWER) {
		PanelWidget *p = gtk_object_get_data(GTK_OBJECT(info->widget),
						     PANEL_APPLET_ASSOC_PANEL_KEY);
		DrawerWidget *dw = gtk_object_get_data(GTK_OBJECT(p),
						       PANEL_PARENT);
		if(dw->state == DRAWER_SHOWN)
			SNAPPED_WIDGET(panelw)->drawers_open++;
	}

	/*pop the panel up on addition*/
	if(IS_SNAPPED_WIDGET(panelw)) {
		snapped_widget_pop_up(SNAPPED_WIDGET(panelw));
		/*try to pop down though if the mouse is out*/
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(panelw));
	}

	gtk_idle_add(panel_applet_added_idle,GINT_TO_POINTER(applet_id));

	need_complete_save = TRUE;
}

static void
count_open_drawers(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);
	int *count = user_data;
	if(info->type == APPLET_DRAWER &&
	   DRAWER_WIDGET(info->assoc)->state == DRAWER_SHOWN)
		(*count)++;
}

static void
panel_applet_removed(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	GtkWidget *parentw = gtk_object_get_data(GTK_OBJECT(widget),
						 PANEL_PARENT);
	if(IS_SNAPPED_WIDGET(parentw)) {
		int drawers_open = 0;

		panel_widget_foreach(PANEL_WIDGET(widget),
				     count_open_drawers,
				     &drawers_open);
		SNAPPED_WIDGET(parentw)->drawers_open = drawers_open;
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(parentw));
	}

	need_complete_save = TRUE;
}

static void
panel_menu_position (GtkMenu *menu, int *x, int *y, gpointer data)
{
	GtkWidget *w = data;
	int wx, wy;

	g_return_if_fail(w != NULL);

	gdk_window_get_origin (w->window, &wx, &wy);

	if(IS_DRAWER_WIDGET(w)) {
		PanelWidget *panel = PANEL_WIDGET(DRAWER_WIDGET(w)->panel);
		if(panel->orient==PANEL_VERTICAL) {
			gtk_widget_get_pointer(w, NULL, y);
			*x = wx + w->allocation.width;
			*y += wy;
		} else {
			gtk_widget_get_pointer(w, x, NULL);
			*x += wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
		}
	} else if(IS_SNAPPED_WIDGET(w)) {
		switch(SNAPPED_WIDGET(w)->pos) {
		case SNAPPED_BOTTOM:
			gtk_widget_get_pointer(w, x, NULL);
			*x += wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
			break;
		case SNAPPED_TOP:
			gtk_widget_get_pointer(w, x, NULL);
			*x += wx;
			*y = wy + w->allocation.height;
			break;
		case SNAPPED_LEFT:
			gtk_widget_get_pointer(w, NULL, y);
			*x = wx + w->allocation.width;
			*y += wy;
			break;
		case SNAPPED_RIGHT:
			gtk_widget_get_pointer(w, NULL, y);
			*x = wx - GTK_WIDGET (menu)->allocation.width;
			*y += wy;
			break;
		}
	} else if(IS_CORNER_WIDGET(w)) {
		PanelWidget *panel = PANEL_WIDGET(CORNER_WIDGET(w)->panel);
		if(panel->orient==PANEL_HORIZONTAL) {
			switch(CORNER_WIDGET(w)->pos) {
			case CORNER_NE:
			case CORNER_NW:
				gtk_widget_get_pointer(w, x, NULL);
				*x += wx;
				*y = wy + w->allocation.height;
				break;
			case CORNER_SE:
			case CORNER_SW:
				gtk_widget_get_pointer(w, x, NULL);
				*x += wx;
				*y = wy - GTK_WIDGET (menu)->allocation.height;
				break;
			}
		} else { /*vertical*/
			switch(CORNER_WIDGET(w)->pos) {
			case CORNER_NE:
			case CORNER_SE:
				gtk_widget_get_pointer(w, NULL, y);
				*x = wx - GTK_WIDGET (menu)->allocation.width;
				*y += wy;
				break;
			case CORNER_SW:
			case CORNER_NW:
				gtk_widget_get_pointer(w, NULL, y);
				*x = wx + w->allocation.width;
				*y += wy;
				break;
			}
		}
	}

	if(*x + GTK_WIDGET (menu)->allocation.width > gdk_screen_width())
		*x=gdk_screen_width() - GTK_WIDGET (menu)->allocation.width;
	if(*x < 0) *x =0;

	if(*y + GTK_WIDGET (menu)->allocation.height > gdk_screen_height())
		*y=gdk_screen_height() - GTK_WIDGET (menu)->allocation.height;
	if(*y < 0) *y =0;
}

static void
menu_deactivate(GtkWidget *w, GtkWidget *panel)
{
	if(IS_SNAPPED_WIDGET(panel))
		SNAPPED_WIDGET(panel)->autohide_inhibit = FALSE;
}

static void
snapped_panel_move(SnappedWidget *snapped, double x, double y)
{
	SnappedPos newloc;
	int minx, miny, maxx, maxy;

	gdk_window_get_geometry (GTK_WIDGET(snapped)->window,
				 &minx, &miny, &maxx, &maxy, NULL);
	gdk_window_get_origin (GTK_WIDGET(snapped)->window, &minx, &miny);
	maxx += minx;
	maxy += miny;
	if (x >= minx &&
	    x <= maxx &&
	    y >= miny &&
	    y <= maxy)
 	        return;

	if ((x) * gdk_screen_height() > y * gdk_screen_width() ) {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() )
			newloc = SNAPPED_TOP;
		else
			newloc = SNAPPED_RIGHT;
	} else {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() )
			newloc = SNAPPED_LEFT;
		else
			newloc = SNAPPED_BOTTOM;
	}
	if(newloc != snapped->pos)
		snapped_widget_change_pos(snapped, newloc);
}

static void
corner_panel_move(CornerWidget *corner, double x, double y)
{
	CornerPos newloc;
	PanelOrientation neworient;
	int minx, miny, maxx, maxy;

	gdk_window_get_geometry (GTK_WIDGET(corner)->window,
				 &minx, &miny, &maxx, &maxy, NULL);
	gdk_window_get_origin (GTK_WIDGET(corner)->window, &minx, &miny);
	maxx += minx;
	maxy += miny;
	if (x >= minx &&
	    x <= maxx &&
	    y >= miny &&
	    y <= maxy)
 	        return;

	if ((x) * gdk_screen_height() > y * gdk_screen_width() ) {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() ) {
			neworient = PANEL_HORIZONTAL;
			if(x<gdk_screen_width()/2)
				newloc = CORNER_NW;
			else
				newloc = CORNER_NE;
		} else {
			neworient = PANEL_VERTICAL;
			if(y<gdk_screen_height()/2)
				newloc = CORNER_NE;
			else
				newloc = CORNER_SE;
		}
	} else {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() ) {
			neworient = PANEL_VERTICAL;
			if(y<gdk_screen_height()/2)
				newloc = CORNER_NW;
			else
				newloc = CORNER_SW;
		} else {
			neworient = PANEL_HORIZONTAL;
			if(x<gdk_screen_width()/2)
				newloc = CORNER_SW;
			else
				newloc = CORNER_SE;
		}
	}
	if(newloc != corner->pos ||
	   neworient != PANEL_WIDGET(corner->panel)->orient)
		corner_widget_change_pos_orient(corner, newloc, neworient);
}

static int
snapped_panel_move_timeout(gpointer data)
{
	int x,y;

	gdk_window_get_pointer(NULL,&x,&y,NULL);
	snapped_panel_move(data,x,y);
	return TRUE;
}
static int
corner_panel_move_timeout(gpointer data)
{
	int x,y;

	gdk_window_get_pointer(NULL,&x,&y,NULL);
	corner_panel_move(data,x,y);
	return TRUE;
}

static int
panel_destroy(GtkWidget *widget, gpointer data)
{
	PanelData *pd = gtk_object_get_user_data(GTK_OBJECT(widget));
	GtkWidget *panel_menu = data;

	if(IS_DRAWER_WIDGET(widget)) {
		PanelWidget *panel = PANEL_WIDGET(DRAWER_WIDGET(widget)->panel);
		if(panel->master_widget) {
			int applet_id =
				GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(panel->master_widget)));
			AppletInfo *info = get_applet_info(applet_id);
			info->assoc = NULL;
			panel_clean_applet(applet_id);
		}
	} else if(IS_SNAPPED_WIDGET(widget) ||
		  IS_CORNER_WIDGET(widget)) {
		/*this is a base panel and we just lost it*/
		base_panels--;
	}

	if(panel_menu)
		gtk_widget_unref(panel_menu);
	
	panel_list = g_list_remove(panel_list,pd);
	g_free(pd);

	return FALSE;
}

static void
applet_move_foreach(gpointer data, gpointer user_data)
{
	int applet_id = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);
	
	if(g_list_find(applets_to_sync, GINT_TO_POINTER(applet_id))==NULL)
		applets_to_sync = g_list_prepend(applets_to_sync,
						 GINT_TO_POINTER(applet_id));

	if(info->type == APPLET_DRAWER) {
		Drawer *drawer = info->data;
		DrawerWidget *drawerw = DRAWER_WIDGET(info->assoc);
		reposition_drawer(drawer);
		panel_widget_foreach(PANEL_WIDGET(drawerw->panel),
				     applet_move_foreach,
				     NULL);
	}
}

static void
panel_applet_move(GtkWidget *panel,GtkWidget *widget, gpointer data)
{
	applet_move_foreach(widget,NULL);
}


static int
panel_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GdkEventButton *bevent;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		bevent = (GdkEventButton *) event;
		switch(bevent->button) {
		case 3: /* fall through */
		case 1:
			if(!panel_applet_in_drag) {
				if(IS_DRAWER_WIDGET(widget)) {
					PanelWidget *panel =
						PANEL_WIDGET(DRAWER_WIDGET(widget)->panel);
					GtkWidget *rem = 
						gtk_object_get_data(GTK_OBJECT(widget),
								    "remove_item");
					if(panel_widget_get_applet_count(panel)>0)
						gtk_widget_set_sensitive(rem,FALSE);
					else
						gtk_widget_set_sensitive(rem,TRUE);
				} else if(IS_SNAPPED_WIDGET(widget)) {
					SnappedWidget *snapped =
						SNAPPED_WIDGET(widget);
					PanelWidget *panel = PANEL_WIDGET(snapped->panel);
					GtkWidget *rem = 
						gtk_object_get_data(GTK_OBJECT(widget),
								    "remove_item");
					if(panel_widget_get_applet_count(panel)>0 ||
					   base_panels <= 1)
						gtk_widget_set_sensitive(rem,FALSE);
					else
						gtk_widget_set_sensitive(rem,TRUE);
					snapped->autohide_inhibit = TRUE;
					snapped_widget_queue_pop_down(snapped);
				} else if(IS_CORNER_WIDGET(widget)) {
					CornerWidget *corner =
						CORNER_WIDGET(widget);
					PanelWidget *panel = PANEL_WIDGET(corner->panel);
					GtkWidget *rem = 
						gtk_object_get_data(GTK_OBJECT(widget),
								    "remove_item");
					if(panel_widget_get_applet_count(panel)>0 ||
					   base_panels <= 1)
						gtk_widget_set_sensitive(rem,FALSE);
					else
						gtk_widget_set_sensitive(rem,TRUE);
				}
				gtk_menu_popup(GTK_MENU(data), NULL, NULL,
					       panel_menu_position,
					       widget, bevent->button,
					       bevent->time);
				return TRUE;
			}
			break;
		case 2:
			/*this should probably be in snapped widget*/
			if(!panel_dragged &&
			   (IS_SNAPPED_WIDGET(widget) ||
			    IS_CORNER_WIDGET(widget))) {
				GdkCursor *cursor = gdk_cursor_new (GDK_FLEUR);
				gtk_grab_add(widget);
				gdk_pointer_grab (widget->window,
						  FALSE,
						  PANEL_EVENT_MASK,
						  NULL,
						  cursor,
						  bevent->time);
				gdk_cursor_destroy (cursor);
				if(IS_SNAPPED_WIDGET(widget)) {
					SNAPPED_WIDGET(widget)->autohide_inhibit = TRUE;
					panel_dragged =
						gtk_timeout_add(30,
								snapped_panel_move_timeout,
								widget);
				} else { /*CORNER_WIDGET*/
					panel_dragged =
						gtk_timeout_add(30,
								corner_panel_move_timeout,
								widget);
				}
				return TRUE;
			}
			break;
		}
		break;

	case GDK_BUTTON_RELEASE:
		bevent = (GdkEventButton *) event;
		if(panel_dragged) {
			if(IS_SNAPPED_WIDGET(widget)) {
				snapped_panel_move(SNAPPED_WIDGET(widget),
					   bevent->x_root, bevent->y_root);
				SNAPPED_WIDGET(widget)->autohide_inhibit = FALSE;
				snapped_widget_queue_pop_down(SNAPPED_WIDGET(widget));
			} else
				corner_panel_move(CORNER_WIDGET(widget),
					   bevent->x_root, bevent->y_root);
			gdk_pointer_ungrab(bevent->time);
			gtk_grab_remove(widget);
			gtk_timeout_remove(panel_dragged);
			panel_dragged = 0;
			return TRUE;
		}

		break;

	default:
		break;
	}

	return FALSE;
}


static GtkWidget *
listening_parent(GtkWidget *widget)
{
	if (GTK_WIDGET_NO_WINDOW(widget))
		return listening_parent(widget->parent);

	return widget;
}

static int
panel_sub_event_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GdkEventButton *bevent;
	switch (event->type) {
		/*pass these to the parent!*/
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
			bevent = (GdkEventButton *) event;
			/*if the widget is a button we want to keep the
			  button 1 events*/
			if(!GTK_IS_BUTTON(widget) || bevent->button!=1)
				return gtk_widget_event(
					listening_parent(widget->parent),
							 event);

			break;

		default:
			break;
	}

	return FALSE;
}


static void
bind_panel_events(GtkWidget *widget, gpointer data)
{
	if (!GTK_WIDGET_NO_WINDOW(widget))
		gtk_signal_connect(GTK_OBJECT(widget), "event",
				   (GtkSignalFunc) panel_sub_event_handler,
				   NULL);
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_panel_events, NULL);
}



static void
panel_widget_setup(PanelWidget *panel)
{
	gtk_signal_connect(GTK_OBJECT(panel),
			   "applet_added",
			   GTK_SIGNAL_FUNC(panel_applet_added),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "applet_removed",
			   GTK_SIGNAL_FUNC(panel_applet_removed),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "applet_move",
			   GTK_SIGNAL_FUNC(panel_applet_move),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panel),
			   "back_change",
			   GTK_SIGNAL_FUNC(panel_back_change),
			   NULL);
}

void
panel_setup(GtkWidget *panelw)
{
	GtkWidget *panel_menu;
	PanelData *pd;
	
	pd = g_new(PanelData,1);
	pd->panel = panelw;
	if(IS_DRAWER_WIDGET(panelw))
		pd->type = DRAWER_PANEL;
	else if(IS_SNAPPED_WIDGET(panelw))
		pd->type = SNAPPED_PANEL;
	else if(IS_CORNER_WIDGET(panelw))
		pd->type = CORNER_PANEL;
	else
		g_warning("unknown panel type");
	
	panel_list = g_list_append(panel_list,pd);
	
	gtk_object_set_user_data(GTK_OBJECT(panelw),pd);

	panel_menu = create_panel_root_menu(panelw);

	if(IS_DRAWER_WIDGET(panelw)) {
		PanelWidget *panel = PANEL_WIDGET(DRAWER_WIDGET(panelw)->panel);
		panel_widget_setup(panel);
		gtk_signal_connect(GTK_OBJECT(panel),
				   "orient_change",
				   GTK_SIGNAL_FUNC(panel_orient_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "state_change",
				   GTK_SIGNAL_FUNC(drawer_state_change),
				   NULL);
	} else if(IS_SNAPPED_WIDGET(panelw)) {
		PanelWidget *panel =
			PANEL_WIDGET(SNAPPED_WIDGET(panelw)->panel);
		snapped_widget_disable_buttons(SNAPPED_WIDGET(panelw));
		panel_widget_setup(panel);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "pos_change",
				   GTK_SIGNAL_FUNC(snapped_pos_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "state_change",
				   GTK_SIGNAL_FUNC(snapped_state_change),
				   NULL);
		
		/*this is a base panel*/
		base_panels++;
	} else if(IS_CORNER_WIDGET(panelw)) {
		PanelWidget *panel =
			PANEL_WIDGET(CORNER_WIDGET(panelw)->panel);
		corner_widget_disable_buttons(CORNER_WIDGET(panelw));
		panel_widget_setup(panel);
		gtk_signal_connect(GTK_OBJECT(panel),
				   "orient_change",
				   GTK_SIGNAL_FUNC(panel_orient_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "pos_change",
				   GTK_SIGNAL_FUNC(corner_pos_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "state_change",
				   GTK_SIGNAL_FUNC(corner_state_change),
				   NULL);
		
		/*this is a base panel*/
		base_panels++;
	} else
		g_warning("unknown panel type");
	
	gtk_widget_set_events(panelw,
			      gtk_widget_get_events(panelw) |
			      PANEL_EVENT_MASK);

	gtk_signal_connect(GTK_OBJECT(panelw),
			   "size_allocate",
			   GTK_SIGNAL_FUNC(panel_size_allocate),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panelw),
			   "destroy",
			   GTK_SIGNAL_FUNC(panel_destroy),
			   panel_menu);

	/*with this we capture button presses throughout all the widgets of the
	  panel*/
	gtk_signal_connect(GTK_OBJECT(panelw),
			   "event",
			   GTK_SIGNAL_FUNC(panel_event),
			   panel_menu);
	if (GTK_IS_CONTAINER(panelw))
		gtk_container_foreach (GTK_CONTAINER (panelw),
				       bind_panel_events, NULL);

	gtk_signal_connect(GTK_OBJECT(panel_menu),
			   "deactivate",
			   GTK_SIGNAL_FUNC(menu_deactivate),
			   panelw);

	if(GTK_WIDGET_REALIZED(GTK_WIDGET(panelw)))
		panel_realize(GTK_WIDGET(panelw),NULL);
	else
		gtk_signal_connect_after(GTK_OBJECT(panelw), "realize",
					 GTK_SIGNAL_FUNC(panel_realize),
					 NULL);
}

static void
init_user_panels(void)
{
	char  buf[256];
	int   count,num;	
	GtkWidget *panel;

	g_snprintf(buf,256,"%spanel/Config/panel_count=0",old_panel_cfg_path);
	count=gnome_config_get_int(buf);

	/*load a default snapped panel on the bottom of the screen,
	  it is required to have at least one panel for this all
	  to work, so this is the way we find out if there was no
	  config from last time*/
	if(count<=0)  {
		panel = snapped_widget_new(SNAPPED_BOTTOM,
					   SNAPPED_EXPLICIT_HIDE,
					   SNAPPED_SHOWN,
					   PANEL_BACK_NONE,
					   NULL,
					   TRUE,
					   NULL);
		panel_setup(panel);
		gtk_widget_show(panel);

		/*load up default applets on the default panel*/
		load_default_applets();

		return;
	}

	for(num=1;num<=count;num++) {
		PanelType type;
		PanelBackType back_type;
		char *back_pixmap, *color;
		GdkColor back_color = {0,0,0,1};
		int fit_pixmap_bg;

		g_snprintf(buf,256,"%spanel/Panel_%d/", old_panel_cfg_path, num);
		gnome_config_push_prefix (buf);
		
		back_pixmap = gnome_config_get_string ("backpixmap=");
		if (back_pixmap && *back_pixmap == '\0') {
			g_free(back_pixmap);
			back_pixmap = NULL;
		}

		color = gnome_config_get_string("backcolor=#ffffff");
		if(color && *color)
			gdk_color_parse(color, &back_color);

		g_snprintf(buf,256,"back_type=%d",PANEL_BACK_NONE);
		back_type=gnome_config_get_int(buf);
		fit_pixmap_bg = gnome_config_get_bool ("fit_pixmap_bg=TRUE");

		/*now for type specific config*/

		g_snprintf(buf,256,"type=%d", SNAPPED_PANEL);
		type = gnome_config_get_int(buf);

		switch(type) {
		case SNAPPED_PANEL:
			{
				SnappedPos pos;
				SnappedMode mode;
				SnappedState state;

				g_snprintf(buf,256,"pos=%d", SNAPPED_BOTTOM);
				pos=gnome_config_get_int(buf);

				g_snprintf(buf,256,"mode=%d", SNAPPED_EXPLICIT_HIDE);
				mode=gnome_config_get_int(buf);

				g_snprintf(buf,256,"state=%d", SNAPPED_SHOWN);
				state=gnome_config_get_int(buf);
				
				panel = snapped_widget_new(pos,
							   mode,
							   state,
							   back_type,
							   back_pixmap,
							   fit_pixmap_bg,
							   &back_color);
				break;
			}
		case DRAWER_PANEL:
			{
				DrawerState state;
				PanelOrientation orient;
				DrawerDropZonePos drop_pos;

				g_snprintf(buf,256,"state=%d", DRAWER_SHOWN);
				state=gnome_config_get_int(buf);

				g_snprintf(buf,256,"orient=%d", PANEL_HORIZONTAL);
				orient=gnome_config_get_int(buf);

				g_snprintf(buf,256,
					   "drop_zone_pos=%d",
					   DROP_ZONE_LEFT);
				drop_pos=gnome_config_get_int(buf);

				panel = drawer_widget_new(orient,
							  state,
							  drop_pos,
							  back_type,
							  back_pixmap,
							  fit_pixmap_bg,
							  &back_color);
				break;
			}
		case CORNER_PANEL:
			{
				CornerPos pos;
				PanelOrientation orient;
				CornerState state;
				
				g_snprintf(buf,256,"pos=%d", CORNER_NE);
				pos=gnome_config_get_int(buf);

				g_snprintf(buf,256,"orient=%d",
					   PANEL_HORIZONTAL);
				orient=gnome_config_get_int(buf);

				g_snprintf(buf,256,"state=%d", CORNER_SHOWN);
				state=gnome_config_get_int(buf);
				
				panel = corner_widget_new(pos,
							  orient,
							  state,
							  back_type,
							  back_pixmap,
							  fit_pixmap_bg,
							  &back_color);
				break;
			}
		default: break;
		}

		gnome_config_pop_prefix ();
		
		g_free(color);
		g_free(back_pixmap);

		panel_setup(panel);

		if(!IS_DRAWER_WIDGET(panel) ||
		   DRAWER_WIDGET(panel)->state ==  DRAWER_SHOWN)
			gtk_widget_show(panel);
	}
}

/*send state change to all the panels*/
static void
send_state_change(void)
{
	GList *list;
	/*process drawers first*/
	/*for(list = panel_list; list != NULL; list = g_list_next(list)) {
		PanelData *pd = list->data;
		if(IS_DRAWER_WIDGET(pd->panel))
			drawer_state_change(pd->panel,
					    DRAWER_WIDGET(pd->panel)->state,
					    NULL);
	}*/
	for(list = panel_list; list != NULL; list = g_list_next(list)) {
		PanelData *pd = list->data;
		if(IS_SNAPPED_WIDGET(pd->panel))
			snapped_state_change(pd->panel,
					     SNAPPED_WIDGET(pd->panel)->state,
					     NULL);
		else if(IS_CORNER_WIDGET(pd->panel))
			corner_state_change(pd->panel,
					    CORNER_WIDGET(pd->panel)->state,
					    NULL);
	}
}

/*I guess this should be called after we load up, but the problem is
  we never know when all the applets are going to finish loading and
  we don't want to clean the file before they load up, so now we
  only call it on the discard cmdline argument*/
void
discard_session (char *id)
{
	char *sess;

	/*FIXME: hmm this won't work ... there needs to be a clean_dir*/
	sess = g_copy_strings ("/panel.d/Session-", id, NULL);
	gnome_config_clean_file (sess);
	g_free (sess);

	gnome_config_sync ();

	return;
}

	
static error_t
parse_an_arg (int key, char *arg, struct argp_state *state)
{
	if (key == DISCARD_KEY) {
		gnome_client_disable_master_connection ();
		discard_session (arg);
		just_exit = 1;
		return 0;
	}

	/* We didn't recognize it.  */
	return ARGP_ERR_UNKNOWN;
}

/*
static void
panel_connect_client (GnomeClient *client,
		      int was_restarted,
		      gpointer client_data)
{
	char *session_id;

	session_id = gnome_client_get_previous_id (client);
	
	if(session_id) {
		g_free(old_panel_cfg_path);
		old_panel_cfg_path = g_copy_strings("/panel.d/Session-",
						    session_id,"/",NULL);
	}
	puts("connected");
	puts(old_panel_cfg_path);
}
*/

void
sigchld_handler(int type)
{
	GList *list;
	pid_t pid = waitpid(0,NULL,WNOHANG);

	if(pid <= 0)
		return;

	for(list=children;list!=NULL;list=g_list_next(list)) {
		AppletChild *child=list->data;
		if(child->pid == pid) {
			AppletInfo *info = get_applet_info(child->applet_id);
			if(info &&
			   info->widget &&
			   info->id_str) {
				int i;
				char *s = g_strdup(info->id_str);
				for(i=0,info=(AppletInfo *)applets->data;
				    i<applet_count;
				    i++,info++) {
					if(info->id_str &&
					   strcmp(info->id_str,s)==0)
						panel_clean_applet(info->applet_id);
				}
				g_free(s);
			}
			exec_queue_done(child->applet_id);

			g_free(child);
			children=g_list_remove_link(children,list);
			return;
		}
	}
}

static int
try_config_sync(gpointer data)
{
	panel_config_sync();
	return TRUE;
}


int
main(int argc, char **argv)
{
	char buf[256];
	struct sigaction sa;
	
	panel_cfg_path = g_strdup("/panel.d/default/");
	old_panel_cfg_path = g_strdup("/panel.d/default/");

	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	sigemptyset (&sa.sa_mask);
	sa.sa_handler = sigchld_handler;
	sa.sa_flags   = 0;
	sigaction (SIGCHLD, &sa, NULL);

	panel_corba_register_arguments ();

	gnome_init("panel", &parser, argc, argv, 0, NULL);

	if (just_exit)
		return 0;

	client= gnome_master_client ();

	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
			    GTK_SIGNAL_FUNC (panel_session_save), NULL);
	gtk_object_set_data(GTK_OBJECT(client),"argv0",g_strdup(argv[0]));
	gtk_signal_connect (GTK_OBJECT (client), "die",
			    GTK_SIGNAL_FUNC (panel_session_die), NULL);

	if (GNOME_CLIENT_CONNECTED (client)) {
		char *session_id;

		session_id= gnome_client_get_id (gnome_cloned_client ());

		if(session_id) {
			g_free(old_panel_cfg_path);
			old_panel_cfg_path = g_copy_strings("/panel.d/Session-",
							    session_id,"/",
							    NULL);
		}
		puts("connected to session manager");
	}

	/* Tell session manager how to run us.  */
	gnome_client_set_clone_command (client, 1, argv);
	gnome_client_set_restart_command (client, 1, argv);

	/* Setup the cookie */
	cookie = create_cookie ();
	gnome_config_private_set_string ("/panel/Secret/cookie", cookie);
	
	applets = g_array_new(FALSE);
	applet_count=0;

	/*set up global options*/
	
	g_snprintf(buf,256,"%spanel/Config/",old_panel_cfg_path);
	gnome_config_push_prefix(buf);

	global_config.tooltips_enabled =
		gnome_config_get_bool("tooltips_enabled=TRUE");

	global_config.show_small_icons =
		gnome_config_get_bool("show_small_icons=TRUE");
		
	global_config.prompt_for_logout =
		gnome_config_get_bool("prompt_for_logout=TRUE");

	global_config.disable_animations =
		gnome_config_get_bool("disable_animations=FALSE");
		
	g_snprintf(buf,256,"auto_hide_step_size=%d",
		   DEFAULT_AUTO_HIDE_STEP_SIZE);
	global_config.auto_hide_step_size=gnome_config_get_int(buf);
		
	g_snprintf(buf,256,"explicit_hide_step_size=%d",
		   DEFAULT_EXPLICIT_HIDE_STEP_SIZE);
	global_config.explicit_hide_step_size=gnome_config_get_int(buf);
		
	g_snprintf(buf,256,"drawer_step_size=%d",
		   DEFAULT_DRAWER_STEP_SIZE);
	global_config.drawer_step_size=gnome_config_get_int(buf);
		
	g_snprintf(buf,256,"minimize_delay=%d", DEFAULT_MINIMIZE_DELAY);
	global_config.minimize_delay=gnome_config_get_int(buf);
		
	g_snprintf(buf,256,"minimized_size=%d", DEFAULT_MINIMIZED_SIZE);
	global_config.minimized_size=gnome_config_get_int(buf);
		
	g_snprintf(buf,256,"movement_type=%d", PANEL_SWITCH_MOVE);
	global_config.movement_type=gnome_config_get_int(buf);
		
	gnome_config_pop_prefix();

	init_user_panels();
	init_user_applets();

	panel_tooltips = gtk_tooltips_new();

	/*set the globals*/
	apply_global_config();

	/*everything is ready ... load up the applets*/
	load_queued_applets();

	/*add forbidden lists to ALL panels*/
	g_list_foreach(panels,(GFunc)panel_widget_add_forbidden,NULL);

	/*this will make the drawers be hidden for closed panels etc ...*/
	send_state_change();
	
	/*attempt to sync the config every 10 seconds, only if a change was
	  indicated though*/
	config_sync_timeout = gtk_timeout_add(10*1000,try_config_sync,NULL);
	
	/* I use the glue code to avoid making this a C++ file */
	panel_corba_gtk_main ("IDL:GNOME/Panel:1.0");

	return 0;
}
