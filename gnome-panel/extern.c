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
#include "gnome-panel.h"

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

extern GList *applets;
extern GList *applets_last;
extern int applet_count;

extern int config_sync_timeout;
extern GList *applets_to_sync;
extern int panels_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

extern GtkTooltips *panel_tooltips;

extern GlobalConfig global_config;

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

static GList *extern_applets = NULL;

/********************* CORBA Stuff *******************/

CORBA_ORB orb = NULL;
CORBA_Environment ev;
PortableServer_POA thepoa;

/***Panel stuff***/
static GNOME_PanelSpot
s_panel_add_applet(POA_GNOME_Panel *servant,
		   GNOME_Applet panel_applet,
		   CORBA_char *goad_id,
		   CORBA_char ** cfgpath,
		   CORBA_char ** globcfgpath,
		   CORBA_unsigned_long* wid,
		   CORBA_Environment *ev);

static GNOME_PanelSpot
s_panel_add_applet_full(POA_GNOME_Panel *servant,
			GNOME_Applet panel_applet,
			CORBA_char *goad_id,
			CORBA_short panel,
			CORBA_short pos,
			CORBA_char ** cfgpath,
			CORBA_char ** globcfgpath,
			CORBA_unsigned_long* wid,
			CORBA_Environment *ev);

static void
s_panel_quit(POA_GNOME_Panel *servant, CORBA_Environment *ev);

static CORBA_boolean
s_panel_get_in_drag(POA_GNOME_Panel *servant, CORBA_Environment *ev);



/*** PanelSpot stuff ***/

static CORBA_char *
s_panelspot_get_tooltip(POA_GNOME_PanelSpot *servant,
			CORBA_Environment *ev);

static void
s_panelspot_set_tooltip(POA_GNOME_PanelSpot *servant,
			CORBA_char *val,
			CORBA_Environment *ev);

static CORBA_short
s_panelspot_get_parent_panel(POA_GNOME_PanelSpot *servant,
			     CORBA_Environment *ev);

static CORBA_short
s_panelspot_get_spot_pos(POA_GNOME_PanelSpot *servant,
			 CORBA_Environment *ev);

static GNOME_Panel_OrientType
s_panelspot_get_parent_orient(POA_GNOME_PanelSpot *servant,
			      CORBA_Environment *ev);

static void
s_panelspot_register_us(POA_GNOME_PanelSpot *servant,
		     CORBA_Environment *ev);

static void
s_panelspot_unregister_us(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev);

static void
s_panelspot_abort_load(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev);

static void
s_panelspot_show_menu(POA_GNOME_PanelSpot *servant,
		      CORBA_Environment *ev);

static void
s_panelspot_drag_start(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev);

static void
s_panelspot_drag_stop(POA_GNOME_PanelSpot *servant,
		      CORBA_Environment *ev);

static void
s_panelspot_add_callback(POA_GNOME_PanelSpot *servant,
			 CORBA_char *callback_name,
			 CORBA_char *stock_item,
			 CORBA_char *menuitem_text,
			 CORBA_Environment *ev);

static void
s_panelspot_remove_callback(POA_GNOME_PanelSpot *servant,
			    CORBA_char *callback_name,
			    CORBA_Environment *ev);

static void
s_panelspot_sync_config(POA_GNOME_PanelSpot *servant,
			CORBA_Environment *ev);

static PortableServer_ServantBase__epv panel_base_epv = {
  NULL, /* _private */
  NULL, /* finalize */
  NULL, /* use base default_POA function */
};

static POA_GNOME_Panel__epv panel_epv = {
  NULL, /* private data */
  (gpointer)&s_panel_add_applet,
  (gpointer)&s_panel_add_applet_full,
  (gpointer)&s_panel_quit,
  (gpointer)&s_panel_get_in_drag
};
static POA_GNOME_Panel__vepv panel_vepv = { &panel_base_epv, &panel_epv };
static POA_GNOME_Panel panel_servant = { NULL, &panel_vepv };


