#include <config.h>
#include <gnome.h>
#include <orb/orbit.h>

#include "panel-include.h"
#include "gnome-panel.h"

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

extern int config_sync_timeout;
extern GList *panels_to_sync;
extern GList *applets_to_sync;
extern int globals_to_sync;
extern int need_complete_save;

static CORBA_short
server_applet_request_id(POA_GNOME_Panel *servant,
			 CORBA_char *ccookie,
			 CORBA_char *path,
			 CORBA_char * param,
			 CORBA_short dorestart,
			 CORBA_char ** cfgpath,
			 CORBA_char ** globcfgpath,
			 CORBA_unsigned_long* wid,
			 CORBA_Environment *ev);

static void
server_applet_register(POA_GNOME_Panel *servant,
		       CORBA_char * ccookie,
		       CORBA_char * ior,
		       CORBA_short applet_id,
		       CORBA_Environment *ev);

static void
server_applet_abort_id(POA_GNOME_Panel *servant,
		       CORBA_char * ccookie,
		       CORBA_short applet_id,
		       CORBA_Environment *ev);

static void
server_applet_request_glob_cfg(POA_GNOME_Panel *servant,
			       CORBA_char * ccookie,
			       CORBA_char ** globcfgpath,
			       CORBA_Environment *ev);

static void
server_applet_remove_from_panel(POA_GNOME_Panel *servant,
				CORBA_char * ccookie,
				CORBA_short applet_id,
				CORBA_Environment *ev);

static CORBA_short
server_applet_get_panel(POA_GNOME_Panel *servant,
			CORBA_char * ccookie,
			CORBA_short applet_id,
			CORBA_Environment *ev);

static CORBA_short
server_applet_get_pos(POA_GNOME_Panel *servant,
		      CORBA_char * ccookie,
		      CORBA_short applet_id,
		      CORBA_Environment *ev);

static CORBA_short
server_applet_get_panel_orient(POA_GNOME_Panel *servant,
			       CORBA_char * ccookie,
			       CORBA_short applet_id,
			       CORBA_Environment *ev);

static void
server_applet_show_menu(POA_GNOME_Panel *servant,
			CORBA_char * ccookie,
			CORBA_short applet_id,
			CORBA_Environment *ev);

static void
server_applet_drag_start(POA_GNOME_Panel *servant,
			 CORBA_char * ccookie,
			 CORBA_short applet_id,
			 CORBA_Environment *ev);

static void
server_applet_drag_stop(POA_GNOME_Panel *servant,
			CORBA_char * ccookie,
			CORBA_short applet_id,
			CORBA_Environment *ev);

static void
server_applet_add_callback(POA_GNOME_Panel *servant,
			   CORBA_char * ccookie,
			   CORBA_short applet_id,
			   CORBA_char * callback_name,
			   CORBA_char * stock_item,
			   CORBA_char * menuitem_text,
			   CORBA_Environment *ev);

static void
server_applet_remove_callback(POA_GNOME_Panel *servant,
			      CORBA_char * ccookie,
			      CORBA_short applet_id,
			      CORBA_char * callback_name,
			      CORBA_Environment *ev);

static void
server_applet_add_tooltip(POA_GNOME_Panel *servant,
			  CORBA_char * ccookie,
			  CORBA_short applet_id,
			  CORBA_char * tooltip,
			  CORBA_Environment *ev);

static void
server_applet_remove_tooltip(POA_GNOME_Panel *servant,
			     CORBA_char * ccookie,
			     CORBA_short applet_id,
			     CORBA_Environment *ev);

static CORBA_short
server_applet_in_drag(POA_GNOME_Panel *servant,
		      CORBA_char * ccookie,
		      CORBA_Environment *ev);

static void
server_sync_config(POA_GNOME_Panel *servant,
		   CORBA_char * ccookie,
		   CORBA_short applet_id,
		   CORBA_Environment *ev);

static void
server_quit(POA_GNOME_Panel *servant,
	    CORBA_char * ccookie,
	    CORBA_Environment *ev);
void panel_corba_gtk_init(void);

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
			 CORBA_char *ccookie,
			 CORBA_char *path,
			 CORBA_char * param,
			 CORBA_short dorestart,
			 CORBA_char ** cfgpath,
			 CORBA_char ** globcfgpath,
			 CORBA_unsigned_long* wid,
			 CORBA_Environment *ev)
{
  char *cfg=NULL;
  char *globcfg=NULL;
  int applet_id;
  CORBA_unsigned_long winid;

  CHECK_COOKIE_V ((*globcfgpath = NULL, *cfgpath = NULL, 0));
  applet_id = applet_request_id (path,param,dorestart,
				 &cfg, &globcfg, &winid);
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
		       CORBA_char * ccookie,
		       CORBA_char * ior,
		       CORBA_short applet_id,
		       CORBA_Environment *ev)
{
  CHECK_COOKIE();
  applet_register(ior, applet_id);
}


static void
server_applet_abort_id(POA_GNOME_Panel *servant,
		       CORBA_char * ccookie,
		       CORBA_short applet_id,
		       CORBA_Environment *ev)
{
  CHECK_COOKIE();
  applet_abort_id(applet_id);
}

