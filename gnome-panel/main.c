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
#include "panel.h"
#include "panel_config_global.h"
#include "menu.h"
#include "drawer.h"
#include "swallow.h"
#include "logout.h"
#include "mico-glue.h"
#include "mico-parse.h"
#include "panel-util.h"
#include "launcher.h"

#include "cookie.h"

/*GList *panels = NULL;*/
GArray *applets;
gint applet_count;

extern GtkWidget * root_menu;

char *panel_cfg_path=NULL;
char *old_panel_cfg_path=NULL;

gint main_menu_count=0;

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
	gchar *id_str;
	gchar *path;
	gchar *params;
	gint dorestart;
	gint pos;
	gint panel;
	gchar *cfgpath;
};

typedef struct _AppletChild AppletChild;
static GList * children = NULL;

/*used in the SIGCHLD handler*/
struct _AppletChild {
	gint applet_id;
	pid_t pid;
};


GList *load_queue=NULL;
	
/* True if parsing determined that all the work is already done.  */
gint just_exit = 0;

/* The security cookie */
char *cookie;

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
static void panel_setup(PanelWidget *panel);

static void
queue_load_applet(gchar *id_str, gchar *path, gchar *params, gint dorestart,
		  gint pos, gint panel, gchar *cfgpath)
{
	LoadApplet *l;
	l = g_new(LoadApplet,1);

	l->id_str=g_strdup(id_str);
	if(path) l->path=g_strdup(path);
	else l->path = NULL;
	l->params=g_strdup(params);
	l->dorestart=dorestart;
	l->pos=pos;
	l->panel=panel;
	l->cfgpath=g_strdup(cfgpath);

	load_queue = g_list_append(load_queue,l);
}

static gint
monitor_drawers(GtkWidget *w, gpointer data)
{
	PanelWidget **panel=data;

	if(panel[0]->state==PANEL_SHOWN)
		panel[1]->drawers_open++;
	else
		panel[1]->drawers_open--;

	return FALSE;
}


void
load_applet(gchar *id_str, gchar *path, gchar *params, gint dorestart,
	    gint pos, gint panel, gchar *cfgpath)
{
	if(strcmp(id_str,EXTERN_ID) == 0) {
		gchar *fullpath;
		char *param;

		/*start nothing, applet is taking care of everything*/
		if(path == NULL ||
		   path[0] == '\0')
		   	return;

		if(!params)
			param = "";
		else
			param = params;

		/*VERY UGLY compatibility hack for the old launcher applet*/
		if(strcmp(path,"#panel.application.launcher")==0) {
			gchar *p;
			p = g_copy_strings(cfgpath,"path=",NULL);
			fullpath = gnome_config_get_string(p);
			g_free(p);
			load_applet(LAUNCHER_ID,NULL,fullpath,TRUE,pos,panel,
				    cfgpath);
			g_free(fullpath);
			return;
		}

		/*make it an absolute path, same as the applets will
		  interpret it and the applets will sign themselves as
		  this, so it has to be exactly the same*/
		if(path[0]!='#')
			fullpath = get_full_path(path);
		else
			fullpath = g_strdup(path);
	

		if(reserve_applet_spot (id_str, fullpath,params, dorestart,
					panel, pos, cfgpath,
					APPLET_EXTERN_PENDING)==0) {
			g_free(fullpath);
			return;
		}
		
		/*'#' marks an applet that will take care of starting
		  itself but wants us to reserve a spot for it*/
		if(!dorestart) {
			if(mulapp_is_in_queue(fullpath)) {
				mulapp_load_or_add_to_queue(fullpath,param);
				g_free(fullpath);
				return;
			} else {
				mulapp_add_to_queue(fullpath,applet_count-1);
			}
		}

		if(path[0]!='#') {
			/*this applet is dumb and wants us to start it :)*/
			AppletChild *child;

			child = g_new(AppletChild,1);

			child->pid = fork();
			if(child->pid < 0)
				g_error("Can't fork!");
			if(child->pid == 0) {
				if(strlen(param)>0)
					execl(fullpath,fullpath,param,NULL);
				else
					execl(fullpath,fullpath,NULL);
				g_error("Can't execl!");
			}

			printf("started applet, pid: %d\n",child->pid);
			
			child->applet_id = applet_count-1;
				
			children = g_list_prepend(children,child);
		}

		g_free(fullpath);
	} else if(strcmp(id_str,MENU_ID) == 0) {
		Menu *menu;

		menu = create_menu_applet(params, MENU_UP);

		register_toy(menu->button,menu->menu,menu,MENU_ID,NULL,params,
			     pos,panel,NULL,APPLET_MENU);
		if(!params || strcmp(params,".")==0)
			main_menu_count++;
	} else if(strcmp(id_str,LAUNCHER_ID) == 0) {
		Launcher *launcher;

		launcher = create_launcher(params);

		register_toy(launcher->button,NULL,launcher,LAUNCHER_ID,NULL,
			     params,pos,panel,NULL,APPLET_LAUNCHER);

		gtk_tooltips_set_tip (panel_tooltips,launcher->button->parent,
				      launcher->dentry->comment,NULL);

		applet_add_callback(applet_count-1,"properties",
				    _("Properties..."));
	} else if(strcmp(id_str,DRAWER_ID) == 0) {
		Drawer *drawer;
		PanelWidget *parent;
		DrawerOrient orient=DRAWER_UP;
		PanelWidget **panelarr;

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
			     NULL, params, pos, panel, NULL, APPLET_DRAWER);

		panelarr = g_new(PanelWidget *,2);
		panelarr[0] = PANEL_WIDGET(drawer->drawer);
		panelarr[1] = g_list_nth(panels,panel)->data;
		gtk_signal_connect(GTK_OBJECT(drawer->button), "clicked",
				   GTK_SIGNAL_FUNC(monitor_drawers),
				   panelarr);
		/*default is open so we track it*/
		panelarr[1]->drawers_open++;
		/*pop up, if popped down*/
		panel_widget_pop_up(panelarr[1]);

		reposition_drawer(drawer);

		panel_widget_add_forbidden(PANEL_WIDGET(drawer->drawer));
	} else if(strcmp(id_str,SWALLOW_ID) == 0) {
		Swallow *swallow;

		swallow = create_swallow_applet(params, SWALLOW_HORIZONTAL);
		
		register_toy(swallow->table,NULL,swallow,SWALLOW_ID,NULL,
			     params,pos, panel,NULL,APPLET_SWALLOW);
	} else if(strcmp(id_str,LOGOUT_ID) == 0) {
		GtkWidget *logout;

		logout = create_logout_widget();
		
		register_toy(logout,NULL,NULL,LOGOUT_ID,NULL,params,pos,
			     panel,NULL,APPLET_LOGOUT);
	}
}

