#include <string.h>
#include "applet-lib.h"

#include <orb/orbit.h>

static void orb_add_connection(GIOPConnection *cnx);
static void orb_remove_connection(GIOPConnection *cnx);

/*
 *
 * there is a lot of functionality in this file that is then slightly
 * redundant in the applet-widget ... this is because I wish to keep
 * applet-widget a clean C file, while applet-lib as the corba interface
 * after we have a C orb, we can do everything in applet-widget
 *
 */

#include "cookie.h"
#include "gnome-panel.h"

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

GNOME_Panel panel_client = NULL;


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

char *cookie;

static PortableServer_ServantBase__epv base_epv = {
  NULL, NULL, NULL
};

void server_change_orient(POA_GNOME_Applet *servant, CORBA_char * cookie, CORBA_short applet_id, CORBA_short orient, CORBA_Environment *ev);

void server_do_callback(POA_GNOME_Applet *servant, CORBA_char * cookie, CORBA_short applet_id, CORBA_char * callback_name, CORBA_Environment *ev);

CORBA_short server_session_save(POA_GNOME_Applet *servant, CORBA_char * cookie, CORBA_short applet_id, CORBA_char * cfgpath, CORBA_char * globcfgpath, CORBA_Environment *ev);

void server_start_new_applet(POA_GNOME_Applet *servant, CORBA_char * cookie, CORBA_char * param, CORBA_Environment *ev);

void server_back_change(POA_GNOME_Applet *servant, CORBA_char * cookie, CORBA_short applet_id, CORBA_short back_type, CORBA_char * pixmap, CORBA_short c_red, CORBA_short c_green, CORBA_short c_blue, CORBA_Environment *ev);

void server_tooltips_state(POA_GNOME_Applet *servant, CORBA_char * cookie, CORBA_short enabled, CORBA_Environment *ev);

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

BEGIN_GNOME_DECLS

void _gnome_applet_change_orient(int applet_id, int orient);
int _gnome_applet_session_save(int applet_id, const char *cfgpath,
			       const char *globcfgpath);
int _gnome_applet_start_new_applet(const char *params);
void _gnome_applet_back_change(int applet_id, int back_type,
			       const char *pixmap, GdkColor *color);
void _gnome_applet_tooltips_state(int enabled);

END_GNOME_DECLS


void server_change_orient(POA_GNOME_Applet *servant, CORBA_char * ccookie, CORBA_short applet_id, CORBA_short orient, CORBA_Environment *ev)
{
  CHECK_COOKIE();

  _gnome_applet_change_orient(applet_id, orient);
}

