#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gnome.h>

#include <applet-widget.h>
#include <libgnorba/gnorba.h>

#include "gnome-panel.h"

/*****************************************************************************
  CORBA STUFF
 *****************************************************************************/

typedef struct _CallbackInfo CallbackInfo;
struct _CallbackInfo {
  char *name;
  AppletCallbackFunc func;
  gpointer data;
};

GNOME_Panel panel_client;

#define APPLET_ID_KEY "applet_id_key"
#define APPLET_WIDGET_KEY "applet_widget_key"
#define pg_return_if_fail(x) if(!(x)) { g_print("type = %d exid = %s\n", ev._major, ev._repo_id); g_return_if_fail(x); }
#define pg_return_val_if_fail(x,y) if(!(x)) { g_print("type = %d exid = %s\n", ev._major, ev._repo_id); g_return_val_if_fail(x, y); }

typedef struct {
  POA_GNOME_Applet servant;
  PortableServer_ObjectId *objid;
  PortableServer_POA poa;

  AppletWidget *appwidget;
  GSList *callbacks;
  GNOME_PanelSpot pspot;
  GNOME_Applet obj;
  guint32 winid;

  char                    *goad_id;

} CustomAppletServant;

static PortableServer_ServantBase__epv base_epv = {
  NULL, NULL, NULL
};

static void
server_applet_change_orient(CustomAppletServant *servant,
			    GNOME_Panel_OrientType orient,
			    CORBA_Environment *ev);

static void
server_applet_do_callback(CustomAppletServant *servant,
			  CORBA_char * callback_name,
			  CORBA_Environment *ev);

static CORBA_boolean
server_applet_session_save(CustomAppletServant *servant,
			   CORBA_char * cfgpath,
			   CORBA_char * globcfgpath,
			   CORBA_Environment *ev);

static void
server_applet_back_change(CustomAppletServant *servant,
			  GNOME_Panel_BackInfoType *backing,
			  CORBA_Environment *ev);

static void
server_applet_set_tooltips_state(CustomAppletServant *servant,
				 CORBA_boolean enabled,
				 CORBA_Environment *ev);

static CORBA_char *
server_applet__get_goad_id(CustomAppletServant *servant,
			   CORBA_Environment *ev);

static POA_GNOME_Applet__epv applet_epv = {
  NULL,
  (gpointer)&server_applet_change_orient,
  (gpointer)&server_applet_do_callback,
  (gpointer)&server_applet_session_save,
  (gpointer)&server_applet_back_change,
  (gpointer)&server_applet_set_tooltips_state,
  (gpointer)&server_applet__get_goad_id
};

static POA_GNOME_Applet__vepv vepv = { &base_epv, &applet_epv };

/*****************************************************************************
  WIDGET STUFF
 *****************************************************************************/
static void applet_widget_class_init	(AppletWidgetClass *klass);
static void wapplet_widget_init		(AppletWidget      *applet_widget);

typedef void (*AppletWidgetOrientSignal) (GtkObject * object,
					  GNOME_Panel_OrientType orient,
					  gpointer data);

typedef int (*AppletWidgetSaveSignal) (GtkObject * object,
				        char *cfgpath,
				        char *globcfgpath,
				        gpointer data);

typedef void (*AppletWidgetBackSignal) (GtkObject * object,
					GNOME_Panel_BackType type,
					char *pixmap,
					GdkColor *color,
					gpointer data);

typedef void (*AppletWidgetTooltipSignal) (GtkObject * object,
					   int enabled,
					   gpointer data);

static GList *applet_widgets = NULL;
static int applet_count = 0;

static int do_multi = FALSE;
static int die_on_last = FALSE;

static GtkPlugClass *parent_class;

static GtkTooltips *applet_tooltips=NULL;

#define CD(applet) ((CustomAppletServant *)APPLET_WIDGET(applet)->corbadat)