static void
server_applet_request_glob_cfg(POA_GNOME_Panel *servant,
			       CORBA_char * ccookie,
			       CORBA_char ** globcfgpath,
			       CORBA_Environment *ev)
{
  CHECK_COOKIE();
  *globcfgpath = CORBA_string_dup(old_panel_cfg_path);
}


static void
server_applet_remove_from_panel(POA_GNOME_Panel *servant,
				CORBA_char * ccookie,
				CORBA_short applet_id,
				CORBA_Environment *ev)
{
  CHECK_COOKIE();
  panel_clean_applet(applet_id);
}


static CORBA_short
server_applet_get_panel(POA_GNOME_Panel *servant,
			CORBA_char * ccookie,
			CORBA_short applet_id,
			CORBA_Environment *ev)
{
  CHECK_COOKIE_V(FALSE);

  return applet_get_panel(applet_id);
}


static CORBA_short
server_applet_get_pos(POA_GNOME_Panel *servant,
		      CORBA_char * ccookie,
		      CORBA_short applet_id,
		      CORBA_Environment *ev)
{
  CHECK_COOKIE_V(FALSE);

  return applet_get_pos(applet_id);
}


static CORBA_short
server_applet_get_panel_orient(POA_GNOME_Panel *servant,
			       CORBA_char * ccookie,
			       CORBA_short applet_id,
			       CORBA_Environment *ev)
{
  CHECK_COOKIE_V(FALSE);

  return applet_get_panel_orient(applet_id);
}


static void
server_applet_show_menu(POA_GNOME_Panel *servant,
			CORBA_char * ccookie,
			CORBA_short applet_id,
			CORBA_Environment *ev)
{
  CHECK_COOKIE();

  applet_show_menu(applet_id);
}


static void
server_applet_drag_start(POA_GNOME_Panel *servant,
			 CORBA_char * ccookie,
			 CORBA_short applet_id,
			 CORBA_Environment *ev)
{
  CHECK_COOKIE();

  applet_drag_start(applet_id);
}


static void
server_applet_drag_stop(POA_GNOME_Panel *servant,
			CORBA_char * ccookie,
			CORBA_short applet_id,
			CORBA_Environment *ev)
{
  CHECK_COOKIE();

  applet_drag_stop(applet_id);
}


static void
server_applet_add_callback(POA_GNOME_Panel *servant,
			   CORBA_char * ccookie,
			   CORBA_short applet_id,
			   CORBA_char * callback_name,
			   CORBA_char * stock_item,
			   CORBA_char * menuitem_text,
			   CORBA_Environment *ev)
{
  CHECK_COOKIE();
  applet_add_callback(applet_id, callback_name, stock_item, menuitem_text);
}


static void
server_applet_remove_callback(POA_GNOME_Panel *servant,
			      CORBA_char * ccookie,
			      CORBA_short applet_id,
			      CORBA_char * callback_name,
			      CORBA_Environment *ev)
{
  CHECK_COOKIE();
  applet_remove_callback(applet_id, callback_name);
}


static void
server_applet_add_tooltip(POA_GNOME_Panel *servant,
			  CORBA_char * ccookie,
			  CORBA_short applet_id,
			  CORBA_char * tooltip,
			  CORBA_Environment *ev)
{
  CHECK_COOKIE();
  applet_set_tooltip(applet_id, tooltip);
}


static void
server_applet_remove_tooltip(POA_GNOME_Panel *servant,
			     CORBA_char * ccookie,
			     CORBA_short applet_id,
			     CORBA_Environment *ev)
{
  CHECK_COOKIE();
  applet_set_tooltip(applet_id, NULL);
}


static CORBA_short
server_applet_in_drag(POA_GNOME_Panel *servant,
		      CORBA_char * ccookie,
		      CORBA_Environment *ev)
{
  CHECK_COOKIE_V(FALSE);

  return panel_applet_in_drag;
}


static void
server_sync_config(POA_GNOME_Panel *servant,
		   CORBA_char * ccookie,
		   CORBA_short applet_id,
		   CORBA_Environment *ev)
{
  CHECK_COOKIE();

  if(g_list_find(applets_to_sync, GINT_TO_POINTER(applet_id))==NULL)
    applets_to_sync = g_list_prepend(applets_to_sync,
				     GINT_TO_POINTER(applet_id));
  panel_config_sync();
 
}


static void
server_quit(POA_GNOME_Panel *servant,
	    CORBA_char * ccookie,
	    CORBA_Environment *ev)
{
  CHECK_COOKIE();

  panel_quit();
}


/* And on the client side... Inefficiency abounds at present.
   We need to move CORBA into the core of the panel */

CORBA_ORB orb = NULL;
CORBA_Environment ev;