void server_do_callback(POA_GNOME_Applet *servant, CORBA_char * ccookie, CORBA_short applet_id, CORBA_char * callback_name, CORBA_Environment *ev)
{
  GList *list;

  CHECK_COOKIE ();

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

CORBA_short server_session_save(POA_GNOME_Applet *servant, CORBA_char * ccookie, CORBA_short applet_id, CORBA_char * cfgpath, CORBA_char * globcfgpath, CORBA_Environment *ev)
{
  CHECK_COOKIE_V(0);

  return _gnome_applet_session_save(applet_id, cfgpath, globcfgpath);
}

void server_start_new_applet(POA_GNOME_Applet *servant, CORBA_char * ccookie, CORBA_char * param, CORBA_Environment *ev)
{
  CHECK_COOKIE();

  _gnome_applet_start_new_applet(param);
}

void server_back_change(POA_GNOME_Applet *servant, CORBA_char * ccookie, CORBA_short applet_id, CORBA_short back_type, CORBA_char * pixmap, CORBA_short c_red, CORBA_short c_green, CORBA_short c_blue, CORBA_Environment *ev)
{
  GdkColor color = {1, c_red, c_green, c_blue};

  CHECK_COOKIE();

  _gnome_applet_back_change(applet_id, back_type, pixmap, &color);
}

void server_tooltips_state(POA_GNOME_Applet *servant, CORBA_char * ccookie, CORBA_short enabled, CORBA_Environment *ev)
{
  CHECK_COOKIE();

  _gnome_applet_tooltips_state(enabled);
}

int gnome_panel_applet_reinit_corba(void)
{
  char *name;
  char *iior;
  char hostname [1024];
  char buf[256];
  int i;
	
  gethostname (hostname, sizeof (hostname));
  if (hostname [0] == 0)
    strcpy (hostname, "unknown-host");

	/*do a 20 second timeout until we get the iior*/
  for(i=0;i<20;i++) {
    name = g_copy_strings ("/CORBA-servers/Panel-", hostname, 
			   "/DISPLAY-", getenv ("DISPLAY"), NULL);

    iior = gnome_config_get_string (name);
    g_free (name);

    if(iior)
      break;
    sleep(1);
  }
  if(!iior)
    return 0;

  g_snprintf(buf,256,"/panel/Secret/cookie-DISPLAY-%s=",getenv("DISPLAY"));
  cookie = gnome_config_private_get_string (buf);

  if(panel_client)
    CORBA_Object_release(panel_client, &ev);

  panel_client = CORBA_ORB_string_to_object(orb, iior, &ev);

  g_return_val_if_fail(panel_client, 0);
	
  return 1;
}

int
gnome_panel_applet_init_corba (void)
{
  int n = 1;
  char *foo = NULL;

  g_message("Initializing CORBA for applet\n");

  CORBA_exception_init(&ev);

  IIOPAddConnectionHandler = orb_add_connection;
  IIOPRemoveConnectionHandler = orb_remove_connection;

  orb = CORBA_ORB_init(&n, &foo, "mico-local-orb", &ev);
  ORBit_custom_run_setup(orb, &ev);

  n = gnome_panel_applet_reinit_corba();

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
	GNOME_Panel_applet_add_callback(panel_client, cookie,
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
	GNOME_Panel_applet_remove_callback(panel_client, cookie, applet_id, name, &ev);
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
	GNOME_Panel_applet_add_callback(panel_client,cookie,applet_id,n,stock_item,menutext, &ev);
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
	GNOME_Panel_applet_remove_callback(panel_client,cookie,applet_id,n, &ev);
	g_free(n);
}

/*catch events relevant to the panel and notify the panel*/
static int
applet_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	int ourid = GPOINTER_TO_INT(data);
	GdkEventButton *bevent;
	int in_drag;

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;
			in_drag = GNOME_Panel_applet_in_drag(panel_client, cookie, &ev);
			/*check to see if there is an applet being dragged*/
			if(bevent->button == 2 && !in_drag) {
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				gtk_grab_remove(widget);
				GNOME_Panel_applet_drag_start(panel_client,
							      cookie, ourid, &ev);
				return TRUE;
			} else if(in_drag) {
				GNOME_Panel_applet_drag_stop(panel_client,
							     cookie,ourid, &ev);
				return TRUE;
			} else if(bevent->button == 3 && !in_drag) {
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				gtk_grab_remove(widget);
				GNOME_Panel_applet_show_menu(panel_client,
							     cookie, ourid, &ev);
				return TRUE;
			}
			break;
		case GDK_BUTTON_RELEASE:
			if(GNOME_Panel_applet_in_drag(panel_client, cookie, &ev)) {
				GNOME_Panel_applet_drag_stop(panel_client,
							     cookie, ourid, &ev);
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
applet_sub_event_handler(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	switch (event->type) {
		/*pass these to the parent!*/
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
			return gtk_widget_event(
				listening_parent(widget->parent), event);

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
				   NULL);
	}
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, NULL);
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
				       bind_applet_events, NULL);
}

/*id will return a unique id for this applet for the applet to identify
  itself as*/
