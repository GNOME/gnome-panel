#include <config.h>
#include <mico/gtkmico.h>
#include <mico/naming.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include "panel.h"
#include "gnome-panel.h"
#include "applet-lib.h"
#include "launcher-lib.h"
#include "panel-widget.h"
#include "mico-parse.h"

#include "cookie.h"

extern CORBA::ORB_ptr orb_ptr;
extern CORBA::BOA_ptr boa_ptr;

char *cookie;

/*every launcher must implement these*/
BEGIN_GNOME_DECLS
void start_new_launcher(const char *path);
void restart_all_launchers(void);
END_GNOME_DECLS

class Launcher_impl : virtual public GNOME::Launcher_skel {
public:
	void start_new_launcher (const char *ccookie, const char *path) {
		/*avoid races*/
		cookie = gnome_config_private_get_string (
			"/panel/Secret/cookie=");
		CHECK_COOKIE ();
		::start_new_launcher(path);
	}
	void restart_all_launchers (const char *ccookie) {
		/*avoid races*/
		cookie = gnome_config_private_get_string (
			"/panel/Secret/cookie=");
		CHECK_COOKIE ();
		::restart_all_launchers ();
	}
};

void
launcher_corba_gtk_main (char *str)
{
	GNOME::Launcher_ptr acc = new Launcher_impl ();
	char hostname [4096];
	char *name;

	gethostname (hostname, sizeof (hostname));
	if (hostname [0] == 0)
		strcpy (hostname, "unknown-host");

	name = g_copy_strings ("/CORBA-servers/Launcher-", hostname, 
			       "/DISPLAY-", getenv ("DISPLAY"), NULL);

	gnome_config_set_string (name, orb_ptr->object_to_string (acc));
	gnome_config_sync ();
	g_free (name);
	
	panel_initialize_corba (&orb_ptr, &boa_ptr);

	orb_ptr->dispatcher (new GtkDispatcher ());

	boa_ptr->impl_is_ready (CORBA::ImplementationDef::_nil());
}
