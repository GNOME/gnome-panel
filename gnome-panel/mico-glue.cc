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
   
class Panel_impl : virtual public GNOME::Panel_skel {
public:
	CORBA::Short reparent_window_id (CORBA::ULong wid,
					 CORBA::Short panel,
					 CORBA::Short pos) {
		printf ("REPARENT!\n");

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
	void applet_moved_to (CORBA::Short id,
			      CORBA::Short x,
			      CORBA::Short y) {
		printf ("APPLET_MOVED_TO!\n");
	}
	void applet_remove_from_panel (CORBA::Short id) {
		printf ("APPLET_REMOVE_FROM_PANEL!\n");
	}
};

void
panel_corba_gtk_main (int *argc, char ***argv, char *service_name)
{
	CORBA::ORB_var orb = CORBA::ORB_init (*argc, *argv, "mico-local-orb");
	CORBA::BOA_var boa = orb->BOA_init (*argc, *argv, "mico-local-boa");
	GNOME::Panel_ptr acc = new Panel_impl ();
	char hostname [4096];
	char *name;

	gethostname (hostname, sizeof (hostname));
	if (hostname [0] == 0)
		strcpy (hostname, "unknown-host");

	name = g_copy_strings ("/CORBA-servers/Panel-", hostname, 
			       "/DISPLAY-", getenv ("DISPLAY"), NULL);

	gnome_config_set_string (name, orb->object_to_string (acc));
	gnome_config_sync ();
	g_free (name);
	
	orb->dispatcher (new GtkDispatcher ());
	boa->impl_is_ready (CORBA::ImplementationDef::_nil());
}

