/* Gnome panel: extern applet functions
 * (C) 1997,1998,1999,2000 the Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors:  George Lebl
 *           Federico Mena
 *           Miguel de Icaza
 */

#include "config.h"

#include <gdk/gdkx.h>
#include <string.h>
#include <signal.h>

#include <libgnome/libgnome.h>
#include <libbonobo.h>
#include <bonobo-activation/bonobo-activation.h>

#include "extern.h"

#include "gnome-run.h"
#include "launcher.h"
#include "menu-fentry.h"
#include "menu.h"
#include "panel-util.h"
#include "panel_config_global.h"
#include "session.h"
#include "status.h"

#undef EXTERN_DEBUG

#ifndef EXTERN_DEBUG

static inline void dprintf (const char *format, ...) { };

#else

#include <stdio.h>
#define dprintf(format...) fprintf(stderr, format)

#endif /* EXTERN_DEBUG */

struct Extern_struct {
	POA_GNOME_PanelSpot  servant;

	GNOME_PanelSpot      pspot;
	GNOME_Applet2        applet;

	gint                 refcount;

	/*
	 * normally FALSE, if an app returned TRUE from save session 
	 * it's set to TRUE, in which case we won't warn about it
	 * croaking and wanting a reload, because it wouldn't work anyway.
	 */
	gboolean             didnt_want_save; 

	/*
	 * normally FALSE, if TRUE, the user or the applet requested to 
	 * be killed, thus the panel should not ask about putting the
	 * applet back as if it may have crashed, this is why it is important 
	 * that applets use the _remove method (unregister_us corba call), so 
	 * that this gets set, when they want to leave cleanly
	 */
	gboolean             clean_remove; 

	gchar               *iid;
	gchar               *config_string;
	GtkWidget           *ebox;
	gboolean             started;
	gboolean             exactpos;
	gboolean             send_position;
	gboolean             send_draw;

	/*
	 * current orient, if it doesn't change, don't send any orient change
	 */
	PanelOrientation     orient;

	gint                 send_draw_timeout;
	gint                 send_draw_idle;
	gboolean             send_draw_queued;

	AppletInfo          *info;
};

extern GSList *panels;

extern GSList *applets;
extern GSList *applets_last;

extern int applets_to_sync;
extern int need_complete_save;

extern GtkTooltips *panel_tooltips;

extern GlobalConfig global_config;
extern gboolean commie_mode;

extern int ss_cur_applet;
extern gboolean ss_done_save;
extern GtkWidget* ss_timeout_dlg;
extern gushort ss_cookie;

extern gboolean panel_in_startup;

/* Launching applets into other things then the panel */

typedef struct {
	char                    *iid;
	GNOME_PanelAppletBooter  booter;
} OutsideExtern;

static GSList *outside_externs = NULL;

#ifdef FIXME
static void
push_outside_extern (const char                    *iid,
		     const GNOME_PanelAppletBooter  booter,
		     CORBA_Environment             *ev)
{
	OutsideExtern *oe;

	g_return_if_fail (iid && booter != CORBA_OBJECT_NIL);

	oe = g_new0 (OutsideExtern, 1);

	oe->iid    = g_strdup (iid);
	oe->booter = CORBA_Object_duplicate (booter, ev);

	outside_externs = g_slist_prepend (outside_externs, oe);
}
#endif /* FIXME */

static GNOME_PanelAppletBooter
pop_outside_extern (const char *iid)
{
	GSList *li;

	g_return_val_if_fail (iid, CORBA_OBJECT_NIL);

	for (li = outside_externs; li; li = li->next) {
		OutsideExtern *oe = li->data;

		if (!strcmp (oe->iid, iid)) {
			g_free (oe->iid);
			oe->iid = NULL;

			g_free (oe);

			return oe->booter;
		}
	}

	return CORBA_OBJECT_NIL;
}

/***Panel stuff***/
static GNOME_PanelSpot
s_panel_add_applet (PortableServer_Servant servant,
		    const GNOME_Applet panel_applet,
		    const CORBA_char *goad_id,
		    CORBA_char ** cfgpath,
		    CORBA_char ** globcfgpath,
		    CORBA_unsigned_long* wid,
		    CORBA_Environment *ev);

static GNOME_PanelSpot
s_panel_add_applet_full (PortableServer_Servant servant,
			 const GNOME_Applet panel_applet,
			 const CORBA_char *goad_id,
			 const CORBA_short panel,
			 const CORBA_short pos,
			 CORBA_char ** cfgpath,
			 CORBA_char ** globcfgpath,
			 CORBA_unsigned_long* wid,
			 CORBA_Environment *ev);

static void
s_panel_quit(PortableServer_Servant servant, CORBA_Environment *ev);

static CORBA_boolean
s_panel_get_in_drag(PortableServer_Servant servant, CORBA_Environment *ev);

static GNOME_StatusSpot
s_panel_add_status(PortableServer_Servant servant,
		   CORBA_unsigned_long* wid,
		   CORBA_Environment *ev);

static void
s_panel_notice_config_changes(PortableServer_Servant servant,
			      CORBA_Environment *ev);


/** Panel2 additions **/
void s_panel_suggest_sync (PortableServer_Servant _servant,
			   CORBA_Environment * ev);
void s_panel_add_launcher (PortableServer_Servant _servant,
			   const CORBA_char * launcher_desktop,
			   const CORBA_short panel,
			   const CORBA_short pos,
			   CORBA_Environment * ev);
void s_panel_ask_about_launcher (PortableServer_Servant _servant,
				 const CORBA_char * exec_string,
				 const CORBA_short panel,
				 const CORBA_short pos,
				 CORBA_Environment * ev);
void s_panel_add_launcher_from_info (PortableServer_Servant _servant,
				     const CORBA_char * name,
				     const CORBA_char * comment,
				     const CORBA_char * exec,
				     const CORBA_char * icon,
				     const CORBA_short panel,
				     const CORBA_short pos,
				     CORBA_Environment * ev);
void s_panel_add_launcher_from_info_url (PortableServer_Servant _servant,
					 const CORBA_char * name,
					 const CORBA_char * comment,
					 const CORBA_char * url,
					 const CORBA_char * icon,
					 const CORBA_short panel,
					 const CORBA_short pos,
					 CORBA_Environment * ev);
void s_panel_run_box (PortableServer_Servant _servant,
		      const CORBA_char * initial_string,
		      CORBA_Environment * ev);
void s_panel_main_menu (PortableServer_Servant _servant,
			CORBA_Environment * ev);
void s_panel_launch_an_applet (PortableServer_Servant _servant,
			       const CORBA_char * goad_id,
			       const GNOME_PanelSpot spot,
			       CORBA_Environment * ev);

/*** PanelSpot stuff ***/

static CORBA_char *
s_panelspot_get_tooltip(PortableServer_Servant servant,
			CORBA_Environment *ev);

static void
s_panelspot_set_tooltip(PortableServer_Servant servant,
			const CORBA_char *val,
			CORBA_Environment *ev);

static CORBA_short
s_panelspot_get_parent_panel(PortableServer_Servant servant,
			     CORBA_Environment *ev);

static CORBA_short
s_panelspot_get_spot_pos(PortableServer_Servant servant,
			 CORBA_Environment *ev);

static GNOME_Panel_OrientType
s_panelspot_get_parent_orient(PortableServer_Servant servant,
			      CORBA_Environment *ev);

static CORBA_short
s_panelspot_get_parent_size(PortableServer_Servant servant,
			    CORBA_Environment *ev);

static CORBA_short
s_panelspot_get_free_space(PortableServer_Servant servant,
			   CORBA_Environment *ev);

static CORBA_boolean
s_panelspot_get_send_position(PortableServer_Servant servant,
			      CORBA_Environment *ev);
static void
s_panelspot_set_send_position(PortableServer_Servant servant,
			      CORBA_boolean,
			      CORBA_Environment *ev);

static CORBA_boolean
s_panelspot_get_send_draw(PortableServer_Servant servant,
			  CORBA_Environment *ev);
static void
s_panelspot_set_send_draw(PortableServer_Servant servant,
			  CORBA_boolean,
			  CORBA_Environment *ev);

static GNOME_Panel_RgbImage *
s_panelspot_get_rgb_background(PortableServer_Servant servant,
			       CORBA_Environment *ev);

