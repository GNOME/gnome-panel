#include <string.h>
#include "applet-lib.h"

/*
 *
 * there is a lot of functionality in this file that is then slightly
 * redundant in the applet-widget ... this is because I wish to keep
 * applet-widget a clean C file, while applet-lib as the corba interface
 * after we have a C orb, we can do everything in applet-widget
 *
 */

#include "gnome-panel.h"

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

GNOME_Panel panel_client = NULL;
GNOME_Applet applet_obj = NULL;


typedef struct _CallbackInfo CallbackInfo;
struct _CallbackInfo {
	char *name;
	int applet_id;
	AppletCallbackFunc func;
	gpointer data;
};

GList *applet_callbacks = NULL;

#define APPLET_ID_KEY "applet_id_key"
#define APPLET_WIDGET_KEY "applet_widget_key"

CORBA_ORB orb;
CORBA_Environment ev;

static PortableServer_ServantBase__epv base_epv = {
  NULL, NULL, NULL
};

void server_change_orient(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short orient, CORBA_Environment *ev);

void server_do_callback(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_char * callback_name, CORBA_Environment *ev);

CORBA_short server_session_save(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_char * cfgpath, CORBA_char * globcfgpath, CORBA_Environment *ev);

void server_start_new_applet(POA_GNOME_Applet *servant, CORBA_char * goad_id, CORBA_Environment *ev);

void server_back_change(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short back_type, CORBA_char * pixmap, CORBA_short c_red, CORBA_short c_green, CORBA_short c_blue, CORBA_Environment *ev);

void server_tooltips_state(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short enabled, CORBA_Environment *ev);

static POA_GNOME_Applet__epv applet_epv = {
  NULL, /* _private */
  (gpointer)&server_change_orient,
  (gpointer)&server_do_callback,
  (gpointer)&server_session_save,
  (gpointer)&server_start_new_applet,
  (gpointer)&server_back_change,
  (gpointer)&server_tooltips_state
};
static POA_GNOME_Applet__vepv vepv = { &base_epv, &applet_epv };
static POA_GNOME_Applet applet_servant = { NULL, &vepv };


void server_change_orient(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short orient, CORBA_Environment *ev)
{
  _gnome_applet_change_orient(applet_id, orient);
}

void server_do_callback(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_char * callback_name, CORBA_Environment *ev)
{
  GList *list;

  for(list = applet_callbacks;
      list!=NULL;list = (GList *) g_list_next (list)) {
    CallbackInfo *info = (CallbackInfo *)list->data;
    if(info->applet_id == applet_id &&
       strcmp(info->name,(char *)callback_name)==0) {
      (*(info->func)) (
		       applet_widget_get_by_id(applet_id),
		       info->data);
      return;
    }
  }
}

CORBA_short server_session_save(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_char * cfgpath, CORBA_char * globcfgpath, CORBA_Environment *ev)
{
  return _gnome_applet_session_save(applet_id, cfgpath, globcfgpath);
}

void server_start_new_applet(POA_GNOME_Applet *servant, CORBA_char * goad_id, CORBA_Environment *ev)
{
  _gnome_applet_start_new_applet(goad_id);
}

void server_back_change(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short back_type, CORBA_char * pixmap, CORBA_short c_red, CORBA_short c_green, CORBA_short c_blue, CORBA_Environment *ev)
{
  GdkColor color = {1, c_red, c_green, c_blue};

  _gnome_applet_back_change(applet_id, back_type, pixmap, &color);
}

void server_tooltips_state(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short enabled, CORBA_Environment *ev)
{
  _gnome_applet_tooltips_state(applet_id, enabled);
}

int gnome_panel_applet_reinit_corba(CORBA_ORB panel_orb)
{
  char *name;
  char hostname [1024];
  char buf[256];
  int i;
	
  if(panel_client)
    CORBA_Object_release(panel_client, &ev);

  panel_client = goad_server_activate_with_repo_id(NULL, "IDL:GNOME/Panel:1.0", 0);

  g_return_val_if_fail(!CORBA_Object_is_nil(panel_client, &ev), 0);
	
  return 1;
}

int
gnome_panel_applet_init_corba (CORBA_ORB applet_orb)
{
  int n = 1;
  char *foo = NULL;

  CORBA_exception_init(&ev);

  orb = applet_orb;

  n = gnome_panel_applet_reinit_corba(orb);

  return n;
}


static CallbackInfo *
get_callback_info(int applet_id, char *name)
{
	GList *list;
	for(list=applet_callbacks;list!=NULL;list=g_list_next(list)) {
		CallbackInfo *info = (CallbackInfo *)list->data;
		if(applet_id == info->applet_id && strcmp(name,info->name)==0)
			return info;
	}
	return NULL;
}

