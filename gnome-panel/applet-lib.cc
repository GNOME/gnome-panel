#include <config.h>
#include <mico/gtkmico.h>
#include <mico/naming.h>
#include <fcntl.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include "panel.h"
#include "gnome-panel.h"
#include "applet-lib.h"
#include "applet-widget.h"
#include "panel-widget.h"
#include "mico-parse.h"

/*
 *
 * there is a lot of functionality in this file that is then slightly
 * redundant in the applet-widget ... this is because I wish to keep
 * applet-widget a clean C file, while applet-lib as the corba interface
 * after we have a C orb, we can do everything in applet-widget
 *
 */



#include "cookie.h"

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

GNOME::Panel_var panel_client;

typedef struct _CallbackInfo CallbackInfo;
struct _CallbackInfo {
	char *name;
	gint applet_id;
	AppletCallbackFunc func;
	gpointer data;
};

GList *applet_callbacks = NULL;

#define APPLET_ID_KEY "applet_id_key"
#define APPLET_WIDGET_KEY "applet_widget_key"

CORBA::ORB_ptr orb_ptr;
CORBA::BOA_ptr boa_ptr;

char *cookie;

BEGIN_GNOME_DECLS

void _gnome_applet_change_orient(int applet_id, int orient);
int _gnome_applet_session_save(int applet_id, const char *cfgpath,
			       const char *globcfgpath);
int _gnome_applet_start_new_applet(const char *params);

END_GNOME_DECLS

class Applet_impl : virtual public GNOME::Applet_skel {
public:
	void change_orient (const char *ccookie, CORBA::Short applet_id,
			    CORBA::Short orient) {
		CHECK_COOKIE ();
		::_gnome_applet_change_orient(applet_id,orient);
	}
	CORBA::Short session_save (const char *ccookie,
				   CORBA::Short applet_id,
			   	   const char *cfgpath,
			   	   const char *globcfgpath) {
		CHECK_COOKIE_V (0);
		return ::_gnome_applet_session_save(applet_id,cfgpath,
						    globcfgpath);
	}
        void do_callback (const char *ccookie,
			  CORBA::Short applet_id,
			  const char *callback_name)
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
	void start_new_applet (const char *ccookie,
			       const char *param)
	{
		CHECK_COOKIE ();
		::_gnome_applet_start_new_applet(param);
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

	cookie = gnome_config_private_get_string ("/panel/Secret/cookie=");

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

	cookie = gnome_config_private_get_string ("/panel/Secret/cookie=");

	CORBA::Object_var obj = orb_ptr->string_to_object (iior);
	
	panel_client = GNOME::Panel::_narrow (obj);
	return 1;
}

/*adds a callback to the callback hash*/
void
gnome_panel_applet_register_callback(int applet_id,
				     char *name,
				     char *menutext,
				     AppletCallbackFunc func,
				     gpointer data)
{
	CallbackInfo *info = g_new(CallbackInfo,1);
	GList *list;

	info->name = g_strdup(name);
	info->applet_id = applet_id;
	info->func = func;
	info->data = data;

	applet_callbacks = g_list_prepend(applet_callbacks,info);

	/*register the callback with the panel*/
	panel_client->applet_add_callback(cookie,
					  applet_id,name,menutext);
}

/*catch events relevant to the panel and notify the panel*/
static gint
applet_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	int ourid = PTOI(data);
	GdkEventButton *bevent;
	gint in_drag;

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			bevent = (GdkEventButton *) event;
			in_drag = panel_client->applet_in_drag(cookie);
			/*check to see if there is an applet being dragged*/
			if(bevent->button == 2 && !in_drag) {
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				gtk_grab_remove(widget);
				panel_client->applet_drag_start(cookie,ourid);
				return TRUE;
			} else if(in_drag) {
				panel_client->applet_drag_stop(cookie,ourid);
				return TRUE;
			} else if(bevent->button == 3) {
				gdk_pointer_ungrab(GDK_CURRENT_TIME);
				gtk_grab_remove(widget);
				panel_client->applet_show_menu(cookie, ourid);
				return TRUE;
			}
			break;
		case GDK_BUTTON_RELEASE:
			if(panel_client->applet_in_drag(cookie)) {
				panel_client->applet_drag_stop(cookie, ourid);
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
bind_top_applet_events(GtkWidget *widget, int applet_id)
{
	gtk_signal_connect(GTK_OBJECT(widget),
			   "event",
			   GTK_SIGNAL_FUNC(applet_event),
			   ITOP(applet_id));

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
	char *result;
	char *cfg = NULL;
	char *globcfg = NULL;
	CORBA::ULong wid;
	int i;

	/*this is the first call to panel so we'll do a loop and timeout
	  after 20 seconds if we don't find a panel*/
	*applet_id = -1;

	for(i=0;i<20;i++) {
		try {
			/*reserve a spot and get an id for this applet*/
			*applet_id = panel_client->applet_request_id(cookie,
								     path,
								     param,
								     dorestart,
								     cfg,
							             globcfg,
								     wid);
		} catch (...) {
			sleep(1);
			gnome_panel_applet_reinit_corba ();
			continue;
		}
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
gnome_panel_applet_register (GtkWidget *widget, int applet_id)
{
	char *result;
	static char *ior=NULL;

	/*the applet implementation, it's only created once*/
	static GNOME::Applet_ptr applet = new Applet_impl ();

	/* Now a way to find out the CORBA impl from the widget */
	gtk_object_set_data (GTK_OBJECT (widget), "CORBA_object", applet);

	ior = orb_ptr->object_to_string (applet);

	panel_client->applet_register(cookie, ior,applet_id);

	bind_top_applet_events(widget,applet_id);

	return 0;
}

char *
gnome_panel_applet_abort_id (gint applet_id)
{
	panel_client->applet_abort_id(cookie, applet_id);

	return 0;
}

char *
gnome_panel_applet_remove_from_panel (gint applet_id)
{
	panel_client->applet_remove_from_panel(cookie, applet_id);

	return 0;
}

char *
gnome_panel_applet_add_tooltip (gint applet_id, char *tooltip)
{
	panel_client->applet_add_tooltip(cookie, applet_id,tooltip);

	return 0;
}

char *
gnome_panel_applet_remove_tooltip (gint applet_id)
{
	panel_client->applet_remove_tooltip(cookie, applet_id);

	return 0;
}

/*id will return a unique id for this applet for the applet to identify
  itself as*/
char *
gnome_panel_applet_request_glob_cfg (char **globcfgpath)
{
	char *globcfg = NULL;

	g_return_val_if_fail(globcfgpath!=NULL,0);

	panel_client->applet_request_glob_cfg(cookie, globcfg);

	if(globcfg!= NULL) {
		*globcfgpath = g_strdup(globcfg);
		CORBA::string_free(globcfg);
	} else {
		*globcfgpath = NULL;
	}

	return 0;
}

void
gnome_panel_applet_cleanup(gint applet_id)
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
gnome_panel_quit (void)
{
	char *result;

	panel_client->quit (cookie);

	return 0;
}

void
applet_corba_gtk_main (char *str)
{
	boa_ptr->impl_is_ready (CORBA::ImplementationDef::_nil());
}
