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
	void stat_new_launcher (const char *path) {
		::start_new_launcher(path);
	}
};
