#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gnome.h>

#include <applet-widget.h>

#include "gnome-panel.h"

/*****************************************************************************
  CORBA STUFF
 *****************************************************************************/

static GNOME_Panel panel_client = NULL;
static GNOME_Applet applet_obj = NULL;

typedef struct _CallbackInfo CallbackInfo;
struct _CallbackInfo {
	char *name;
	int applet_id;
	AppletCallbackFunc func;
	gpointer data;
};

static GList *applet_callbacks = NULL;

#define APPLET_ID_KEY "applet_id_key"
#define APPLET_WIDGET_KEY "applet_widget_key"

static CORBA_ORB orb;
static CORBA_Environment ev;

static PortableServer_ServantBase__epv base_epv = {
  NULL, NULL, NULL
};

static void server_change_orient(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short orient, CORBA_Environment *ev);

static void server_do_callback(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_char * callback_name, CORBA_Environment *ev);

static CORBA_short server_session_save(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_char * cfgpath, CORBA_char * globcfgpath, CORBA_Environment *ev);

static void server_start_new_applet(POA_GNOME_Applet *servant, CORBA_char * goad_id, CORBA_Environment *ev);

static void server_back_change(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short back_type, CORBA_char * pixmap, CORBA_short c_red, CORBA_short c_green, CORBA_short c_blue, CORBA_Environment *ev);

static void server_tooltips_state(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short enabled, CORBA_Environment *ev);

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


/*****************************************************************************
  WIDGET STUFF
 *****************************************************************************/
static void applet_widget_class_init	(AppletWidgetClass *klass);
static void wapplet_widget_init		(AppletWidget      *applet_widget);

typedef void (*AppletWidgetOrientSignal) (GtkObject * object,
					  PanelOrientType orient,
					  gpointer data);

typedef int (*AppletWidgetSaveSignal) (GtkObject * object,
				        char *cfgpath,
				        char *globcfgpath,
				        gpointer data);

typedef void (*AppletWidgetBackSignal) (GtkObject * object,
					PanelBackType type,
					char *pixmap,
					GdkColor *color,
					gpointer data);

typedef void (*AppletWidgetTooltipSignal) (GtkObject * object,
					   int enabled,
					   gpointer data);

static GList *applet_widgets = NULL;
static int applet_count = 0;

static int do_multi = FALSE;
static int die_on_last = TRUE;

static GtkPlugClass *parent_class;

static AppletStartNewFunc start_new_func=NULL;
static gpointer start_new_func_data=NULL;

static GtkTooltips *applet_tooltips=NULL;

static GList *goad_ids_list = NULL;