static void
s_panelspot_register_us(PortableServer_Servant servant,
		     CORBA_Environment *ev);

static void
s_panelspot_unregister_us(PortableServer_Servant servant,
		       CORBA_Environment *ev);

static void
s_panelspot_abort_load(PortableServer_Servant servant,
		       CORBA_Environment *ev);

static void
s_panelspot_show_menu(PortableServer_Servant servant,
		      CORBA_Environment *ev);

static void
s_panelspot_drag_start(PortableServer_Servant servant,
		       CORBA_Environment *ev);

static void
s_panelspot_drag_stop(PortableServer_Servant servant,
		      CORBA_Environment *ev);

static void
s_panelspot_add_callback(PortableServer_Servant servant,
			 const CORBA_char *callback_name,
			 const CORBA_char *stock_item,
			 const CORBA_char *menuitem_text,
			 CORBA_Environment *ev);

static void
s_panelspot_remove_callback(PortableServer_Servant servant,
			    const CORBA_char *callback_name,
			    CORBA_Environment *ev);
static void
s_panelspot_callback_set_sensitive(PortableServer_Servant servant,
				   const CORBA_char *callback_name,
				   const CORBA_boolean sensitive,
				   CORBA_Environment *ev);

static void
s_panelspot_sync_config(PortableServer_Servant servant,
			CORBA_Environment *ev);

static void
s_panelspot_done_session_save(PortableServer_Servant servant,
			      CORBA_boolean ret,
			      CORBA_unsigned_long cookie,
			      CORBA_Environment *ev);

/*** StatusSpot stuff ***/

static void
s_statusspot_remove(POA_GNOME_StatusSpot *servant,
		    CORBA_Environment *ev);


static PortableServer_ServantBase__epv panel_base_epv = {
	NULL, /* _private */
	NULL, /* finalize */
	NULL  /* use base default_POA function */
};

static POA_GNOME_Panel__epv panel_epv = {
	NULL, /* private data */
	s_panel_add_applet,
	s_panel_add_applet_full,
	s_panel_quit,
	s_panel_get_in_drag,
	s_panel_add_status,
	s_panel_notice_config_changes
};

static POA_GNOME_Panel2__epv panel2_epv = {
	NULL, /* private data */
	s_panel_suggest_sync,
	s_panel_add_launcher,
	s_panel_ask_about_launcher,
	s_panel_add_launcher_from_info,
	s_panel_add_launcher_from_info_url,
	s_panel_run_box,
	s_panel_main_menu,
	s_panel_launch_an_applet
};

static POA_GNOME_Panel2__vepv panel_vepv = { &panel_base_epv, &panel_epv, &panel2_epv };
static POA_GNOME_Panel2 panel_servant = { NULL, &panel_vepv };


static PortableServer_ServantBase__epv panelspot_base_epv = {
  NULL, /* _private */
  NULL, /* finalize */
  NULL  /* use base default_POA function */
};

static POA_GNOME_PanelSpot__epv panelspot_epv = {
  NULL, /* private data */
  s_panelspot_get_tooltip,
  s_panelspot_set_tooltip,
  s_panelspot_get_parent_panel,
  s_panelspot_get_spot_pos,
  s_panelspot_get_parent_orient,
  s_panelspot_get_parent_size,
  s_panelspot_get_free_space,
  s_panelspot_get_send_position,
  s_panelspot_set_send_position,
  s_panelspot_get_send_draw,
  s_panelspot_set_send_draw,
  s_panelspot_get_rgb_background,
  s_panelspot_register_us,
  s_panelspot_unregister_us,
  s_panelspot_abort_load,
  s_panelspot_show_menu,
  s_panelspot_drag_start,
  s_panelspot_drag_stop,
  s_panelspot_add_callback,
  s_panelspot_remove_callback,
  s_panelspot_callback_set_sensitive,
  s_panelspot_sync_config,
  s_panelspot_done_session_save
};
static POA_GNOME_PanelSpot__vepv panelspot_vepv = { &panelspot_base_epv, &panelspot_epv };

static PortableServer_ServantBase__epv statusspot_base_epv = {
  NULL, /* _private */
  NULL, /* finalize */
  NULL  /* use base default_POA function */
};

static POA_GNOME_StatusSpot__epv statusspot_epv = {
  NULL, /* private data */
  (gpointer)&s_statusspot_remove
};
static POA_GNOME_StatusSpot__vepv statusspot_vepv = { &statusspot_base_epv, &statusspot_epv };

/********************* NON-CORBA Stuff *******************/

static void
extern_activate (Extern ext)
{
        CORBA_Environment env;
	CORBA_Object      obj;

	dprintf ("extern_activate: ");

	CORBA_exception_init (&env);

	obj = bonobo_activation_activate_from_id (ext->iid, 0, NULL, &env);
	if (BONOBO_EX (&env) || obj == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&env);
		dprintf ("failed\n");
		return;
	}

	dprintf ("successful\n");
					    
	CORBA_Object_release (obj, &env);

	CORBA_exception_free (&env);
}

static Extern
extern_ref (Extern ext)
{
	ext->refcount++;
	return ext;
}

static void
extern_unref (Extern ext)
{
	ext->refcount--;
	if (ext->refcount == 0)
		g_free (ext);
}

GNOME_Applet2 
extern_get_applet (Extern ext)
{
	return ext->applet;
}

void
extern_set_config_string (Extern  ext,
			  gchar  *config_string)
{
	if (ext->config_string)
		g_free (ext->config_string);

	ext->config_string = config_string;
}

/*
 * FIXME: is this all really needed ?
 *        I think the whole purpose of the ref/unref is
 *        to make sure we can set ext->info
 */
static void
extern_remove_applet (Extern ext)
{
	g_assert (ext->info);

	extern_ref (ext);

	panel_clean_applet (ext->info);
	ext->info = NULL;

	extern_unref (ext);
}

