#include <mico/gtkmico.h>
#include <mico/naming.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include "panel.h"
#include "gnome-panel.h"
#include "applet-lib.h"
#include "applet-widget.h"
#include "panel-widget.h"

GNOME::Panel_var panel_client;

/*this might be done better but I doubt there will be more then one
  drag at a time :) Blah blah */
static int currently_dragged_id = -1;

static GtkWidget *applet_menu;
static GtkWidget *applet_menu_prop_separator;
static GtkWidget *applet_menu_prop_item;

#define APPLET_ID_KEY "applet_id_key"
#define APPLET_WIDGET_KEY "applet_widget_key"

static CORBA::ORB_ptr orb_ptr;
static CORBA::BOA_ptr boa_ptr;

/*every applet must implement these*/
BEGIN_GNOME_DECLS
void change_orient(int id, int orient);
void session_save(int id, int panel, int pos);
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
			   CORBA::Short panel,
			   CORBA::Short pos) {
		::session_save(id,panel,pos);
	}
	void shutdown_applet (CORBA::Short id) {
		::shutdown_applet(id);
	}
        void do_callback (CORBA::Short id,
			  const char *callback_name)
        {
	        ::do_callback(id, callback_name);
	}
};

int
gnome_panel_applet_init_corba (int *argc, char ***argv)
{
	char *binder_address;
	char *name;
	char *iior;
	char hostname [1024];
	
	gethostname (hostname, sizeof (hostname));
	if (hostname [0] == 0)
		strcpy (hostname, "unknown-host");

	name = g_copy_strings ("/CORBA-servers/Panel-", hostname, 
			       "/DISPLAY-", getenv ("DISPLAY"), NULL);

	iior = gnome_config_get_string (name);
	g_free (name);
	
	if (!iior)
		return 0;

	panel_initialize_corba (&orb_ptr, &boa_ptr)

	orb_ptr->dispatcher (new GtkDispatcher ());

	CORBA::Object_var obj = orb_ptr->string_to_object (iior);
	
	panel_client = GNOME::Panel::_narrow (obj);
	return 1;
}

static void
move_applet_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget *applet_menu = GTK_WIDGET(data);
	int ourid = (int)gtk_object_get_data(GTK_OBJECT(applet_menu),
					     APPLET_ID_KEY);
	GtkWidget *applet = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(
							      applet_menu),
							     APPLET_WIDGET_KEY);

	if(currently_dragged_id==-1) {
		panel_client->applet_drag_start(ourid);
		currently_dragged_id = ourid;
		applet_widget_move_grab_add(APPLET_WIDGET(applet));
	}
}


static void
remove_applet_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget *applet_menu = GTK_WIDGET(data);
	int ourid = (int)gtk_object_get_data(GTK_OBJECT(applet_menu),
					     APPLET_ID_KEY);

	panel_client->applet_remove_from_panel(ourid);
}


static void
applet_properties_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget *applet_menu = GTK_WIDGET(data);
	int ourid = (int)gtk_object_get_data(GTK_OBJECT(applet_menu),
					     APPLET_ID_KEY);

	puts("Properties ... we don't need no stinkin' properties ...!");
}

static void
create_applet_menu(void)
{
	GtkWidget *menuitem;

	applet_menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("Remove from panel"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) remove_applet_callback,
			   applet_menu);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);

	
	menuitem = gtk_menu_item_new_with_label(_("Move applet"));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) move_applet_callback,
			   applet_menu);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	
	menuitem = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	applet_menu_prop_separator = menuitem;

	menuitem = gtk_menu_item_new_with_label(_("Applet properties..."));
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) applet_properties_callback,
			   applet_menu);
	gtk_menu_append(GTK_MENU(applet_menu), menuitem);
	gtk_widget_show(menuitem);
	applet_menu_prop_item = menuitem;
}


static void
show_applet_menu(GtkWidget *widget, int id)
{
	/*FIXME: only if applet has properties*/
	if (1) {
		gtk_widget_show(applet_menu_prop_separator);
		gtk_widget_show(applet_menu_prop_item);
	} else {
		gtk_widget_hide(applet_menu_prop_separator);
		gtk_widget_hide(applet_menu_prop_item);
	}

	gtk_object_set_data(GTK_OBJECT(applet_menu), APPLET_ID_KEY,
			    (gpointer)id);
	gtk_object_set_data(GTK_OBJECT(applet_menu), APPLET_WIDGET_KEY,
			    (gpointer)widget);

	gtk_menu_popup(GTK_MENU(applet_menu), NULL, NULL, NULL, NULL, 0/*3*/, time(NULL));
	/*FIXME: make it pop-up on some title bar of the applet menu or
	  somehow avoid pressing remove applet being under the cursor!*/
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
				applet_widget_move_grab_add(
					APPLET_WIDGET(widget));
				return TRUE;
			} else if(currently_dragged_id > -1) {
				panel_client->applet_drag_stop(ourid);
				currently_dragged_id = -1;
				applet_widget_move_grab_remove(
					APPLET_WIDGET(widget));
				return TRUE;
			} else if(bevent->button == 3) {
				if(!applet_menu)
					create_applet_menu();
				show_applet_menu(widget,ourid);
				return TRUE;
			}
			break;
		case GDK_BUTTON_RELEASE:
			if(currently_dragged_id > -1) {
				panel_client->applet_drag_stop(ourid);
				currently_dragged_id = -1;
				applet_widget_move_grab_remove(
					APPLET_WIDGET(widget));
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
gnome_panel_prepare_and_transfer (GtkWidget *widget, char *path, int *id,
				  int panel, int pos)
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

	gtk_widget_realize(widget);
	gtk_widget_show(widget);
	
	printf ("Transfiriendo: %d\n", GDK_WINDOW_XWINDOW (widget->window));

	/*reserve a spot and get an id for this applet*/
	*id = panel_client->reserve_applet_spot(ior,path,panel,pos);

	/*reparent the window*/
	panel_client->reparent_window_id (GDK_WINDOW_XWINDOW (widget->window),
					  *id);
	bind_top_applet_events(widget,*id);


	printf ("Transferido\n");
	return 0;
}

void
applet_corba_gtk_main (char *str)
{
	boa_ptr->impl_is_ready (CORBA::ImplementationDef::_nil());
}