guint
applet_widget_get_type ()
{
	static guint applet_widget_type = 0;

	if (!applet_widget_type) {
		static const GtkTypeInfo applet_widget_info = {
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
	class->back_change = NULL;
	class->tooltip_state = NULL;
}

static void
wapplet_widget_init (AppletWidget *applet)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	applet->corbadat = NULL;
}

static void
applet_servant_destroy(CustomAppletServant *servant)
{
  GSList *list;
  PortableServer_POA poa;
  CORBA_Environment ev;

  for(list = servant->callbacks; list; list = g_slist_next(list)) {
    CallbackInfo *info = (CallbackInfo *)list->data;
    g_free(info->name);
    g_free(info);
  }
  g_slist_free(servant->callbacks);

  CORBA_exception_init(&ev);
  poa = servant->poa;
  PortableServer_POA_deactivate_object(poa, servant->objid, &ev);
  CORBA_free(servant->objid);

  goad_server_unregister(CORBA_OBJECT_NIL,servant->goad_id,
			 "server", &ev);
  g_free(servant->goad_id);

  CORBA_Object_release(servant->pspot, &ev);
  CORBA_Object_release(servant->obj, &ev);
  POA_GNOME_Applet__fini((PortableServer_Servant) servant, &ev);
  g_free(servant);
  CORBA_Object_release((CORBA_Object)poa, &ev);
  CORBA_exception_free(&ev);
}

static int
applet_widget_destroy(GtkWidget *w, gpointer data)
{
  AppletWidget *applet;
  CORBA_Environment ev;

  g_return_val_if_fail(w != NULL,FALSE);
  g_return_val_if_fail(IS_APPLET_WIDGET(w),FALSE);

  applet = APPLET_WIDGET(w);
  if(!applet->privcfgpath)
    return FALSE;

  g_free(applet->privcfgpath);
  g_free(applet->globcfgpath);

  applet->privcfgpath = NULL;
  applet->globcfgpath = NULL;

  CORBA_exception_init(&ev);
  if(GTK_BIN(w)->child == NULL)
    GNOME_PanelSpot_abort_load(CD(applet)->pspot, &ev);
  CORBA_exception_free(&ev);

  applet_servant_destroy(applet->corbadat);

  applet_count--;

  if(die_on_last && applet_count == 0)
    applet_widget_gtk_main_quit();

  return FALSE;
}

void
applet_widget_abort_load(AppletWidget *applet)
{
  CORBA_Environment ev;
  CORBA_exception_init(&ev);
  GNOME_PanelSpot_abort_load(CD(applet)->pspot, &ev);
  CORBA_exception_free(&ev);
}

void
applet_widget_remove(AppletWidget *applet)
{
  CORBA_Object ns;
  CORBA_Environment ev;
  CustomAppletServant *servant;
  g_return_if_fail(applet != NULL);
  g_return_if_fail(IS_APPLET_WIDGET(applet));

  CORBA_exception_init(&ev);
  servant = applet->corbadat;
  goad_server_unregister(CORBA_OBJECT_NIL, servant->goad_id, "server", &ev);

  GNOME_PanelSpot_unregister_us(CD(applet)->pspot, &ev);
  CORBA_exception_free(&ev);
}

void
applet_widget_sync_config(AppletWidget *applet)
{
  CORBA_Environment ev;
  g_return_if_fail(applet != NULL);
  g_return_if_fail(IS_APPLET_WIDGET(applet));
  
  CORBA_exception_init(&ev);
  GNOME_PanelSpot_sync_config(CD(applet)->pspot, &ev);
  CORBA_exception_free(&ev);
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
get_callback_info(GtkWidget *applet, char *name)
{
  GSList *list;
  CallbackInfo *info;

  for(list=CD(applet)->callbacks;
      list!=NULL;
      list=g_slist_next(list)) {
    info = (CallbackInfo *)list->data;
    if(strcmp(name,info->name)==0)
      return info;
  }

  return NULL;
}


/*adds a callback to the callback hash*/
static void
gnome_panel_applet_register_callback(GtkWidget *applet,
				     char *name,
				     char *stock_item,
				     char *menutext,
				     AppletCallbackFunc func,
				     gpointer data)
{
	CallbackInfo *info;
	CORBA_Environment ev;

	/*skip over leading '/'s*/
	name = make_sane_name(name);

	g_return_if_fail(name!=NULL);
	
	info = get_callback_info(applet,name);
	if(!info) {
		info = g_new(CallbackInfo,1);
		CD(applet)->callbacks = g_slist_prepend(CD(applet)->callbacks,
							info);
	} else
		g_free(info->name);

	info->name = g_strdup(name);
	info->func = func;
	info->data = data;

	CORBA_exception_init(&ev);
	/*register the callback with the panel*/
	GNOME_PanelSpot_add_callback(CD(applet)->pspot,
				     name, stock_item,
				     menutext, &ev);
	CORBA_exception_free(&ev);
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

	gnome_panel_applet_register_callback (GTK_WIDGET(applet),name,
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

	gnome_panel_applet_register_callback (GTK_WIDGET(applet),name,
					      stock_type,menutext,func,data);
}

void
applet_widget_unregister_callback(AppletWidget *applet,
				  char *name)
{
  GSList *ltmp;
  CallbackInfo *cbi = NULL;
  CORBA_Environment ev;

  g_return_if_fail(applet != NULL);
  g_return_if_fail(IS_APPLET_WIDGET(applet));

  /*skip over leading '/'s*/
  name = make_sane_name(name);

  g_return_if_fail(name!=NULL);

  for(ltmp = CD(applet)->callbacks; ltmp && !cbi; ltmp = g_slist_next(ltmp)) {
    if(!strcmp(((CallbackInfo*)ltmp->data)->name, name))
      cbi = ltmp->data;
  }

  if(!cbi) return;
  CD(applet)->callbacks = g_slist_remove(CD(applet)->callbacks, cbi);

  CORBA_exception_init(&ev);
  GNOME_PanelSpot_remove_callback(CD(applet)->pspot, name, &ev);
  CORBA_exception_free(&ev);
}

static void
gnome_panel_applet_register_callback_dir(GtkWidget *applet,
					 char *name,
					 char *stock_item,
					 char *menutext)
{
	char *n;
	CORBA_Environment ev;

	/*skip over leading '/'s*/
	name = make_sane_name(name);
	g_return_if_fail(name!=NULL);

	if(name[strlen(name)-1]!='/')
		n = g_copy_strings(name,"/",NULL);
	else
		n = g_strdup(name);
	CORBA_exception_init(&ev);
	/*unregister the dir with the panel*/
	GNOME_PanelSpot_add_callback(CD(applet)->pspot,
				     n,stock_item,menutext, &ev);
	CORBA_exception_free(&ev);
	g_free(n);
}


void
applet_widget_register_callback_dir(AppletWidget *applet,
				    char *name,
				    char *menutext)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	gnome_panel_applet_register_callback_dir (GTK_WIDGET(applet),name,
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

	gnome_panel_applet_register_callback_dir (GTK_WIDGET(applet),name,
						  stock_type,menutext);
}

void
applet_widget_unregister_callback_dir(AppletWidget *applet, char *name)
{
	CORBA_Environment ev;
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
	CORBA_exception_init(&ev);
	GNOME_PanelSpot_remove_callback(CD(applet)->pspot, n, &ev);
	CORBA_exception_free(&ev);
	g_free(n);
}

static CustomAppletServant *
gnome_panel_applet_corba_init(AppletWidget *applet, const char *goad_id)
{
  PortableServer_POA poa;
  CustomAppletServant *applet_servant;
  CORBA_Environment ev;
  GNOME_Applet applet_obj;
  CORBA_ORB orb;
  CORBA_char *privcfg;
  CORBA_char *globcfg;
  static volatile int stop_here = 0;

  CORBA_exception_init(&ev);

  orb = gnome_CORBA_ORB();

  applet_servant = g_new0(CustomAppletServant, 1);
  applet_servant->servant.vepv = &vepv;

  POA_GNOME_Applet__init((POA_GNOME_Applet *)applet_servant, &ev);
  
  pg_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);

  applet_servant->poa = poa = (PortableServer_POA)
    CORBA_ORB_resolve_initial_references(orb, "RootPOA", &ev);

  PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(poa, &ev), &ev);
  pg_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);
  
  applet_servant->objid = PortableServer_POA_activate_object(poa, applet_servant,
							     &ev);
  pg_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);

  applet_servant->obj = applet_obj =
    PortableServer_POA_servant_to_reference(poa, applet_servant, &ev);
  pg_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);
  
  goad_server_register(CORBA_OBJECT_NIL, applet_obj, goad_id, "server", &ev);
  pg_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);

  if(!panel_client)
    panel_client =
      goad_server_activate_with_repo_id(NULL, "IDL:GNOME/Panel:1.0", 0,
					NULL);


  while(stop_here);

  applet_servant->pspot = GNOME_Panel_add_applet(panel_client, applet_obj,
						 (char *)goad_id,
						 &privcfg,&globcfg,
						 &applet_servant->winid, &ev);
  pg_return_val_if_fail(ev._major == CORBA_NO_EXCEPTION, NULL);
  
  if(privcfg && *privcfg)
	  applet->privcfgpath = g_strdup(privcfg);
  else
	  applet->privcfgpath = NULL;
  CORBA_free(privcfg);
  if(globcfg && *globcfg)
	  applet->globcfgpath = g_strdup(globcfg);
  else
	  applet->globcfgpath = NULL;
  CORBA_free(globcfg);

  CORBA_exception_free(&ev);

  return applet_servant;
}