gboolean
extern_handle_back_change (Extern       ext,
			   PanelWidget *panel)
{
	GNOME_Panel_BackInfoType backing;
	CORBA_Environment        env;
	GNOME_Applet2            applet;

	g_assert (ext);

	applet = ext->applet;

	if (applet == CORBA_OBJECT_NIL)
		return TRUE;

	CORBA_exception_init (&env);

	backing._d = panel->back_type;

	switch (panel->back_type) {
	case PANEL_BACK_PIXMAP:
		backing._u.pmap = panel->back_pixmap;
		break;
	case PANEL_BACK_COLOR:
		backing._u.c.red   = panel->back_color.red;
		backing._u.c.green = panel->back_color.green;
		backing._u.c.blue  = panel->back_color.blue;
		break;
	case PANEL_BACK_NONE:
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	GNOME_Applet2_back_change (applet, &backing, &env);
	if (BONOBO_EX (&env)) {
		extern_remove_applet (ext);

		CORBA_exception_free (&env);

		return FALSE;
		}

	return TRUE;
}

gboolean
extern_handle_freeze_changes (Extern ext)
{
	CORBA_Environment env;
	GNOME_Applet2     applet;

	g_assert (ext);

	applet = ext->applet;

	if (applet == CORBA_OBJECT_NIL)
		return TRUE;

	CORBA_exception_init (&env);

	GNOME_Applet2_freeze_changes (applet, &env);
	if (BONOBO_EX (&env)) {
		extern_remove_applet (ext);

		CORBA_exception_free (&env);

		return FALSE;
		}

	return TRUE;
}

gboolean
extern_handle_thaw_changes (Extern ext)
{
	CORBA_Environment env;
	GNOME_Applet2     applet;

	g_assert (ext);

	applet = ext->applet;

	if (applet == CORBA_OBJECT_NIL)
		return TRUE;

	CORBA_exception_init (&env);

	GNOME_Applet2_thaw_changes (applet, &env);
	if (BONOBO_EX (&env)) {
		extern_remove_applet (ext);

		CORBA_exception_free (&env);

		return FALSE;
		}

	return TRUE;
}

gboolean
extern_handle_change_orient (Extern ext,
			     int    orient)
{
	CORBA_Environment env;
	GNOME_Applet2     applet;

	g_assert (ext);

	applet = ext->applet;

	if (applet == CORBA_OBJECT_NIL || ext->orient == orient)
		return TRUE;

	CORBA_exception_init (&env);

	GNOME_Applet2_change_orient (applet, orient, &env);
	if (BONOBO_EX (&env)) {
		extern_remove_applet (ext);

		CORBA_exception_free (&env);

		return FALSE;
		}

	ext->orient = orient;

	return TRUE;
}

gboolean
extern_handle_change_size (Extern ext,
			   int    size)
{
	CORBA_Environment env;
	GNOME_Applet2     applet;

	g_assert (ext);

	applet = ext->applet;

	if (applet == CORBA_OBJECT_NIL)
		return TRUE;

	CORBA_exception_init (&env);

	GNOME_Applet2_change_size (applet, size, &env);
	if (BONOBO_EX (&env)) {
		extern_remove_applet (ext);

		CORBA_exception_free (&env);

		return FALSE;
		}

	return TRUE;
}

gboolean
extern_handle_do_callback (Extern  ext,
			   char   *name)
{
	CORBA_Environment env;
	GNOME_Applet2     applet;

	g_assert (ext);

	applet = ext->applet;

	if (applet == CORBA_OBJECT_NIL)
		return TRUE;

	CORBA_exception_init (&env);

	GNOME_Applet2_do_callback (applet, name, &env);
	if (BONOBO_EX (&env)) {
		extern_remove_applet (ext);

		CORBA_exception_free (&env);

		return FALSE;
		}

	return TRUE;
}

gboolean
extern_handle_set_tooltips_state (Extern   ext,
				  gboolean enabled)
{
	CORBA_Environment env;
	GNOME_Applet2     applet;

	g_assert (ext);

	applet = ext->applet;

	if (applet == CORBA_OBJECT_NIL)
		return TRUE;

	CORBA_exception_init (&env);

	GNOME_Applet2_set_tooltips_state (applet, enabled, &env);
	if (BONOBO_EX (&env)) {
		extern_remove_applet (ext);

		CORBA_exception_free (&env);

		return FALSE;
		}

	return TRUE;
}

typedef struct {
	char *iid;
	char *cfgpath;
	int   pos;
	int   panel;
} ReloadCallbackData;

#ifdef FIXME
static void
destroy_reload_callback_data (gpointer data)
{
	ReloadCallbackData *d = data;

	g_free (d->iid);
	d->iid = NULL;

	g_free (d->cfgpath);
	d->cfgpath = NULL;

	g_free (d);
}
#endif /* FIXME */

#ifdef FIXME
static void
reload_applet_callback (GtkWidget *w,
			int        button,
			gpointer   data)
{
	ReloadCallbackData *d = data;
	PanelWidget        *panel;

	/* 
	 * unless the button was YES, just do nothing
	 */
	if (button) {
		return;
	}

	/*
	 * select the nth panel
	 */
	g_assert (panels);

	panel = g_slist_nth_data (panels, d->panel);
	if (!panel)
		panel = panels->data;

	extern_load_applet (d->iid, d->cfgpath, panel,
			    d->pos, TRUE, FALSE);
}
#endif /* FIXME */

void
extern_before_remove (Extern ext)
{
#ifdef FIXME
	char *s;
	const char *id ="";
	GtkWidget *dlg;
	ReloadCallbackData *d;

	if (ext->clean_remove ||
	    ext->didnt_want_save)
		return;

	id = ext->iid != NULL ? ext->iid : "";

	/* a hack, but useful to users */
	if (strcmp (id, "deskguide_applet") == 0) {
		id = _("Deskguide (the desktop pager)");
	} else if (strcmp (id, "tasklist_applet") == 0) {
		id = _("Tasklist");
	} else if (strcmp (id, "battery_applet") == 0) {
		id = _("The Battery");
	}

	s = g_strdup_printf (_("%s applet appears to have "
			       "died unexpectedly.\n\n"
			       "Reload this applet?\n\n"
			       "(If you choose not to reload it at "
			       "this time you can always add it from\n"
			       "the \"Applets\" submenu in the main "
			       "menu.)"), id);

	dlg = gnome_message_box_new (s, GNOME_MESSAGE_BOX_QUESTION,
				     _("Reload"),
				     GNOME_STOCK_BUTTON_CANCEL,
				     NULL);
	gnome_dialog_set_close (GNOME_DIALOG (dlg),
				TRUE /* click_closes */);
	gtk_window_set_wmclass (GTK_WINDOW (dlg),
				"applet_crashed", "Panel");

	g_free (s);

	d = g_new0 (ReloadCallbackData, 1);
	d->iid = g_strdup (ext->iid);
	d->cfgpath = g_strdup (ext->config_string);

	if (ext->info->widget != NULL) {
		AppletData *ad;
		ad = gtk_object_get_data (GTK_OBJECT (ext->info->widget),
					  PANEL_APPLET_DATA);
		d->pos = ad->pos;
		d->panel = g_slist_index (panels,
					  ext->info->widget->parent);
		if (d->panel < 0)
			d->panel = 0;
	} else {
		d->panel = 0;
		d->pos = 0;
	}

	gtk_signal_connect_full
		(GTK_OBJECT (dlg), "clicked",
		 GTK_SIGNAL_FUNC (reload_applet_callback),
		 NULL,
		 d,
		 destroy_reload_callback_data,
		 FALSE /*object*/,
		 FALSE /*after*/);

	gtk_widget_show (dlg);

	ext->clean_remove = TRUE;
#endif
}

static void
extern_clean (Extern ext)
{
	CORBA_Environment        env;
	PortableServer_ObjectId *id;
	PortableServer_POA       poa;

	CORBA_exception_init (&env);

	poa = bonobo_poa ();

	/*
	 * to catch any weird cases, we won't be able to 
	 * at the position here though so it will go to 0,0
	 */
	extern_before_remove (ext);

	g_free (ext->iid);
	ext->iid = NULL;

	g_free (ext->config_string);
	ext->config_string = NULL;

	CORBA_Object_release (ext->pspot, &env);
	ext->pspot = NULL;

	CORBA_Object_release (ext->applet, &env);
	ext->applet = NULL;

	id = PortableServer_POA_servant_to_id (poa, ext, &env);
	PortableServer_POA_deactivate_object (poa, id, &env);
	CORBA_free (id);

	POA_GNOME_PanelSpot__fini ((PortableServer_Servant) ext, &env);
	
	if (ext->send_draw_timeout) {
		gtk_timeout_remove (ext->send_draw_timeout);
		ext->send_draw_timeout = 0;
	}
	if (ext->send_draw_idle) {
		gtk_idle_remove (ext->send_draw_idle);
		ext->send_draw_idle = 0;
	}

	extern_unref (ext);

	CORBA_exception_free (&env);
}

static void
extern_socket_destroy(GtkWidget *w, gpointer data)
{
	GtkSocket *socket = GTK_SOCKET(w);
	Extern     ext = data;

	if (socket->same_app &&
	    socket->plug_window != NULL) {
		GtkWidget *plug_widget;
		gdk_window_get_user_data (socket->plug_window,
					  (gpointer *)&plug_widget);
		if(plug_widget != NULL) {
			/* XXX: hackaround to broken gtkplug/gtksocket!!!
			   KILL all references to ourselves on the plug
			   and all our references to the plug and then
			   destroy it.*/
			GtkWidget *toplevel = gtk_widget_get_toplevel (w);
			if (toplevel && GTK_IS_WINDOW (toplevel))
				gtk_window_remove_embedded_xid (GTK_WINDOW (toplevel), 
								GDK_WINDOW_XWINDOW (socket->plug_window));

			socket->plug_window = NULL;
			socket->same_app = FALSE;
			GTK_PLUG(plug_widget)->socket_window = NULL;
			GTK_PLUG(plug_widget)->same_app = FALSE;
			gtk_widget_destroy(plug_widget);
		}
	}

	if (ext->ebox != NULL)
		gtk_widget_destroy(ext->ebox);
	ext->ebox = NULL;

	extern_unref (ext);
}

static void
send_position_change (Extern ext)
{
	/*ingore this until we get an ior*/
	if (ext->applet) {
		CORBA_Environment  env;
		int                x, y;
		GtkWidget         *wid;

		wid = ext->ebox;

		x = y = 0;
		
		CORBA_exception_init (&env);

		/* 
		 * go the the toplevel panel widget
		 */
		for (;;) {
			if (!GTK_WIDGET_NO_WINDOW (wid)) {
				x += wid->allocation.x;
				y += wid->allocation.y;
			}

			if (wid->parent)
				wid = wid->parent;
			else
				break;
		}

		GNOME_Applet2_change_position (ext->applet, x, y, &env);
		if (BONOBO_EX (&env))
			panel_clean_applet (ext->info);

		CORBA_exception_free (&env);
	}
}

static void
ebox_size_allocate (GtkWidget *applet, GtkAllocation *alloc, Extern ext)
{
	if (ext->send_position)
		send_position_change (ext);
}

static void
socket_size_allocate (GtkWidget *applet, GtkAllocation *alloc)
{
	
	GtkRequisition req;

	gtk_widget_get_child_requisition (applet, &req);

	/* This hack must be here.  The problem is that since it is a two
	 * widget deep hierarchy, an applet that shrink will not cause the
	 * panel to allocate less space.  Such as a tasklist in dynamic
	 * mode.  Since applets always get their requisition, then if they
	 * are ever allocated more, that means we can have the panel resize
	 * them to their preferred size.  Here applet->parent is the
	 * PanelWidget actually. */
	if (req.width > 0 &&
	    req.height > 0 &&
	    (alloc->width > req.width ||
	     alloc->height > req.height))
		gtk_widget_queue_resize (applet->parent);

}

static void
socket_set_loading (GtkWidget *socket, PanelWidget *panel)
{
	static gboolean   tried_loading = FALSE;
	static GdkPixbuf *pb = NULL;
	int               size;

	if (!socket || !socket->window)
		return;

	size = CLAMP (panel->sz, 0, 14);

	if (!tried_loading) {
		char *file;

		file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
						  "gnome-unknown.png", TRUE, NULL);

		if (file) {
			pb = gdk_pixbuf_new_from_file (file, NULL);

			g_free (file);
		}
	}
	tried_loading = TRUE;

	if (pb) {
		GdkPixmap *pm = NULL;
		GdkPixbuf *scaled;

		if (gdk_pixbuf_get_width (pb)  != size ||
		    gdk_pixbuf_get_height (pb) != size) {
			scaled = gdk_pixbuf_scale_simple (pb, size, size,
							  GDK_INTERP_BILINEAR);
		} else {
			scaled = gdk_pixbuf_ref (pb);
		}

		gdk_pixbuf_render_pixmap_and_mask (scaled, &pm, NULL, 127);

		gdk_pixbuf_unref (scaled);

		if (pm) {
			gdk_window_set_back_pixmap (socket->window, pm, FALSE);

			gdk_pixmap_unref (pm);
		}
	}
}

