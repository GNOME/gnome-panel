#include <config.h>
#include <gnome.h>
#include <orb/orbit.h>

#include "panel-include.h"
#include "gnome-panel.h"
#include "orbit-glue.h"

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

extern int config_sync_timeout;
extern GList *panels_to_sync;
extern GList *applets_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

static CORBA_short
server_applet_request_id(POA_GNOME_Panel *servant,
			 CORBA_char *goad_id,
			 CORBA_char ** cfgpath,
			 CORBA_char ** globcfgpath,
			 CORBA_unsigned_long* wid,
			 CORBA_Environment *ev);

static void
server_applet_register(POA_GNOME_Panel *servant,
		       CORBA_Object obj,
		       CORBA_short applet_id,
		       CORBA_char *goad_id,
		       CORBA_char *goad_ids,
		       CORBA_Environment *ev);

static void
server_applet_abort_id(POA_GNOME_Panel *servant,
		       CORBA_short applet_id,
		       CORBA_Environment *ev);

static void
server_applet_request_glob_cfg(POA_GNOME_Panel *servant,
			       CORBA_char ** globcfgpath,
			       CORBA_Environment *ev);

static void
server_applet_remove_from_panel(POA_GNOME_Panel *servant,
				CORBA_short applet_id,
				CORBA_Environment *ev);

static CORBA_short
server_applet_get_panel(POA_GNOME_Panel *servant,
			CORBA_short applet_id,
			CORBA_Environment *ev);

static CORBA_short
server_applet_get_pos(POA_GNOME_Panel *servant,
		      CORBA_short applet_id,
		      CORBA_Environment *ev);

static CORBA_short
server_applet_get_panel_orient(POA_GNOME_Panel *servant,
			       CORBA_short applet_id,
			       CORBA_Environment *ev);

static void
server_applet_show_menu(POA_GNOME_Panel *servant,
			CORBA_short applet_id,
			CORBA_Environment *ev);

static void
server_applet_drag_start(POA_GNOME_Panel *servant,
			 CORBA_short applet_id,
			 CORBA_Environment *ev);

static void
server_applet_drag_stop(POA_GNOME_Panel *servant,
			CORBA_short applet_id,
			CORBA_Environment *ev);

static void
server_applet_add_callback(POA_GNOME_Panel *servant,
			   CORBA_short applet_id,
			   CORBA_char * callback_name,
			   CORBA_char * stock_item,
			   CORBA_char * menuitem_text,
			   CORBA_Environment *ev);

static void
server_applet_remove_callback(POA_GNOME_Panel *servant,
			      CORBA_short applet_id,
			      CORBA_char * callback_name,
			      CORBA_Environment *ev);

static void
server_applet_add_tooltip(POA_GNOME_Panel *servant,
			  CORBA_short applet_id,
			  CORBA_char * tooltip,
			  CORBA_Environment *ev);

static void
server_applet_remove_tooltip(POA_GNOME_Panel *servant,
			     CORBA_short applet_id,
			     CORBA_Environment *ev);

static CORBA_short
server_applet_in_drag(POA_GNOME_Panel *servant,
		      CORBA_Environment *ev);

static void
server_sync_config(POA_GNOME_Panel *servant,
		   CORBA_short applet_id,
		   CORBA_Environment *ev);

static void
server_quit(POA_GNOME_Panel *servant,
	    CORBA_Environment *ev);

static PortableServer_ServantBase__epv base_epv = {
  NULL, /* _private */
  NULL, /* finalize */
  NULL, /* use base default_POA function */
};

static POA_GNOME_Panel__epv panel_epv = {
  NULL, /* private data */
  (gpointer)&server_applet_request_id,
  (gpointer)&server_applet_register,
  (gpointer)&server_applet_abort_id,
  (gpointer)&server_applet_request_glob_cfg,
  (gpointer)&server_applet_remove_from_panel,
  (gpointer)&server_applet_get_panel,
  (gpointer)&server_applet_get_pos,
  (gpointer)&server_applet_get_panel_orient,
  (gpointer)&server_applet_show_menu,
  (gpointer)&server_applet_drag_start,
  (gpointer)&server_applet_drag_stop,
  (gpointer)&server_applet_add_callback,
  (gpointer)&server_applet_remove_callback,
  (gpointer)&server_applet_add_tooltip,
  (gpointer)&server_applet_remove_tooltip,
  (gpointer)&server_applet_in_drag,
  (gpointer)&server_sync_config,
  (gpointer)&server_quit
};
static POA_GNOME_Panel__vepv vepv = { &base_epv, &panel_epv };
static POA_GNOME_Panel servant = { NULL, &vepv };