GtkWidget *
applet_widget_new(const char *goad_id)
{
	AppletWidget *applet;
	int applet_id=-1;
	CustomAppletServant *corbadat;
	CORBA_unsigned_long wid;
	
	g_return_val_if_fail(goad_id != NULL,NULL);

	applet = APPLET_WIDGET (gtk_type_new (applet_widget_get_type ()));

	CD(applet) = corbadat = gnome_panel_applet_corba_init(applet,goad_id);

	g_return_val_if_fail(corbadat!=NULL,NULL);

	corbadat->appwidget = applet;

	gtk_plug_construct(GTK_PLUG(applet), corbadat->winid);
	
	corbadat->goad_id = g_strdup(goad_id);

	gtk_signal_connect(GTK_OBJECT(applet),"destroy",
			   GTK_SIGNAL_FUNC(applet_widget_destroy),
			   NULL);

	applet_count++;

	return GTK_WIDGET(applet);
}

int
applet_widget_get_applet_count()
{
	return applet_count;
}

/*button press event over the applet*/
static int
applet_bevent(GtkWidget *widget, GdkEventButton *bevent, GtkWidget *appw)
{
	GtkWidget *w;
	CORBA_Environment ev;
	int retval = FALSE;
	
	CORBA_exception_init(&ev);

	if(GNOME_Panel__get_in_drag(panel_client, &ev)) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget),
					     "button_press_event");
		GNOME_PanelSpot_drag_stop(CD(appw)->pspot, &ev);
		retval = TRUE;
	} else if(bevent->button == 2) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget),
					     "button_press_event");
		if((w = gtk_grab_get_current()))
			gtk_grab_remove(w);
		gdk_keyboard_ungrab(GDK_CURRENT_TIME);
		gdk_pointer_ungrab(GDK_CURRENT_TIME);
		gdk_flush();
		GNOME_PanelSpot_drag_start(CD(appw)->pspot, &ev);
		retval = TRUE;
	} else if(bevent->button == 3) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget),
					     "button_press_event");
		if((w = gtk_grab_get_current()))
			gtk_grab_remove(w);
		gdk_pointer_ungrab(GDK_CURRENT_TIME);
		gdk_keyboard_ungrab(GDK_CURRENT_TIME);
		gdk_flush();
		GNOME_PanelSpot_show_menu(CD(appw)->pspot, &ev);
		retval = TRUE;
	}

	CORBA_exception_free(&ev);

	return retval;
}

