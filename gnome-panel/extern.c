/* Gnome panel: extern applet functions
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Federico Mena
 *           Miguel de Icaza
 */

#include <gdk/gdkx.h>
#include <config.h>
#include <string.h>
#include <signal.h>
#include <gnome.h>

#include "panel-include.h"

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

extern GArray *applets;
extern int applet_count;

extern GtkTooltips *panel_tooltips;

extern GlobalConfig global_config;

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

static GList *extern_applets = NULL;
static char *goad_id_starting = NULL; /*the goad id of the applet that is
					 being started right now, before it
					 does applet_register*/
static GList *start_queue = NULL; /*the queue of the applets to be
				    started*/
static int start_timeout = -1; /*id of the timeout for starting new applet*/

/*see if there is an extern applet running capable of doing
  something with this goad_id*/
static CORBA_Object
extern_is_goad_ready(const char *goad_id)
{
	GList *li;
	printf("goad_id:%s\n",goad_id);
	for(li=extern_applets;li!=NULL;li=g_list_next(li)) {
		Extern *e = li->data;
		GList *l;
		puts("GONE THROUGH ONE");
		for(l=e->goad_ids;l!=NULL;l=g_list_next(l)) {
			printf("%s\n",l->data);
			if(strcmp(l->data,goad_id)==0)
				/*found one*/
				return e->obj;
		}
	}
	puts("NOT FOUND");
	return CORBA_OBJECT_NIL;
}

static int
start_timeout_handler(gpointer data)
{
	start_timeout = -1;
	extern_start_next();
	return FALSE;
}


/*queue up a new goad id to start or start it if nothing else is
  starting*/
static void
extern_start_new_goad_id(char *goad_id)
{
        CORBA_Environment ev;
	if(!goad_id_starting) {
		CORBA_Object obj;
		obj = extern_is_goad_ready(goad_id);
		if(obj==CORBA_OBJECT_NIL) {
			CORBA_Object_release(goad_server_activate_with_id(NULL, goad_id, GOAD_ACTIVATE_NEW_ONLY|GOAD_ACTIVATE_ASYNC, NULL), &ev);
		} else {
			send_applet_start_new_applet(obj,goad_id);
		}
		goad_id_starting = g_strdup(goad_id);
	} else {
		if(start_timeout>-1)
			gtk_timeout_remove(start_timeout);
		start_timeout = -1;
		start_queue = g_list_append(start_queue,g_strdup(goad_id));
		start_timeout = gtk_timeout_add(100*1000,start_timeout_handler,NULL);
	}
}

void
extern_start_next(void)
{
	char *goad_id;
	if(goad_id_starting)
		g_free(goad_id_starting);
	goad_id_starting = NULL;
	if(!start_queue)
		return;
	goad_id = start_queue->data;
	start_queue = g_list_remove(start_queue,goad_id);
	
	extern_start_new_goad_id(goad_id);
	
	g_free(goad_id);
}


void
extern_clean(Extern *ext)
{
	extern_applets = g_list_remove(extern_applets,ext);

	if(ext->obj != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;
		CORBA_exception_init(&ev);
		CORBA_Object_release(ext->obj, &ev);
		CORBA_exception_free(&ev);
	}

	g_free(ext->goad_id);
	g_free(ext->cfg);
	g_free(ext);
}


void
applet_show_menu(int applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	GtkWidget *panel;

	g_return_if_fail(info != NULL);

	if (!info->menu)
		create_applet_menu(info);

	panel = get_panel_parent(info->widget);
	if(IS_SNAPPED_WIDGET(panel)) {
		SNAPPED_WIDGET(panel)->autohide_inhibit = TRUE;
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(panel));
	}

	gtk_menu_popup(GTK_MENU(info->menu), NULL, NULL, applet_menu_position,
		       GINT_TO_POINTER(applet_id), 3, GDK_CURRENT_TIME);
}

PanelOrientType
applet_get_panel_orient(int applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	PanelWidget *panel;

	g_return_val_if_fail(info != NULL,ORIENT_UP);

	panel = PANEL_WIDGET(info->widget->parent);

	g_return_val_if_fail(panel != NULL,ORIENT_UP);

	return get_applet_orient(panel);
}


