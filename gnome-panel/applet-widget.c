#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <applet-widget.h>
#include <applet-lib.h>

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

static char *myinvoc= NULL;

static GtkTooltips *applet_tooltips=NULL;

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
	SESSION_SAVE_SIGNAL,
	BACK_CHANGE_SIGNAL,
	TOOLTIP_STATE_SIGNAL,
	LAST_SIGNAL
};

static int applet_widget_signals[LAST_SIGNAL] = {0,0,0,0,0};

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
	/*this one should be phased out*/
	applet_widget_signals[SESSION_SAVE_SIGNAL] =
		gtk_signal_new("session_save",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 session_save),
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
	class->session_save = NULL;
}

static void
wapplet_widget_init (AppletWidget *applet_widget)
{
	g_return_if_fail(applet_widget != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet_widget));

	applet_widget->applet_id = -1;
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
	g_free(applet->cfgpath);
	applet->privcfgpath = NULL;
	applet->globcfgpath = NULL;
	applet->cfgpath = NULL;
	gnome_panel_applet_cleanup(applet->applet_id);
	if(GTK_BIN(w)->child == NULL)
		gnome_panel_applet_abort_id(applet->applet_id);

	applet_count--;

	if(die_on_last && applet_count == 0)
		applet_corba_gtk_main_quit();
		/*gtk_exit(0);*/

	return FALSE;
}

void
applet_widget_remove_from_panel(AppletWidget *applet)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	gnome_panel_applet_remove_from_panel(applet->applet_id);
}

void
applet_widget_sync_config(AppletWidget *applet)
{
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	gnome_panel_sync_config(applet->applet_id);
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

	gnome_panel_applet_unregister_callback (applet->applet_id,name);
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
	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));

	gnome_panel_applet_unregister_callback_dir (applet->applet_id,name);
}


GtkWidget *
applet_widget_new_with_param(const char *param)
{
	AppletWidget *applet;
	char *result;
	char *privcfgpath;
	char *globcfgpath;
	guint32 winid;
	int applet_id;

	if(!param)
		param="";

	result = gnome_panel_applet_request_id(myinvoc, param,
					       do_multi?FALSE:TRUE,
					       &applet_id,
					       &privcfgpath, &globcfgpath,
					       &winid);
	if (result)
		g_error("Could not talk to the panel: %s\n", result);
	
	printf("got winid: %ld\n",winid);

	applet = APPLET_WIDGET (gtk_type_new (applet_widget_get_type ()));

	GTK_PLUG(applet)->socket_window = gdk_window_foreign_new (winid);
	GTK_PLUG(applet)->same_app = FALSE;

	applet->applet_id = applet_id;
	applet->privcfgpath = privcfgpath;
	applet->globcfgpath = globcfgpath;
	applet->cfgpath = g_copy_strings(privcfgpath,"dummy_section/",NULL);

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

void
applet_widget_add(AppletWidget *applet, GtkWidget *widget)
{
	char *result;

	g_return_if_fail(applet != NULL);
	g_return_if_fail(IS_APPLET_WIDGET(applet));
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_WIDGET(widget));

	gtk_container_add(GTK_CONTAINER(applet),widget);

	result = gnome_panel_applet_register(GTK_WIDGET(applet),
					     applet->applet_id);
	if (result)
		g_error("Could not talk to the Panel: %s\n", result);
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
		gnome_panel_applet_add_tooltip (applet->applet_id, text);
	else
		gnome_panel_applet_remove_tooltip (applet->applet_id);
}

/* Get the oprientation the applet should use */
PanelOrientType
applet_widget_get_panel_orient(AppletWidget *applet)
{
	g_return_val_if_fail(applet != NULL,ORIENT_UP);
	g_return_val_if_fail(IS_APPLET_WIDGET(applet),ORIENT_UP);

	return (PanelOrientType)gnome_panel_applet_get_panel_orient (applet->applet_id);
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

error_t
applet_widget_init(char *app_id,
		   struct argp *app_parser,
		   int argc,
		   char **argv,
		   unsigned int flags,
		   int *arg_index,
		   char *argv0,
		   int last_die,
		   int multi_applet,
		   AppletStartNewFunc new_func,
		   gpointer new_func_data)
{
	error_t ret;

	if(!argv0)
		g_error("Invalid argv0 argument!\n");

	if(argv0[0]!='#')
		myinvoc = get_full_path(argv0);
	else
		myinvoc = g_strdup(argv0);
	if(!myinvoc)
		g_error("Invalid argv0 argument!\n");

	do_multi = (multi_applet!=FALSE);
	start_new_func = new_func;
	start_new_func_data = new_func_data;
	die_on_last = last_die;

	panel_corba_register_arguments();

	gnome_client_disable_master_connection ();
	ret = gnome_init(app_id,app_parser,argc,argv,flags,arg_index);

	if (!gnome_panel_applet_init_corba())
		g_error("Could not communicate with the panel\n");
	
	applet_tooltips = gtk_tooltips_new();

	return ret;
}


void
applet_widget_gtk_main(void)
{
	applet_corba_gtk_main("IDL:GNOME/Applet:1.0");
}

void
_gnome_applet_change_orient(int applet_id, int orient)
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

int
_gnome_applet_session_save(int applet_id, const char *cfgpath, const char *globcfgpath)
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

		/*this should be ripped out when session_save is abandoned
		  for save_session*/
		{
			char *oldcfg = g_copy_strings(cfg,"dummy_section/",NULL);
			gtk_signal_emit(GTK_OBJECT(applet),
					applet_widget_signals[SESSION_SAVE_SIGNAL],
					oldcfg,globcfg,&return_val);
			g_free(oldcfg);
		}
	}
	g_free(cfg);
	g_free(globcfg);

	/*return_val of true would mean that the applet handeled the
	  session saving itself, therefore we pass the reverse to the
	  corba function*/
	return !return_val;
}

void
_gnome_applet_start_new_applet(const char *param)
{
	if(!do_multi)
		g_warning("This applet was not started as a multiapplet, yet "
			  "it recieved a start_new_applet, weird!");
	else if(start_new_func)
		(*start_new_func)(param,start_new_func_data);
}

void
_gnome_applet_back_change(int applet_id,
			  int back_type,
			  const char *pixmap,
			  GdkColor *color)
{
	AppletWidget *applet;

	applet = applet_widget_get_by_id(applet_id);
	if(applet) {
		gtk_signal_emit(GTK_OBJECT(applet),
				applet_widget_signals[BACK_CHANGE_SIGNAL],
				back_type,pixmap,color);
	}
}

void
_gnome_applet_tooltips_state(int enabled)
{
	GList *list;
	if(enabled)
		gtk_tooltips_enable(applet_tooltips);
	else
		gtk_tooltips_disable(applet_tooltips);

	for(list=applet_widgets;list!=NULL;list=g_list_next(list)) {
		gtk_signal_emit(GTK_OBJECT(list->data),
				applet_widget_signals[TOOLTIP_STATE_SIGNAL],
				enabled);
	}
}


/* convenience function for multi applets */
char *
make_param_string(int argc, char *argv[])
{
	char *s;
	int i;
	int len=0;

	for(i=1;i<argc;i++)
		len = strlen(argv[i])+1;

	if(len==0)
		return g_strdup("");

	s = g_malloc(len);
	s[0]= '\0';

	for(i=1;i<argc;i++) {
		strcat(s,argv[i]);
		if((i+1)<argc)
			strcat(s," ");
	}
	return s;
}