guint
applet_widget_get_type ()
{
	static guint applet_widget_type = 0;

	if (!applet_widget_type) {
		GtkTypeInfo applet_widget_info = {
			"AppletWidget",
			sizeof (AppletWidget),
			sizeof (AppletWidgetClass),
			(GtkClassInitFunc) applet_widget_class_init,
			(GtkObjectInitFunc) wapplet_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		applet_widget_type = gtk_type_unique (gtk_plug_get_type (), &applet_widget_info);
	}

	return applet_widget_type;
}

enum {
	CHANGE_ORIENT_SIGNAL,
	SAVE_SESSION_SIGNAL,
	BACK_CHANGE_SIGNAL,
	TOOLTIP_STATE_SIGNAL,
	LAST_SIGNAL
};

static int applet_widget_signals[LAST_SIGNAL] = {0,0,0,0};

static void
applet_widget_marshal_signal_orient (GtkObject * object,
				     GtkSignalFunc func,
				     gpointer func_data,
				     GtkArg * args)
{
	AppletWidgetOrientSignal rfunc;

	rfunc = (AppletWidgetOrientSignal) func;

	(*rfunc) (object, GTK_VALUE_ENUM (args[0]),
		  func_data);
}

static void
applet_widget_marshal_signal_save (GtkObject * object,
				   GtkSignalFunc func,
				   gpointer func_data,
				   GtkArg * args)
{
	AppletWidgetSaveSignal rfunc;
	int *retval;

	rfunc = (AppletWidgetSaveSignal) func;

	retval = GTK_RETLOC_BOOL(args[2]);

	*retval = (*rfunc) (object, GTK_VALUE_STRING (args[0]),
		  	    GTK_VALUE_STRING (args[1]),
		  	    func_data);
	
	/*make applets that forget to do this not fsckup*/
	gnome_config_sync();
	gnome_config_drop_all();
}

static void
applet_widget_marshal_signal_back (GtkObject * object,
				   GtkSignalFunc func,
				   gpointer func_data,
				   GtkArg * args)
{
	AppletWidgetBackSignal rfunc;

	rfunc = (AppletWidgetBackSignal) func;

	(*rfunc) (object, GTK_VALUE_ENUM (args[0]),
		  GTK_VALUE_POINTER (args[1]),
		  GTK_VALUE_POINTER (args[2]),
		  func_data);
}

static void
applet_widget_marshal_signal_tooltip (GtkObject * object,
				      GtkSignalFunc func,
				      gpointer func_data,
				      GtkArg * args)
{
	AppletWidgetTooltipSignal rfunc;

	rfunc = (AppletWidgetTooltipSignal) func;

	(*rfunc) (object, GTK_VALUE_INT (args[0]),
		  func_data);
}

static void
applet_widget_class_init (AppletWidgetClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) class;

	parent_class = gtk_type_class (gtk_plug_get_type ());

	applet_widget_signals[CHANGE_ORIENT_SIGNAL] =
		gtk_signal_new("change_orient",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 change_orient),
			       applet_widget_marshal_signal_orient,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);
	applet_widget_signals[SAVE_SESSION_SIGNAL] =
		gtk_signal_new("save_session",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 save_session),
			       applet_widget_marshal_signal_save,
			       GTK_TYPE_BOOL,
			       2,
			       GTK_TYPE_STRING,
			       GTK_TYPE_STRING);
	applet_widget_signals[BACK_CHANGE_SIGNAL] =
		gtk_signal_new("back_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 back_change),
			       applet_widget_marshal_signal_back,
			       GTK_TYPE_NONE,
			       3,
			       GTK_TYPE_ENUM,
			       GTK_TYPE_POINTER,
			       GTK_TYPE_POINTER);
	applet_widget_signals[TOOLTIP_STATE_SIGNAL] =
		gtk_signal_new("tooltip_state",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 tooltip_state),
			       applet_widget_marshal_signal_tooltip,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_INT);

	gtk_object_class_add_signals(object_class,applet_widget_signals,
				     LAST_SIGNAL);

	class->change_orient = NULL;
	class->save_session = NULL;
}

static void
wapplet_widget_init (AppletWidget *applet)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	applet->applet_id = -1;
}

static void
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

static int
applet_widget_destroy(GtkWidget *w, gpointer data)
{
	AppletWidget *applet;

	g_return_val_if_fail(w != NULL,FALSE);
	g_return_val_if_fail(IS_APPLET_WIDGET(w),FALSE);

	applet = APPLET_WIDGET(w);
	if(!applet->privcfgpath)
		return FALSE;
	g_free(applet->privcfgpath);
	g_free(applet->globcfgpath);
	applet->privcfgpath = NULL;
	applet->globcfgpath = NULL;
	gnome_panel_applet_cleanup(applet->applet_id);
	if(GTK_BIN(w)->child == NULL)
		GNOME_Panel_applet_abort_id(panel_client, applet->applet_id, &ev);

	applet_count--;

	if(die_on_last && applet_count == 0)
		applet_widget_gtk_main_quit();
		/*gtk_exit(0);*/

	return FALSE;
}

void
applet_widget_abort_load(AppletWidget *applet)
{
	GNOME_Panel_applet_abort_id(panel_client, applet->applet_id, &ev);
}

void
applet_widget_remove_from_panel(AppletWidget *applet)
{
	CORBA_Object ns;
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	ns = gnome_name_service_get();
	goad_server_unregister(ns, applet->goad_id, "server", &ev);
	CORBA_Object_release(ns, &ev);

	GNOME_Panel_applet_remove_from_panel(panel_client, applet->applet_id, &ev);
}

void
applet_widget_sync_config(AppletWidget *applet)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	GNOME_Panel_sync_config(panel_client, (CORBA_short)applet->applet_id, &ev);
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


/*adds a callback to the callback hash*/
static void
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