static PortableServer_ServantBase__epv panelspot_base_epv = {
  NULL, /* _private */
  NULL, /* finalize */
  NULL, /* use base default_POA function */
};

static POA_GNOME_PanelSpot__epv panelspot_epv = {
  NULL, /* private data */
  (gpointer)&s_panelspot_get_tooltip,
  (gpointer)&s_panelspot_set_tooltip,
  (gpointer)&s_panelspot_get_parent_panel,
  (gpointer)&s_panelspot_get_spot_pos,
  (gpointer)&s_panelspot_get_parent_orient,
  (gpointer)&s_panelspot_register_us,
  (gpointer)&s_panelspot_unregister_us,
  (gpointer)&s_panelspot_abort_load,
  (gpointer)&s_panelspot_show_menu,
  (gpointer)&s_panelspot_drag_start,
  (gpointer)&s_panelspot_drag_stop,
  (gpointer)&s_panelspot_add_callback,
  (gpointer)&s_panelspot_remove_callback,
  (gpointer)&s_panelspot_sync_config
};
static POA_GNOME_PanelSpot__vepv panelspot_vepv = { &panelspot_base_epv, &panelspot_epv };

/********************* NON-CORBA Stuff *******************/

static void
extern_start_new_goad_id(Extern *e)
{
        CORBA_Environment ev;
	CORBA_exception_init(&ev);
	CORBA_Object_release(goad_server_activate_with_id(NULL, e->goad_id, GOAD_ACTIVATE_NEW_ONLY|GOAD_ACTIVATE_ASYNC, NULL),&ev);
	CORBA_exception_free(&ev);
}

void
extern_clean(Extern *ext)
{
	CORBA_Environment ev;
	CORBA_exception_init(&ev);

	extern_applets = g_list_remove(extern_applets,ext);

	g_free(ext->goad_id);
	g_free(ext->cfg);

	CORBA_Object_release(ext->pspot, &ev);
	CORBA_Object_release(ext->applet, &ev);
	POA_GNOME_PanelSpot__fini((PortableServer_Servant) ext, &ev);

	g_free(ext);

	CORBA_exception_free(&ev);
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
static CORBA_unsigned_long
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
	ext->info = NULL;
	if(!register_toy(ext->ebox,ext,panel,pos,type)) {
		g_warning("Couldn't add applet");
		return 0;
	}
	ext->info = applets_last->data;
	
	if(!GTK_WIDGET_REALIZED(socket))
		gtk_widget_realize(socket);

	return GDK_WINDOW_XWINDOW(socket->window);
}

void
load_extern_applet(char *goad_id, char *cfgpath, PanelWidget *panel, int pos)
{
	Extern *ext;
	POA_GNOME_PanelSpot *panelspot_servant;

	if(!cfgpath || !*cfgpath)
		cfgpath = g_copy_strings(old_panel_cfg_path,
					 "Applet_Dummy/",NULL);
	else
		/*we will free this lateer*/
		cfgpath = g_strdup(cfgpath);
	
	ext = g_new(Extern,1);

	panelspot_servant = (POA_GNOME_PanelSpot *)ext;
	panelspot_servant->_private = NULL;
	panelspot_servant->vepv = &panelspot_vepv;

	POA_GNOME_PanelSpot__init(panelspot_servant, &ev);
	
	CORBA_free(PortableServer_POA_activate_object(thepoa, panelspot_servant, &ev));
	g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

	ext->pspot = CORBA_OBJECT_NIL; /*will be filled in during add_applet*/
	ext->applet = CORBA_OBJECT_NIL;
	ext->goad_id = g_strdup(goad_id);
	ext->cfg = cfgpath;
	extern_applets = g_list_prepend(extern_applets,ext);

	if(reserve_applet_spot (ext, panel, pos, APPLET_EXTERN_PENDING)==0) {
		g_warning("Whoops! for some reason we can't add "
			  "to the panel");
		extern_clean(ext);
		return;
	}

	extern_start_new_goad_id(ext);
}

/********************* CORBA Stuff *******************/