static char*
make_sane_name(char *name)
{
	if(!name)
		return NULL;
	while(*name=='/')
		name++;
	if(*name)
		return name;
	return NULL;
}

/*adds a callback to the callback hash*/
void
gnome_panel_applet_register_callback(int applet_id,
				     char *name,
				     char *stock_item,
				     char *menutext,
				     AppletCallbackFunc func,
				     gpointer data)
{
	CallbackInfo *info;
	/*skip over leading '/'s*/
	name = make_sane_name(name);

	g_return_if_fail(name!=NULL);
	
	info = get_callback_info(applet_id,name);
	if(!info) {
		info = g_new(CallbackInfo,1);
		applet_callbacks = g_list_prepend(applet_callbacks,info);
	} else
		g_free(info->name);

	info->name = g_strdup(name);
	info->applet_id = applet_id;
	info->func = func;
	info->data = data;

	/*register the callback with the panel*/
	GNOME_Panel_applet_add_callback(panel_client,
					applet_id, name, stock_item,
					menutext, &ev);
}

/*removes a callback from the callback hash*/
void
gnome_panel_applet_unregister_callback(int applet_id,
				       char *name)
{
	/*skip over leading '/'s*/
	name = make_sane_name(name);

	g_return_if_fail(name!=NULL);

	/*unregister the callback with the panel*/
	GNOME_Panel_applet_remove_callback(panel_client, applet_id, name, &ev);
}

void
gnome_panel_applet_register_callback_dir(int applet_id,
					 char *name,
					 char *stock_item,
					 char *menutext)
{
	char *n;
	/*skip over leading '/'s*/
	name = make_sane_name(name);
	g_return_if_fail(name!=NULL);

	if(name[strlen(name)-1]!='/')
		n = g_copy_strings(name,"/",NULL);
	else
		n = g_strdup(name);
	/*unregister the dir with the panel*/
	GNOME_Panel_applet_add_callback(panel_client,
					applet_id,
					n,stock_item,menutext, &ev);
	g_free(n);
}

/*removes a callback dir from the callback menu*/
void
gnome_panel_applet_unregister_callback_dir(int applet_id,
					   char *name)
{
	char *n;

	/*skip over leading '/'s*/
	name = make_sane_name(name);
	if(name[strlen(name)-1]!='/')
		n = g_copy_strings(name,"/",NULL);
	else
		n = g_strdup(name);

	g_return_if_fail(name!=NULL);

	/*unregister the callback with the panel*/
	GNOME_Panel_applet_remove_callback(panel_client,applet_id,n, &ev);
	g_free(n);
}

#if 0
static void
gtk_plug_forward_button_press (GtkPlug *plug, GdkEventButton *event)
{
  XEvent xevent;
  
  xevent.xbutton.type = KeyPress;
  xevent.xbutton.display = GDK_WINDOW_XDISPLAY (GTK_WIDGET(plug)->window);
  xevent.xbutton.window = GDK_WINDOW_XWINDOW (plug->socket_window);
  xevent.xbutton.root = GDK_ROOT_WINDOW (); /* FIXME */
  xevent.xbutton.time = event->time;
  /* FIXME, the following might cause big problems for
   * non-GTK apps */
  xevent.xbutton.x = 0;
  xevent.xbutton.y = 0;
  xevent.xbutton.x_root = 0;
  xevent.xbutton.y_root = 0;
  xevent.xbutton.state = event->state;
  xevent.xbutton.keycode =  XKeysymToKeycode(GDK_DISPLAY(), 
					  event->keyval);
  xevent.xbutton.same_screen = TRUE; /* FIXME ? */
  
  XSendEvent (gdk_display,
	      GDK_WINDOW_XWINDOW (plug->socket_window),
	      False, NoEventMask, &xevent);
}
#endif