int
send_applet_session_save (const char *ior, int applet_id,
                               const char *cfgpath,
                               const char *globcfgpath)
{
  CORBA_short retval;
  GNOME_Applet appl = CORBA_ORB_string_to_object(orb, (CORBA_char *)ior, &ev);

  retval = GNOME_Applet_session_save(appl, cookie, applet_id,
				     (CORBA_char *)cfgpath,
				     (CORBA_char *)globcfgpath, &ev);

  if(ev._major)
    panel_clean_applet(applet_id);

  CORBA_Object_release(appl, &ev);

  return retval;
}

void
send_applet_change_orient (const char *ior, int applet_id,  int orient)
{
  GNOME_Applet appl = CORBA_ORB_string_to_object(orb, (CORBA_char *)ior, &ev);

  GNOME_Applet_change_orient(appl, cookie, applet_id, orient, &ev);

  if(ev._major)
    panel_clean_applet(applet_id);

  CORBA_Object_release(appl, &ev);
}

void send_applet_do_callback (const char *ior, int applet_id,
			      const char *callback_name)
{
  GNOME_Applet appl = CORBA_ORB_string_to_object(orb, (CORBA_char *)ior, &ev);

  GNOME_Applet_do_callback(appl, cookie, applet_id, (CORBA_char *)callback_name, &ev);

  if(ev._major)
    panel_clean_applet(applet_id);
  
  CORBA_Object_release(appl, &ev);
}

void send_applet_start_new_applet (const char *ior, const char *param)
{
  GNOME_Applet appl = CORBA_ORB_string_to_object(orb, (CORBA_char *)ior, &ev);

  GNOME_Applet_start_new_applet(appl, cookie, (CORBA_char *)param, &ev);
  
  CORBA_Object_release(appl, &ev);
}

void send_applet_change_back (const char *ior, int applet_id,
                              PanelBackType back_type, const char *pixmap,
                              const GdkColor* color)
{
  GNOME_Applet appl = CORBA_ORB_string_to_object(orb, (CORBA_char *)ior, &ev);

  GNOME_Applet_back_change(appl, cookie, applet_id, back_type,
			   (CORBA_char *)pixmap, color->red, color->green, color->blue, &ev);
  
  if(ev._major)
    panel_clean_applet(applet_id);
  
  CORBA_Object_release(appl, &ev);
}

void send_applet_tooltips_state (const char *ior, int enabled)
{
  GNOME_Applet appl = CORBA_ORB_string_to_object(orb, (CORBA_char *)ior, &ev);

  GNOME_Applet_tooltips_state(appl, cookie, enabled, &ev);

  CORBA_Object_release(appl, &ev);
}

static void
orb_handle_connection(GIOPConnection *cnx, gint source, GdkInputCondition cond)
{
  switch(cond) {
  case GDK_INPUT_EXCEPTION:
    giop_main_handle_connection_exception(cnx);
    break;
  default:
    giop_main_handle_connection(cnx);
  }
}

static void orb_add_connection(GIOPConnection *cnx)
{
  cnx->user_data = (gpointer)gtk_input_add_full(GIOP_CONNECTION_GET_FD(cnx),
						GDK_INPUT_READ|GDK_INPUT_EXCEPTION,
						(GdkInputFunction)orb_handle_connection,
						NULL, cnx, NULL);
}

static void orb_remove_connection(GIOPConnection *cnx)
{
  gtk_input_remove((guint)cnx->user_data);
  cnx->user_data = (gpointer)-1;
}

void
panel_corba_gtk_main (char *service_name)
{
  if(!orb)
    panel_corba_gtk_init();

  gtk_main();
}

void
panel_corba_gtk_main_quit(void)
{
  CORBA_ORB_shutdown(orb, CORBA_FALSE, &ev);

  gtk_main_quit();
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

void
panel_corba_register_arguments(void)
{
}

void
panel_corba_gtk_init(void)
{
  PortableServer_ObjectId objid = {0, sizeof("Panel"), "Panel" };
  GNOME_Panel acc;
  char hostname [4096];
  char *name, *ior;
  int n = 1;

  g_message("Initializing CORBA for panel\n");

  CORBA_exception_init(&ev);

  IIOPAddConnectionHandler = orb_add_connection;
  IIOPRemoveConnectionHandler = orb_remove_connection;

  /* It's not like ORBit reads the cmdline anyways, right now */
  orb = CORBA_ORB_init(&n, &name /* dummy */, "mico-local-orb", &ev);

  POA_GNOME_Panel__init(&servant, &ev);

  PortableServer_POA_activate_object_with_id((PortableServer_POA)orb->root_poa, &objid, &servant, &ev);

  acc = PortableServer_POA_servant_to_reference((PortableServer_POA)orb->root_poa, &servant, &ev);

  ior = CORBA_ORB_object_to_string(orb, acc, &ev);
  
  gethostname (hostname, sizeof (hostname));
  if (hostname [0] == 0)
    strcpy (hostname, "unknown-host");
  
  name = g_copy_strings ("/CORBA-servers/Panel-", hostname, 
			 "/DISPLAY-", getenv ("DISPLAY"), NULL);

  gnome_config_set_string (name, ior);
  gnome_config_sync ();
  g_free (name);
  CORBA_free(ior);

  CORBA_Object_release(acc, &ev);

  ORBit_custom_run_setup(orb, &ev);
}