static void
socket_unset_loading (GtkWidget *socket)
{
	g_return_if_fail (socket);

	if (socket->parent)
		gtk_widget_queue_resize (socket->parent);

	if (socket->window)
		gdk_window_set_back_pixmap (socket->window, NULL, FALSE);

	if (socket->parent) {
		gtk_widget_set_usize (socket->parent, -1, -1);

		gtk_signal_connect_after (GTK_OBJECT (socket),
					  "size_allocate",
					  GTK_SIGNAL_FUNC (socket_size_allocate),
					  NULL);
	}
}

/*
 * note that type should be APPLET_EXTERN_RESERVED or 
 * APPLET_EXTERN_PENDING only
 */
static CORBA_unsigned_long
reserve_applet_spot (Extern       ext,
		     PanelWidget *panel,
		     int          pos,
		     AppletType   type)
{
	GtkWidget  *socket;
	AppletInfo *info;
	gint        events;
	gint        size;

	ext->ebox = gtk_event_box_new ();

	/*
	 * FIXME: duplicated in panel_register_applet ?
	 */
	events  = gtk_widget_get_events (ext->ebox) | APPLET_EVENT_MASK;
	events &= ~(GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

	gtk_widget_set_events (ext->ebox, events);

	size = CLAMP (panel->sz, 0, 14);

	gtk_widget_set_usize (ext->ebox, size, size);

	gtk_signal_connect_after (GTK_OBJECT (ext->ebox),
				  "size_allocate",
				  GTK_SIGNAL_FUNC (ebox_size_allocate),
				  ext);

	gtk_widget_show (ext->ebox);

	socket = gtk_socket_new ();

	gtk_container_add (GTK_CONTAINER (ext->ebox), socket);

	gtk_widget_show (socket);

	/*
	 * we save the obj in the id field of the 
	 * appletinfo and the path in the path field.
	 */
	ext->info = NULL;

	info = panel_register_applet (ext->ebox, ext,
				      (GDestroyNotify)extern_clean,
				      panel, pos, ext->exactpos, type);
	if (!info) {
		/*
		 * the ebox is destroyed in panel_register_applet.
		 */
		ext->ebox = NULL;

		g_warning (_("Couldn't register applet."));

		return 0;
	}

	ext->info = applets_last->data;

	gtk_signal_connect (GTK_OBJECT (socket),
			    "destroy",
			    GTK_SIGNAL_FUNC (extern_socket_destroy),
			    extern_ref (ext));
	
	if (!GTK_WIDGET_REALIZED (socket))
		gtk_widget_realize (socket);

	socket_set_loading (socket, panel);

	return GDK_WINDOW_XWINDOW (socket->window);
}

/* Note exactpos may NOT be changed */
static PanelWidget *
get_us_position (const int   panel,
		 const int   pos,
		 const char *iid,
		 int        *newpos,
		 gboolean   *exactpos)
{
	PanelWidget *pw = NULL;

	*newpos = pos;

	/* Sanity? can this ever happen? */
	if (!iid) {
		g_warning ("get_us_position: iid == NULL, bad bad");
		iid = "foo";
	}

	if (panel < 0 || pos < 0) {
		char *key = g_strdup_printf ("%sApplet_Position_Memory/%s/",
					     PANEL_CONFIG_PATH,
					     iid);
		gnome_config_push_prefix (key);
		g_free (key);

		if (pos < 0)
			*newpos = gnome_config_get_int ("position=-1");
		if (panel < 0) {
			guint32 unique_id = gnome_config_get_int ("panel_unique_id=0");
			if (unique_id != 0) {
				pw = panel_widget_get_by_id (unique_id);
			}
			if (pw == NULL) {
				*newpos = -1;
			}
		}

		gnome_config_pop_prefix ();
	}

	if (pw == NULL && panel < 0)
		pw = panels->data;

	if (*newpos < 0)
		*newpos = 0;
	else if (exactpos != NULL)
		 *exactpos = TRUE;

	if (pw == NULL) {
		/*select the nth panel*/
		GSList *node = g_slist_nth (panels, panel);
		if (node == NULL)
			node = panels;
		/* There's always at least one */
		g_assert (node != NULL);
		pw = node->data;
	}

	return pw;
}

static void
extern_activate_panelspot (Extern ext)
{
	POA_GNOME_PanelSpot *panelspot_servant;
	PortableServer_POA   poa;
	CORBA_Environment    env;

	CORBA_exception_init (&env);

	poa = bonobo_poa ();

	panelspot_servant = (POA_GNOME_PanelSpot *)ext;

	panelspot_servant->_private = NULL;
	panelspot_servant->vepv     = &panelspot_vepv;

	POA_GNOME_PanelSpot__init (panelspot_servant, &env);
	
	CORBA_free (PortableServer_POA_activate_object (
				poa, panelspot_servant, &env));
	if (BONOBO_EX (&env)) {
		CORBA_exception_free (&env);
		return;
	}

	CORBA_exception_free (&env);
}

void
extern_load_applet (const char  *iid,
		    const char  *cfgpath,
		    PanelWidget *panel,
		    int          pos,
		    gboolean     exactpos,
		    gboolean     queue)
{
	Extern               ext;
	char                *cfg;

	dprintf ("extern_load_applet: %s\n", iid);

	if (string_empty (cfgpath))
		cfg = g_strconcat (PANEL_CONFIG_PATH,
				   "Applet_Dummy/", NULL);
	else
		cfg = g_strdup (cfgpath);

	ext = g_new0 (struct Extern_struct, 1);

	extern_activate_panelspot (ext);

	ext->refcount          = 1;
	ext->started           = FALSE;
	ext->exactpos          = exactpos;
	ext->send_position     = FALSE;
	ext->send_draw         = FALSE;
	ext->orient            = -1;
	ext->didnt_want_save   = FALSE;
	ext->clean_remove      = TRUE;
	ext->send_draw_timeout = 0;
	ext->send_draw_idle    = 0;
	ext->send_draw_queued  = FALSE;
	ext->pspot             = CORBA_OBJECT_NIL;
	ext->applet            = CORBA_OBJECT_NIL;
	ext->iid               = g_strdup (iid);
	ext->config_string     = cfg;

	if (panel) {
		PanelWidget *pw;
		gboolean     exactpos;

		pw = get_us_position (-1, pos, iid, &pos, &exactpos);

		if (pw == panel)
			ext->exactpos = exactpos;
		else
			pos = 0;
	}
	else
		panel = get_us_position (-1, -1, iid, &pos, &ext->exactpos);
			
	if (!reserve_applet_spot (ext, panel, pos, APPLET_EXTERN_PENDING)) {
		g_warning (_("Whoops! for some reason we "
			     "can't add to the panel"));
		extern_clean (ext);
		return;
	}

	if (!queue) {
		extern_activate (ext);
		ext->started = TRUE;
	}
#ifdef EXTERN_DEBUG
	else
		dprintf ("extern_load_applet: queueing %s\n", ext->iid);
#endif
}

void
extern_load_queued (void)
{
	GSList *li;

	for (li = applets; li ; li = li->next) {
		AppletInfo *info = li->data;

		if (info->type == APPLET_EXTERN_PENDING ||
		    info->type == APPLET_EXTERN_RESERVED) {
			Extern ext = info->data;

			if (!ext->started) {
				extern_activate (ext);
				ext->started = TRUE;
			}
		}
	}
}

/********************* CORBA Stuff *******************/


static GNOME_PanelSpot
s_panel_add_applet (PortableServer_Servant servant,
		    const GNOME_Applet panel_applet,
		    const CORBA_char *goad_id,
		    CORBA_char ** cfgpath,
		    CORBA_char ** globcfgpath,
		    CORBA_unsigned_long* wid,
		    CORBA_Environment *ev)
{
	return s_panel_add_applet_full (servant, panel_applet, goad_id, -1, -1,
					cfgpath, globcfgpath, wid, ev);
}

static GNOME_PanelSpot
s_panel_add_applet_full (PortableServer_Servant   servant,
			 const GNOME_Applet       panel_applet,
			 const CORBA_char        *goad_id,
			 const CORBA_short        panel,
			 const CORBA_short        pos,
			 CORBA_char            **cfgpath,
			 CORBA_char            **globcfgpath,
			 CORBA_unsigned_long    *wid,
			 CORBA_Environment      *ev)
{
	PortableServer_POA       poa;
	GNOME_PanelAppletBooter  booter;
	POA_GNOME_PanelSpot     *panelspot_servant;
	PanelWidget             *pw;
	Extern                   ext;
	GSList                  *l;
	int                      newpos;

	if (panel_in_startup)
		return CORBA_OBJECT_NIL;

	poa = bonobo_poa ();

	booter = pop_outside_extern (goad_id);

	if (booter != CORBA_OBJECT_NIL) {
		GNOME_PanelSpot psot;
		/* 
		 * yeah, we do dump the panel and pos, since they
		 * really don't make any sesnse, and aren't really
		 * used anywhere in the first place 
		 */
		psot = GNOME_PanelAppletBooter_add_applet (booter,
							   panel_applet,
							   goad_id,
							   cfgpath,
							   globcfgpath,
							   wid,
							   ev);

		/* 
		 * if we succeeded, then all fine and dandy, otherwise
		 * this thing was some stale thingie, so just ignore it
		 * and launch us into the panel.  This way we're always
		 * getting the applet, even if the booter crashed or we
		 * had a stale one around 
		 */
		if (!BONOBO_EX (ev))
			return psot;

		CORBA_exception_free (ev);
		CORBA_exception_init (ev);
	}
	
	for (l = applets; l; l = l->next) {
		AppletInfo *info = l->data;

		if (info && info->type == APPLET_EXTERN_PENDING) {
			Extern ext = info->data;

			g_assert (ext);
			g_assert (ext->info == info);
			g_assert (ext->iid);

			if (!strcmp (ext->iid, goad_id)) {
				GtkWidget *socket;

				/*
				 * we started this and already reserved a spot
				 * for it, including the socket widget
				 */
				socket = GTK_BIN (info->widget)->child;

				if (!socket) {
					g_warning (_("No socket was created"));
					return CORBA_OBJECT_NIL;
				}

				ext->applet = CORBA_Object_duplicate (panel_applet, ev);

				*cfgpath = CORBA_string_dup (ext->config_string);

				*globcfgpath = CORBA_string_dup (PANEL_CONFIG_PATH);

				info->type = APPLET_EXTERN_RESERVED;

				*wid = GDK_WINDOW_XWINDOW (socket->window);

				dprintf ("\nSOCKET XID: %lX\n\n", (long)*wid);

				panelspot_servant = (POA_GNOME_PanelSpot *)ext;

				ext->pspot = PortableServer_POA_servant_to_reference (
								poa,
								panelspot_servant,
								ev);
				BONOBO_RET_VAL_EX (ev, CORBA_OBJECT_NIL);

				return CORBA_Object_duplicate (ext->pspot, ev);
			}
		}
	}
	
	/*
	 * this is an applet that was started from outside, otherwise
	 * we would have already reserved a spot for it
	 */
	ext = g_new0 (struct Extern_struct, 1);

	ext->refcount      = 1;
	ext->started       = FALSE;
	ext->exactpos      = FALSE;
	ext->send_position = FALSE;
	ext->send_draw     = FALSE;
	ext->orient        = -1;
	ext->applet        = CORBA_Object_duplicate (panel_applet, ev);
	ext->iid           = g_strdup (goad_id);
	ext->config_string = NULL;

	extern_activate_panelspot (ext);

	panelspot_servant = (POA_GNOME_PanelSpot *)ext;

	ext->pspot = PortableServer_POA_servant_to_reference (
						poa,
						panelspot_servant,
						ev);
	BONOBO_RET_VAL_EX (ev, CORBA_OBJECT_NIL);

	pw = get_us_position (panel, pos, goad_id, &newpos, &ext->exactpos);

	*wid = reserve_applet_spot (ext, pw, newpos,
				    APPLET_EXTERN_RESERVED);
	if (!*wid) {
		extern_clean (ext);

		*globcfgpath = NULL;
		*cfgpath     = NULL;

		return CORBA_OBJECT_NIL;
	}

	*cfgpath     = CORBA_string_dup (PANEL_CONFIG_PATH "Applet_Dummy/");
	*globcfgpath = CORBA_string_dup (PANEL_CONFIG_PATH);

	return CORBA_Object_duplicate (ext->pspot, ev);
}

static void
s_panel_quit (PortableServer_Servant servant, CORBA_Environment *ev)
{
	panel_quit ();
}

static CORBA_boolean
s_panel_get_in_drag (PortableServer_Servant servant, CORBA_Environment *ev)
{
	return panel_applet_in_drag;
}

static GNOME_StatusSpot
s_panel_add_status (PortableServer_Servant  servant,
		    CORBA_unsigned_long    *wid,
		    CORBA_Environment      *ev)
{
	PortableServer_POA    poa;
	POA_GNOME_StatusSpot *statusspot_servant;
	GNOME_StatusSpot      acc;
	StatusSpot           *ss;

	poa = bonobo_poa ();
	
	*wid = 0;
	
	ss = new_status_spot();
	if (!ss)
		return CORBA_OBJECT_NIL;
	
	statusspot_servant = (POA_GNOME_StatusSpot *)ss;

	statusspot_servant->_private = NULL;
	statusspot_servant->vepv     = &statusspot_vepv;

	POA_GNOME_StatusSpot__init (statusspot_servant, ev);
	
	CORBA_free (PortableServer_POA_activate_object (poa, 
							statusspot_servant,
							ev));
	BONOBO_RET_VAL_EX (ev, CORBA_OBJECT_NIL);

	acc = PortableServer_POA_servant_to_reference (poa,
						       statusspot_servant,
						       ev);
	BONOBO_RET_VAL_EX (ev, CORBA_OBJECT_NIL);

	ss->sspot = CORBA_Object_duplicate (acc, ev);
	BONOBO_RET_VAL_EX (ev, CORBA_OBJECT_NIL);

	*wid = ss->wid;

	return CORBA_Object_duplicate (acc, ev);
}

static void
s_panel_notice_config_changes(PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	load_up_globals();
}

/** Panel2 additions **/

void
s_panel_suggest_sync (PortableServer_Servant _servant,
		      CORBA_Environment * ev)
{
	need_complete_save = TRUE;
	panel_config_sync();
}

void
s_panel_add_launcher (PortableServer_Servant _servant,
		      const CORBA_char * launcher_desktop,
		      const CORBA_short panel,
		      const CORBA_short pos,
		      CORBA_Environment * ev)
{
	Launcher *launcher;
	PanelWidget *panel_widget;

	if (panel_in_startup)
		return;

	g_assert (panels != NULL);

	panel_widget = g_slist_nth_data (panels, panel);
	if (panel_widget == NULL)
		panel_widget = panels->data;

	launcher = load_launcher_applet (launcher_desktop,
					 panel_widget, pos, FALSE);
	if (launcher != NULL)
		launcher_hoard (launcher);
}

void
s_panel_ask_about_launcher (PortableServer_Servant _servant,
			    const CORBA_char * exec_string,
			    const CORBA_short panel,
			    const CORBA_short pos,
			    CORBA_Environment * ev)
{
	PanelWidget *panel_widget;

	if (panel_in_startup)
		return;

	g_assert (panels != NULL);

	panel_widget = g_slist_nth_data (panels, panel);
	if (panel_widget == NULL)
		panel_widget = panels->data;

	ask_about_launcher (exec_string, panel_widget, pos, FALSE);
}

void
s_panel_add_launcher_from_info (PortableServer_Servant _servant,
				const CORBA_char * name,
				const CORBA_char * comment,
				const CORBA_char * exec,
				const CORBA_char * icon,
				const CORBA_short panel,
				const CORBA_short pos,
				CORBA_Environment * ev)
{
	PanelWidget *panel_widget;

	if (panel_in_startup)
		return;

	g_assert (panels != NULL);

	panel_widget = g_slist_nth_data (panels, panel);
	if (panel_widget == NULL)
		panel_widget = panels->data;

	load_launcher_applet_from_info (name, comment, exec,
					icon, panel_widget, pos, FALSE);
}

void
s_panel_add_launcher_from_info_url (PortableServer_Servant _servant,
				    const CORBA_char * name,
				    const CORBA_char * comment,
				    const CORBA_char * url,
				    const CORBA_char * icon,
				    const CORBA_short panel,
				    const CORBA_short pos,
				    CORBA_Environment * ev)
{
	PanelWidget *panel_widget;

	if (panel_in_startup)
		return;

	g_assert (panels != NULL);

	panel_widget = g_slist_nth_data (panels, panel);
	if (panel_widget == NULL)
		panel_widget = panels->data;

	load_launcher_applet_from_info_url (name, comment, url,
					    icon, panel_widget, pos, FALSE);
}

void
s_panel_run_box (PortableServer_Servant  servant,
		 const CORBA_char       *initial_string,
		 CORBA_Environment      *ev)
{
	if (!initial_string || initial_string [0] == '\0')
		show_run_dialog ();
	else
		show_run_dialog_with_text (initial_string);
}

void
s_panel_main_menu (PortableServer_Servant _servant,
		   CORBA_Environment * ev)
{
	PanelWidget *panel;
	GtkWidget *menu, *basep;

	/* check if anybody else has a grab */
	if (gdk_pointer_grab (GDK_ROOT_PARENT(), FALSE, 
			      0, NULL, NULL, GDK_CURRENT_TIME)
	    != GrabSuccess) {
		return;
	} else {
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
	}

	panel = panels->data;
	menu = make_popup_panel_menu (panel);
	basep = panel->panel_parent;
	if (BASEP_IS_WIDGET(basep)) {
		BASEP_WIDGET(basep)->autohide_inhibit = TRUE;
		basep_widget_autohide (BASEP_WIDGET (basep));
	}
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			NULL, NULL, 0, GDK_CURRENT_TIME);
}

