#include <iostream.h>
#include <mico/gtkmico.h>
#include <mico/naming.h>
#include <gnome.h>
#include "mico-glue.h"
#include "gnome-panel.h"
#include "panel.h"
#include "mico-parse.h"

/* This implements the server-side of the gnome-panel.idl
 * specification Currently there is no way to create new CORBA
 * "instances" of the panel, as there is only one panel running on the
 * screen.
 * */
   
class Panel_impl : virtual public GNOME::Panel_skel {
public:
	CORBA::Short applet_request_id (const char *path,
					char *&cfgpath,
					char *&globcfgpath,
					CORBA::ULong &wid) {
		char *cfg=NULL;
		char *globcfg=NULL;
		int id;
		guint32 winid;

		id = ::applet_request_id (path,&cfg,&globcfg,&winid);
		wid = winid;

		if(cfg) {
			cfgpath = CORBA::string_dup(cfg);
			g_free(cfg);
		} else 
			cfgpath = CORBA::string_dup("");
		if(globcfg) {
			globcfgpath = CORBA::string_dup(globcfg);
			g_free(globcfg);
		} else
			globcfgpath = CORBA::string_dup("");
		return id;
	}
	void applet_register (const char *ior, CORBA::Short id) {
		::applet_register(ior, id);
	}
	void applet_abort_id (CORBA::Short id) {
		::applet_abort_id (id);
	}
	void applet_request_glob_cfg (char *&globcfgpath) {
		char *globcfg=NULL;

		::applet_request_glob_cfg (&globcfg);
		if(globcfg) {
			globcfgpath = CORBA::string_dup(globcfg);
			g_free(globcfg);
		} else
			globcfgpath = CORBA::string_dup("");
	}
	CORBA::Short applet_get_panel (CORBA::Short id) {
		return ::applet_get_panel (id);
	}
	CORBA::Short applet_get_pos (CORBA::Short id) {
		return ::applet_get_pos (id);
	}
	void applet_show_menu (CORBA::Short id) {
		::applet_show_menu (id);
	}
	void applet_drag_start (CORBA::Short id) {
		::applet_drag_start (id);
	}
	void applet_drag_stop (CORBA::Short id) {
		::applet_drag_stop (id);
	}
	void applet_remove_from_panel (CORBA::Short id) {
		printf ("APPLET_REMOVE_FROM_PANEL not implemented!\n");
		/*FIXME:  */
	}
        void applet_add_callback (CORBA::Short id,
				  const char *callback_name,
				  const char *menuitem_text) {
		::applet_add_callback(id,
				      (char *)callback_name,
				      (char *)menuitem_text);
	}
	void quit(void) {
		::panel_quit();
	}
};

CORBA::ORB_ptr orb_ptr;
CORBA::BOA_ptr boa_ptr;

void
panel_corba_gtk_main (char *service_name)
{
	GNOME::Panel_ptr acc = new Panel_impl ();
	char hostname [4096];
	char *name;

	panel_initialize_corba (&orb_ptr, &boa_ptr);

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

int
panel_corba_call_launcher(const char *path)
{
	char *name;
	char *iior;
	char hostname [1024];
	
	gethostname (hostname, sizeof (hostname));
	if (hostname [0] == 0)
		strcpy (hostname, "unknown-host");

	name = g_copy_strings ("/CORBA-servers/Launcher-", hostname, 
			       "/DISPLAY-", getenv ("DISPLAY"), NULL);

	iior = gnome_config_get_string (name);
	g_free (name);
	
	puts("LAUNCHER_CALL");
	if (!iior)
		return 0;

	puts("LAUNCHER_iior");
	puts(iior);

	CORBA::Object_var obj = orb_ptr->string_to_object (iior);
	GNOME::Launcher_var launcher_client = GNOME::Launcher::_narrow (obj);

	launcher_client->start_new_launcher(path);
	
	return 1;
}

void
send_applet_session_save (const char *ior, int id, const char *cfgpath,
			  const char *globcfgpath)
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (ior);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	applet->session_save(id,cfgpath,globcfgpath);
}

void
send_applet_shutdown_applet (const char *ior, int id)
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (ior);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	applet->shutdown_applet(id);
}

void
send_applet_change_orient (const char *ior, int id, int orient)
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (ior);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	applet->change_orient(id,orient);
}

void
send_applet_do_callback (const char *ior, int id, char *callback_name)
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (ior);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	applet->do_callback(id, callback_name);
}