static void
bind_applet_events(GtkWidget *widget, gpointer data)
{
	if (!GTK_WIDGET_NO_WINDOW(widget)) {
		gtk_signal_connect(GTK_OBJECT(widget), "button_press_event",
				   (GtkSignalFunc) applet_bevent,
				   data);
	}
	
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, data);
}

void
applet_widget_add(AppletWidget *applet, GtkWidget *widget)
{
	GString *str;
	CORBA_Environment ev;

	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_WIDGET(widget));

	gtk_container_add(GTK_CONTAINER(applet),widget);

	CORBA_exception_init(&ev);
  
	GNOME_PanelSpot_register_us(CD(applet)->pspot, &ev);
	CORBA_exception_free(&ev);

	bind_applet_events(GTK_WIDGET(applet),applet);
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
	CORBA_Environment ev;
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	CORBA_exception_init(&ev);
	GNOME_PanelSpot__set_tooltip(CD(applet)->pspot, text?text:"", &ev);
	CORBA_exception_free(&ev);
}

/* Get the oprientation the applet should use */
GNOME_Panel_OrientType
applet_widget_get_panel_orient(AppletWidget *applet)
{
	CORBA_Environment ev;
	GNOME_Panel_OrientType r;
	g_return_val_if_fail(applet != NULL,ORIENT_UP);
	g_return_val_if_fail(IS_APPLET_WIDGET(applet), ORIENT_UP);
	
	CORBA_exception_init(&ev);
	r = GNOME_PanelSpot__get_parent_orient(CD(applet)->pspot, &ev);
	CORBA_exception_free(&ev);
	return r;
}