int
applet_get_panel(int applet_id)
{
	int panel;
	GList *list;
	AppletInfo *info = get_applet_info(applet_id);
	gpointer p;

	g_return_val_if_fail(info != NULL,-1);

	p = PANEL_WIDGET(info->widget->parent);

	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
		if(list->data == p)
			return panel;
	return -1;
}

void
applet_abort_id(int applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	Extern *e;

	g_return_if_fail(info != NULL);
	
	e = info->data;

	g_return_if_fail(e != NULL);
	
	if(goad_id_starting && strcmp(e->goad_id,goad_id_starting)==0)
		extern_start_next();

	/*only reserved spots can be canceled, if an applet
	  wants to chance a pending applet it needs to first
	  user reserve spot to obtain id and make it EXTERN_RESERVED*/
	if(info->type != APPLET_EXTERN_RESERVED)
		return;

	panel_clean_applet(applet_id);
}


int
applet_get_pos(int applet_id)
{
	AppletInfo *info = get_applet_info(applet_id);
	AppletData *ad;

	g_return_val_if_fail(info != NULL,-1);

	ad = gtk_object_get_data(GTK_OBJECT(info->widget),
				 PANEL_APPLET_DATA);
	if(!ad)
		return -1;
	return ad->pos;
}

void
applet_drag_start(int applet_id)
{
	PanelWidget *panel;
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info != NULL);

	panel = PANEL_WIDGET(info->widget->parent);

	g_return_if_fail(panel!=NULL);

	/*panel_widget_applet_drag_start(panel,info->widget);
	panel_widget_applet_drag_end(panel);*/
	panel_widget_applet_drag_start(panel,info->widget);
	panel_widget_applet_move_use_idle(panel);
}

void
applet_drag_stop(int applet_id)
{
	PanelWidget *panel;
	AppletInfo *info = get_applet_info(applet_id);

	g_return_if_fail(info != NULL);

	panel = PANEL_WIDGET(info->widget->parent);

	g_return_if_fail(panel!=NULL);

	panel_widget_applet_drag_end(panel);
}

int
applet_request_id (const char *goad_id, char **cfgpath,
		   char **globcfgpath, guint32 * winid)
{
	AppletInfo *info;
	int i;
	Extern *ext;
	
	for(info=(AppletInfo *)applets->data,i=0;i<applet_count;i++,info++) {
		if(info && info->type == APPLET_EXTERN_PENDING) {
			Extern *ext = info->data;
			g_assert(ext);
			if(strcmp(ext->goad_id,goad_id)==0) {
				/*we started this and already reserved a spot
				  for it, including the socket widget*/
				GtkWidget *socket =
					GTK_BIN(info->widget)->child;
				g_return_val_if_fail(GTK_IS_SOCKET(socket),-1);

				*cfgpath = ext->cfg;
				ext->cfg = NULL;
				*globcfgpath = g_strdup(old_panel_cfg_path);
				info->type = APPLET_EXTERN_RESERVED;
				*winid=GDK_WINDOW_XWINDOW(socket->window);
				return i;
			}
		}
	}
	
	/*this is an applet that was started from outside, otherwise we would
	  have already reserved a spot for it*/
	ext = g_new(Extern,1);
	ext->obj = CORBA_OBJECT_NIL;
	ext->goad_id = g_strdup(goad_id);
	ext->cfg = NULL;
	ext->goad_ids = NULL;
	extern_applets = g_list_prepend(extern_applets,ext);

	*winid = reserve_applet_spot (ext, panels->data, 0,
				      APPLET_EXTERN_RESERVED);
	if(*winid == 0) {
		*globcfgpath = NULL;
		*cfgpath = NULL;
		return -1;
	}
	*cfgpath = g_copy_strings(old_panel_cfg_path,"Applet_Dummy/",NULL);
	*globcfgpath = g_strdup(old_panel_cfg_path);

	/*the i will now be the applet_id*/
	return i;
}

