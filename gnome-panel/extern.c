/* Gnome panel: extern applet functions
 * (C) 1997,1998,1999,2000 the Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors:  George Lebl
 *           Federico Mena
 *           Miguel de Icaza
 */

#include "config.h"
#include <gnome.h>

#include <gdk/gdkx.h>
#include <string.h>
#include <signal.h>

#include "panel-include.h"
#include "gnome-panel.h"
#include "gnome-run.h"

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

#define pg_return_if_fail(evp,x) {if(!(x)) { g_warning("file %s: line %d: Corba Exception: type = %d exid = %s\n", __FILE__, __LINE__, (evp)->_major, (evp)->_repo_id); return; }}
#define pg_return_val_if_fail(evp,x,y) {if(!(x)) { g_warning("file %s: line %d: Corba Exception: type = %d exid = %s\n", __FILE__, __LINE__, (evp)->_major, (evp)->_repo_id); return y;}}

extern GSList *panels;

extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

extern int config_sync_timeout;
extern int applets_to_sync;
extern int panels_to_sync;
extern int need_complete_save;

extern GtkTooltips *panel_tooltips;

extern GlobalConfig global_config;
extern gboolean commie_mode;

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

extern int ss_cur_applet;
extern gboolean ss_done_save;
extern GtkWidget* ss_timeout_dlg;
extern gushort ss_cookie;

extern gboolean panel_in_startup;

/* Launching applets into other things then the panel */

typedef struct {
	char *goad_id;
	GNOME_PanelAppletBooter booter;
} OutsideExtern;

static GSList *outside_externs = NULL;

static void
push_outside_extern (const char *goad_id,
		     const GNOME_PanelAppletBooter booter,
		     CORBA_Environment *ev)
{
	OutsideExtern *oe;

	g_return_if_fail (goad_id != NULL);
	g_return_if_fail (booter != CORBA_OBJECT_NIL);

	oe = g_new0 (OutsideExtern, 1);

	oe->goad_id = g_strdup (goad_id);
	oe->booter = CORBA_Object_duplicate (booter, ev);

	outside_externs = g_slist_prepend (outside_externs, oe);
}

static GNOME_PanelAppletBooter
pop_outside_extern (const char *goad_id)
{
	GSList *li;

	g_return_val_if_fail (goad_id != NULL, CORBA_OBJECT_NIL);

	for (li = outside_externs; li != NULL; li = li->next) {
		OutsideExtern *oe = li->data;

		if (strcmp (oe->goad_id, goad_id) == 0) {
			GNOME_PanelAppletBooter booter = oe->booter;
			g_free (oe->goad_id);
			oe->goad_id = NULL;
			g_free (oe);
			return booter;
		}
	}
	return CORBA_OBJECT_NIL;
}

/********************* CORBA Stuff *******************/

CORBA_ORB orb = NULL;
CORBA_Environment ev;
PortableServer_POA thepoa;

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
extern_start_new_goad_id(Extern *e)
{
        CORBA_Environment ev;
	CORBA_exception_init(&ev);
	CORBA_Object_release(goad_server_activate_with_id(NULL, e->goad_id, GOAD_ACTIVATE_NEW_ONLY|GOAD_ACTIVATE_ASYNC, NULL),&ev);
	CORBA_exception_free(&ev);
}

Extern *
extern_ref (Extern *ext)
{
	ext->refcount++;
	return ext;
}

void
extern_unref (Extern *ext)
{
	ext->refcount--;
	if (ext->refcount == 0)
		g_free (ext);
}

typedef struct {
	char *goad_id;
	char *cfgpath;
	int pos;
	int panel;
} ReloadCallbackData;

static void
destroy_reload_callback_data (gpointer data)
{
	ReloadCallbackData *d = data;

	g_free (d->goad_id);
	d->goad_id = NULL;
	g_free (d->cfgpath);
	d->cfgpath = NULL;

	g_free (d);
}