void
applet_widget_register_callback(AppletWidget *applet,
				char *name,
				char *menutext,
				AppletCallbackFunc func,
				gpointer data)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	gnome_panel_applet_register_callback (applet->applet_id,name,
					      "",menutext,func,data);
}
void
applet_widget_register_stock_callback(AppletWidget *applet,
				      char *name,
				      char *stock_type,
				      char *menutext,
				      AppletCallbackFunc func,
				      gpointer data)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	gnome_panel_applet_register_callback (applet->applet_id,name,
					      stock_type,menutext,func,data);
}

void
applet_widget_unregister_callback(AppletWidget *applet,
				  char *name)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	/*skip over leading '/'s*/
	name = make_sane_name(name);

	g_return_if_fail(name!=NULL);

	/*unregister the callback with the panel*/
	GNOME_Panel_applet_remove_callback(panel_client, applet->applet_id, name, &ev);
}

static void
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


void
applet_widget_register_callback_dir(AppletWidget *applet,
				    char *name,
				    char *menutext)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	gnome_panel_applet_register_callback_dir (applet->applet_id,name,
						  "",menutext);
}
void
applet_widget_register_stock_callback_dir(AppletWidget *applet,
					  char *name,
					  char *stock_type,
					  char *menutext)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	gnome_panel_applet_register_callback_dir (applet->applet_id,name,
						  stock_type,menutext);
}

void
applet_widget_unregister_callback_dir(AppletWidget *applet, char *name)
{
	char *n;
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	/*skip over leading '/'s*/
	name = make_sane_name(name);
	if(name[strlen(name)-1]!='/')
		n = g_copy_strings(name,"/",NULL);
	else
		n = g_strdup(name);

	g_return_if_fail(name!=NULL);

	/*unregister the callback with the panel*/
	GNOME_Panel_applet_remove_callback(panel_client,applet->applet_id,n, &ev);
	g_free(n);
}

static int
gnome_panel_applet_corba_init(const char *goad_id)
{
  if(!applet_obj) {
    PortableServer_POA poa;

    PortableServer_ObjectId *objid;
    POA_GNOME_Applet__init(&applet_servant, &ev);

    g_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, FALSE);

    poa = (PortableServer_POA)
      CORBA_ORB_resolve_initial_references(orb, "RootPOA", &ev);

    PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(poa, &ev), &ev);
    g_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, FALSE);

    objid = PortableServer_POA_activate_object(poa, &applet_servant,
					       &ev);
    g_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, FALSE);

    applet_obj = PortableServer_POA_servant_to_reference(poa,
							 &applet_servant,
							 &ev);

    g_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, FALSE);

    /* Now a way to find out the CORBA impl from the widget */

    g_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, FALSE);
  }

  goad_server_register(CORBA_OBJECT_NIL, applet_obj, goad_id, "server", &ev);
  return TRUE;
}


GtkWidget *
applet_widget_new(const char *goad_id)
{
	AppletWidget *applet;
	CORBA_char *privcfgpath = NULL;
	CORBA_char *globcfgpath = NULL;
	int applet_id=-1;

	CORBA_unsigned_long wid;

	if(!gnome_panel_applet_corba_init(goad_id))
		g_error("Cannot initialize corba!\n");

	/*reserve a spot and get an id for this applet*/
	applet_id = GNOME_Panel_applet_request_id(panel_client,
						  (char *)goad_id,
						  &privcfgpath,
						  &globcfgpath,
						  &wid, &ev);
	if(applet_id==-1)
		g_error("Could not talk to the panel or the panel is giving us a bogus window id!\n");

	applet = APPLET_WIDGET (gtk_type_new (applet_widget_get_type ()));
	
	gtk_plug_construct(GTK_PLUG(applet),wid);

	applet->applet_id = applet_id;
	applet->privcfgpath = g_strdup(privcfgpath);
	CORBA_free(privcfgpath);
	applet->globcfgpath = g_strdup(globcfgpath);
	CORBA_free(globcfgpath);

	applet->goad_id = g_strdup(goad_id);

	gtk_signal_connect(GTK_OBJECT(applet),"destroy",
			   GTK_SIGNAL_FUNC(applet_widget_destroy),
			   NULL);

	applet_widgets = g_list_prepend(applet_widgets,applet);

	applet_count++;

	return GTK_WIDGET(applet);
}