void
s_panel_launch_an_applet (PortableServer_Servant _servant,
			  const CORBA_char *goad_id,
			  const GNOME_PanelSpot spot,
			  CORBA_Environment *ev)
{
#ifdef FIXME
	if (goad_id != NULL &&
	    spot != CORBA_OBJECT_NIL) {
		CORBA_Object applet;

		/* push us */
		push_outside_extern (goad_id, spot, ev);

		/* launch the applet, EVIL! this way shlib applets
		 * get dumped into the panel.  Reason?  Simple:  the
		 * shlib applet logic is complex, broken, evil and
		 * whatever, we do not want to impose it upon an
		 * unsuspecting PanelSpot. */
		applet = goad_server_activate_with_id (NULL, goad_id,
						       GOAD_ACTIVATE_NEW_ONLY |
						       GOAD_ACTIVATE_ASYNC,
						       NULL);

		CORBA_Object_release (applet, ev);
	}
#endif
}

/*** PanelSpot stuff ***/

static CORBA_char *
s_panelspot_get_tooltip(PortableServer_Servant servant,
			CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;
	GtkTooltipsData *d = gtk_tooltips_data_get(ext->ebox);
	if(!d || !d->tip_text)
		return CORBA_string_dup("");
	else
		return CORBA_string_dup(d->tip_text);
}