static void
reload_applet_callback (GtkWidget *w, int button, gpointer data)
{
	PanelWidget *panel;
	ReloadCallbackData *d = data;

	/* unless the button was YES, just do nothing */
	if (button != 0) {
		return;
	}

	/*select the nth panel*/
	g_assert (panels != NULL);
	panel = g_slist_nth_data (panels, d->panel);
	if (panel == NULL)
		panel = panels->data;

	load_extern_applet (d->goad_id, d->cfgpath, panel,
			    d->pos, TRUE /*exactpos*/, FALSE /*queue*/);
}

void
extern_before_remove (Extern *ext)
{
	char *s;
	const char *id ="";
	GtkWidget *dlg;
	ReloadCallbackData *d;

	if (ext->clean_remove ||
	    ext->didnt_want_save)
		return;

	id = ext->goad_id != NULL ? ext->goad_id : "";

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
	d->goad_id = g_strdup (ext->goad_id);
	d->cfgpath = g_strdup (ext->cfg);

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
}

void
extern_clean (Extern *ext)
{
	CORBA_Environment ev;
	PortableServer_ObjectId *id;
	CORBA_exception_init (&ev);

	/* to catch any weird cases, we won't be able to at the position here
	 * though so it will go to 0,0 */
	extern_before_remove (ext);

	g_free (ext->goad_id);
	ext->goad_id = NULL;

	g_free (ext->cfg);
	ext->cfg = NULL;

	CORBA_Object_release (ext->pspot, &ev);
	ext->pspot = NULL;
	CORBA_Object_release (ext->applet, &ev);
	ext->applet = NULL;
	id = PortableServer_POA_servant_to_id (thepoa, ext, &ev);
	PortableServer_POA_deactivate_object (thepoa, id, &ev);
	CORBA_free (id);
	POA_GNOME_PanelSpot__fini ((PortableServer_Servant) ext, &ev);
	
	if (ext->send_draw_timeout != 0) {
		gtk_timeout_remove (ext->send_draw_timeout);
		ext->send_draw_timeout = 0;
	}
	if (ext->send_draw_idle != 0) {
		gtk_idle_remove (ext->send_draw_idle);
		ext->send_draw_idle = 0;
	}

	extern_unref (ext);

	CORBA_exception_free (&ev);
}

static void
extern_socket_destroy(GtkWidget *w, gpointer data)
{
	GtkSocket *socket = GTK_SOCKET(w);
	Extern *ext = data;

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

/*static void
sal(GtkWidget *applet, GtkAllocation *alloc)
{
	printf("SOCKET req:   %dx%d\nSOCKET alloc: %dx%d\n",
	       applet->requisition.width,
	       applet->requisition.height,
	       applet->allocation.width,
	       applet->allocation.height);
}*/


static void
send_position_change(Extern *ext)
{
	/*ingore this until we get an ior*/
	if(ext->applet) {
		int x=0,y=0;
		GtkWidget *wid=ext->ebox;
		
		CORBA_Environment ev;
		CORBA_exception_init(&ev);
		/*go the the toplevel panel widget*/
		for(;;) {
			if(!GTK_WIDGET_NO_WINDOW(wid)) {
				x += wid->allocation.x;
				y += wid->allocation.y;
			}
			if(wid->parent)
				wid = wid->parent;
			else
				break;
		}
		GNOME_Applet_change_position(ext->applet, x, y, &ev);
		if(ev._major)
			panel_clean_applet (ext->info);
		CORBA_exception_free (&ev);
	}
}

static void
ebox_size_allocate (GtkWidget *applet, GtkAllocation *alloc, Extern *ext)
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
	static gboolean tried_loading = FALSE;
	static GdkPixbuf *pb = NULL;
	int size;

	/* sanity */
	if (socket == NULL ||
	    socket->window == NULL)
		return;

	size = panel->sz < 14 ? panel->sz : 14;

	if ( ! tried_loading) {
		char *file;
		file = gnome_pixmap_file ("gnome-unknown.png");

		if (file != NULL) {
			pb = gdk_pixbuf_new_from_file (file);

			g_free (file);
		}
	}
	tried_loading = TRUE;

	if (pb != NULL) {
		GdkPixmap *pm;
		GdkPixbuf *scaled;

		if (gdk_pixbuf_get_width (pb) != size ||
		    gdk_pixbuf_get_height (pb) != size) {
			scaled = gdk_pixbuf_scale_simple (pb, size, size,
							  GDK_INTERP_BILINEAR);
		} else {
			scaled = gdk_pixbuf_ref (pb);
		}


		pm = NULL;
		gdk_pixbuf_render_pixmap_and_mask (scaled, &pm, NULL, 127);

		gdk_pixbuf_unref (scaled);

		if (pm != NULL) {
			gdk_window_set_back_pixmap (socket->window, pm, FALSE);

			gdk_pixmap_unref (pm);
		}
	}
}