static GNOME_PanelSpot
s_panel_add_applet(POA_GNOME_Panel *servant,
		   GNOME_Applet panel_applet,
		   CORBA_char *goad_id,
		   CORBA_char ** cfgpath,
		   CORBA_char ** globcfgpath,
		   CORBA_unsigned_long* wid,
		   CORBA_Environment *ev)
{
	return s_panel_add_applet_full(servant,panel_applet,goad_id,0,0,
				       cfgpath,globcfgpath,wid,ev);
}

static GNOME_PanelSpot
s_panel_add_applet_full(POA_GNOME_Panel *servant,
			GNOME_Applet panel_applet,
			CORBA_char *goad_id,
			CORBA_short panel,
			CORBA_short pos,
			CORBA_char ** cfgpath,
			CORBA_char ** globcfgpath,
			CORBA_unsigned_long* wid,
			CORBA_Environment *ev)
{
	GList *li;
	int i;
	Extern *ext;
	char *p;
	POA_GNOME_PanelSpot *panelspot_servant;
	GNOME_PanelSpot acc;
	
	for(li=applets;li!=NULL;li=g_list_next(li)) {
		AppletInfo *info = li->data;
		if(info && info->type == APPLET_EXTERN_PENDING) {
			Extern *ext = info->data;
			g_assert(ext);
			g_assert(ext->info == info);
			if(strcmp(ext->goad_id,goad_id)==0) {
				/*we started this and already reserved a spot
				  for it, including the socket widget*/
				GtkWidget *socket =
					GTK_BIN(info->widget)->child;
				g_return_val_if_fail(GTK_IS_SOCKET(socket),
						     NULL);

				ext->applet = CORBA_Object_duplicate(panel_applet, ev);
				*cfgpath = CORBA_string_dup(ext->cfg);
				g_free(ext->cfg);
				ext->cfg = NULL;
				*globcfgpath = CORBA_string_dup(old_panel_cfg_path);
				info->type = APPLET_EXTERN_RESERVED;
				*wid=GDK_WINDOW_XWINDOW(socket->window);
				printf("\nSOCKET XID: %lX\n\n", (long)*wid);

				panelspot_servant = (POA_GNOME_PanelSpot *)ext;
				acc = PortableServer_POA_servant_to_reference(thepoa, panelspot_servant, ev);
				g_return_val_if_fail(ev->_major == CORBA_NO_EXCEPTION,NULL);
				ext->pspot = CORBA_Object_duplicate(acc, ev);
				g_return_val_if_fail(ev->_major == CORBA_NO_EXCEPTION,NULL);

				return CORBA_Object_duplicate(acc, ev);
			}
		}
	}
	
	/*this is an applet that was started from outside, otherwise we would
	  have already reserved a spot for it*/
	ext = g_new(Extern,1);

	panelspot_servant = (POA_GNOME_PanelSpot *)ext;
	panelspot_servant->_private = NULL;
	panelspot_servant->vepv = &panelspot_vepv;

	POA_GNOME_PanelSpot__init(panelspot_servant, ev);
	
	CORBA_free(PortableServer_POA_activate_object(thepoa, panelspot_servant, ev));
	g_return_val_if_fail(ev->_major == CORBA_NO_EXCEPTION,NULL);

	acc = PortableServer_POA_servant_to_reference(thepoa, panelspot_servant, ev);
	g_return_val_if_fail(ev->_major == CORBA_NO_EXCEPTION,NULL);

	ext->pspot = CORBA_Object_duplicate(acc, ev);

	g_return_val_if_fail(ev->_major == CORBA_NO_EXCEPTION,NULL);

	ext->applet = CORBA_Object_duplicate(panel_applet, ev);
	ext->goad_id = g_strdup(goad_id);
	ext->cfg = NULL;
	extern_applets = g_list_prepend(extern_applets,ext);

	/*select the nth panel*/
	if(panel)
		li = g_list_nth(panels,panel);
	else
		li = NULL;
	if(!li)
		li = panels;

	*wid = reserve_applet_spot (ext, li->data, pos,
				      APPLET_EXTERN_RESERVED);
	if(*wid == 0) {
		extern_clean(ext);
		*globcfgpath = NULL;
		*cfgpath = NULL;
		return CORBA_OBJECT_NIL;
	}
	p = g_copy_strings(old_panel_cfg_path,"Applet_Dummy/",NULL);
	*cfgpath = CORBA_string_dup(p);
	g_free(p);
	*globcfgpath = CORBA_string_dup(old_panel_cfg_path);

	return CORBA_Object_duplicate(acc, ev);
}