static void
s_panelspot_set_tooltip(PortableServer_Servant servant,
			const CORBA_char *val,
			CORBA_Environment *ev)
{
	Extern ext = (Extern )servant;
	if(val && *val)
		gtk_tooltips_set_tip (panel_tooltips, ext->ebox, val, NULL);
	else
		gtk_tooltips_set_tip (panel_tooltips, ext->ebox, NULL, NULL);
}

static CORBA_short
s_panelspot_get_parent_panel(PortableServer_Servant servant,
			     CORBA_Environment *ev)
{
	int panel;
	GSList *list;
	gpointer p;
	Extern ext = (Extern)servant;

	g_assert(ext);
	g_assert(ext->info);

	p = PANEL_WIDGET(ext->info->widget->parent);

	for(panel=0,list=panels;list!=NULL;list=g_slist_next(list),panel++)
		if(list->data == p)
			return panel;
	return -1;
}

static CORBA_short
s_panelspot_get_spot_pos(PortableServer_Servant servant,
			 CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;
	AppletData *ad;

	g_assert(ext);
	g_assert(ext->info);
	
	ad = gtk_object_get_data(GTK_OBJECT(ext->info->widget),
				 PANEL_APPLET_DATA);
	if(!ad)
		return -1;
	return ad->pos;
}

static GNOME_Panel_OrientType
s_panelspot_get_parent_orient(PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;
	PanelWidget *panel;

	g_assert(ext);
	g_assert(ext->info);

	panel = PANEL_WIDGET(ext->info->widget->parent);

	if(!panel) {
		g_warning("%s:%d ??? Applet with no panel ???",
			 __FILE__, __LINE__);
		return ORIENT_UP;
	}

	return get_applet_orient(panel);
}

static CORBA_short
s_panelspot_get_parent_size(PortableServer_Servant servant,
			    CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;
	PanelWidget *panel;

	g_assert(ext);
	g_assert(ext->info);

	panel = PANEL_WIDGET(ext->info->widget->parent);

	if(!panel) {
		g_warning("%s:%d ??? Applet with no panel ???",
			 __FILE__, __LINE__);
		return SIZE_STANDARD;
	}

	return panel->sz;
}

static CORBA_short
s_panelspot_get_free_space(PortableServer_Servant servant,
			   CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;
	PanelWidget *panel;

	g_assert(ext);
	g_assert(ext->info);

	panel = PANEL_WIDGET(ext->info->widget->parent);

	if(!panel) {
		g_warning("%s:%d ??? Applet with no panel ???",
			 __FILE__, __LINE__);
		return 0;
	}
	
	return panel_widget_get_free_space(panel,ext->info->widget);
}

static CORBA_boolean
s_panelspot_get_send_position(PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;

	g_assert(ext);
	
	return ext->send_position;
}