int	
applet_widget_init(const char *app_id,
		   const char *app_version,
		   int argc,
		   char **argv,
		   struct poptOption *options,
		   unsigned int flags,
		   poptContext *return_ctx)
{
	int ret = TRUE; /*bogus value, this should be if we succeded or not
			  or something*/
	CORBA_Environment ev;
	CORBA_ORB orb;

	die_on_last = TRUE;

	gnome_client_disable_master_connection ();
	CORBA_exception_init(&ev);
	orb = gnome_CORBA_init_with_popt_table(app_id, VERSION, &argc, argv,
					       options, flags, return_ctx,
					       GNORBA_INIT_SERVER_FUNC, &ev);

	CORBA_exception_free(&ev);

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
}

void
applet_widget_gtk_main_quit (void)
{
  gtk_main_quit();
}

void
applet_widget_panel_quit (void)
{
  CORBA_Environment ev;

  CORBA_exception_init(&ev);
  GNOME_Panel_quit(panel_client, &ev);
  CORBA_exception_free(&ev);
}

static void
server_applet_change_orient(CustomAppletServant *servant,
			    GNOME_Panel_OrientType orient,
			    CORBA_Environment *ev)
{
  AppletWidget *applet;

  applet = servant->appwidget;
  if(applet) {
    gtk_signal_emit(GTK_OBJECT(applet),
		    applet_widget_signals[CHANGE_ORIENT_SIGNAL],
		    (GNOME_Panel_OrientType)orient);
  }
}

static void
server_applet_do_callback(CustomAppletServant *servant,
			  CORBA_char * callback_name,
			  CORBA_Environment *ev)
{
  GSList *list;
  CallbackInfo *info;

  for(list = servant->callbacks;
      list!=NULL;list = g_slist_next (list)) {
    info = (CallbackInfo *)list->data;
    if(strcmp(info->name,(char *)callback_name)==0) {
      (*(info->func)) (servant->appwidget,
		       info->data);
      return;
    }
  }
}

static CORBA_boolean
server_applet_session_save(CustomAppletServant *servant,
			   CORBA_char * cfgpath,
			   CORBA_char * globcfgpath,
			   CORBA_Environment *ev)
{
	AppletWidget *applet;
	char *cfg = g_strdup(cfgpath);
	char *globcfg = g_strdup(globcfgpath);

	int return_val = FALSE;

	applet = servant->appwidget;
	gtk_signal_emit(GTK_OBJECT(applet),
			applet_widget_signals[SAVE_SESSION_SIGNAL],
			cfg, globcfg, &return_val);
	g_free(cfg);
	g_free(globcfg);

	/*return_val of true would mean that the applet handeled the
	  session saving itself, therefore we pass the reverse to the
	  corba function */

	return !return_val;
}