static void
s_panel_quit(POA_GNOME_Panel *servant, CORBA_Environment *ev)
{
	panel_quit();
}

static CORBA_boolean
s_panel_get_in_drag(POA_GNOME_Panel *servant, CORBA_Environment *ev)
{
	return panel_applet_in_drag;
}



/*** PanelSpot stuff ***/

static CORBA_char *
s_panelspot_get_tooltip(POA_GNOME_PanelSpot *servant,
			CORBA_Environment *ev)
{
	/*FIXME: get the tooltip text*/
	return CORBA_string_dup("");
}

static void
s_panelspot_set_tooltip(POA_GNOME_PanelSpot *servant,
			CORBA_char *val,
			CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	if(val && *val)
		gtk_tooltips_set_tip (panel_tooltips,ext->ebox,val,NULL);
	else
		gtk_tooltips_set_tip (panel_tooltips,ext->ebox,NULL,NULL);
}

static CORBA_short
s_panelspot_get_parent_panel(POA_GNOME_PanelSpot *servant,
			     CORBA_Environment *ev)
{
	int panel;
	GList *list;
	gpointer p;
	Extern *ext = (Extern *)servant;

	g_assert(ext);
	g_assert(ext->info);

	p = PANEL_WIDGET(ext->info->widget->parent);

	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
		if(list->data == p)
			return panel;
	return -1;
}

static CORBA_short
s_panelspot_get_spot_pos(POA_GNOME_PanelSpot *servant,
			 CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	PanelWidget *panel;
	AppletData *ad;

	g_assert(ext);
	g_assert(ext->info);
	
	ad = gtk_object_get_data(GTK_OBJECT(ext->info->widget),
				 PANEL_APPLET_DATA);
	if(!ad)
		return -1;
	return ad->pos;
}

static GNOME_Panel_OrientType
s_panelspot_get_parent_orient(POA_GNOME_PanelSpot *servant,
			      CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	PanelWidget *panel;

	g_assert(ext);
	g_assert(ext->info);

	panel = PANEL_WIDGET(ext->info->widget->parent);

	g_return_val_if_fail(panel != NULL,ORIENT_UP);

	return get_applet_orient(panel);
}

static void
s_panelspot_register_us(POA_GNOME_PanelSpot *servant,
			CORBA_Environment *ev)
{
	PanelWidget *panel;
	Extern *ext = (Extern *)servant;

	g_assert(ext);
	g_assert(ext->info);
	
	printf("register ext: %lX\n",(long)ext);
	printf("register ext->info: %lX\n",(long)(ext->info));

	panel = PANEL_WIDGET(ext->info->widget->parent);
	g_return_if_fail(panel!=NULL);

	/*no longer pending*/
	ext->info->type = APPLET_EXTERN;

	orientation_change(ext->info,panel);
	back_change(ext->info,panel);

	GNOME_Applet_set_tooltips_state(ext->applet,
					global_config.tooltips_enabled, ev);
	if(ev->_major)
		panel_clean_applet(ext->info);
}

static void
s_panelspot_unregister_us(POA_GNOME_PanelSpot *servant,
			  CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	panel_clean_applet(ext->info);
}

static void
s_panelspot_abort_load(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;

	g_return_if_fail(ext != NULL);

	g_return_if_fail(ext->info != NULL);
	
	/*only reserved spots can be canceled, if an applet
	  wants to chance a pending applet it needs to first
	  user reserve spot to obtain id and make it EXTERN_RESERVED*/
	if(ext->info->type != APPLET_EXTERN_RESERVED)
		return;

	panel_clean_applet(ext->info);
}