static void
s_panelspot_set_send_position(PortableServer_Servant servant,
			      CORBA_boolean enable,
			      CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;

	g_assert(ext);
	
	ext->send_position = enable?TRUE:FALSE;
}

static CORBA_boolean
s_panelspot_get_send_draw(PortableServer_Servant servant,
			  CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;

	g_assert(ext);
	
	return ext->send_draw;
}

static void
s_panelspot_set_send_draw(PortableServer_Servant servant,
			  CORBA_boolean enable,
			  CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;

	g_assert(ext);
	
	ext->send_draw = enable?TRUE:FALSE;
}

static GNOME_Panel_RgbImage *
s_panelspot_get_rgb_background(PortableServer_Servant servant,
			       CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;
	PanelWidget *panel;
	GNOME_Panel_RgbImage *image;
	int w, h, rowstride;
	guchar *rgb;
	int r,g,b;

	g_assert(ext);
	g_assert(ext->info);

	panel = PANEL_WIDGET(ext->info->widget->parent);

	panel_widget_get_applet_rgb_bg(panel, ext->ebox,
				       &rgb,&w,&h,&rowstride,
				       TRUE,&r,&g,&b);

	image = GNOME_Panel_RgbImage__alloc();
	image->width = w;
	image->height = h;
		
	/* if we got an rgb */
	if(rgb) {
		image->data._buffer = CORBA_sequence_CORBA_octet_allocbuf(h*rowstride);
		image->data._length = image->data._maximum = h*rowstride;
		memcpy(image->data._buffer,rgb,sizeof(guchar)*h*rowstride);
		image->rowstride = rowstride;
		image->color_only = FALSE;
		g_free(rgb);
	} else { /* we must have gotten a color */
		image->data._buffer = CORBA_sequence_CORBA_octet_allocbuf(3);
		image->data._length = image->data._maximum = 3;
		*(image->data._buffer) = r;
		*(image->data._buffer+1) = g;
		*(image->data._buffer+2) = b;
		image->rowstride = 0;
		image->color_only = TRUE;
	}
	CORBA_sequence_set_release(&image->data, TRUE);

	return image;
}

static void
s_panelspot_register_us (PortableServer_Servant  servant,
			 CORBA_Environment      *ev)
{
	PanelWidget *panel;
	Extern       ext = (Extern)servant;

	g_assert (ext && ext->info);
	
	dprintf ("register ext: %lX\n", (long)ext);
	dprintf ("register ext->info: %lX\n", (long)(ext->info));

	panel = PANEL_WIDGET (ext->info->widget->parent);
	if (!panel) {
		g_warning ("%s:%d ??? Applet with no panel ???",
			   __FILE__, __LINE__);
		return;
	}

	/*
	 * no longer pending
	 */
	ext->info->type = APPLET_EXTERN;

	/*
	 * from now on warn on unclean removal
	 */
	ext->clean_remove = FALSE;

	if (ext->ebox)
		socket_unset_loading (GTK_BIN (ext->ebox)->child);

	extern_ref (ext);

	if (ext->info) {
		freeze_changes (ext->info);

		orientation_change (ext->info, panel);

		size_change (ext->info, panel);

		back_change (ext->info, panel);

		if (ext->send_position)
			send_position_change (ext);

		thaw_changes (ext->info);
	}

	if (ext->applet)
		GNOME_Applet2_set_tooltips_state (ext->applet,
						  global_config.tooltips_enabled,
						  ev);

	if (BONOBO_EX (ev)) {
		/*
		 * well, we haven't really gotten anywhere, 
		 * so it's not a good idea to bother the user
		 * with restarting us
		 */
		ext->clean_remove = TRUE;

		panel_clean_applet (ext->info);
	}

	extern_unref (ext);
}


static void
s_panelspot_unregister_us(PortableServer_Servant servant,
			  CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;

	extern_save_last_position (ext, TRUE /* sync */);

	panel_clean_applet(ext->info);
}

static void
s_panelspot_abort_load(PortableServer_Servant servant,
		       CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;

	g_assert (ext != NULL);
	g_assert (ext->info != NULL);
	
	/*only reserved spots can be canceled, if an applet
	  wants to chance a pending applet it needs to first
	  user reserve spot to obtain id and make it EXTERN_RESERVED*/
	if (ext->info->type != APPLET_EXTERN_RESERVED)
		return;

	ext->clean_remove = TRUE;
	panel_clean_applet (ext->info);
}

static void
s_panelspot_show_menu(PortableServer_Servant servant,
		      CORBA_Environment *ev)
{
	GtkWidget *panel;
	Extern ext = (Extern)servant;
	
	dprintf("show menu ext: %lX\n",(long)ext);
	dprintf("show menu ext->info: %lX\n",(long)(ext->info));

	g_assert (ext != NULL);
	g_assert (ext->info != NULL);

	panel = get_panel_parent (ext->info->widget);

	if (!ext->info->menu)
		create_applet_menu(ext->info, BASEP_IS_WIDGET (panel));

	if (BASEP_IS_WIDGET (panel)) {
		BASEP_WIDGET(panel)->autohide_inhibit = TRUE;
		basep_widget_queue_autohide(BASEP_WIDGET(panel));
	}

	ext->info->menu_age = 0;
#ifdef FIXME
	gtk_menu_popup(GTK_MENU(ext->info->menu), NULL, NULL,
		       global_config.off_panel_popups?applet_menu_position:NULL,
		       ext->info, 3, GDK_CURRENT_TIME);
#endif
}


static void
s_panelspot_drag_start(PortableServer_Servant servant,
		       CORBA_Environment *ev)
{
	PanelWidget *panel;
	Extern ext = (Extern)servant;

	g_assert (ext != NULL);
	g_assert (ext->info != NULL);

	panel = PANEL_WIDGET (ext->info->widget->parent);
	if (panel == NULL) {
		g_warning("%s:%d ??? Applet with no panel ???",
			 __FILE__, __LINE__);
		return;
	}

	panel_widget_applet_drag_start (panel, ext->info->widget,
					PW_DRAG_OFF_CENTER);
}

static void
s_panelspot_drag_stop(PortableServer_Servant servant,
		      CORBA_Environment *ev)
{
	PanelWidget *panel;
	Extern ext = (Extern)servant;

	g_assert(ext != NULL);
	g_assert(ext->info != NULL);

	panel = PANEL_WIDGET(ext->info->widget->parent);
	if (panel == NULL) {
		g_warning("%s:%d ??? Applet with no panel ???",
			 __FILE__, __LINE__);
		return;
	}

	panel_widget_applet_drag_end(panel);
}

static void
s_panelspot_add_callback(PortableServer_Servant servant,
			 const CORBA_char *callback_name,
			 const CORBA_char *stock_item,
			 const CORBA_char *menuitem_text,
			 CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;

	printf("add callback ext: %lX\n",(long)ext);

	g_assert(ext != NULL);
	g_assert(ext->info != NULL);

	if (commie_mode &&
	    callback_name != NULL &&
	    (strcmp (callback_name, "preferences") == 0 ||
	     strcmp (callback_name, "properties") == 0))
		return;

	applet_add_callback(ext->info, callback_name, stock_item,
			    menuitem_text);
}

static void
s_panelspot_remove_callback(PortableServer_Servant servant,
			    const CORBA_char *callback_name,
			    CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;

	g_assert(ext != NULL);
	g_assert(ext->info != NULL);
	applet_remove_callback(ext->info, callback_name);
}

static void
s_panelspot_callback_set_sensitive(PortableServer_Servant servant,
				   const CORBA_char *callback_name,
				   const CORBA_boolean sensitive,
				   CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;

	g_assert(ext != NULL);
	g_assert(ext->info != NULL);
	applet_callback_set_sensitive(ext->info, callback_name, sensitive);
}

static void
s_panelspot_sync_config(PortableServer_Servant servant,
			CORBA_Environment *ev)
{
	Extern ext = (Extern)servant;

	g_assert(ext != NULL);
	g_assert(ext->info != NULL);
	applets_to_sync = TRUE;
	panel_config_sync();
}

static int
save_next_idle(gpointer data)
{
	save_next_applet();
	return FALSE;
}

