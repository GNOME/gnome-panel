#include <config.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <status-docklet.h>
#include <bonobo-activation/bonobo-activation.h>

#include "GNOME_Panel.h"

#define pg_return_if_fail(x) if(!(x)) { g_print("type = %d exid = %s\n", ev._major, ev._repo_id); return; }
#define pg_return_val_if_fail(x,y) if(!(x)) { g_print("type = %d exid = %s\n", ev._major, ev._repo_id); return y;}

static void status_docklet_class_init	 (StatusDockletClass *klass);
static void status_docklet_instance_init (StatusDocklet      *status_docklet);
static void status_docklet_destroy	 (GtkObject          *o);
static void status_docklet_build_plug	 (StatusDocklet      *docklet,
					  GtkWidget          *plug);


typedef void (*BuildSignal) (GtkObject * object,
			     GtkWidget * wid,
			     gpointer data);

static GtkObjectClass *status_docklet_parent_class;

GType
status_docklet_get_type (void)
{
	static GType object_type= 0;

	if (status_docklet_type == 0) {
		static const GTypeInfo object_info = {
                    sizeof (StatusDockletClass),
                    (GBaseInitFunc)         NULL,
                    (GBaseFinalizeFunc)     NULL,
                    (GClassInitFunc)        status_docklet_class_init,
                    NULL,                   /* class_finalize */
                    NULL,                   /* class_data */
                    sizeof (StatusDocklet),
                    0,                      /* n_preallocs */
                    (GInstanceInitFunc)     status_docklet_instance_init
		};

		object_type = g_type_register_static (GTK_TYPE_OBJECT, "StatusDocklet", &object_info, 0);
		status_docklet_parent_class = g_type_class_ref (GTK_TYPE_OBJECT);
	}

	return object_type;
}

enum {
	BUILD_PLUG_SIGNAL,
	LAST_SIGNAL
};

static guint status_docklet_signals[LAST_SIGNAL] = {0};

static void
status_docklet_class_init (StatusDockletClass *class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;

	object_class =  G_OBJECT_CLASS (class);
	gtk_object_class = GTK_OBJECT_CLASS (class);

	status_docklet_signals[BUILD_PLUG_SIGNAL] =
                g_signal_new ("build_plug",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (StatusDockletClass, build_plug),
                              NULL,
                              NULL, 
                              panel_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER)		

	class->build_plug = status_docklet_build_plug;
	gtk_object_class->destroy = status_docklet_destroy;
}

static void
status_docklet_init (StatusDocklet *docklet)
{
	g_return_if_fail(docklet != NULL);
	g_return_if_fail(IS_STATUS_DOCKLET(docklet));
	
	docklet->timeout_handle = -1;
	docklet->sspot = CORBA_OBJECT_NIL;
}