char *
gnome_panel_applet_request_id (const char *path,
			       const char *param,
			       int dorestart,
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
	  after 20 seconds if we don't find a panel*/
	*applet_id = -1;

	for(i=0;i<20;i++) {
		  /*reserve a spot and get an id for this applet*/
	  *applet_id = GNOME_Panel_applet_request_id(panel_client,
						     cookie,
						     (CORBA_char *)path,
						     (CORBA_char *)param,
						     dorestart,
						     &cfg,
						     &globcfg,
						     &wid, &ev);
	  sleep(1);
	  gnome_panel_applet_reinit_corba ();
	  if(*applet_id!=-1)
	    break;
	  sleep(1);
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


/* this function will register the ior with the panel so it can call us */
char *
gnome_panel_applet_register (GtkWidget *widget, int applet_id)
{
	static char *ior=NULL;

	/*the applet implementation, it's only created once*/
	static GNOME_Applet applet = NULL;

	if(!ior) {
	  PortableServer_POA poa;

	  static PortableServer_ObjectId objid = {0, sizeof("GNOME/Applet"),
						  "GNOME/Applet"};
	  POA_GNOME_Applet__init(&applet_servant, &ev);

	  poa = orb->root_poa; /* non-portable temp hack */

	  PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(poa, &ev), &ev);

	  PortableServer_POA_activate_object_with_id(poa,
						     &objid, &applet_servant,
						     &ev);

	  applet = PortableServer_POA_servant_to_reference(poa,
							   &applet_servant,
							   &ev);

	/* Now a way to find out the CORBA impl from the widget */

	  ior = CORBA_ORB_object_to_string(orb, applet, &ev);
	}

	gtk_object_set_data (GTK_OBJECT (widget), "CORBA_object", applet);

	GNOME_Panel_applet_register(panel_client, cookie, ior, applet_id, &ev);

	CORBA_free(ior);

	ior = NULL;

	bind_top_applet_events(widget, applet_id);

	return 0;
}

char *
gnome_panel_applet_abort_id (int applet_id)
{
  GNOME_Panel_applet_abort_id(panel_client, cookie, applet_id, &ev);

  return 0;
}

int
gnome_panel_applet_get_panel_orient (int applet_id)
{
  return GNOME_Panel_applet_get_panel_orient(panel_client, cookie, applet_id, &ev);
}

char *
gnome_panel_applet_remove_from_panel (int applet_id)
{
  GNOME_Panel_applet_remove_from_panel(panel_client, cookie, applet_id, &ev);

  return 0;
}

char *
gnome_panel_applet_add_tooltip (int applet_id, char *tooltip)
{
  GNOME_Panel_applet_add_tooltip(panel_client, cookie, applet_id,tooltip, &ev);

  return 0;
}

char *
gnome_panel_applet_remove_tooltip (int applet_id)
{
  GNOME_Panel_applet_remove_tooltip(panel_client, cookie, applet_id, &ev);
  
  return 0;
}

/*id will return a unique id for this applet for the applet to identify
  itself as*/
char *
gnome_panel_applet_request_glob_cfg (char **globcfgpath)
{
  CORBA_char *globcfg = NULL;

  g_return_val_if_fail(globcfgpath!=NULL,0);

  GNOME_Panel_applet_request_glob_cfg(panel_client, cookie, &globcfg, &ev);

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
      g_list_remove_link(applet_callbacks,list);
      g_free(info);

    }
    list = nlist;
  }
}


char *
gnome_panel_sync_config (int applet_id)
{
  GNOME_Panel_sync_config(panel_client, cookie, (CORBA_short)applet_id, &ev);
  return 0;
}

char *
gnome_panel_quit (void)
{
  GNOME_Panel_quit(panel_client, cookie, &ev);
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
  CORBA_ORB_shutdown(orb, CORBA_FALSE, &ev);
  gtk_main_quit();
}

void
panel_corba_register_arguments(void)
{
}