void
applet_register (CORBA_Object obj,
		 int applet_id,
		 const char *goad_id,
		 const char *goad_ids)
{
	AppletInfo *info = get_applet_info(applet_id);
	PanelWidget *panel;
	Extern *ext;
	CORBA_Environment ev;
	char *s;
	char *p;

	g_return_if_fail(info != NULL);

	ext = info->data;
	g_assert(ext);

	s = g_strdup(goad_ids);

	p = strtok(s,",");
	while(p) {
		ext->goad_ids = g_list_prepend(ext->goad_ids,
					       g_strdup(p));
		p = strtok(NULL,",");
	}
	g_free(s);
	
	/*if we should start the next applet*/
	if(goad_id_starting && strcmp(goad_id,goad_id_starting)==0)
		extern_start_next();


	panel = PANEL_WIDGET(info->widget->parent);
	g_return_if_fail(panel!=NULL);

	/*no longer pending*/
	info->type = APPLET_EXTERN;

	/*set the obj*/
	CORBA_exception_init(&ev);
	CORBA_Object_release(ext->obj, &ev);
	ext->obj = CORBA_Object_duplicate(obj, &ev);
	CORBA_exception_free(&ev);

	orientation_change(applet_id,panel);
	back_change(applet_id,panel);
	send_applet_tooltips_state(ext->obj, applet_id,
				   global_config.tooltips_enabled);
}

static int
extern_socket_destroy(GtkWidget *w, gpointer data)
{
	Extern *ext = data;
	gtk_widget_destroy(ext->ebox);
	extern_clean(ext);
	return FALSE;
}

/*note that type should be APPLET_EXTERN_RESERVED or APPLET_EXTERN_PENDING
  only*/
guint32
reserve_applet_spot (Extern *ext, PanelWidget *panel, int pos,
		     AppletType type)
{
	GtkWidget *socket;

	ext->ebox = gtk_event_box_new();
	gtk_widget_set_events(ext->ebox, (gtk_widget_get_events(ext->ebox) |
					  APPLET_EVENT_MASK) &
			      ~( GDK_POINTER_MOTION_MASK |
				 GDK_POINTER_MOTION_HINT_MASK));

	socket = gtk_socket_new();

	g_return_val_if_fail(socket!=NULL,0);

	gtk_container_add(GTK_CONTAINER(ext->ebox),socket);

	gtk_signal_connect(GTK_OBJECT(socket),"destroy",
			   GTK_SIGNAL_FUNC(extern_socket_destroy),
			   ext);

	gtk_widget_show_all (ext->ebox);
	
	/*we save the obj in the id field of the appletinfo and the 
	  path in the path field */
	if(!register_toy(ext->ebox,ext,panel,pos,type)) {
		g_warning("Couldn't add applet");
		return 0;
	}
	
	if(!GTK_WIDGET_REALIZED(socket))
		gtk_widget_realize(socket);

	return GDK_WINDOW_XWINDOW(socket->window);
}

void
applet_set_tooltip(int applet_id, const char *tooltip)
{
	AppletInfo *info = get_applet_info(applet_id);
	g_return_if_fail(info != NULL);

	gtk_tooltips_set_tip (panel_tooltips,info->widget,tooltip,NULL);
}

void
load_extern_applet(char *goad_id, char *cfgpath, PanelWidget *panel, int pos)
{
	Extern *ext;

	if(!cfgpath || !*cfgpath)
		cfgpath = g_copy_strings(old_panel_cfg_path,
					 "Applet_Dummy/",NULL);
	else
		/*we will free this lateer*/
		cfgpath = g_strdup(cfgpath);
	
	ext = g_new(Extern,1);
	ext->obj = CORBA_OBJECT_NIL;
	ext->goad_id = g_strdup(goad_id);
	ext->cfg = cfgpath;
	ext->goad_ids = NULL;
	extern_applets = g_list_prepend(extern_applets,ext);

	if(reserve_applet_spot (ext, panel, pos, APPLET_EXTERN_PENDING)==0) {
		g_warning("Whoops! for some reason we can't add "
			  "to the panel");
		extern_clean(ext);
		return;
	}

	extern_start_new_goad_id(goad_id);
}