int
applet_widget_get_applet_count()
{
	return applet_count;
}

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
				return TRUE;
			}else if(bevent->button == 2) {
				if((w = gtk_grab_get_current()))
					gtk_grab_remove(w);
				gdk_keyboard_ungrab(GDK_CURRENT_TIME);
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				gdk_flush();
				GNOME_Panel_applet_drag_start(panel_client,
							      ourid, &ev);
				return TRUE;
			} else if(bevent->button == 3) {
				if((w = gtk_grab_get_current()))
					gtk_grab_remove(w);
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				gdk_keyboard_ungrab(GDK_CURRENT_TIME);
				gdk_flush();
				GNOME_Panel_applet_show_menu(panel_client,
							     ourid, &ev);
				return TRUE;
			}
			break;
		case GDK_BUTTON_RELEASE:
			if(GNOME_Panel_applet_in_drag(panel_client, &ev)) {
				GNOME_Panel_applet_drag_stop(panel_client,
							     ourid, &ev);
				return TRUE;
			}
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

void
applet_widget_add(AppletWidget *applet, GtkWidget *widget)
{
	GString *str;

	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_WIDGET(widget));

	gtk_container_add(GTK_CONTAINER(applet),widget);

	str = g_string_new("");
	if(goad_ids_list) {
		GList *li;
		g_string_append(str,goad_ids_list->data);
		/* g_free(goad_ids_list->data); */
		for(li=goad_ids_list->next;li!=NULL;li=g_list_next(li)) {
			g_string_append_c(str,',');
			g_string_append(str,li->data);
			/* g_free(li->data); */
		}
	}

	GNOME_Panel_applet_register(panel_client, applet_obj, applet->applet_id, applet->goad_id, str->str, &ev);

	bind_top_applet_events(GTK_WIDGET(applet), applet->applet_id);

	g_string_free(str,TRUE);
}

void
applet_widget_set_widget_tooltip(AppletWidget *applet,
				 GtkWidget *widget,
				 char *text)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_WIDGET(widget));

	gtk_tooltips_set_tip (applet_tooltips,widget,text,NULL);
}

void
applet_widget_set_tooltip(AppletWidget *applet, char *text)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	if(text)
		GNOME_Panel_applet_add_tooltip(panel_client, applet->applet_id,text, &ev);
	else
		GNOME_Panel_applet_remove_tooltip(panel_client, applet->applet_id, &ev);
}

/* Get the oprientation the applet should use */
PanelOrientType
applet_widget_get_panel_orient(AppletWidget *applet)
{
	g_return_val_if_fail(applet != NULL,ORIENT_UP);
	g_return_val_if_fail(IS_APPLET_WIDGET(applet),ORIENT_UP);

	return (PanelOrientType)GNOME_Panel_applet_get_panel_orient(panel_client, applet->applet_id, &ev);
}

AppletWidget*
applet_widget_get_by_id(int applet_id)
{
	GList *list;
	for(list = applet_widgets;list!=NULL;list=g_list_next(list)) {
		AppletWidget *applet = list->data;
		if(applet->applet_id == applet_id)
			return applet;
	}
	return NULL;
}

static int
gnome_panel_applet_reinit_corba(CORBA_ORB panel_orb)
{
  char *name;
  char hostname [1024];
  char buf[256];
  int i;
	
  if(panel_client)
    CORBA_Object_release(panel_client, &ev);

  panel_client = goad_server_activate_with_repo_id(NULL, "IDL:GNOME/Panel:1.0", 0, NULL);

  g_return_val_if_fail(!CORBA_Object_is_nil(panel_client, &ev), 0);
	
  return 1;
}

static int
gnome_panel_applet_init_corba (CORBA_ORB applet_orb)
{
  int n = 1;
  char *foo = NULL;

  CORBA_exception_init(&ev);

  orb = applet_orb;

  n = gnome_panel_applet_reinit_corba(orb);

  return n;
}