/*catch events relevant to the panel and notify the panel*/
static int
applet_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	int ourid = GPOINTER_TO_INT(data);
	GdkEventButton *bevent;
	int in_drag;
	GtkWidget *w;
		
	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;
			in_drag = GNOME_Panel_applet_in_drag(panel_client, &ev);
			/*check to see if there is an applet being dragged*/
			if(in_drag) {
				GNOME_Panel_applet_drag_stop(panel_client,
							     ourid, &ev);
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				gdk_keyboard_ungrab(GDK_CURRENT_TIME);
				gtk_grab_remove(widget);
				return TRUE;
			}else if(bevent->button == 2) {
				GdkCursor *fleur_cursor =
					gdk_cursor_new(GDK_FLEUR);

				gdk_keyboard_ungrab(GDK_CURRENT_TIME);
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				if((w = gtk_grab_get_current()))
					gtk_grab_remove(w);
				gtk_grab_add(widget);
				if(widget->window) {
					gdk_pointer_grab(widget->window,
							 FALSE,
							 GDK_BUTTON_PRESS_MASK |
							  GDK_BUTTON_RELEASE_MASK,
							 NULL,
							 fleur_cursor,
							 GDK_CURRENT_TIME);
				}
				GNOME_Panel_applet_drag_start(panel_client,
							      ourid, &ev);
				gdk_cursor_destroy(fleur_cursor);
				return TRUE;
			} else if(bevent->button == 3) {
				if((w = gtk_grab_get_current()))
					gtk_grab_remove(w);
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				gdk_keyboard_ungrab(GDK_CURRENT_TIME);
				GNOME_Panel_applet_show_menu(panel_client,
							     ourid, &ev);
				return TRUE;
			}
			break;
		case GDK_BUTTON_RELEASE:
			if(GNOME_Panel_applet_in_drag(panel_client, &ev)) {
				GNOME_Panel_applet_drag_stop(panel_client,
							     ourid, &ev);
				gdk_keyboard_ungrab(GDK_CURRENT_TIME);
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				gtk_grab_remove(widget);
				return TRUE;
			}
			break;
		case GDK_MOTION_NOTIFY:
			puts("MOTION");
			if(GNOME_Panel_applet_in_drag(panel_client, &ev))
				return TRUE;
			break;
		default:
			break;
	}

	return FALSE;
}

static int
applet_sub_event_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	switch (event->type) {
		/*pass these to the parent!*/
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
		case GDK_MOTION_NOTIFY:
			return gtk_widget_event(GTK_WIDGET(data), event);

			break;

		default:
			break;
	}

	return FALSE;
}


static void
bind_applet_events(GtkWidget *widget, gpointer data)
{
	if (!GTK_WIDGET_NO_WINDOW(widget)) {
		gtk_signal_connect(GTK_OBJECT(widget), "event",
				   (GtkSignalFunc) applet_sub_event_handler,
				   data);
	}
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, data);
}

static void
bind_top_applet_events(GtkWidget *widget, int applet_id)
{
	gtk_signal_connect(GTK_OBJECT(widget),
			   "event",
			   GTK_SIGNAL_FUNC(applet_event),
			   GINT_TO_POINTER(applet_id));

	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events,widget);
}

/*id will return a unique id for this applet for the applet to identify
  itself as*/
char *
gnome_panel_applet_request_id (const char *goad_id,
			       int *applet_id,
			       char **cfgpath,
			       char **globcfgpath,
			       guint32 *winid)
{
	CORBA_char *cfg = NULL;
	CORBA_char *globcfg = NULL;
	CORBA_unsigned_long wid;
	int i;

	/*this is the first call to panel so we'll do a loop and timeout
	  after 30 seconds if we don't find a panel*/
	*applet_id = -1;

	for(i=0;i<30;i++) {
		  /*reserve a spot and get an id for this applet*/
	  *applet_id = GNOME_Panel_applet_request_id(panel_client,
						     (char *)goad_id,
						     &cfg,
						     &globcfg,
						     &wid, &ev);
	  if(*applet_id!=-1)
	    break;
	  sleep(1);
	  gnome_panel_applet_reinit_corba (orb);
	}
	/*if the request_id never completed*/
	if(*applet_id == -1)
		return g_strdup("Can't talk to a panel\n");

	if(winid)
		*winid = wid;

	if(cfgpath==NULL) {
		CORBA_free(cfg);
	} else if(cfg != NULL) {
		*cfgpath = g_strdup(cfg);
		CORBA_free(cfg);
	} else {
		*cfgpath = NULL;
	}
	if(globcfgpath==NULL) {
		CORBA_free(globcfg);
	} else if(globcfg != NULL) {
		*globcfgpath = g_strdup(globcfg);
		CORBA_free(globcfg);
	} else {
		*globcfgpath = NULL;
	}

	return 0;
}

CORBA_Object
gnome_panel_applet_corba_init(const char *goad_id)
{
  if(!applet_obj) {
    PortableServer_POA poa;

    PortableServer_ObjectId *objid;
    POA_GNOME_Applet__init(&applet_servant, &ev);

    g_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);

    poa = (PortableServer_POA)
      CORBA_ORB_resolve_initial_references(orb, "RootPOA", &ev);

    PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(poa, &ev), &ev);
    g_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);

    objid = PortableServer_POA_activate_object(poa, &applet_servant,
					       &ev);
    g_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);

    applet_obj = PortableServer_POA_servant_to_reference(poa,
							 &applet_servant,
							 &ev);

    g_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);

    /* Now a way to find out the CORBA impl from the widget */

    g_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);
  }

  {
    CORBA_Object ns;
    CORBA_char *ior;
	  
    ns = gnome_name_service_get();
    goad_server_register(ns, applet_obj, goad_id, "server", &ev);
    CORBA_Object_release(ns, &ev);

    ior = CORBA_ORB_object_to_string(orb, applet_obj, &ev);

    printf("%s\n", ior); fflush(stdout);

    CORBA_free(ior);
  }

  return applet_obj;
}

