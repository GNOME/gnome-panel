#include <config.h>
#include <iostream.h>
#include <mico/gtkmico.h>
#include <mico/naming.h>
#include <gnome.h>
#include "mico-glue.h"
#include "gnome-panel.h"
#include "panel.h"
#include "panel-widget.h"
#include "mico-parse.h"

#include "cookie.h"

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

/* This implements the server-side of the gnome-panel.idl
 * specification Currently there is no way to create new CORBA
 * "instances" of the panel, as there is only one panel running on the
 * screen.
 * */

class Panel_impl : virtual public GNOME::Panel_skel {
public:
	CORBA::Short applet_request_id (const char *ccookie,
					const char *path,
					const char *param,
					CORBA::Short dorestart,
					char *&cfgpath,
					char *&globcfgpath,
					CORBA::ULong &wid) {
		char *cfg=NULL;
		char *globcfg=NULL;
		int applet_id;
		guint32 winid;

		CHECK_COOKIE_V (0);
		applet_id = ::applet_request_id (path,param,dorestart,
						 &cfg,&globcfg,&winid);
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
		return applet_id;
	}
	void applet_register (const char *ccookie, const char *ior,
			      CORBA::Short applet_id) {
		CHECK_COOKIE ();
		::applet_register(ior, applet_id);
	}
	void applet_abort_id (const char *ccookie, CORBA::Short applet_id) {
		CHECK_COOKIE ();
		::applet_abort_id (applet_id);
	}
	void applet_request_glob_cfg (const char *ccookie,
				      char *&globcfgpath) {
		CHECK_COOKIE ();
		globcfgpath = CORBA::string_dup(old_panel_cfg_path);
	}
	CORBA::Short applet_get_panel (const char *ccookie,
				       CORBA::Short applet_id) {
		CHECK_COOKIE_V (0);
		return ::applet_get_panel (applet_id);
	}
	CORBA::Short applet_get_pos (const char *ccookie,
				     CORBA::Short applet_id) {
		CHECK_COOKIE_V (0);
		return ::applet_get_pos (applet_id);
	}
	CORBA::Short applet_get_panel_orient (const char *ccookie,
				              CORBA::Short applet_id) {
		CHECK_COOKIE_V (0);
		return (int)::applet_get_panel_orient (applet_id);
	}
	void applet_show_menu (const char *ccookie, CORBA::Short applet_id) {
		CHECK_COOKIE ();
		::applet_show_menu (applet_id);
	}
	void applet_drag_start (const char *ccookie, CORBA::Short applet_id) {
		CHECK_COOKIE ();
		::applet_drag_start (applet_id);
	}
	void applet_drag_stop (const char *ccookie, CORBA::Short applet_id) {
		CHECK_COOKIE ();
		::applet_drag_stop (applet_id);
	}
	void applet_remove_from_panel (const char *ccookie,
				       CORBA::Short applet_id) {
		CHECK_COOKIE ();
		::panel_clean_applet(applet_id);
	}
        void applet_add_callback (const char *ccookie, 
				  CORBA::Short applet_id,
				  const char *callback_name,
				  const char *stock_item,
				  const char *menuitem_text) {
		CHECK_COOKIE ();
		::applet_add_callback(applet_id,
				      (char *)callback_name,
				      (char *)stock_item,
				      (char *)menuitem_text);
	}
        void applet_remove_callback (const char *ccookie, 
				     CORBA::Short applet_id,
				     const char *callback_name) {
		CHECK_COOKIE ();
		::applet_remove_callback(applet_id,
					 (char *)callback_name);
	}
	void applet_add_tooltip (const char *ccookie, CORBA::Short applet_id,
				 const char *tooltip) {
		CHECK_COOKIE ();
		::applet_set_tooltip(applet_id,tooltip);
	}
	void applet_remove_tooltip (const char *ccookie,
				    CORBA::Short applet_id) {
		CHECK_COOKIE ();
		::applet_set_tooltip(applet_id,NULL);
	}
	CORBA::Short applet_in_drag (const char *ccookie) {
		CHECK_COOKIE_V (FALSE);
		return panel_applet_in_drag;
	}
	void sync_config(const char *ccookie) {
		CHECK_COOKIE ();
		::panel_sync_config();
	}
	void quit(const char *ccookie) {
		CHECK_COOKIE ();
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
#ifdef HAVE_MICO_ORB_RUN	
	orb_ptr->run ();
#endif
}

void
panel_corba_gtk_main_quit(void)
{
	boa_ptr->deactivate_impl (CORBA::ImplementationDef::_nil());
#ifdef HAVE_MICO_ORB_RUN	
	orb_ptr->shutdown ();
#endif
}


void
panel_corba_clean_up(void)
{
	char hostname [4096];
	char *name;

	gethostname (hostname, sizeof (hostname));
	if (hostname [0] == 0)
		strcpy (hostname, "unknown-host");


	name = g_copy_strings ("/CORBA-servers/Panel-", hostname, NULL);
	if(gnome_config_has_section(name))
		gnome_config_clean_section(name);

	gnome_config_sync ();
	g_free (name);
}

int
send_applet_session_save (const char *ior, int applet_id, const char *cfgpath,
			  const char *globcfgpath)
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (ior);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	try {
		return applet->session_save(cookie, applet_id,cfgpath,globcfgpath);
	} catch(...) {
		g_warning(_("Error on sending 'session save' to applet!"));
		return TRUE;
	}
}

void
send_applet_change_orient (const char *ior, int applet_id, int orient)
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (ior);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	try {
		applet->change_orient(cookie,applet_id,orient);
	} catch(...) {
		g_warning(_("Error on sending 'orient change' to applet!"));
	}
}

void
send_applet_do_callback (const char *ior, int applet_id,
			 const char *callback_name)
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (ior);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	try {
		applet->do_callback(cookie,applet_id, callback_name);
	} catch(...) {
		g_warning(_("Error on sending 'do callback' to applet!"));
	}
}

void
send_applet_start_new_applet (const char *ior, const char *param)
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (ior);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	try {
		applet->start_new_applet(cookie,param);
	} catch(...) {
		g_warning(_("Error on sending 'start new applet' to applet!"));
	}
}

void
send_applet_change_back (const char *ior, int applet_id,
			 PanelBackType back_type, const char *pixmap,
			 const GdkColor* color)
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (ior);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	try {
		applet->back_change(cookie,applet_id, back_type, pixmap,
				    color->red,color->green,color->blue);
	} catch(...) {
		g_warning(_("Error on sending 'change back' to applet!"));
	}
}

void
send_applet_tooltips_state (const char *ior, int enabled)
{
	/* Use the ior that was sent to us to get an Applet CORBA object */
	CORBA::Object_var obj = orb_ptr->string_to_object (ior);
	GNOME::Applet_var applet = GNOME::Applet::_narrow (obj);

	/* Now, use corba to invoke the routine in the panel */
	try {
		applet->tooltips_state(cookie, enabled);
	} catch(...) {
		g_warning(_("Error on sending 'tooltips state' to applet!"));
	}
}