void
extern_save_applet (AppletInfo *info,
		    gboolean    ret)
{
	char *buf;
	PanelWidget *panel;
	AppletData *ad;
	Extern ext;
	int panel_num;
	
	ext = info->data;
	
	buf = g_strdup_printf ("%sApplet_Config/Applet_%d/",
			       PANEL_CONFIG_PATH,
			       info->applet_id + 1);
	gnome_config_push_prefix (buf);
	g_free (buf);

	panel = PANEL_WIDGET (info->widget->parent);
	ad = gtk_object_get_data (GTK_OBJECT (info->widget),
				  PANEL_APPLET_DATA);

	panel_num = g_slist_index (panels, panel);
	if (panel_num == -1) {
		/* Eeeeeeeeeeeeeek! */
		gnome_config_set_string ("id", EMPTY_ID);
		gnome_config_pop_prefix ();
		gnome_config_sync ();
		/*save next applet, but from an idle handler, so that
		  this call returns*/
		gtk_idle_add (save_next_idle, NULL);
		return;
	}
		
	/*have the applet do it's own session saving*/
	if (ret) {
		/* wanted to save */
		ext->didnt_want_save = FALSE;

		gnome_config_set_string ("id", EXTERN_ID);
		gnome_config_set_string ("goad_id",
					 ext->iid);
		gnome_config_set_int ("position", ad->pos);
		gnome_config_set_int ("panel", panel_num);
		gnome_config_set_int ("unique_panel_id", panel->unique_id);
		gnome_config_set_bool ("right_stick",
				       panel_widget_is_applet_stuck (panel,
								     info->widget));
	} else {
		/* ahh didn't want to save */
		ext->didnt_want_save = TRUE;

		gnome_config_set_string ("id", EMPTY_ID);
	}

	gnome_config_pop_prefix ();

	gnome_config_sync ();
	/*save next applet, but from an idle handler, so that
	  this call returns*/
	gtk_idle_add (save_next_idle, NULL);
}

static void
s_panelspot_done_session_save(PortableServer_Servant servant,
			      CORBA_boolean ret,
			      CORBA_unsigned_long cookie,
			      CORBA_Environment *ev)
{
	GSList *cur;
	AppletInfo *info;
	
	/*ignore bad cookies*/
	if(cookie != ss_cookie)
		return;

	/*increment cookie to kill the timeout warning*/
	ss_cookie++;
	
	if(ss_timeout_dlg) {
		gtk_widget_destroy(ss_timeout_dlg);
		ss_timeout_dlg = NULL;
	}

	if(g_slist_length(applets)<=ss_cur_applet) {
		ss_done_save = TRUE;
		return;
	}
	
	cur = g_slist_nth(applets,ss_cur_applet);
	
	if(!cur) {
		ss_done_save = TRUE;
		return;
	}
	
	info = cur->data;

	/*hmm, this came from a different applet?, we are
	  getting seriously confused*/
	if(info->type!=APPLET_EXTERN ||
	   (gpointer)servant!=(gpointer)info->data) {
		applets_to_sync = TRUE; /*we need to redo this yet again*/
		/*save next applet, but from an idle handler, so that
		  this call returns*/
		gtk_idle_add(save_next_idle,NULL);
		return;
	}

	extern_save_applet (info, ret);
}

/*** StatusSpot stuff ***/

static void
s_statusspot_remove(POA_GNOME_StatusSpot *servant,
		    CORBA_Environment *ev)
{
	StatusSpot *ss = (StatusSpot *)servant;
	status_spot_remove(ss, TRUE);
}

/*
 * extern_shutdown:
 *
 * Unregisters the #GNOME::Panel object and shuts down the ORB.
 */
void
extern_shutdown (void)
{
	CORBA_Environment  env;
	CORBA_ORB          orb;
	PortableServer_POA poa;
	GNOME_Panel        panel;

	CORBA_exception_init (&env);

	orb = bonobo_orb ();
	poa = bonobo_poa ();

	panel = PortableServer_POA_servant_to_reference (poa,
							 &panel_servant,
							 &env);
	if (BONOBO_EX (&env)) {
		CORBA_exception_free (&env);
		return;
	}

	bonobo_activation_active_server_unregister ("OAFIID:GNOME_Panel",
						    panel);

	CORBA_Object_release (panel, &env);

	CORBA_ORB_shutdown (orb, CORBA_FALSE, &env);

	CORBA_exception_free (&env);
}

/*
 * extern_init:
 * 
 * Starts a #GNOME_Panel server and registers with the 
 * bonobo-actvation daemon.
 *
 * Return value: #EXTERN_SUCCESS on success, #EXTERN_FAILURE on 
 *               failure, #EXTERN_ALREADY_ACTIVE if there is a 
 *               #GNOME_Panel server already registered.
 */
ExternResult
extern_init ()
{
	PortableServer_POA        poa;
	CORBA_Environment         env;
	GNOME_Panel               panel;
	Bonobo_RegistrationResult result;
	ExternResult              retval;

	CORBA_exception_init (&env);

	poa = bonobo_poa ();

	POA_GNOME_Panel2__init (&panel_servant, &env);

	panel = PortableServer_POA_servant_to_reference (poa,
							 &panel_servant,
							 &env);
	if (BONOBO_EX (&env)) {
		CORBA_exception_free (&env);
		return EXTERN_FAILURE;
		}

	result = bonobo_activation_active_server_register (
					"OAFIID:GNOME_Panel",
					panel);
	switch (result) {
	case Bonobo_ACTIVATION_REG_SUCCESS:
		retval = EXTERN_SUCCESS;
		break;
	case Bonobo_ACTIVATION_REG_ALREADY_ACTIVE:
		retval = EXTERN_ALREADY_ACTIVE;
		break;
	default:
		retval = EXTERN_FAILURE;
		break;
	}

	CORBA_Object_release (panel, &env);

	CORBA_exception_free (&env);

	return retval;
}

static void
send_draw (Extern ext)
{
	CORBA_Environment env;

	CORBA_exception_init (&env);

	if (ext->applet != NULL) {
		GNOME_Applet2_draw (ext->applet, &env);
		if (BONOBO_EX (&env))
			panel_clean_applet (ext->info);
	}

	CORBA_exception_free (&env);
}

static gboolean
send_draw_timeout(gpointer data)
{
	Extern ext = data;
	if(ext->send_draw && ext->send_draw_queued) {
		ext->send_draw_queued = FALSE;
		send_draw(ext);
		return TRUE;
	}
	ext->send_draw_timeout = 0;
	return FALSE;
}

static gboolean
send_draw_idle(gpointer data)
{
	Extern ext = data;
	ext->send_draw_idle = 0;
	if(!ext->send_draw)
		return FALSE;
	if(!ext->send_draw_timeout) {
		ext->send_draw_queued = FALSE;
		send_draw(ext);
		ext->send_draw_timeout =
			gtk_timeout_add(1000, send_draw_timeout, ext);
	} else 
		ext->send_draw_queued = TRUE;
	return FALSE;
}

void
extern_send_draw(Extern ext)
{
	if(!ext || !ext->applet || !ext->send_draw)
		return;
	if(!ext->send_draw_idle)
		ext->send_draw_idle =
			gtk_idle_add(send_draw_idle, ext);
}

void
extern_save_last_position (Extern ext, gboolean sync)
{
	char *key;
	int panel_num;
	AppletData *ad;

	ext->clean_remove = TRUE;

	if (!ext->iid)
		return;

	/* Here comes a hack.  We probably want the next applet to load at
	 * a similar location.  If none is given.  Think xchat, or any
	 * app that just adds applets on it's own by just running them. */
	key = g_strdup_printf ("%sApplet_Position_Memory/%s/",
			       PANEL_CONFIG_PATH,
			       ext->iid);
	gnome_config_push_prefix (key);
	g_free (key);

	ad = gtk_object_get_data (GTK_OBJECT (ext->info->widget),
				  PANEL_APPLET_DATA);

	panel_num = g_slist_index (panels, ext->info->widget->parent);

	if (ad != NULL)
		gnome_config_set_int ("position", ad->pos);
	if (panel_num >= 0)
		gnome_config_set_int
			("panel_unique_id",
			 PANEL_WIDGET (ext->info->widget->parent)->unique_id);
	
	gnome_config_pop_prefix ();
	if (sync)
		gnome_config_sync ();
}

