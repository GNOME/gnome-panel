#include <iostream.h>
#include <mico/gtkmico.h>
#include <mico/naming.h>
#include <gnome.h>
#include "mico-glue.h"
#include "gnome-panel.h"
#include "panel.h"

/* This implements the server-side of the gnome-panel.idl
 * specification Currently there is no way to create new CORBA
 * "instances" of the panel, as there is only one panel running on the
 * screen.
 * */
   
char *first_ior_sent;

class Panel_impl : virtual public GNOME::Panel_skel {
public:
	CORBA::Short reparent_window_id (const char *ior,
		                         CORBA:: CORBA::ULong wid,
					 CORBA::Short panel,
					 CORBA::Short pos) {
		printf ("REPARENT!\n");
		printf ("applet registered with IOR: %s\n", ior);

		/* 
		 * Ultra bad hack: this is only used to illustrate how to
		 * call the applet.  
		 * the IOR should actually be stored somewhere in the panel applet
		 * structure
		 */
		first_ior_sent = g_strdup (ior);

		return ::reparent_window_id (wid,panel,pos);
	}
	CORBA::Short applet_get_panel (CORBA::Short id) {
		printf ("APPLET_GET_PANEL!\n");

		return ::applet_get_panel (id);
	}
	CORBA::Short applet_get_pos (CORBA::Short id) {
		printf ("APPLET_GET_POS!\n");

		return ::applet_get_pos (id);
	}
	void applet_drag_start (CORBA::Short id) {
		printf ("APPLET_DRAG_START!\n");

		::applet_drag_start (id);
	}
	void applet_drag_stop (CORBA::Short id) {
		printf ("APPLET_DRAG_STOP!\n");

		::applet_drag_stop (id);
	}
	void applet_remove_from_panel (CORBA::Short id) {
		printf ("APPLET_REMOVE_FROM_PANEL!\n");
		/*FIXME:  */
	}
};

CORBA::ORB_ptr orb_ptr;
CORBA::BOA_ptr boa_ptr;

void
panel_corba_gtk_main (int *argc, char ***argv, char *service_name)
{
	GNOME::Panel_ptr acc = new Panel_impl ();
	char hostname [4096];
	char *name;

	orb_ptr = CORBA::ORB_init (*argc, *argv, "mico-local-orb");
	boa_ptr = orb_ptr->BOA_init (*argc, *argv, "mico-local-boa");

	gethostname (hostname, sizeof (hostname));
	if (hostname [0] == 0)
		strcpy (hostname, "unknown-host");

	name = g_copy_strings ("/CORBA-servers/Panel-", hostname, 
			       "/DISPLAY-", getenv ("DISPLAY"), NULL);

	gnome_config_set_string (name, orb_ptr->object_to_string (acc));
	gnome_config_sync ();
	g_free (name);
	
	orb_ptr->dispatcher (new GtkDispatcher ());
	boa_ptr->impl_is_ready (CORBA::ImplementationDef::_nil());
}

void
ask_first_applet_to_print_a_message ()
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (first_ior_sent);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	applet->print_string ("1, 2, 3, testing");
}