static void
load_queued_applets(void)
{
	GList *list;

	for(list = load_queue;list!=NULL;list=g_list_next(list)) {
		LoadApplet *l=list->data;
		load_applet(l->id_str,l->path,l->params,l->dorestart,
			    l->pos,l->panel,l->cfgpath);
		g_free(l->id_str);
		if(l->path) g_free(l->path);
		g_free(l->params);
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
	queue_load_applet(MENU_ID, NULL, ".", TRUE,
			  PANEL_UNKNOWN_APPLET_POSITION, 0,NULL);
	queue_load_applet(EXTERN_ID, "clock_applet", "", TRUE,
			  PANEL_UNKNOWN_APPLET_POSITION,0,NULL);
}

static void
init_user_applets(void)
{
	char *applet_name;
	char *applet_params;
	char *applet_path;
	int   pos=0,panel;
	char  buf[256];
	int   count,num;	
	gint  dorestart;

	g_snprintf(buf,256,"%sConfig/applet_count=0",old_panel_cfg_path);
	count=gnome_config_get_int(buf);
	if(count<=0)
		load_default_applets();
	for(num=1;num<=count;num++) {
		g_snprintf(buf,256,"%sApplet_%d/", old_panel_cfg_path, num);
		gnome_config_push_prefix(buf);
		applet_name = gnome_config_get_string("id=Unknown");

		/*fairly ugly hack to preserve config file compatibility*/
		if(strcmp(applet_name,"Extern")==0) {
			applet_path = gnome_config_get_string("parameters=");
			applet_params = gnome_config_get_string("parameters2=");
			dorestart = gnome_config_get_bool("dorestart=true");
		} else {
			applet_path = NULL;
			applet_params = gnome_config_get_string("parameters=");
			dorestart = TRUE;
		}

		g_snprintf(buf,256,"position=%d",
			   PANEL_UNKNOWN_APPLET_POSITION);
		pos = gnome_config_get_int(buf);
		panel = gnome_config_get_int("panel=0");

		/*this is the config path to be passed to the applet when it
		  loads*/
		g_snprintf(buf,256,"%sApplet_%d/",old_panel_cfg_path,num);
		queue_load_applet(applet_name, applet_path, applet_params,
				  dorestart, pos, panel, buf);

		gnome_config_pop_prefix();

		g_free(applet_name);
		if(applet_path) g_free(applet_path);
		g_free(applet_params);
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

	panel_widget_enable_buttons(PANEL_WIDGET(widget));
}

/*we call this recursively*/
static void orient_change_foreach(gpointer data, gpointer user_data);

void
orientation_change(gint applet_id, PanelWidget *panel)
{
	AppletInfo *info = get_applet_info(applet_id);
	if(info->type == APPLET_EXTERN) {
		PanelOrientType orient=ORIENT_UP;
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
		send_applet_change_orient(info->id_str,info->applet_id,orient);
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
	gint applet_id = PTOI(gtk_object_get_user_data(GTK_OBJECT(data)));
	PanelWidget *panel = user_data;

	orientation_change(applet_id,panel);
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
	gint applet_id = PTOI(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);

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
	gint applet_id = PTOI(gtk_object_get_user_data(GTK_OBJECT(data)));
	AppletInfo *info = get_applet_info(applet_id);

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
panel_size_allocate(GtkWidget *widget, GtkAllocation *alloc, gpointer data)
{
	Drawer *drawer = gtk_object_get_data(GTK_OBJECT(widget),DRAWER_PANEL);
	PanelWidget *panel = PANEL_WIDGET(widget);

	if(drawer)
		if(panel->state == PANEL_SHOWN)
			reposition_drawer(drawer);
	return TRUE;
}

struct _added_info {
	gint applet_id;
	PanelWidget *panel;
};

static gint
panel_applet_added_idle(gpointer data)
{
	struct _added_info *ai = data;

	orientation_change(ai->applet_id,ai->panel);
	g_free(ai);

	return FALSE;
}

static gint
panel_applet_added(GtkWidget *widget, GtkWidget *applet, gpointer data)
{
	gint applet_id = PTOI(gtk_object_get_user_data(GTK_OBJECT(applet)));
	PanelWidget *panel = PANEL_WIDGET(widget);
	struct _added_info *ai = g_new(struct _added_info,1);

	g_return_val_if_fail(ai != NULL, FALSE);

	ai->applet_id = applet_id;
	ai->panel = panel;

	gtk_idle_add(panel_applet_added_idle,ai);

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
						       NULL, y);
				*x = wx + GTK_WIDGET(panel)->allocation.width;
				*y += wy;
				break;
			}
			/*fall through for horizontal*/
		case PANEL_BOTTOM:
			gtk_widget_get_pointer(GTK_WIDGET(panel),
					       x, NULL);
			*x += wx;
			*y = wy - GTK_WIDGET (menu)->allocation.height;
			break;
		case PANEL_TOP:
			gtk_widget_get_pointer(GTK_WIDGET(panel),
					       x, NULL);
			*x += wx;
			*y = wy + GTK_WIDGET(panel)->allocation.height;
			break;
		case PANEL_LEFT:
			gtk_widget_get_pointer(GTK_WIDGET(panel),
					       NULL, y);
			*x = wx + GTK_WIDGET(panel)->allocation.width;
			*y += wy;
			break;
		case PANEL_RIGHT:
			gtk_widget_get_pointer(GTK_WIDGET(panel),
					       NULL, y);
			*x = wx - GTK_WIDGET (menu)->allocation.width;
			*y += wy;
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

	return FALSE;
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

	if(GTK_WIDGET_REALIZED(GTK_WIDGET(panel)))
		panel_realize(GTK_WIDGET(panel),NULL);
	else
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
	char *back_pixmap;

	g_snprintf(buf,256,"%sConfig/panel_count=0",old_panel_cfg_path);
	count=gnome_config_get_int(buf);
	if(count<=0) count++; /*this will load up a single panel with
				default settings*/

	panel_widget_change_global(DEFAULT_EXPLICIT_HIDE_STEP_SIZE,
				   DEFAULT_AUTO_HIDE_STEP_SIZE,
				   DEFAULT_DRAWER_STEP_SIZE,
				   DEFAULT_MINIMIZED_SIZE,
				   DEFAULT_MINIMIZE_DELAY,
				   PANEL_SWITCH_MOVE,
				   FALSE);

	for(num=1;num<=count;num++) {
		g_snprintf(buf,256,"%sPanel_%d/", old_panel_cfg_path, num);

		gnome_config_push_prefix (buf);
		
		/*these are only for free floating non-drawer like panels */
		size = gnome_config_get_int("size=50");
		x    = gnome_config_get_int("position_x=0");
		y    = gnome_config_get_int("position_y=0");

		g_snprintf(buf,256,"snapped=%d", PANEL_BOTTOM);
		config.snapped=gnome_config_get_int(buf);

		g_snprintf(buf,256,"orient=%d", PANEL_HORIZONTAL);
		config.orient=gnome_config_get_int(buf);

		g_snprintf(buf,256,"mode=%d", PANEL_EXPLICIT_HIDE);
		config.mode=gnome_config_get_int(buf);

		g_snprintf(buf,256,"state=%d", PANEL_SHOWN);
		state=gnome_config_get_int(buf);

		g_snprintf(buf,256,"drawer_drop_zone_pos=%d", DRAWER_LEFT);
		drop_pos=gnome_config_get_int(buf);

		back_pixmap = gnome_config_get_string ("backpixmap=");
		if (back_pixmap && *back_pixmap == 0)
			back_pixmap = 0;
		
		gnome_config_pop_prefix ();
		panel = panel_widget_new(size,
					 config.orient,
					 config.snapped,
					 config.mode,
					 state,
					 x,
					 y,
					 drop_pos,
					 back_pixmap
					 );
		panel_widget_disable_buttons(PANEL_WIDGET(panel));

		panel_setup(PANEL_WIDGET(panel));

		gtk_widget_show(panel);

		panels = g_list_append(panels,panel);
	}
}

/*I guess this should be called after we load up, but the problem is
  we never know when all the applets are going to finish loading and
  we don't want to clean the file before they load up, so now we
  only call it on the discard cmdline argument*/
void
discard_session (gchar *id)
{
  gchar *sess;

  sess = g_copy_strings ("/panel-Session-", id, NULL);

  gnome_config_clean_file (sess);
  gnome_config_sync ();

  g_free (sess);
  return;
}

	
static error_t
parse_an_arg (int key, char *arg, struct argp_state *state)
{
  if (key == DISCARD_KEY)
    {
      discard_session (arg);
      just_exit = 1;
      return 0;
    }

  /* We didn't recognize it.  */
  return ARGP_ERR_UNKNOWN;
}

static void
panel_connect_client (GnomeClient *client,
		      gint was_restarted,
		      gpointer client_data)
{
	gchar *session_id;

	session_id = gnome_client_get_previous_id (client);
	
	if(session_id) {
		g_free(old_panel_cfg_path);
		old_panel_cfg_path = g_copy_strings("/panel-Session-",
						    session_id,"/",NULL);
	}
	puts("connected");
	puts(old_panel_cfg_path);
}
	

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

			g_free(child);
			children=g_list_remove_link(children,list);
			return;
		}
	}
}