static void
server_applet_back_change(CustomAppletServant *servant,
			  GNOME_Panel_BackInfoType *backing,
			  CORBA_Environment *ev)
{
  GdkColor color, *cptr = NULL;
  char *pptr = NULL;
  AppletWidget *applet;

  switch(backing->_d) {
  case GNOME_Panel_BACK_COLOR:
    color.red = backing->_u.c.red;
    color.green = backing->_u.c.green;
    color.blue = backing->_u.c.blue;
    cptr = &color;
    break;
  case GNOME_Panel_BACK_PIXMAP:
    pptr = backing->_u.pmap;
    break;
  default:
  }

  applet = servant->appwidget;
  gtk_signal_emit(GTK_OBJECT(applet),
		  applet_widget_signals[BACK_CHANGE_SIGNAL],
		  (GNOME_Panel_BackType)backing->_d,
		  pptr,
		  cptr);
}

static void
server_applet_set_tooltips_state(CustomAppletServant *servant,
				 CORBA_boolean enabled,
				 CORBA_Environment *ev)
{
	if(enabled)
		gtk_tooltips_enable(applet_tooltips);
	else
		gtk_tooltips_disable(applet_tooltips);

	gtk_signal_emit(GTK_OBJECT(servant->appwidget),
			applet_widget_signals[TOOLTIP_STATE_SIGNAL],
			enabled);
}

static CORBA_char *
server_applet__get_goad_id(CustomAppletServant *servant,
			   CORBA_Environment *ev)
{
	return CORBA_string_dup(servant->goad_id);
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

/* Used by shlib applets */
CORBA_Object
applet_widget_corba_activate(GtkWidget *applet,
			     PortableServer_POA poa,
			     const char *goad_id,
			     const char **params,
			     gpointer *impl_ptr,
			     CORBA_Environment *ev)
{
  return CORBA_Object_duplicate(CD(applet)->obj, ev);
}

void
applet_widget_corba_deactivate(PortableServer_POA poa,
			       const char *goad_id,
			       gpointer impl_ptr,
			       CORBA_Environment *ev)
{
	/*FIXME: fill this in*/
}

typedef struct {
  POA_GNOME_GenericFactory servant;
  AppletFactoryActivator afunc;
  AppletFactoryQuerier qfunc;
  CORBA_Object fobj;
  PortableServer_ObjectId *objid;
} AppletFactory;

static CORBA_boolean
server_applet_factory_supports(AppletFactory *servant,
			       CORBA_char * obj_goad_id,
			       CORBA_Environment * ev)
{
  if(servant->qfunc)
    return servant->qfunc(obj_goad_id);

  g_message("No AppletFactoryQuerier to check on %s in panel applet",
	    obj_goad_id);

  return CORBA_FALSE;
}

static CORBA_Object
server_applet_factory_create_object(AppletFactory *servant,
				    CORBA_char * goad_id,
				    GNOME_stringlist * params,
				    CORBA_Environment * ev)
{
  GtkWidget *applet;

  applet = servant->afunc(goad_id, (const char **)params->_buffer, params->_length);

  g_return_val_if_fail(applet, CORBA_OBJECT_NIL);

  return CORBA_Object_duplicate(CD(applet)->obj, ev);
}

static POA_GNOME_GenericFactory__epv applet_factory_epv = {
  NULL,
  (gpointer)&server_applet_factory_supports,
  (gpointer)&server_applet_factory_create_object,  
};

static POA_GNOME_GenericFactory__vepv applet_factory_vepv = {
  &base_epv,
  &applet_factory_epv
};

void applet_factory_new(const char *goad_id, AppletFactoryQuerier qfunc,
			AppletFactoryActivator afunc)
{
  AppletFactory *f;
  CORBA_Environment ev;
  PortableServer_POA poa;

  g_return_if_fail(afunc);

  CORBA_exception_init(&ev);
  f = g_new0(AppletFactory, 1);
  f->servant.vepv = &applet_factory_vepv;
  f->afunc = afunc; f->qfunc = qfunc;
  POA_GNOME_GenericFactory__init((PortableServer_Servant)f, &ev);

  CORBA_exception_free(&ev);

  poa = (PortableServer_POA)
    CORBA_ORB_resolve_initial_references(gnome_CORBA_ORB(), "RootPOA", &ev);

  PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(poa, &ev), &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  f->objid = PortableServer_POA_activate_object(poa, f,
						&ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  f->fobj = PortableServer_POA_servant_to_reference(poa, f, &ev);

  goad_server_register(CORBA_OBJECT_NIL, f->fobj, goad_id, "server", &ev);
}
