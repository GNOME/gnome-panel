#include <mico/gtkmico.h>
#include <mico/naming.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include "panel.h"
#include "gnome-panel.h"
#include "applet-lib.h"
#include "panel-widget.h"
#include "mico-parse.h"
#include <fcntl.h>

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

GNOME::Panel_var panel_client;

/*this might be done better but I doubt there will be more then one
  drag at a time :) Blah blah */
static int currently_dragged_id = -1;

static GdkCursor *fleur_cursor=NULL;

typedef struct _CallbackInfo CallbackInfo;
struct _CallbackInfo {
	gint applet_id;
	AppletCallbackFunc func;
	gpointer data;
};

GHashTable *applet_callbacks=NULL;

#define APPLET_ID_KEY "applet_id_key"
#define APPLET_WIDGET_KEY "applet_widget_key"

CORBA::ORB_ptr orb_ptr;
CORBA::BOA_ptr boa_ptr;

/*every applet must implement these*/
BEGIN_GNOME_DECLS
void change_orient(int id, int orient);
void session_save(int id, const char *cfgpath, const char *globcfgpath);
void shutdown_applet(int id);
END_GNOME_DECLS

class Applet_impl : virtual public GNOME::Applet_skel {
	GtkWidget *the_widget;
public:
	Applet_impl (GtkWidget *widget) { the_widget = widget; };
	void change_orient (CORBA::Short id, CORBA::Short orient) {
		::change_orient(id,orient);
	}
	void session_save (CORBA::Short id,
			   const char *cfgpath,
			   const char *globcfgpath) {
		::session_save(id,cfgpath,globcfgpath);
	}
	void shutdown_applet (CORBA::Short id) {
		::shutdown_applet(id);
	}
        void do_callback (CORBA::Short id,
			  const char *callback_name)
        {
		GList *list;

		if(!applet_callbacks)
			return;

		list = (GList *)g_hash_table_lookup(applet_callbacks,
						    (char *)callback_name);

		for(;list!=NULL;list = g_list_next(list)) {
			CallbackInfo *info = (CallbackInfo *)list->data;
			if(info->applet_id == id) {
				(*(info->func))(id,info->data);
				return;
			}
		}
	}
};

int
gnome_panel_applet_init_corba (void)
{
	char *name;
	char *iior;
	char hostname [1024];
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

	panel_initialize_corba (&orb_ptr, &boa_ptr);

	orb_ptr->dispatcher (new GtkDispatcher ());

	CORBA::Object_var obj = orb_ptr->string_to_object (iior);
	
	panel_client = GNOME::Panel::_narrow (obj);
	return 1;
}

/*reread the panel's address*/
int
gnome_panel_applet_reinit_corba (void)
{
	char *name;
	char *iior;
	char hostname [1024];
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

	CORBA::Object_var obj = orb_ptr->string_to_object (iior);
	
	panel_client = GNOME::Panel::_narrow (obj);
	return 1;
}

/*adds a callback to the callback hash*/
void
gnome_panel_applet_register_callback(int id,
				     char *name,
				     char *menutext,
				     AppletCallbackFunc func,
				     gpointer data)
{
	CallbackInfo *info = g_new(CallbackInfo,1);
	GList *list;

	if(!applet_callbacks)
		 applet_callbacks = g_hash_table_new (g_str_hash, g_str_equal);

	info->applet_id = id;
	info->func = func;
	info->data = data;

	list = (GList *)g_hash_table_lookup(applet_callbacks,name);

	if(list)
		g_hash_table_remove(applet_callbacks,name);
	list = g_list_prepend(list,info);
	g_hash_table_insert(applet_callbacks,name,list);

	/*register the callback with the panel*/
	panel_client->applet_add_callback(id,name,menutext);
}

