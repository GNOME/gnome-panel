#include <mico/gtkmico.h>
#include <mico/naming.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include "panel.h"
#include "gnome-panel.h"
#include "applet-lib.h"
#include "launcher-lib.h"
#include "applet-widget.h"
#include "panel-widget.h"
#include "mico-parse.h"

GNOME::Panel_var panel_client;

CORBA::ORB_ptr orb_ptr;
static CORBA::BOA_ptr boa_ptr;

/*every applet must implement these*/
BEGIN_GNOME_DECLS
void start_new_launcher(const char *path);
END_GNOME_DECLS

class Launcher_impl : virtual public GNOME::Launcher_skel {
public:
	void start_new_launcher (const char *path) {
		::start_new_launcher(path);
	}
};

void
launcher_corba_gtk_main (char *str)
{
	GNOME::Panel_ptr acc = new Launcher_impl ();
	char hostname [4096];
	char *name;

	panel_initialize_corba (&orb_ptr, &boa_ptr);

	gethostname (hostname, sizeof (hostname));
	if (hostname [0] == 0)
		strcpy (hostname, "unknown-host");

	name = g_copy_strings ("/CORBA-servers/Launcher-", hostname, 
			       "/DISPLAY-", getenv ("DISPLAY"), NULL);

	gnome_config_set_string (name, orb_ptr->object_to_string (acc));
	gnome_config_sync ();
	g_free (name);
	
	orb_ptr->dispatcher (new GtkDispatcher ());

	boa_ptr->impl_is_ready (CORBA::ImplementationDef::_nil());
}