/* this function will register the ior with the panel so it can call us */
char *
gnome_panel_applet_register (GtkWidget *widget, int applet_id,
			     const char *goad_id, const char *goad_ids,
			     CORBA_Object applet)
{
	GNOME_Panel_applet_register(panel_client, applet, applet_id, (char *)goad_id, (char *)goad_ids, &ev);

	bind_top_applet_events(widget, applet_id);

	return 0;
}

char *
gnome_panel_applet_abort_id (int applet_id)
{
  GNOME_Panel_applet_abort_id(panel_client, applet_id, &ev);

  return 0;
}

int
gnome_panel_applet_get_panel_orient (int applet_id)
{
  return GNOME_Panel_applet_get_panel_orient(panel_client, applet_id, &ev);
}

char *
gnome_panel_applet_remove_from_panel (int applet_id, const char *goad_id)
{
  CORBA_Object ns;
  
  ns = gnome_name_service_get();
  goad_server_unregister(ns, goad_id, "server", &ev);
  CORBA_Object_release(ns, &ev);

  GNOME_Panel_applet_remove_from_panel(panel_client, applet_id, &ev);

  return 0;
}

char *
gnome_panel_applet_add_tooltip (int applet_id, char *tooltip)
{
  GNOME_Panel_applet_add_tooltip(panel_client, applet_id,tooltip, &ev);

  return 0;
}

char *
gnome_panel_applet_remove_tooltip (int applet_id)
{
  GNOME_Panel_applet_remove_tooltip(panel_client, applet_id, &ev);
  
  return 0;
}

/*id will return a unique id for this applet for the applet to identify
  itself as*/
char *
gnome_panel_applet_request_glob_cfg (char **globcfgpath)
{
  CORBA_char *globcfg = NULL;

  g_return_val_if_fail(globcfgpath!=NULL,0);

  GNOME_Panel_applet_request_glob_cfg(panel_client, &globcfg, &ev);

  if(globcfg!= NULL) {
    *globcfgpath = g_strdup(globcfg);
    CORBA_free(globcfg);
  } else {
    *globcfgpath = NULL;
  }

  return 0;
}

void
gnome_panel_applet_cleanup(int applet_id)
{
  GList *list;
  GList *nlist;

  for(list = applet_callbacks;list!=NULL;) {
    CallbackInfo *info = (CallbackInfo *)list->data;
    nlist = g_list_next(list);
    if(info->applet_id == applet_id) {
      g_free(info->name);
      applet_callbacks = g_list_remove_link(applet_callbacks,list);
      g_list_free_1(list);
      g_free(info);
    }
    list = nlist;
  }
}


char *
gnome_panel_sync_config (int applet_id)
{
  GNOME_Panel_sync_config(panel_client, (CORBA_short)applet_id, &ev);
  return 0;
}

char *
gnome_panel_quit (void)
{
  GNOME_Panel_quit(panel_client, &ev);
  return 0;
}


static void
applet_handle_connection(GIOPConnection *cnx, gint source,
			 GdkInputCondition cond)
{
  switch(cond) {
  case GDK_INPUT_EXCEPTION:
    giop_main_handle_connection_exception(cnx);
    break;
  default:
    giop_main_handle_connection(cnx);
  }
}

static void orb_add_connection(GIOPConnection *cnx)
{
  cnx->user_data = GINT_TO_POINTER(gtk_input_add_full(GIOP_CONNECTION_GET_FD(cnx),
					   GDK_INPUT_READ|GDK_INPUT_EXCEPTION,
					   (GdkInputFunction)applet_handle_connection,
					   NULL, cnx, NULL));
}

static void orb_remove_connection(GIOPConnection *cnx)
{
  gtk_input_remove(GPOINTER_TO_INT(cnx->user_data));
}

void
applet_corba_gtk_main (char *str)
{
  gtk_main();
}

void
applet_corba_gtk_main_quit (void)
{
  if(panel_client)
    CORBA_Object_release(panel_client, &ev);

  CORBA_ORB_shutdown(orb, CORBA_TRUE, &ev);
  gtk_main_quit();
}

void
panel_corba_register_arguments(void)
{
}