static void
s_panelspot_show_menu(POA_GNOME_PanelSpot *servant,
		      CORBA_Environment *ev)
{
	GtkWidget *panel;
	Extern *ext = (Extern *)servant;
	
	printf("show menu ext: %lX\n",(long)ext);
	printf("show menu ext->info: %lX\n",(long)(ext->info));

	g_return_if_fail(ext != NULL);
	g_return_if_fail(ext->info != NULL);

	if (!ext->info->menu)
		create_applet_menu(ext->info);

	panel = get_panel_parent(ext->info->widget);
	if(IS_SNAPPED_WIDGET(panel)) {
		SNAPPED_WIDGET(panel)->autohide_inhibit = TRUE;
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(panel));
	}

	gtk_menu_popup(GTK_MENU(ext->info->menu), NULL, NULL,
		       applet_menu_position,
		       ext->info, 3, GDK_CURRENT_TIME);
}


static void
s_panelspot_drag_start(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev)
{
	PanelWidget *panel;
	Extern *ext = (Extern *)servant;

	g_return_if_fail(ext != NULL);
	g_return_if_fail(ext->info != NULL);

	panel = PANEL_WIDGET(ext->info->widget->parent);

	g_return_if_fail(panel!=NULL);

	/*panel_widget_applet_drag_start(panel,info->widget);
	panel_widget_applet_drag_end(panel);*/
	panel_widget_applet_drag_start(panel,ext->info->widget);
}

static void
s_panelspot_drag_stop(POA_GNOME_PanelSpot *servant,
		      CORBA_Environment *ev)
{
	PanelWidget *panel;
	Extern *ext = (Extern *)servant;

	g_return_if_fail(ext != NULL);
	g_return_if_fail(ext->info != NULL);

	panel = PANEL_WIDGET(ext->info->widget->parent);

	g_return_if_fail(panel!=NULL);

	panel_widget_applet_drag_end(panel);
}

static void
s_panelspot_add_callback(POA_GNOME_PanelSpot *servant,
			 CORBA_char *callback_name,
			 CORBA_char *stock_item,
			 CORBA_char *menuitem_text,
			 CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;

	printf("add callback ext: %lX\n",(long)ext);

	g_return_if_fail(ext != NULL);
	g_return_if_fail(ext->info != NULL);
	applet_add_callback(ext->info, callback_name, stock_item,
			    menuitem_text);
}

static void
s_panelspot_remove_callback(POA_GNOME_PanelSpot *servant,
			    CORBA_char *callback_name,
			    CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;

	g_return_if_fail(ext != NULL);
	g_return_if_fail(ext->info != NULL);
	applet_remove_callback(ext->info, callback_name);
}

static void
s_panelspot_sync_config(POA_GNOME_PanelSpot *servant,
			CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;

	g_return_if_fail(ext != NULL);
	g_return_if_fail(ext->info != NULL);
	if(g_list_find(applets_to_sync, ext->info)==NULL)
		applets_to_sync = g_list_prepend(applets_to_sync,ext->info);
	panel_config_sync();
}

void
panel_corba_clean_up(void)
{
  goad_server_unregister(CORBA_OBJECT_NIL, "gnome_panel", "server", &ev);
  CORBA_ORB_shutdown(orb, CORBA_FALSE, &ev);
}

void
panel_corba_gtk_init(CORBA_ORB panel_orb)
{
  GNOME_Panel acc;
  char hostname [4096];
  char *name;
  CORBA_Object ns;

  CORBA_exception_init(&ev);

  orb = panel_orb;

  POA_GNOME_Panel__init(&panel_servant, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  thepoa = (PortableServer_POA)
    CORBA_ORB_resolve_initial_references(orb, "RootPOA", &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(thepoa, &ev), &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  CORBA_free(PortableServer_POA_activate_object(thepoa,
						&panel_servant, &ev));
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  acc = PortableServer_POA_servant_to_reference(thepoa, &panel_servant, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  goad_server_register(CORBA_OBJECT_NIL, acc, "gnome_panel", "server", &ev);

  /*
  //CORBA_Object_release(acc, &ev);
  */
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);
}
