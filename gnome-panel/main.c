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

int config_sync_timeout = 0;
int config_changed = FALSE;

GArray *applets;
int applet_count;

extern GtkWidget * root_menu;

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
	char *pixmap;
	char *tooltip;
	int pos;
	int panel;
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


/*needed for drawers*/
static void panel_setup(GtkWidget *panel);

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
	} else if(IS_DRAWER_WIDGET(panel)) {
		return PANEL_WIDGET(DRAWER_WIDGET(panel)->panel);
	}
	g_warning("unknown panel type");
	return NULL;
}


static void
queue_load_applet(char *id_str, char *path, char *params,
		  char *pixmap, char *tooltip,
		  int pos, int panel, char *cfgpath)
{
	LoadApplet *l;
	l = g_new(LoadApplet,1);

	l->id_str=g_strdup(id_str);
	if(path) l->path=g_strdup(path);
	else l->path = NULL;
	l->params=g_strdup(params);
	l->pixmap=g_strdup(pixmap);
	l->tooltip=g_strdup(tooltip);
	l->pos=pos;
	l->panel=panel;
	l->cfgpath=g_strdup(cfgpath);

	load_queue = g_list_append(load_queue,l);
}

static int
monitor_drawers(GtkWidget *w, gpointer data)
{
	PanelWidget **panel=data;
	DrawerWidget *drawer = gtk_object_get_data(GTK_OBJECT(panel[0]),
						   PANEL_PARENT);

	if(drawer->state==DRAWER_SHOWN)
		panel[1]->drawers_open++;
	else
		panel[1]->drawers_open--;

	return FALSE;
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
				orient=(porient==PANEL_VERTICAL)?
					ORIENT_RIGHT:ORIENT_UP;
				break;
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
load_applet(char *id_str, char *path, char *params,
	    char *pixmap, char *tooltip,
	    int pos, int panel, char *cfgpath)
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
				     LAUNCHER_ID,NULL, params,pos,panel,
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
		PanelWidget *parent;
		PanelWidget **panelarr;
		PanelWidget *dr_panel;

		parent = PANEL_WIDGET(g_list_nth(panels,panel)->data);

		if(!params) {
			drawer = create_empty_drawer_applet(
				tooltip,pixmap,
				get_applet_orient(parent));
			if(drawer) panel_setup(drawer->drawer);
		} else {
			int i;
			PanelData *dr_pd;

			sscanf(params,"%d",&i);
			dr_pd = g_list_nth(panel_list,i)->data;

			drawer=create_drawer_applet(dr_pd->panel,
						    tooltip,pixmap,
						    get_applet_orient(parent));
		}
		
		if(!drawer)
			return;

		g_return_if_fail(drawer != NULL);

		register_toy(drawer->button,drawer->drawer,drawer,DRAWER_ID,
			     NULL, params, pos, panel, NULL, APPLET_DRAWER);
		
		/*the panel of the drawer*/
		dr_panel = PANEL_WIDGET(DRAWER_WIDGET(drawer->drawer)->panel);

		panelarr = g_new(PanelWidget *,2);
		panelarr[0] = dr_panel;
		panelarr[1] = parent;

		gtk_signal_connect(GTK_OBJECT(drawer->button), "clicked",
				   GTK_SIGNAL_FUNC(monitor_drawers),
				   panelarr);

		if(DRAWER_WIDGET(drawer->drawer)->state == DRAWER_SHOWN) {
			GtkWidget *wparent;
			/*drawer is open so we track it*/
			dr_panel->drawers_open++;
			/*pop up, if popped down*/
			wparent = gtk_object_get_data(GTK_OBJECT(parent),
						      PANEL_PARENT);
			if(IS_SNAPPED_WIDGET(wparent))
				snapped_widget_pop_up(SNAPPED_WIDGET(wparent));
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
				gtk_widget_realize(drawer->drawer);
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

		swallow = create_swallow_applet(params, SWALLOW_HORIZONTAL);
		
		if(swallow)
			register_toy(swallow->table,NULL,swallow,SWALLOW_ID,
				     NULL, params,pos, panel,NULL,
				     APPLET_SWALLOW);
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
		load_applet(l->id_str,l->path,l->params,
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
add_forbidden_to_panels(void)
{
	GList *list;

	for(list = panels;list!=NULL;list=g_list_next(list)) {
		PanelWidget *panel = list->data;
		panel_widget_add_forbidden(panel);
	}
}


static void
load_default_applets(void)
{
	queue_load_applet(MENU_ID, NULL, ".", NULL, NULL,
			  PANEL_UNKNOWN_APPLET_POSITION, 0,NULL);
	queue_load_applet(EXTERN_ID, "gen_util_applet", "--clock", NULL, NULL,
			  PANEL_UNKNOWN_APPLET_POSITION,0,NULL);
}

static void
init_user_applets(void)
{
	char *applet_name;
	char *applet_params;
	char *applet_pixmap;
	char *applet_tooltip;
	char *applet_path;
	int   pos=0,panel;
	char  buf[256];
	int   count,num;	

	g_snprintf(buf,256,"%spanel/Config/applet_count=0",old_panel_cfg_path);
	count=gnome_config_get_int(buf);
	if(count<=0)
		load_default_applets();
	for(num=1;num<=count;num++) {
		g_snprintf(buf,256,"%sApplet_%d/config/", old_panel_cfg_path, num);
		gnome_config_push_prefix(buf);
		applet_name = gnome_config_get_string("id=Unknown");

		applet_path = gnome_config_get_string("execpath=");
		applet_params = gnome_config_get_string("parameters=");
		applet_pixmap = gnome_config_get_string("pixmap=");
		applet_tooltip = gnome_config_get_string("tooltip=");

		g_snprintf(buf,256,"position=%d",
			   PANEL_UNKNOWN_APPLET_POSITION);
		pos = gnome_config_get_int(buf);
		panel = gnome_config_get_int("panel=0");
		
		/*if we are to right stick this, make the number large, 
		 INT_MAX/2 should allways be large enough */
		pos += gnome_config_get_bool("right_stick=false")?INT_MAX/2:0;

		gnome_config_pop_prefix();

		/*this is the config path to be passed to the applet when it
		  loads*/
		g_snprintf(buf,256,"%sApplet_%d/",old_panel_cfg_path,num);
		queue_load_applet(applet_name, applet_path, applet_params,
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
	GdkCursor *cursor;

	cursor = gdk_cursor_new(cursor_type);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
}

static void
panel_realize(GtkWidget *widget, gpointer data)
{
	change_window_cursor(widget->window, GDK_ARROW);
	
	if(IS_SNAPPED_WIDGET(widget))
		snapped_widget_enable_buttons(SNAPPED_WIDGET(widget));
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
	int applet_id = PTOI(gtk_object_get_user_data(GTK_OBJECT(data)));
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
	config_changed = TRUE;
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
	config_changed = TRUE;
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
	int applet_id = PTOI(gtk_object_get_user_data(GTK_OBJECT(data)));
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
	config_changed = TRUE;
	/*update the configuration box if it is displayed*/
	update_config_back(widget);
}

static void
state_restore_foreach(gpointer data, gpointer user_data)
{
	int applet_id = PTOI(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);

	if(info->type == APPLET_DRAWER) {
		DrawerWidget *drawer = DRAWER_WIDGET(info->assoc);
		if(drawer->state == DRAWER_SHOWN) {
			drawer_widget_restore_state(drawer);
			panel_widget_foreach(PANEL_WIDGET(drawer->panel),
					     state_restore_foreach,
					     NULL);
		}
	}
}

static void
state_hide_foreach(gpointer data, gpointer user_data)
{
	int applet_id = PTOI(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);

	if(info->type == APPLET_DRAWER) {
		DrawerWidget *drawer = DRAWER_WIDGET(info->assoc);
		if(drawer->state == DRAWER_SHOWN) {
			gtk_widget_hide(info->assoc);
			panel_widget_foreach(PANEL_WIDGET(drawer->panel),
					     state_hide_foreach,
					     NULL);
		}
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

	config_changed = TRUE;

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

	config_changed = TRUE;

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
		config_changed = TRUE;
	}
	return FALSE;
}

/*the following is slightly ugly .... but it works, I need to send the
  orient change in an idle handeler*/

struct _added_info {
	int applet_id;
	PanelWidget *panel;
};

static int
panel_applet_added_idle(gpointer data)
{
	struct _added_info *ai = data;

	orientation_change(ai->applet_id,ai->panel);
	g_free(ai);

	config_changed = TRUE;

	return FALSE;
}

static void
panel_applet_added(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	int applet_id = PTOI(gtk_object_get_user_data(GTK_OBJECT(applet)));
	PanelWidget *panel = PANEL_WIDGET(widget);
	struct _added_info *ai = g_new(struct _added_info,1);

	g_return_if_fail(ai != NULL);

	ai->applet_id = applet_id;
	ai->panel = panel;

	gtk_idle_add(panel_applet_added_idle,ai);
}

static void
panel_applet_removed(GtkWidget *widget, gpointer data)
{
	config_changed = TRUE;
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
panel_move(SnappedWidget *snapped, double x, double y)
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

static int
panel_move_timeout(gpointer data)
{
	int x,y;

	gdk_window_get_pointer(NULL,&x,&y,NULL);
	panel_move(data,x,y);
	return TRUE;
}

/* DOES NOT WORK!
static int
panel_move_callback(GtkWidget *w, GdkEventMotion *event, gpointer data)
{
	puts("TEST");
	if(panel_dragged)
		panel_move(PANEL_WIDGET(w), event->x_root, event->y_root);
	return FALSE;
}*/


static int
panel_button_release_callback(GtkWidget *w,GdkEventButton *event, gpointer data)
{
	if(panel_dragged) {
		panel_move(SNAPPED_WIDGET(w), event->x_root, event->y_root);
		gdk_pointer_ungrab(event->time);
		gtk_grab_remove(w);
		gtk_timeout_remove(panel_dragged);
		SNAPPED_WIDGET(w)->autohide_inhibit = FALSE;
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(w));
		panel_dragged = 0;
	}
	return FALSE;
}

static int
panel_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	GdkCursor *cursor;
	switch(event->button) {
		case 3: /* fall through */
		case 1:
			if(!panel_applet_in_drag) {
				if(IS_SNAPPED_WIDGET(widget)) {
					SnappedWidget *snapped = SNAPPED_WIDGET(widget);
					snapped->autohide_inhibit = TRUE;
					snapped_widget_queue_pop_down(snapped);
				}
				gtk_menu_popup(GTK_MENU(data), NULL, NULL,
					       panel_menu_position,
					       widget, event->button,
					       event->time);
				return TRUE;
			}
			break;
		case 2:
			/*this should probably be in snapped widget*/
			if(!panel_dragged &&
			   IS_SNAPPED_WIDGET(widget)) {
				cursor = gdk_cursor_new (GDK_FLEUR);
				gtk_grab_add(widget);
				gdk_pointer_grab (widget->window,
						  TRUE,
						  PANEL_EVENT_MASK,
						  NULL,
						  cursor,
						  event->time);
				gdk_cursor_destroy (cursor);
				SNAPPED_WIDGET(widget)->autohide_inhibit = TRUE;
				panel_dragged = gtk_timeout_add(30,
						panel_move_timeout,widget);
				return TRUE;
			}
			break;
	}
	return FALSE;
}

static int
panel_destroy(GtkWidget *widget, gpointer data)
{
	PanelData *pd = gtk_object_get_user_data(GTK_OBJECT(widget));
	GtkWidget *panel_menu = data;

	if(panel_menu)
		gtk_widget_unref(panel_menu);
	
	panel_list = g_list_remove(panel_list,pd);
	g_free(pd);

	return FALSE;
}

static void
applet_move_foreach(gpointer data, gpointer user_data)
{
	int applet_id = PTOI(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);

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
	config_changed = TRUE;
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

static void
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
		panel_widget_setup(panel);
		snapped_widget_disable_buttons(SNAPPED_WIDGET(panelw));
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "pos_change",
				   GTK_SIGNAL_FUNC(snapped_pos_change),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(panelw),
				   "state_change",
				   GTK_SIGNAL_FUNC(snapped_state_change),
				   NULL);
	} else
		g_warning("unknown panel type");

	gtk_signal_connect(GTK_OBJECT(panelw),
			   "size_allocate",
			   GTK_SIGNAL_FUNC(panel_size_allocate),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(panelw),
			   "button_press_event",
			   GTK_SIGNAL_FUNC(panel_button_press),
			   panel_menu);
	gtk_signal_connect(GTK_OBJECT(panelw),
			   "destroy",
			   GTK_SIGNAL_FUNC(panel_destroy),
			   panel_menu);
	gtk_signal_connect(GTK_OBJECT(panelw),
			   "button_release_event",
			   GTK_SIGNAL_FUNC(panel_button_release_callback),
			   NULL);

	/* DOES NOT WORK!
	gtk_signal_connect(GTK_OBJECT(panel),
			   "motion_notify_event",
			   GTK_SIGNAL_FUNC(panel_move_callback),
			   panel);*/

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

	/*load a default snapped panel on the bottom of the screen*/
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
		old_panel_cfg_path = g_copy_strings("/panel-Session-",
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
			if(!info) return;
			if(info->type!=APPLET_EXTERN_RESERVED &&
			   info->type!=APPLET_EXTERN_PENDING)
			   	info->widget = NULL;

			panel_clean_applet(child->applet_id);

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
	if(config_changed)
		panel_sync_config();
	config_changed = FALSE;
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
			    GTK_SIGNAL_FUNC (panel_session_save), argv[0]);
	gtk_signal_connect (GTK_OBJECT (client), "die",
			    GTK_SIGNAL_FUNC (panel_session_die), NULL);

	if (GNOME_CLIENT_CONNECTED (client)) {
		char *session_id;

		session_id= gnome_client_get_id (gnome_cloned_client ());

		if(session_id) {
			g_free(old_panel_cfg_path);
			old_panel_cfg_path = g_copy_strings("/panel-Session-",
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

	init_main_menu();
	init_user_panels();
	init_user_applets();

	panel_tooltips = gtk_tooltips_new();

	/*set the globals*/
	apply_global_config();

	/*everything is ready ... load up the applets*/
	load_queued_applets();

	add_forbidden_to_panels();
	
	/*attempt to sync the config every 10 seconds, only if a change was
	  indicated though*/
	config_sync_timeout = gtk_timeout_add(10*1000,try_config_sync,NULL);
	
	/* I use the glue code to avoid making this a C++ file */
	panel_corba_gtk_main ("IDL:GNOME/Panel:1.0");

	return 0;
}