/*catch events relevant to the panel and notify the panel*/
static gint
applet_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	int ourid = (int)data;
	GdkEventButton *bevent;

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;
			if(bevent->button == 2 && currently_dragged_id==-1) {
				panel_client->applet_drag_start(ourid);
				currently_dragged_id = ourid;
				move_grab_add(widget);
				return TRUE;
			} else if(currently_dragged_id > -1) {
				panel_client->applet_drag_stop(ourid);
				currently_dragged_id = -1;
				move_grab_remove(widget);
				return TRUE;
			} else if(bevent->button == 3) {
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				gtk_grab_remove(widget);
				panel_client->applet_show_menu(ourid);
				return TRUE;
			}
			break;
		case GDK_BUTTON_RELEASE:
			if(currently_dragged_id > -1) {
				panel_client->applet_drag_stop(ourid);
				currently_dragged_id = -1;
				move_grab_remove(widget);
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

static gint
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
bind_top_applet_events(GtkWidget *widget, int id)
{
	gtk_signal_connect(GTK_OBJECT(widget),
			   "event",
			   GTK_SIGNAL_FUNC(applet_event),
			   (gpointer)id);

	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach (GTK_CONTAINER (widget),
				       bind_applet_events, NULL);
}

/*id will return a unique id for this applet for the applet to identify
  itself as*/
char *
gnome_panel_applet_request_id (char *path,
			       int *id,
			       char **cfgpath,
			       char **globcfgpath,
			       guint32 *winid)
{
	char *result;
	char *cfg = NULL;
	char *globcfg = NULL;
	guint32 wid;
	int i;

	/*this is the first call to panel so we'll do a loop and timeout
	  after 20 seconds if we don't find a panel*/
	*id = -1;

	for(i=0;i<20;i++) {
		try {
			/*reserve a spot and get an id for this applet*/
			*id = panel_client->applet_request_id(path,cfg,
							      globcfg,wid);
		} catch (...) {
			sleep(1);
			gnome_panel_applet_reinit_corba ();
			continue;
		}
		break;
	}
	/*if the request_id never completed*/
	if(*id == -1)
		return g_strdup("Can't talk to a panel\n");

	if(winid)
		*winid = wid;

	if(cfgpath==NULL) {
		CORBA::string_free(cfg);
	} else if(cfg != NULL) {
		*cfgpath = g_strdup(cfg);
		CORBA::string_free(cfg);
	} else {
		*cfgpath = NULL;
	}
	if(globcfgpath==NULL) {
		CORBA::string_free(globcfg);
	} else if(globcfg != NULL) {
		*globcfgpath = g_strdup(globcfg);
		CORBA::string_free(globcfg);
	} else {
		*globcfgpath = NULL;
	}

	return 0;
}


/*this function will register the ior with the panel so it can call us*/
char *
gnome_panel_applet_register (GtkWidget *widget, int id)
{
	char *result;
	char *ior;

	/* Create an applet object, I do pass the widget parameter to the
	 * constructor object to have a way of sort out to which object
	 * implementation the panel is talking to us about (ie, if an applet
	 * can implement various instances of some object, like say a bunch
	 * of "Swallow" applets 
	 */
	GNOME::Applet_ptr applet = new Applet_impl (widget);

	/* Now a way to find out the CORBA impl from the widget */
	gtk_object_set_data (GTK_OBJECT (widget), "CORBA_object", applet);

	ior = orb_ptr->object_to_string (applet);

	panel_client->applet_register(ior,id);

	bind_top_applet_events(widget,id);

	return 0;
}

char *
gnome_panel_applet_abort_id (int id)
{
	panel_client->applet_abort_id(id);

	return 0;
}

/*id will return a unique id for this applet for the applet to identify
  itself as*/
char *
gnome_panel_applet_request_glob_cfg (char **globcfgpath)
{
	char *globcfg = NULL;

	g_return_val_if_fail(globcfgpath!=NULL,0);

	panel_client->applet_request_glob_cfg(globcfg);

	if(globcfg!= NULL) {
		*globcfgpath = g_strdup(globcfg);
		CORBA::string_free(globcfg);
	} else {
		*globcfgpath = NULL;
	}

	return 0;
}


char *
gnome_panel_quit (void)
{
	char *result;

	panel_client->quit ();

	return 0;
}

void
applet_corba_gtk_main (char *str)
{
	boa_ptr->impl_is_ready (CORBA::ImplementationDef::_nil());
}

void
move_grab_add (GtkWidget *applet)
{
	if(!fleur_cursor)
		fleur_cursor = gdk_cursor_new(GDK_FLEUR);

 	gtk_grab_add(applet);
	gdk_pointer_grab(applet->window,
			 TRUE,
			 (enum GdkEventMask)APPLET_EVENT_MASK,
			 NULL,
			 fleur_cursor,
			 GDK_CURRENT_TIME);
}

void
move_grab_remove (GtkWidget *applet)
{
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gtk_grab_remove(applet);
}