static CORBA_short
server_applet_request_id(POA_GNOME_Panel *servant,
			 CORBA_char *goad_id,
			 CORBA_char ** cfgpath,
			 CORBA_char ** globcfgpath,
			 CORBA_unsigned_long* wid,
			 CORBA_Environment *ev)
{
  char *cfg=NULL;
  char *globcfg=NULL;
  int applet_id;
  CORBA_unsigned_long winid;

  applet_id = applet_request_id (goad_id, &cfg, &globcfg, &winid);
  *wid = winid;

  if(cfg) {
    *cfgpath = CORBA_string_dup(cfg);
    g_free(cfg);
  } else 
    *cfgpath = CORBA_string_dup("");
  if(globcfg) {
    *globcfgpath = CORBA_string_dup(globcfg);
  } else
    *globcfgpath = CORBA_string_dup("");
  return applet_id;
}

static void
server_applet_register(POA_GNOME_Panel *servant,
		       CORBA_Object obj,
		       CORBA_short applet_id,
		       CORBA_char *goad_id,
		       CORBA_char *goad_ids,
		       CORBA_Environment *ev)
{
	applet_register(obj, applet_id, goad_id,goad_ids);
}


static void
server_applet_abort_id(POA_GNOME_Panel *servant,
		       CORBA_short applet_id,
		       CORBA_Environment *ev)
{
  applet_abort_id(applet_id);
}

static void
server_applet_request_glob_cfg(POA_GNOME_Panel *servant,
			       CORBA_char ** globcfgpath,
			       CORBA_Environment *ev)
{
  *globcfgpath = CORBA_string_dup(old_panel_cfg_path);
}


static void
server_applet_remove_from_panel(POA_GNOME_Panel *servant,
				CORBA_short applet_id,
				CORBA_Environment *ev)
{
  panel_clean_applet(applet_id);
}


static CORBA_short
server_applet_get_panel(POA_GNOME_Panel *servant,
			CORBA_short applet_id,
			CORBA_Environment *ev)
{
  return applet_get_panel(applet_id);
}


static CORBA_short
server_applet_get_pos(POA_GNOME_Panel *servant,
		      CORBA_short applet_id,
		      CORBA_Environment *ev)
{
  return applet_get_pos(applet_id);
}


static CORBA_short
server_applet_get_panel_orient(POA_GNOME_Panel *servant,
			       CORBA_short applet_id,
			       CORBA_Environment *ev)
{
  return applet_get_panel_orient(applet_id);
}


static void
server_applet_show_menu(POA_GNOME_Panel *servant,
			CORBA_short applet_id,
			CORBA_Environment *ev)
{
  applet_show_menu(applet_id);
}


static void
server_applet_drag_start(POA_GNOME_Panel *servant,
			 CORBA_short applet_id,
			 CORBA_Environment *ev)
{
  applet_drag_start(applet_id);
}


static void
server_applet_drag_stop(POA_GNOME_Panel *servant,
			CORBA_short applet_id,
			CORBA_Environment *ev)
{
  applet_drag_stop(applet_id);
}


static void
server_applet_add_callback(POA_GNOME_Panel *servant,
			   CORBA_short applet_id,
			   CORBA_char * callback_name,
			   CORBA_char * stock_item,
			   CORBA_char * menuitem_text,
			   CORBA_Environment *ev)
{
  applet_add_callback(applet_id, callback_name, stock_item, menuitem_text);
}


static void
server_applet_remove_callback(POA_GNOME_Panel *servant,
			      CORBA_short applet_id,
			      CORBA_char * callback_name,
			      CORBA_Environment *ev)
{
  applet_remove_callback(applet_id, callback_name);
}


static void
server_applet_add_tooltip(POA_GNOME_Panel *servant,
			  CORBA_short applet_id,
			  CORBA_char * tooltip,
			  CORBA_Environment *ev)
{
  applet_set_tooltip(applet_id, tooltip);
}


static void
server_applet_remove_tooltip(POA_GNOME_Panel *servant,
			     CORBA_short applet_id,
			     CORBA_Environment *ev)
{
  applet_set_tooltip(applet_id, NULL);
}