int	
applet_widget_init(const char *app_id,
		   const char *app_version,
		   int argc,
		   char **argv,
		   struct poptOption *options,
		   unsigned int flags,
		   poptContext *return_ctx,
		   int last_die,
		   GList *goad_ids,
		   AppletStartNewFunc new_func,
		   gpointer new_func_data)
{
	int ret = TRUE; /*bogus value, this should be if we succeded or not
			  or something*/
	CORBA_Environment ev;
	CORBA_ORB orb;

	while(goad_ids) {
		goad_ids_list = g_list_prepend(goad_ids_list,
					       g_strdup(goad_ids->data));
		goad_ids = g_list_next(goad_ids);
	}
	start_new_func = new_func;
	start_new_func_data = new_func_data;
	die_on_last = last_die;

	gnome_client_disable_master_connection ();
	CORBA_exception_init(&ev);
	orb = gnome_CORBA_init_with_popt_table(app_id, VERSION, &argc, argv,
					       options, flags, return_ctx,
					       GNORBA_INIT_SERVER_FUNC, &ev);
	CORBA_exception_free(&ev);

	if (!gnome_panel_applet_init_corba(orb))
		g_error("Could not communicate with the panel\n");
	
	applet_tooltips = gtk_tooltips_new();

	return ret;
}

/*****************************************************************************
  CORBA STUFF
 *****************************************************************************/

void
applet_widget_gtk_main(void)
{
	gtk_main();
	/*FIXMEIs the string gonna be actually used??*/
	/*applet_corba_gtk_main("IDL:GNOME/Applet:1.0");*/
}

void
applet_widget_gtk_main_quit (void)
{
	if(panel_client)
		CORBA_Object_release(panel_client, &ev);

	CORBA_ORB_shutdown(orb, CORBA_TRUE, &ev);
	gtk_main_quit();
}

void
applet_widget_panel_quit (void)
{
	GNOME_Panel_quit(panel_client, &ev);
}


static void
server_change_orient(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short orient, CORBA_Environment *ev)
{
	AppletWidget *applet;
	PanelOrientType o = (PanelOrientType) orient;

	applet = applet_widget_get_by_id(applet_id);
	if(applet) {
		gtk_signal_emit(GTK_OBJECT(applet),
				applet_widget_signals[CHANGE_ORIENT_SIGNAL],
				o);
	}
}

static void
server_do_callback(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_char * callback_name, CORBA_Environment *ev)
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

static CORBA_short
server_session_save(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_char * cfgpath, CORBA_char * globcfgpath, CORBA_Environment *ev)
{
	AppletWidget *applet;

	char *cfg = g_strdup(cfgpath);
	char *globcfg = g_strdup(globcfgpath);
	int return_val = FALSE;

	applet = applet_widget_get_by_id(applet_id);
	if(applet) {
		gtk_signal_emit(GTK_OBJECT(applet),
				applet_widget_signals[SAVE_SESSION_SIGNAL],
				cfg,globcfg,&return_val);
	}
	g_free(cfg);
	g_free(globcfg);

	/*return_val of true would mean that the applet handeled the
	  session saving itself, therefore we pass the reverse to the
	  corba function*/
	return !return_val;
}

static void
server_start_new_applet(POA_GNOME_Applet *servant, CORBA_char * goad_id, CORBA_Environment *ev)
{
	if(start_new_func)
		(*start_new_func)(goad_id,start_new_func_data);
}

static void
server_back_change(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short back_type, CORBA_char * pixmap, CORBA_short c_red, CORBA_short c_green, CORBA_short c_blue, CORBA_Environment *ev)
{
	GdkColor color = {1, c_red, c_green, c_blue};

	AppletWidget *applet;

	applet = applet_widget_get_by_id(applet_id);
	if(applet) {
		gtk_signal_emit(GTK_OBJECT(applet),
				applet_widget_signals[BACK_CHANGE_SIGNAL],
				back_type,pixmap,&color);
	}
}

static void
server_tooltips_state(POA_GNOME_Applet *servant, CORBA_short applet_id, CORBA_short enabled, CORBA_Environment *ev)
{
	AppletWidget *applet;

	if(enabled)
		gtk_tooltips_enable(applet_tooltips);
	else
		gtk_tooltips_disable(applet_tooltips);

	applet = applet_widget_get_by_id(applet_id);
	if(applet) {
		gtk_signal_emit(GTK_OBJECT(applet),
				applet_widget_signals[TOOLTIP_STATE_SIGNAL],
				enabled);
	}
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
panel_corba_register_arguments(void)
{
}