int
main(int argc, char **argv)
{
	char buf[256];
	struct sigaction sa;
	
	panel_cfg_path = g_strdup("/panel/");
	old_panel_cfg_path = g_strdup("/panel/");

	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	sigemptyset (&sa.sa_mask);
	sa.sa_handler = sigchld_handler;
	sa.sa_flags   = 0;
	sigaction (SIGCHLD, &sa, NULL);

	client = gnome_client_new_default ();

	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
			    GTK_SIGNAL_FUNC (panel_session_save), argv[0]);
	gtk_signal_connect (GTK_OBJECT (client), "connect",
			    GTK_SIGNAL_FUNC (panel_connect_client), NULL);

	panel_corba_register_arguments ();

	gnome_init("panel", &parser, argc, argv, 0, NULL);

	puts("debug: need old_cfg here");

	if (just_exit)
		return 0;

	/* Tell session manager how to run us.  */
	gnome_client_set_clone_command (client, 1, argv);
	gnome_client_set_restart_command (client, 1, argv);

	/* Setup the cookie */
	cookie = create_cookie ();
	gnome_config_private_set_string ("/panel/Secret/cookie", cookie);
	
	applets = g_array_new(FALSE);
	applet_count=0;

	/*set up global options*/
	
	g_snprintf(buf,256,"%sConfig/",old_panel_cfg_path);
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

	apply_global_config();

	/*everything is erady ... load up the applets*/
	load_queued_applets();

	add_forbidden_to_panels();

	/* I use the glue code to avoid making this a C++ file */
	panel_corba_gtk_main ("IDL:GNOME/Panel:1.0");

	return 0;
}
