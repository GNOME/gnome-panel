#include <gtk/gtk.h>
#include <gnome.h>
#include <applet-widget.h>
#include <applet-lib.h>

static void applet_widget_class_init	(AppletWidgetClass *klass);
static void wapplet_widget_init		(AppletWidget      *applet_widget);

typedef void (*AppletWidgetOrientSignal) (GtkObject * object,
					  PanelOrientType orient,
					  gpointer data);

typedef gint (*AppletWidgetSaveSignal) (GtkObject * object,
				        char *cfgpath,
				        char *globcfgpath,
				        gpointer data);

static GList *applet_widgets = NULL;
static gint applet_count = 0;

static gint do_multi = FALSE;
static gint die_on_last = TRUE;

static GtkPlugClass *parent_class;

static AppletStartNewFunc start_new_func=NULL;
static gpointer start_new_func_data=NULL;

static gchar *myinvoc= NULL;

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
	SESSION_SAVE_SIGNAL,
	LAST_SIGNAL
};

static gint applet_widget_signals[LAST_SIGNAL] = {0,0};

static void
gtk_applet_widget_marshal_signal_orient (GtkObject * object,
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
gtk_applet_widget_marshal_signal_save (GtkObject * object,
				       GtkSignalFunc func,
				       gpointer func_data,
				       GtkArg * args)
{
	AppletWidgetSaveSignal rfunc;
	gint *retval;

	rfunc = (AppletWidgetSaveSignal) func;

	retval = GTK_RETLOC_BOOL(args[2]);

	*retval = (*rfunc) (object, GTK_VALUE_STRING (args[0]),
		  	    GTK_VALUE_STRING (args[1]),
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
			       gtk_applet_widget_marshal_signal_orient,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);
	applet_widget_signals[SESSION_SAVE_SIGNAL] =
		gtk_signal_new("session_save",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(AppletWidgetClass,
			       			 session_save),
			       gtk_applet_widget_marshal_signal_save,
			       GTK_TYPE_BOOL,
			       2,
			       GTK_TYPE_STRING,
			       GTK_TYPE_STRING);

	gtk_object_class_add_signals(object_class,applet_widget_signals,
				     LAST_SIGNAL);

	class->change_orient = NULL;
	class->session_save = NULL;
}

static void
wapplet_widget_init (AppletWidget *applet_widget)
{
	applet_widget->applet_id = -1;
}

static gint
applet_widget_destroy(GtkWidget *w, gpointer data)
{
	AppletWidget *applet = APPLET_WIDGET(w);
	if(!applet->cfgpath)
		return FALSE;
	g_free(applet->cfgpath);
	g_free(applet->globcfgpath);
	applet->cfgpath = NULL;
	applet->globcfgpath = NULL;
	gnome_panel_applet_cleanup(applet->applet_id);
	if(GTK_BIN(w)->child == NULL)
		gnome_panel_applet_abort_id(applet->applet_id);

	applet_count--;

	if(die_on_last && applet_count == 0)
		gtk_exit(0);

	return FALSE;
}

void
applet_widget_remove_from_panel(AppletWidget *applet)
{
	gnome_panel_applet_remove_from_panel(applet->applet_id);
}

void
applet_widget_register_callback(AppletWidget *applet,
				char *name,
				char *menutext,
				AppletCallbackFunc func,
				gpointer data)
{
	gnome_panel_applet_register_callback (applet->applet_id,name,
					      menutext,func,data);
}

GtkWidget *
applet_widget_new_with_param(const gchar *param)
{
	AppletWidget *applet;
	char *result;
	char *cfgpath;
	char *globcfgpath;
	guint32 winid;
	gint applet_id;

	if(!param)
		param="";

	result = gnome_panel_applet_request_id(myinvoc, param,
					       do_multi?FALSE:TRUE,
					       &applet_id,
					       &cfgpath, &globcfgpath,
					       &winid);
	if (result)
		g_error("Could not talk to the panel: %s\n", result);

	applet = APPLET_WIDGET (gtk_type_new (applet_widget_get_type ()));

	GTK_PLUG(applet)->socket_window = gdk_window_foreign_new (winid);
	GTK_PLUG(applet)->same_app = FALSE;

	applet->applet_id = applet_id;
	applet->cfgpath = cfgpath;
	applet->globcfgpath = globcfgpath;

	gtk_signal_connect(GTK_OBJECT(applet),"destroy",
			   GTK_SIGNAL_FUNC(applet_widget_destroy),
			   NULL);

	applet_widgets = g_list_prepend(applet_widgets,applet);

	applet_count++;

	return GTK_WIDGET(applet);
}

gint
applet_widget_get_applet_count()
{
	return applet_count;
}

void
applet_widget_add(AppletWidget *applet, GtkWidget *widget)
{
	char *result;

	gtk_container_add(GTK_CONTAINER(applet),widget);

	result = gnome_panel_applet_register(GTK_WIDGET(applet),
					     applet->applet_id);
	if (result)
		g_error("Could not talk to the Panel: %s\n", result);
}

void
applet_widget_set_tooltip(AppletWidget *applet, gchar *text)
{
	if(text)
		gnome_panel_applet_add_tooltip (applet->applet_id, text);
	else
		gnome_panel_applet_remove_tooltip (applet->applet_id);
}

PanelOrientType
applet_widget_get_panel_orient(AppletWidget *applet)
{
	/*FIXME: implement*/
	return (PanelOrientType)0;
}

AppletWidget*
applet_widget_get_by_id(gint applet_id)
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
		   gchar *argv0,
		   gint last_die,
		   gint multi_applet,
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

	ret = gnome_init(app_id,app_parser,argc,argv,flags,arg_index);

	if (!gnome_panel_applet_init_corba())
		g_error("Could not communicate with the panel\n");

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
	gint return_val = FALSE;

	applet = applet_widget_get_by_id(applet_id);
	if(applet) {
		gtk_signal_emit(GTK_OBJECT(applet),
				applet_widget_signals[SESSION_SAVE_SIGNAL],
				cfg,globcfg,&return_val);
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

/* convenience function for multi applets */
gchar *
make_param_string(gint argc, char *argv[])
{
	gchar *s;
	gint i;
	gint len=0;

	for(i=1;i<argc;i++)
		len = strlen(argv[i])+1;

	s = g_malloc(len);
	s[0]= '\0';

	for(i=1;i<argc;i++) {
		strcat(s,argv[i]);
		if((i+1)<argc)
			strcat(s," ");
	}
	return s;
}