static void
socket_unset_loading (GtkWidget *socket)
{
	/* sanity */
	if (socket == NULL)
		return;

	/* sanity */
	if (socket->parent != NULL)
		gtk_widget_queue_resize (socket->parent);

	/* sanity */
	if (socket->window != NULL)
		gdk_window_set_back_pixmap (socket->window, NULL, FALSE);

	/* sanity */
	if (socket->parent != NULL) {
		gtk_widget_set_usize (socket->parent, -1, -1);
		gtk_signal_connect_after
			(GTK_OBJECT (socket),"size_allocate",
			 GTK_SIGNAL_FUNC (socket_size_allocate), NULL);
	}
}

/*note that type should be APPLET_EXTERN_RESERVED or APPLET_EXTERN_PENDING
  only*/
static CORBA_unsigned_long
reserve_applet_spot (Extern *ext, PanelWidget *panel, int pos,
		     AppletType type)
{
	int size;
	GtkWidget *socket;

	ext->ebox = gtk_event_box_new();
	gtk_widget_set_events(ext->ebox, (gtk_widget_get_events(ext->ebox) |
					  APPLET_EVENT_MASK) &
			      ~( GDK_POINTER_MOTION_MASK |
				 GDK_POINTER_MOTION_HINT_MASK));

	size = panel->sz < 14 ? panel->sz : 14;

	gtk_widget_set_usize (ext->ebox, size, size);

	gtk_signal_connect_after (GTK_OBJECT (ext->ebox),"size_allocate",
				  GTK_SIGNAL_FUNC (ebox_size_allocate),
				  ext);

	socket = gtk_socket_new();

	if(!socket) {
		g_warning("Can't create a socket");
		return 0;
	}

	/* here for debugging purposes */
	/*gtk_signal_connect_after(GTK_OBJECT(socket),"size_allocate",
				 GTK_SIGNAL_FUNC(sal),NULL);*/

	gtk_container_add(GTK_CONTAINER(ext->ebox), socket);

	gtk_widget_show_all (ext->ebox);
	
	/*we save the obj in the id field of the appletinfo and the 
	  path in the path field */
	ext->info = NULL;
	if(!register_toy(ext->ebox,
			 ext, (GDestroyNotify)extern_clean,
			 panel, pos, ext->exactpos, type)) {
		/* the ebox is destroyed in register_toy */
		ext->ebox = NULL;
		g_warning(_("Couldn't add applet"));
		return 0;
	}
	ext->info = applets_last->data;

	gtk_signal_connect(GTK_OBJECT (socket), "destroy",
			   GTK_SIGNAL_FUNC (extern_socket_destroy),
			   extern_ref (ext));
	
	if(!GTK_WIDGET_REALIZED(socket))
		gtk_widget_realize(socket);

	socket_set_loading (socket, panel);

	return GDK_WINDOW_XWINDOW(socket->window);
}