static CORBA_short
server_applet_in_drag(POA_GNOME_Panel *servant,
		      CORBA_Environment *ev)
{
  return panel_applet_in_drag;
}


static void
server_sync_config(POA_GNOME_Panel *servant,
		   CORBA_short applet_id,
		   CORBA_Environment *ev)
{
  if(g_list_find(applets_to_sync, GINT_TO_POINTER(((int)applet_id)))==NULL)
    applets_to_sync = g_list_prepend(applets_to_sync,
				     GINT_TO_POINTER(((int)applet_id)));
  panel_config_sync();
 
}


static void
server_quit(POA_GNOME_Panel *servant,
	    CORBA_Environment *ev)
{
  panel_quit();
}


/* And on the client side... Inefficiency abounds at present.
   We need to move CORBA into the core of the panel */

CORBA_ORB orb = NULL;
CORBA_Environment ev;

int
send_applet_session_save (CORBA_Object obj, int applet_id,
			  const char *cfgpath,
			  const char *globcfgpath)
{
  CORBA_short retval;

  retval = GNOME_Applet_session_save(obj, applet_id,
				     (CORBA_char *)cfgpath,
				     (CORBA_char *)globcfgpath, &ev);

  if(ev._major)
    panel_clean_applet(applet_id);
  return retval;
}

void
send_applet_change_orient (CORBA_Object obj, int applet_id,  int orient)
{
  GNOME_Applet_change_orient(obj, applet_id, orient, &ev);

  if(ev._major)
    panel_clean_applet(applet_id);
}

void send_applet_do_callback (CORBA_Object appl, int applet_id,
			      const char *callback_name)
{
  GNOME_Applet_do_callback(appl, applet_id, (CORBA_char *)callback_name, &ev);

  if(ev._major)
    panel_clean_applet(applet_id);
}

void send_applet_start_new_applet (CORBA_Object appl, const char *goad_id)
{
  GNOME_Applet_start_new_applet(appl, (CORBA_char *)goad_id, &ev);
}

void send_applet_change_back (CORBA_Object appl, int applet_id,
                              PanelBackType back_type, const char *pixmap,
                              const GdkColor* color)
{
  GNOME_Applet_back_change(appl, applet_id, back_type,
			   (CORBA_char *)pixmap, color->red, color->green, color->blue, &ev);
  
  if(ev._major)
    panel_clean_applet(applet_id);
}

void send_applet_tooltips_state (CORBA_Object appl, int applet_id, int enabled)
{
  GNOME_Applet_tooltips_state(appl, applet_id, enabled, &ev);
}

void
panel_corba_gtk_main (char *service_name)
{
  g_assert(orb);

  gtk_main();
}

void
panel_corba_gtk_main_quit(void)
{
  gtk_main_quit();
}

void
panel_corba_clean_up(void)
{
  CORBA_Object ns = gnome_name_service_get();
  goad_server_unregister(ns, "gnome_panel", "server", &ev);
  CORBA_Object_release(ns, &ev);
  CORBA_ORB_shutdown(orb, CORBA_FALSE, &ev);
}

void
panel_corba_gtk_init(CORBA_ORB panel_orb)
{
  PortableServer_ObjectId objid = {0, sizeof("Panel"), "Panel" };
  PortableServer_POA thepoa;
  GNOME_Panel acc;
  char hostname [4096];
  char *name;
  CORBA_Object ns;

  CORBA_exception_init(&ev);

  orb = panel_orb;

  POA_GNOME_Panel__init(&servant, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  thepoa = (PortableServer_POA)
    CORBA_ORB_resolve_initial_references(orb, "RootPOA", &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(thepoa, &ev), &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  PortableServer_POA_activate_object_with_id(thepoa, &objid, &servant, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  acc = PortableServer_POA_servant_to_reference(thepoa, &servant, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  ns = gnome_name_service_get();
  goad_server_register(ns, acc, "gnome_panel", "server", &ev);
  CORBA_Object_release(ns, &ev);

  if(goad_server_activation_id()) {
    CORBA_char *ior;
    ior = CORBA_ORB_object_to_string(orb, acc, &ev);
    printf("%s\n", ior); fflush(stdout);
    CORBA_free(ior);
  }

  CORBA_Object_release(acc, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  ORBit_custom_run_setup(orb, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);
}