static void
status_docklet_destroy(GtkObject *o)
{
	StatusDocklet *docklet;

	g_return_if_fail(o != NULL);
	g_return_if_fail(IS_STATUS_DOCKLET(o));
	
	docklet = STATUS_DOCKLET(o);
	
	if(docklet->timeout_handle != -1)
		gtk_timeout_remove(docklet->timeout_handle);
	
	/*if we do have a plug, set the "status_docklet" data to NULL,
	  so that any timeout on it fails*/
	if(docklet->plug)
		gtk_object_set_data(GTK_OBJECT(docklet->plug),"status_docklet",NULL);
	
	/*this will actually kill the status spot, including destroying the
	  plug for us, so we don't really want to destroy it if we can just
	  do this*/
	if(docklet->sspot != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init(&ev);
		GNOME_StatusSpot_remove(docklet->sspot, &ev);
		CORBA_Object_release(docklet->sspot, &ev);
		CORBA_exception_free(&ev);
	}
	
	if (GTK_OBJECT_CLASS (status_docklet_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (status_docklet_parent_class)->destroy) (o);
}

static void
status_docklet_build_plug(StatusDocklet *docklet, GtkWidget *plug)
{
	g_return_if_fail(docklet != NULL);
	g_return_if_fail(IS_STATUS_DOCKLET(docklet));

	docklet->plug = plug;
}

/**
 * status_docklet_new:
 *
 * Description:  Creates a new status docklet object with the default
 * parameters.  By default the docklet object will try to contact a panel
 * %STATUS_DOCKLET_DEFAULT_RETRIES times (20).  It will try to find a
 * panel every 15 seconds.  You need to bind the build_plug signal
 * in which you build your own widget and add it to the provided container.
 * By default the docklet object will handle a panel restart, in which case
 * your widget will be destroyed and when the panel is contacted again the
 * build_plug signal will be emitted again.  You also must call the
 * #status_docklet_run function after you bind the build_plug signal.
 *
 * Returns: new status docklet object.
 **/
GtkObject*
status_docklet_new(void)
{
	return status_docklet_new_full(STATUS_DOCKLET_DEFAULT_RETRIES,TRUE);
}

static void
plug_destroyed(GtkWidget *plug, gpointer data)
{
	StatusDocklet *docklet = gtk_object_get_data(GTK_OBJECT(plug),"status_docklet");
	if(!docklet) return;

	docklet->plug = NULL;

	if(docklet->sspot != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;
		CORBA_exception_init(&ev);
		CORBA_Object_release(docklet->sspot, &ev);
		docklet->sspot = CORBA_OBJECT_NIL;
		CORBA_exception_free(&ev);
	}
	
	if(docklet->handle_restarts) {
		docklet->tries = 0;
		status_docklet_run(docklet);
	}
}

static gboolean
try_getting_plug(StatusDocklet *docklet)
{
	CORBA_Environment ev;
	GNOME_Panel panel_client = CORBA_OBJECT_NIL;
	GNOME_StatusSpot spot = CORBA_OBJECT_NIL;
	CORBA_unsigned_long wid = 0;
	
	/*XXX: can these two next cases actually happen???*/

	/*huh? what's going on here, just smash this and let's do it over*/
	if(docklet->plug != NULL) {
		gtk_object_set_data(GTK_OBJECT(docklet->plug), "status_docklet", NULL);
		gtk_widget_destroy(docklet->plug);
		docklet->plug = NULL;
	}

	/*huh? yet again, something is terribly wrong*/
	if(docklet->sspot != CORBA_OBJECT_NIL) {
		CORBA_exception_init(&ev);
		CORBA_Object_release(docklet->sspot, &ev);
		docklet->sspot = CORBA_OBJECT_NIL;
		CORBA_exception_free(&ev);
	}

	panel_client =
		goad_server_activate_with_repo_id(NULL,
						  "IDL:GNOME/Panel:1.0",
						  GOAD_ACTIVATE_EXISTING_ONLY,
						  NULL);
	
	if(panel_client == NULL)
		return FALSE;

	CORBA_exception_init(&ev);
	spot = GNOME_Panel_add_status(panel_client, &wid, &ev);
	/*something must have gone wrong*/
	if(ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free(&ev);
		return FALSE;
	}

	/*we are probably inhibited from adding because of panel quitting*/
	if(wid == 0) {
		if(spot != CORBA_OBJECT_NIL)
			CORBA_Object_release(spot, &ev);
		CORBA_exception_free(&ev);
		return FALSE;
	}

	docklet->sspot = spot;
	docklet->plug = gtk_plug_new(wid);

	/*something must have gone terribly wrong*/
	if(docklet->plug == NULL) {
		CORBA_Object_release(spot, &ev);
		docklet->sspot = CORBA_OBJECT_NIL;
		CORBA_exception_free(&ev);
		return FALSE;
	}

	CORBA_exception_free(&ev);

	gtk_object_set_data(GTK_OBJECT(docklet->plug), "status_docklet", docklet);
	gtk_signal_connect(GTK_OBJECT(docklet->plug), "destroy",
			   GTK_SIGNAL_FUNC(plug_destroyed),
			   NULL);
	gtk_widget_show(docklet->plug);

	gtk_signal_emit(GTK_OBJECT(docklet),
			status_docklet_signals[BUILD_PLUG_SIGNAL],
			docklet->plug);
	
	return TRUE;
}

/**
 * status_docklet_new_full:
 * @maximum_retries:  Maximum number of times to try to contact panel
 * @handle_restarts:  If you handle panel restarts
 *
 * Description:  Creates a neew status docklet object with the specified
 * parameters.  See the description of #status_docklet_new for details.
 *
 * Returns:  a new docklet object
 **/
GtkObject*
status_docklet_new_full(int maximum_retries, gboolean handle_restarts)
{
	StatusDocklet *docklet;

	docklet = STATUS_DOCKLET(gtk_type_new(status_docklet_get_type()));
	
	docklet->maximum_retries = maximum_retries;
	docklet->handle_restarts = handle_restarts;
	
	return GTK_OBJECT(docklet);
}

static gint
try_timeout(gpointer data)
{
	StatusDocklet *docklet = data;

	g_return_val_if_fail(docklet != NULL,FALSE);
	g_return_val_if_fail(IS_STATUS_DOCKLET(docklet),FALSE);

	docklet->timeout_handle = -1;
	
	/*lets try it again*/
	status_docklet_run(docklet);
	
	return FALSE;
}

/**
 * status_docklet_run:
 * @docklet: #StatusDocklet to run
 *
 * Description:  Search for the panel and add the plug if it finds it.  This
 * function is also called internally from the timeout.  If called externally
 * more times, a panel lookup will be forced and one try will be wasted.  You
 * need to call this function at least once after binding the build_plug
 * signal to tell the status docklet to start looking for the panel.
 * If the status docklet handles restarts you don't have to call this
 * function ever again.
 **/
void
status_docklet_run(StatusDocklet *docklet)
{
	g_return_if_fail(docklet != NULL);
	g_return_if_fail(IS_STATUS_DOCKLET(docklet));

	if(docklet->timeout_handle != -1)
		gtk_timeout_remove(docklet->timeout_handle);

	if(!try_getting_plug(docklet)) {
		docklet->tries ++;
		if(docklet->tries < docklet->maximum_retries)
			docklet->timeout_handle = gtk_timeout_add(STATUS_DOCKLET_RETRY_EVERY*1000,
								  try_timeout,docklet);
		else
			docklet->tries = 0;
	} else {
		docklet->tries = 0;
	}
}