/* Note exactpos may NOT be changed */
static PanelWidget *
get_us_position (const int panel, const int pos, const char *goad_id, int *newpos,
		 gboolean *exactpos)
{
	PanelWidget *pw = NULL;

	*newpos = pos;

	/* Sanity? can this ever happen? */
	if (goad_id == NULL) {
		g_warning ("get_us_position: goad_id == NULL, bad bad");
		goad_id = "foo";
	}

	if (panel < 0 || pos < 0) {
		char *key = g_strdup_printf ("%sApplet_Position_Memory/%s/",
					     PANEL_CONFIG_PATH,
					     goad_id);
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


void
load_extern_applet (const char *goad_id, const char *cfgpath,
		    PanelWidget *panel, int pos, gboolean exactpos,
		    gboolean queue)
{
	char *cfg;
	Extern *ext;
	POA_GNOME_PanelSpot *panelspot_servant;

	if (string_empty (cfgpath))
		cfg = g_strconcat (PANEL_CONFIG_PATH,
				   "Applet_Dummy/", NULL);
	else
		/*we will free this lateer*/
		cfg = g_strdup (cfgpath);

	ext = g_new0 (Extern, 1);
	ext->refcount = 1;
	ext->started = FALSE;
	ext->exactpos = exactpos;
	ext->send_position = FALSE;
	ext->send_draw = FALSE;
	ext->orient = -1;

	/* don't know until first save, but FALSE since
	 * we assume it will want to. */
	ext->didnt_want_save = FALSE;

	/* not until we are properly added */
	ext->clean_remove = TRUE;

	ext->send_draw_timeout = 0;
	ext->send_draw_idle = 0;
	ext->send_draw_queued = FALSE;


	panelspot_servant = (POA_GNOME_PanelSpot *)ext;
	panelspot_servant->_private = NULL;
	panelspot_servant->vepv = &panelspot_vepv;

	POA_GNOME_PanelSpot__init(panelspot_servant, &ev);
	
	CORBA_free(PortableServer_POA_activate_object(thepoa, panelspot_servant, &ev));
	pg_return_if_fail(&ev, ev._major == CORBA_NO_EXCEPTION);

	ext->pspot = CORBA_OBJECT_NIL; /*will be filled in during add_applet*/
	ext->applet = CORBA_OBJECT_NIL;
	ext->goad_id = g_strdup(goad_id);
	ext->cfg = cfg;

	if (panel == NULL || pos < 0) {
		if (panel != NULL) {
			gboolean exactpos;
			/* We have a panel */
			PanelWidget *pw = get_us_position (-1, pos, goad_id,
							   &pos, &exactpos);
			/* only use this position if
			 * pw and panel are the same */
			if (pw != panel)
				pos = 0;
			else
				ext->exactpos = exactpos;
		} else /* panel == NULL && pos < 0 */ {
			panel = get_us_position (-1, -1, goad_id, &pos,
						 &ext->exactpos);
		}
	}

	if(reserve_applet_spot (ext, panel, pos, APPLET_EXTERN_PENDING)==0) {
		g_warning(_("Whoops! for some reason we can't add "
			    "to the panel"));
		extern_clean(ext);
		return;
	}

	if(!queue) {
		extern_start_new_goad_id(ext);
		ext->started = TRUE;
	}
}

void
load_queued_externs(void)
{
	GSList *li;
	for(li=applets;li!=NULL;li=g_slist_next(li)) {
		AppletInfo *info = li->data;
		if(info->type == APPLET_EXTERN_PENDING ||
		   info->type == APPLET_EXTERN_RESERVED) {
			Extern *ext = info->data;
			if(!ext->started) {
				extern_start_new_goad_id(ext);
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
s_panel_add_applet_full (PortableServer_Servant servant,
			 const GNOME_Applet panel_applet,
			 const CORBA_char *goad_id,
			 const CORBA_short panel,
			 const CORBA_short pos,
			 CORBA_char ** cfgpath,
			 CORBA_char ** globcfgpath,
			 CORBA_unsigned_long* wid,
			 CORBA_Environment *ev)
{
	PanelWidget *pw;
	int newpos;
	GSList *li;
	Extern *ext;
	POA_GNOME_PanelSpot *panelspot_servant;
	GNOME_PanelSpot acc;
	GNOME_PanelAppletBooter booter;

	if (panel_in_startup)
		return CORBA_OBJECT_NIL;

	booter = pop_outside_extern (goad_id);
	if (booter != CORBA_OBJECT_NIL) {
		/* yeah, we do dump the panel and pos, since they
		 * really don't make any sesnse, and aren't really
		 * used anywhere in the first place */
		acc = GNOME_PanelAppletBooter_add_applet (booter,
							  panel_applet,
							  goad_id,
							  cfgpath,
							  globcfgpath,
							  wid,
							  ev);

		/* if we succeeded, then all fine and dandy, otherwise
		 * this thing was some stale thingie, so just ignore it
		 * and launch us into the panel.  This way we're always
		 * getting the applet, even if the booter crashed or we
		 * had a stale one around */
		if (ev->_major == CORBA_NO_EXCEPTION)
			return acc;

		/* recycle the exception */
		CORBA_exception_free (ev);
		CORBA_exception_init (ev);
	}
	
	for (li = applets; li != NULL; li = li->next) {
		AppletInfo *info = li->data;
		if (info && info->type == APPLET_EXTERN_PENDING) {
			Extern *ext = info->data;
			g_assert(ext);
			g_assert(ext->info == info);
			g_assert(ext->goad_id != NULL);
			if (strcmp(ext->goad_id, goad_id)==0) {
				/*we started this and already reserved a spot
				  for it, including the socket widget*/
				GtkWidget *socket =
					GTK_BIN(info->widget)->child;
				if(!socket) {
					g_warning(_("No socket was created"));
					return CORBA_OBJECT_NIL;
				}

				ext->applet = CORBA_Object_duplicate(panel_applet, ev);
				*cfgpath = CORBA_string_dup(ext->cfg);

				*globcfgpath = CORBA_string_dup(PANEL_CONFIG_PATH);
				info->type = APPLET_EXTERN_RESERVED;
				*wid = GDK_WINDOW_XWINDOW(socket->window);
#ifdef PANEL_DEBUG
				printf("\nSOCKET XID: %lX\n\n", (long)*wid);
#endif

				panelspot_servant = (POA_GNOME_PanelSpot *)ext;
				acc = PortableServer_POA_servant_to_reference(thepoa, panelspot_servant, ev);
				pg_return_val_if_fail(ev, ev->_major == CORBA_NO_EXCEPTION, CORBA_OBJECT_NIL);
				ext->pspot = CORBA_Object_duplicate(acc, ev);
				pg_return_val_if_fail(ev, ev->_major == CORBA_NO_EXCEPTION, CORBA_OBJECT_NIL);

				return CORBA_Object_duplicate(acc, ev);
			}
		}
	}
	
	/*this is an applet that was started from outside, otherwise we would
	  have already reserved a spot for it*/
	ext = g_new0(Extern, 1);
	ext->refcount = 1;
	ext->started = FALSE;
	ext->exactpos = FALSE;
	ext->send_position = FALSE;
	ext->send_draw = FALSE;
	ext->orient = -1;
	ext->applet = CORBA_Object_duplicate(panel_applet, ev);
	ext->goad_id = g_strdup(goad_id);
	ext->cfg = NULL;

	panelspot_servant = (POA_GNOME_PanelSpot *)ext;
	panelspot_servant->_private = NULL;
	panelspot_servant->vepv = &panelspot_vepv;

	POA_GNOME_PanelSpot__init (panelspot_servant, ev);
	
	CORBA_free(PortableServer_POA_activate_object (thepoa, panelspot_servant, ev));
	pg_return_val_if_fail(ev, ev->_major == CORBA_NO_EXCEPTION,
			      CORBA_OBJECT_NIL);

	acc = PortableServer_POA_servant_to_reference (thepoa, panelspot_servant, ev);
	pg_return_val_if_fail(ev, ev->_major == CORBA_NO_EXCEPTION,
			      CORBA_OBJECT_NIL);

	ext->pspot = CORBA_Object_duplicate(acc, ev);

	pg_return_val_if_fail(ev, ev->_major == CORBA_NO_EXCEPTION,
			      CORBA_OBJECT_NIL);

	pw = get_us_position (panel, pos, goad_id, &newpos, &ext->exactpos);

	*wid = reserve_applet_spot (ext, pw, newpos,
				    APPLET_EXTERN_RESERVED);
	if (*wid == 0) {
		extern_clean(ext);
		*globcfgpath = NULL;
		*cfgpath = NULL;
		return CORBA_OBJECT_NIL;
	}
	*cfgpath = CORBA_string_dup(PANEL_CONFIG_PATH "Applet_Dummy/");
	*globcfgpath = CORBA_string_dup (PANEL_CONFIG_PATH);

	return CORBA_Object_duplicate(acc, ev);
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
s_panel_add_status (PortableServer_Servant servant,
		    CORBA_unsigned_long *wid,
		    CORBA_Environment *ev)
{
	POA_GNOME_StatusSpot *statusspot_servant;
	GNOME_StatusSpot acc;
	StatusSpot *ss;
	
	*wid = 0;
	
	ss = new_status_spot();
	if (ss == NULL)
		return CORBA_OBJECT_NIL;
	
	statusspot_servant = (POA_GNOME_StatusSpot *)ss;
	statusspot_servant->_private = NULL;
	statusspot_servant->vepv = &statusspot_vepv;

	POA_GNOME_StatusSpot__init(statusspot_servant, ev);
	
	CORBA_free (PortableServer_POA_activate_object (thepoa, statusspot_servant, ev));
	pg_return_val_if_fail (ev, ev->_major == CORBA_NO_EXCEPTION,
			       CORBA_OBJECT_NIL);

	acc = PortableServer_POA_servant_to_reference(thepoa, statusspot_servant, ev);
	pg_return_val_if_fail (ev, ev->_major == CORBA_NO_EXCEPTION,
			       CORBA_OBJECT_NIL);

	ss->sspot = CORBA_Object_duplicate (acc, ev);

	pg_return_val_if_fail(ev, ev->_major == CORBA_NO_EXCEPTION,
			      CORBA_OBJECT_NIL);

	*wid = ss->wid;
	return CORBA_Object_duplicate(acc, ev);
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
	char *exec_argv[2] = { NULL, NULL };

	if (panel_in_startup)
		return;

	g_assert (panels != NULL);

	panel_widget = g_slist_nth_data (panels, panel);
	if (panel_widget == NULL)
		panel_widget = panels->data;

	/* ugly but works because of the way this actually works */
	exec_argv[0] = (char *)exec;
	load_launcher_applet_from_info (name, comment, exec_argv, 1,
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
s_panel_run_box (PortableServer_Servant _servant,
		 const CORBA_char * initial_string,
		 CORBA_Environment * ev)
{
	if (string_empty (initial_string))
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
	if (IS_BASEP_WIDGET(basep)) {
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
}

/*** PanelSpot stuff ***/

static CORBA_char *
s_panelspot_get_tooltip(PortableServer_Servant servant,
			CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
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
	Extern *ext = (Extern *)servant;
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
	Extern *ext = (Extern *)servant;

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
	Extern *ext = (Extern *)servant;
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
	Extern *ext = (Extern *)servant;
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
	Extern *ext = (Extern *)servant;
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
	Extern *ext = (Extern *)servant;
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
	Extern *ext = (Extern *)servant;

	g_assert(ext);
	
	return ext->send_position;
}

static void
s_panelspot_set_send_position(PortableServer_Servant servant,
			      CORBA_boolean enable,
			      CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;

	g_assert(ext);
	
	ext->send_position = enable?TRUE:FALSE;
}

static CORBA_boolean
s_panelspot_get_send_draw(PortableServer_Servant servant,
			  CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;

	g_assert(ext);
	
	return ext->send_draw;
}

static void
s_panelspot_set_send_draw(PortableServer_Servant servant,
			  CORBA_boolean enable,
			  CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;

	g_assert(ext);
	
	ext->send_draw = enable?TRUE:FALSE;
}

static GNOME_Panel_RgbImage *
s_panelspot_get_rgb_background(PortableServer_Servant servant,
			       CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
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
s_panelspot_register_us(PortableServer_Servant servant,
			CORBA_Environment *ev)
{
	PanelWidget *panel;
	Extern *ext = (Extern *)servant;

	g_assert (ext != NULL);
	g_assert (ext->info != NULL);
	
#ifdef PANEL_DEBUG
	printf("register ext: %lX\n",(long)ext);
	printf("register ext->info: %lX\n",(long)(ext->info));
#endif

	panel = PANEL_WIDGET (ext->info->widget->parent);
	if (panel == NULL) {
		g_warning ("%s:%d ??? Applet with no panel ???",
			   __FILE__, __LINE__);
		return;
	}

	/*no longer pending*/
	ext->info->type = APPLET_EXTERN;
	/* from now on warn on unclean removal */
	ext->clean_remove = FALSE;

	if (ext->ebox != NULL)
		socket_unset_loading (GTK_BIN (ext->ebox)->child);

	/* make sure things don't screw up */
	extern_ref (ext);

	freeze_changes (ext->info);
	if (ext->info != NULL)
		orientation_change (ext->info, panel);
	if (ext->info != NULL)
		size_change (ext->info, panel);
	if (ext->info != NULL)
		back_change (ext->info, panel);
	if (ext->info != NULL &&
	    ext->send_position)
		send_position_change(ext);
	if (ext->info != NULL)
		thaw_changes (ext->info);

	/* just sanity */
	if (ext->applet != NULL)
		GNOME_Applet_set_tooltips_state
			(ext->applet, global_config.tooltips_enabled, ev);

	if (ev->_major) {
		/* well, we haven't really gotten anywhere, so it's not
		 * a good idea to bother the user with restarting us */
		ext->clean_remove = TRUE;
		panel_clean_applet (ext->info);
	}

	extern_unref (ext);
}


static void
s_panelspot_unregister_us(PortableServer_Servant servant,
			  CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;

	extern_save_last_position (ext, TRUE /* sync */);

	ext->clean_remove = TRUE;
	panel_clean_applet(ext->info);
}

static void
s_panelspot_abort_load(PortableServer_Servant servant,
		       CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;

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
	Extern *ext = (Extern *)servant;
	
#ifdef PANEL_DEBUG
	printf("show menu ext: %lX\n",(long)ext);
	printf("show menu ext->info: %lX\n",(long)(ext->info));
#endif

	g_assert (ext != NULL);
	g_assert (ext->info != NULL);

	panel = get_panel_parent (ext->info->widget);

	if (!ext->info->menu)
		create_applet_menu(ext->info, IS_BASEP_WIDGET (panel));

	if (IS_BASEP_WIDGET (panel)) {
		BASEP_WIDGET(panel)->autohide_inhibit = TRUE;
		basep_widget_queue_autohide(BASEP_WIDGET(panel));
	}

	ext->info->menu_age = 0;
	gtk_menu_popup(GTK_MENU(ext->info->menu), NULL, NULL,
		       global_config.off_panel_popups?applet_menu_position:NULL,
		       ext->info, 3, GDK_CURRENT_TIME);
}


static void
s_panelspot_drag_start(PortableServer_Servant servant,
		       CORBA_Environment *ev)
{
	PanelWidget *panel;
	Extern *ext = (Extern *)servant;

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
	Extern *ext = (Extern *)servant;

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
	Extern *ext = (Extern *)servant;

#ifdef PANEL_DEBUG
	printf("add callback ext: %lX\n",(long)ext);
#endif

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
	Extern *ext = (Extern *)servant;

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
	Extern *ext = (Extern *)servant;

	g_assert(ext != NULL);
	g_assert(ext->info != NULL);
	applet_callback_set_sensitive(ext->info, callback_name, sensitive);
}

static void
s_panelspot_sync_config(PortableServer_Servant servant,
			CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;

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
save_applet (AppletInfo *info, gboolean ret)
{
	char *buf;
	PanelWidget *panel;
	AppletData *ad;
	Extern *ext;
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
					 ext->goad_id);
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

	save_applet(info, ret);
}

/*** StatusSpot stuff ***/

static void
s_statusspot_remove(POA_GNOME_StatusSpot *servant,
		    CORBA_Environment *ev)
{
	StatusSpot *ss = (StatusSpot *)servant;
	status_spot_remove(ss, TRUE);
}


void
panel_corba_clean_up(void)
{
  goad_server_unregister(CORBA_OBJECT_NIL, "gnome_panel", "server", &ev);
  CORBA_ORB_shutdown(orb, CORBA_FALSE, &ev);
}

gint
panel_corba_gtk_init(CORBA_ORB panel_orb)
{
  GNOME_Panel acc;
  CORBA_Object old_server;
  gint status;

  CORBA_exception_init(&ev);

  orb = panel_orb;

  POA_GNOME_Panel2__init (&panel_servant, &ev);
  pg_return_val_if_fail (&ev, ev._major == CORBA_NO_EXCEPTION, -1);

  thepoa = (PortableServer_POA)
    CORBA_ORB_resolve_initial_references (orb, "RootPOA", &ev);
  pg_return_val_if_fail(&ev, ev._major == CORBA_NO_EXCEPTION, -1);

  PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(thepoa, &ev), &ev);
  pg_return_val_if_fail(&ev, ev._major == CORBA_NO_EXCEPTION, -1);

  CORBA_free(PortableServer_POA_activate_object(thepoa,
						&panel_servant, &ev));
  pg_return_val_if_fail(&ev, ev._major == CORBA_NO_EXCEPTION, -1);

  acc = PortableServer_POA_servant_to_reference(thepoa, &panel_servant, &ev);
  pg_return_val_if_fail(&ev, ev._major == CORBA_NO_EXCEPTION, -1);

  old_server = goad_server_activate_with_repo_id (NULL, "IDL:GNOME/Panel:1.0", 
						  GOAD_ACTIVATE_EXISTING_ONLY,
						  NULL);

  if(! CORBA_Object_is_nil(old_server, &ev)) {
    CORBA_Object_release(old_server, &ev);
    return -4;
  }

  status = goad_server_register(CORBA_OBJECT_NIL, acc, "gnome_panel", "server", &ev);

  /*
  CORBA_Object_release(acc, &ev);
  pg_return_val_if_fail(&ev, ev._major == CORBA_NO_EXCEPTION, -1);
  */

  return status;
}

static void
send_draw(Extern *ext)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	if (ext->applet != NULL) /* sanity */
		GNOME_Applet_draw (ext->applet, &ev);
	if (ev._major)
		panel_clean_applet (ext->info);
	CORBA_exception_free (&ev);
}

static gboolean
send_draw_timeout(gpointer data)
{
	Extern *ext = data;
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
	Extern *ext = data;
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
extern_send_draw(Extern *ext)
{
	if(!ext || !ext->applet || !ext->send_draw)
		return;
	if(!ext->send_draw_idle)
		ext->send_draw_idle =
			gtk_idle_add(send_draw_idle, ext);
}

void
extern_save_last_position (Extern *ext, gboolean sync)
{
	char *key;
	int panel_num;
	AppletData *ad;

	if (ext->goad_id == NULL)
		return;

	/* Here comes a hack.  We probably want the next applet to load at
	 * a similar location.  If none is given.  Think xchat, or any
	 * app that just adds applets on it's own by just running them. */
	key = g_strdup_printf ("%sApplet_Position_Memory/%s/",
			       PANEL_CONFIG_PATH,
			       ext->goad_id);
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

