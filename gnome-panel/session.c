/* Gnome panel: panel functionality
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Federico Mena
 *           Miguel de Icaza
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <gdk/gdkx.h>
#include <X11/keysym.h>

#include <libgnome/libgnome.h>
#include <libbonobo.h>

#include "gnome-desktop-item.h"

#include "session.h"

#include "aligned-widget.h"
#include "button-widget.h"
#include "distribution.h"
#include "drawer-widget.h"
#include "edge-widget.h"
#include "extern.h"
#include "floating-widget.h"
#include "foobar-widget.h"
#include "launcher.h"
#include "logout.h"
#include "menu-fentry.h"
#include "menu-util.h"
#include "menu.h"
#include "panel-util.h"
#include "panel-gconf.h"
#include "panel_config_global.h"
#include "session.h"
#include "sliding-widget.h"
#include "status.h"
#include "swallow.h"
#include "gnome-run.h"
#include "global-keys.h"
#include "xstuff.h"
#include "multiscreen-stuff.h"
#include "conditional.h"

#undef SESSION_DEBUG 

int config_sync_timeout = 0;
int applets_to_sync = FALSE;
int panels_to_sync = FALSE;
int need_complete_save = FALSE;

extern GSList *panels;
extern GSList *applets;
extern int applet_count;

extern GtkTooltips *panel_tooltips;

extern GnomeClient *client;

gboolean commie_mode = FALSE;
gboolean no_run_box = FALSE;

GlobalConfig global_config;

char *panel_cfg_path = NULL;
char *old_panel_cfg_path = NULL;

/*list of all panel widgets created*/
extern GSList *panel_list;

extern char *kde_menudir;

int ss_cur_applet = 0;
gboolean ss_done_save = FALSE;
gushort ss_cookie = 0;
GtkWidget *ss_timeout_dlg = NULL;
static gboolean ss_interactive = FALSE;
static int ss_timeout = 500;

/*send the tooltips state to all external applets*/
static void
send_tooltips_state(gboolean enabled)
{
	GSList *li;

	for(li = applets; li != NULL; li = li->next) {
		AppletInfo *info = li->data;

		if (info->type == APPLET_EXTERN)
			extern_handle_set_tooltips_state ((Extern)info->data,
							  enabled);
	}
}

void
apply_global_config (void)
{
	static int layer_old = -1;
	static int menu_flags_old = -1;
	static int old_use_large_icons = -1;
	static int old_merge_menus = -1;
	static int old_menu_check = -1;
	static int old_avoid_collisions = -1;
	GSList *li;

/* FIXME
	panel_widget_change_global (global_config.hiding_step_size,
				    global_config.minimized_size,
				    global_config.minimize_delay,
				    global_config.maximize_delay,
				    global_config.movement_type,
				    global_config.disable_animations);
*/
	if (global_config.tooltips_enabled)
		gtk_tooltips_enable (panel_tooltips);
	else
		gtk_tooltips_disable (panel_tooltips);
	/* not incredibly efficent way to do this, we just make
	 * sure that all directories are reread */
	if (old_merge_menus != global_config.merge_menus ||
	    old_menu_check != global_config.menu_check) {
		fr_force_reread();
	}
	/*if we changed small_icons mark all menus as dirty
	  for rereading, hopefullly the user doesn't do this too often
	  so that he doesn't have to reread his menus all the time:)*/
	if( old_use_large_icons != global_config.use_large_icons ||
	   old_merge_menus != global_config.merge_menus ||
	   old_menu_check != global_config.menu_check) {
		GSList *li;
		for(li = applets; li != NULL; li = g_slist_next(li)) {
			AppletInfo *info = li->data;
			if(info->menu != NULL) {
				gtk_widget_unref(info->menu);
				info->menu = NULL;
				info->menu_age = 0;
			}
			if(info->type == APPLET_MENU) {
				Menu *menu = info->data;
				if(menu->menu != NULL) {
					gtk_widget_unref(menu->menu);
					menu->menu = NULL;
					menu->age = 0;
				}
			}
		}
		for(li = panel_list; li != NULL; li = g_slist_next(li)) {
			PanelData *pd = li->data;
			if(pd->menu != NULL) {
				gtk_widget_unref(pd->menu);
				pd->menu = NULL;
				pd->menu_age = 0;
			}
		}
		foobar_widget_force_menu_remake();
	}
	old_use_large_icons = global_config.use_large_icons;
	old_merge_menus = global_config.merge_menus;
	old_menu_check = global_config.menu_check;
	send_tooltips_state(global_config.tooltips_enabled);

	/* if we changed global menu flags, cmark all main menus that use
	 * the global setting as dirty */
	if(menu_flags_old != global_config.menu_flags) {
		GSList *li;
		for(li = applets; li != NULL; li = g_slist_next(li)) {
			AppletInfo *info = li->data;
			if(info->type == APPLET_MENU) {
				Menu *menu = info->data;
				if(menu->menu != NULL &&
				   menu->global_main) {
					gtk_widget_unref(menu->menu);
					menu->menu = NULL;
					menu->age = 0;
				}
			}
		}
	}

	if (layer_old != global_config.layer) {
		 for (li = panel_list; li != NULL; li = li->next) {
			PanelData *pd = li->data;
			if ( ! GTK_WIDGET_REALIZED (pd->panel))
				continue;
			if (BASEP_IS_WIDGET (pd->panel))
				basep_widget_update_winhints (BASEP_WIDGET (pd->panel));
			else if (IS_FOOBAR_WIDGET (pd->panel))
				foobar_widget_update_winhints (FOOBAR_WIDGET (pd->panel));
		}
		panel_reset_dialog_layers ();
	}
	layer_old = global_config.layer;
	
	for (li = panel_list; li != NULL; li = li->next) {
		PanelData *pd = li->data;
		if (BASEP_IS_WIDGET (pd->panel)) {
			basep_update_frame (BASEP_WIDGET (pd->panel));

			if ((menu_flags_old != global_config.menu_flags) &&
			    pd->menu) {
				gtk_widget_unref (pd->menu);
				pd->menu = NULL;
				pd->menu_age = 0;
			}
				
		}
	}
	menu_flags_old = global_config.menu_flags;

	if (old_avoid_collisions != global_config.avoid_collisions) {
		int i;
		for (i = 0; i < multiscreen_screens (); i++)
			basep_border_queue_recalc (i);
	}
	old_avoid_collisions = global_config.avoid_collisions;

	panel_global_keys_setup();
}

static void
timeout_dlg_realized (GtkWidget *dialog)
{
#if FIXME
	/* always make top layer */
	gnome_win_hints_set_layer (GTK_WIDGET(dialog),
				   WIN_LAYER_ABOVE_DOCK);
#endif
}

static gboolean
session_save_timeout (gpointer data)
{
	int cookie = GPOINTER_TO_INT (data);
	if (cookie != ss_cookie)
		return FALSE;

#ifdef SESSION_DEBUG	
	printf("SAVE TIMEOUT (%u)\n",ss_cookie);
#endif
	if ( ! ss_interactive) {
		ss_cookie ++;
		return FALSE;
	}

	ss_timeout_dlg =
		gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
				       _("An applet is not "
					 "responding to a "
					 "save request.\n"
					 "Remove the applet "
					 "or continue waiting?"));
	gtk_signal_connect(GTK_OBJECT(ss_timeout_dlg),"destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			   &ss_timeout_dlg);
	gtk_dialog_add_button (GTK_DIALOG (ss_timeout_dlg),
			       _("Remove applet"),
			       1); /* FIXME: GNOME_STOCK_PIXMAP_TRASH */
	gtk_dialog_add_button (GTK_DIALOG (ss_timeout_dlg),
			       _("Continue waiting"),
			       2); /* FIXME: GNOME_STOCK_PIXMAP_TIMER */

	gtk_signal_connect_after(GTK_OBJECT(ss_timeout_dlg), "realize",
				 GTK_SIGNAL_FUNC(timeout_dlg_realized),
				 NULL);
	
	if (1 == gtk_dialog_run (GTK_DIALOG (ss_timeout_dlg))) {
		ss_cookie++;
		g_warning(_("Timed out on sending session save to an applet"));
		save_next_applet();
		gtk_widget_destroy (ss_timeout_dlg);
		return FALSE;
	}
	gtk_widget_destroy (ss_timeout_dlg);
	return TRUE;
}

static void
send_applet_session_save (AppletInfo *info,
			  CORBA_Object obj,
			  const char *cfgpath,
			  const char *globcfgpath)
{
	guint timeout;
	CORBA_Environment ev;
	
	/*new unique cookie*/
	ss_cookie++;
	
#ifdef SESSION_DEBUG	
	printf("SENDING_SESSION_SAVE (%u)\n", ss_cookie);
#endif

	timeout = gtk_timeout_add(ss_timeout, session_save_timeout,
				  GINT_TO_POINTER((int)ss_cookie));

	CORBA_exception_init(&ev);
	GNOME_Applet_save_session(obj,
				  (CORBA_char *)cfgpath,
				  (CORBA_char *)globcfgpath,
				  ss_cookie, &ev);
	if(ev._major == CORBA_SYSTEM_EXCEPTION) {
		CORBA_SystemException *exc =
			CORBA_exception_value(&ev);
		if(exc &&
		   (!strcmp (ev._id, ex_CORBA_BAD_OPERATION) ||
		    !strcmp (ev._id, ex_CORBA_NO_IMPLEMENT))) {
			gboolean ret;
			gtk_timeout_remove(timeout);
			CORBA_exception_free(&ev);
			CORBA_exception_init(&ev);
			ret = GNOME_Applet_session_save(obj,
						(CORBA_char *)cfgpath,
						(CORBA_char *)globcfgpath,
						&ev);
			extern_save_applet (info, ret);
		}
	} else if(ev._major) {
		gtk_timeout_remove(timeout);
		panel_applet_clean(info);
		CORBA_exception_free(&ev);
		save_next_applet();
		return;
	}

	CORBA_exception_free(&ev);
}




/*returns TRUE if the save was completed, FALSE if we need to wait
  for the applet to respond*/
static gboolean
save_applet_configuration(AppletInfo *info)
{
	GString       *buf;
	int            panel_num;
	int            panel_id;
	PanelWidget   *panel;
	AppletData    *ad;
	
	g_return_val_if_fail(info!=NULL,TRUE);

	buf = g_string_new(NULL);

	gnome_config_push_prefix("");
	g_string_sprintf(buf, "%sApplet_Config/Applet_%d", PANEL_CONFIG_PATH,
			 info->applet_id+1);
	gnome_config_clean_section(buf->str);
	gnome_config_pop_prefix();

	g_string_sprintf(buf, "%sApplet_Config/Applet_%d/", PANEL_CONFIG_PATH,
			 info->applet_id+1);
	gnome_config_push_prefix(buf->str);

	/*obviously no need for saving*/
	if (info->type == APPLET_EXTERN_PENDING ||
	    info->type == APPLET_EXTERN_RESERVED ||
	    info->type == APPLET_EMPTY) {
		gnome_config_set_string("id", EMPTY_ID);
		gnome_config_pop_prefix();
		g_string_free(buf,TRUE);
		return TRUE;
	}

	panel = PANEL_WIDGET(info->widget->parent);
	ad = gtk_object_get_data(GTK_OBJECT(info->widget),PANEL_APPLET_DATA);

	panel_num = g_slist_index (panels, panel);
	if (panel_num == -1) {
		gnome_config_set_string("id", EMPTY_ID);
		gnome_config_pop_prefix();
		g_string_free(buf,TRUE);
		return TRUE;
	}
	panel_id = panel->unique_id;

	switch(info->type) {
	case APPLET_EXTERN:
		{
			Extern       ext = info->data;
			GNOME_Applet applet;
			char *s;

			g_assert (info->data);

			ext    = info->data;
			applet = extern_get_applet (ext);

			/*just in case the applet times out*/
			gnome_config_set_string("id", EMPTY_ID);
			gnome_config_pop_prefix();

			/*this is the file path we must kill first */
			g_string_sprintf(buf, "%sApplet_%d_Extern",
					 PANEL_CONFIG_PATH, info->applet_id+1);
			
			/* this should really be: */
			/*gnome_config_push_prefix("");
			gnome_config_clean_file(buf->str);
			gnome_config_pop_prefix();
			gnome_config_sync();*/
			/* but gnome-config.[ch] is broken ! */
			s = gnome_config_get_real_path (buf->str);
			unlink(s);
			g_free(s);

			gnome_config_sync();

			/*this is the file path we pass to the applet for it's
			  own config, this is a separate file, so that we */
			g_string_sprintf(buf, "%sApplet_%d_Extern/",
					 PANEL_CONFIG_PATH, info->applet_id+1);
			/*have the applet do it's own session saving*/
			send_applet_session_save (info,
						  applet,
						  buf->str,
						  PANEL_CONFIG_PATH
						  "Applet_All_Extern/");

			/* update the configuration string */
			extern_set_config_string (ext, buf->str);

			return FALSE; /*here we'll wait for done_session_save*/
		}
	case APPLET_DRAWER: 
		{
			int i;
			Drawer *drawer = info->data;

			gnome_config_set_string("id", DRAWER_ID);

			i = g_slist_index(panels,
					  BASEP_WIDGET(drawer->drawer)->panel);
			if(i>=0)
				gnome_config_set_int("parameters",i);
			gnome_config_set_int ("unique_drawer_panel_id", 
					      PANEL_WIDGET (BASEP_WIDGET(drawer->drawer)->panel)->unique_id);
			gnome_config_set_string("pixmap",
						drawer->pixmap);
			gnome_config_set_string("tooltip",
						drawer->tooltip);
			break;
		}
	case APPLET_SWALLOW:
		{
			Swallow *swallow = info->data;
			gnome_config_set_string("id", SWALLOW_ID);
			gnome_config_set_string("parameters",
						swallow->title);
			gnome_config_set_string("execpath",
						swallow->path);
			gnome_config_set_int("width",swallow->width);
			gnome_config_set_int("height",swallow->height);
			break;
		}
	case APPLET_MENU:
		{
			Menu *menu = info->data;
			gnome_config_set_string("id", MENU_ID);
			gnome_config_set_string("parameters",
						menu->path);
			gnome_config_set_int("main_menu_flags",
					     menu->main_menu_flags);
			gnome_config_set_bool("global_main",
					      menu->global_main);
			gnome_config_set_bool("old_style_main", FALSE);
			gnome_config_set_bool("custom_icon",
					      menu->custom_icon);
			gnome_config_set_string("custom_icon_file",
						menu->custom_icon_file);
			break;
		}
	case APPLET_LAUNCHER:
		{
			Launcher *launcher = info->data;
			const char *location;

			gnome_config_set_string ("id", LAUNCHER_ID);

			/* clean old launcher info */
			gnome_config_clean_key ("parameters");

			launcher_save (launcher);
			location = gnome_desktop_item_get_location (launcher->ditem);
			gnome_config_set_string ("base_location", location);
			break;
		}
	case APPLET_LOGOUT:
		gnome_config_set_string("id", LOGOUT_ID);
		break;
	case APPLET_LOCK:
		gnome_config_set_string("id", LOCK_ID);
		break;
	case APPLET_STATUS:
		gnome_config_set_string("id", STATUS_ID);
		break;
	case APPLET_RUN:
		gnome_config_set_string("id", RUN_ID);
		break;
	default:
		g_warning ("Unknown applet type encountered: %d; ignoring.",
			   info->type);
		break;
	}
	gnome_config_set_int("position", ad->pos);
	gnome_config_set_int("panel", panel_num);
	gnome_config_set_int("unique_panel_id", panel_id);
	gnome_config_set_bool("right_stick",
			      panel_widget_is_applet_stuck(panel,
							   info->widget));
	g_string_free(buf,TRUE);
	gnome_config_pop_prefix();
	
	return TRUE;
}

static void
save_panel_configuration(gpointer data, gpointer user_data)
{
	GString       *buf;
	int           *num = user_data;
	PanelData     *pd = data;
	BasePWidget   *basep = NULL; 
	PanelWidget   *panel = NULL;

	if (BASEP_IS_WIDGET (pd->panel)) {
		basep = BASEP_WIDGET (pd->panel);
		panel = PANEL_WIDGET (basep->panel);
	} else if (IS_FOOBAR_WIDGET (pd->panel)) {
		panel = PANEL_WIDGET (FOOBAR_WIDGET(pd->panel)->panel);
	}

	buf = g_string_new(NULL);

	gnome_config_push_prefix ("");
	g_string_sprintf(buf, "%spanel/Panel_%d", PANEL_CONFIG_PATH, *num);
	gnome_config_clean_section(buf->str);
	gnome_config_pop_prefix ();

	g_string_sprintf(buf, "%spanel/Panel_%d/", PANEL_CONFIG_PATH, *num);

	(*num)++;

	gnome_config_push_prefix (buf->str);

	gnome_config_set_int("type",pd->type);

	if (basep != NULL) {
		gnome_config_set_bool("hidebuttons_enabled",
				      basep->hidebuttons_enabled);
		gnome_config_set_bool("hidebutton_pixmaps_enabled",
				      basep->hidebutton_pixmaps_enabled);
	
		gnome_config_set_int ("mode", basep->mode);
		gnome_config_set_int ("state", basep->state);

		gnome_config_set_int ("level", basep->level);
		gnome_config_set_bool ("avoid_on_maximize",
				       basep->avoid_on_maximize);
		gnome_config_set_int ("screen",
				      basep->screen);
	}

	gnome_config_set_int("unique_id", panel->unique_id);

	gnome_config_set_int("sz", panel->sz);

	gnome_config_set_bool("fit_pixmap_bg", panel->fit_pixmap_bg);
	gnome_config_set_bool("strech_pixmap_bg", panel->strech_pixmap_bg);
	gnome_config_set_bool("rotate_pixmap_bg", panel->rotate_pixmap_bg);

	gnome_config_set_string ("backpixmap",
				 sure_string (panel->back_pixmap));

	g_string_sprintf(buf, "#%02x%02x%02x",
			 (guint)panel->back_color.red/256,
			 (guint)panel->back_color.green/256,
			 (guint)panel->back_color.blue/256);
	gnome_config_set_string("backcolor", buf->str);

	gnome_config_set_int("back_type", panel->back_type);
	
	g_string_free(buf,TRUE);

	/* now do different types */
	if (BORDER_IS_WIDGET(pd->panel))
		gnome_config_set_int("edge", BORDER_POS(basep->pos)->edge);

	switch (pd->type) {
	case ALIGNED_PANEL:
		gnome_config_set_int ("align", ALIGNED_POS (basep->pos)->align);
		break;
	case SLIDING_PANEL:
		gnome_config_set_int ("offset", SLIDING_POS (basep->pos)->offset);
		gnome_config_set_int ("anchor", SLIDING_POS (basep->pos)->anchor);
		break;
	case FLOATING_PANEL:
		gnome_config_set_int ("orient", PANEL_WIDGET (basep->panel)->orient);
		gnome_config_set_int ("x", FLOATING_POS (basep->pos)->x);
		gnome_config_set_int ("y", FLOATING_POS (basep->pos)->y);
		break;
	case DRAWER_PANEL:
		gnome_config_set_int ("orient", DRAWER_POS (basep->pos)->orient);
		/*gnome_config_set_int ("temp_hidden", DRAWER_POS (basep->pos)->temp_state);*/
		break;
	case FOOBAR_PANEL:
		gnome_config_set_string ("/panel/Config/clock_format", FOOBAR_WIDGET (pd->panel)->clock_format);
		gnome_config_set_int ("screen", FOOBAR_WIDGET (pd->panel)->screen);
		break;
	default:
		break;
	}

	gnome_config_pop_prefix ();
}

void
save_next_applet(void)
{
	GSList *cur;

	ss_cur_applet++;
	
	if(g_slist_length(applets)<=ss_cur_applet) {
		ss_done_save = TRUE;
		return;
	}
	
	cur = g_slist_nth(applets,ss_cur_applet);
	
	if(!cur) {
		ss_done_save = TRUE;
		return;
	}
	
	if(save_applet_configuration(cur->data))
		save_next_applet();

	gnome_config_sync();
	gnome_config_drop_all();
}

/*
 * We queue the location of .desktop files belonging
 * to dead launchers here, to be removed when saving
 * the session.
 */
static GList *session_dead_launcher_locations = NULL;

void
session_add_dead_launcher (const gchar *location)
{
	g_return_if_fail (location);

	session_dead_launcher_locations =
			g_list_prepend (session_dead_launcher_locations, 
					g_strdup (location));
}

static void
session_unlink_dead_launchers (void)
{
	GList *l;

	for (l = session_dead_launcher_locations; l; l = l->next) {
		gchar *file = l->data;

		unlink (file);

		g_free (file);

		l->data = NULL;
	}

	g_list_free (session_dead_launcher_locations);

	session_dead_launcher_locations = NULL;
}

static void
do_session_save(GnomeClient *client,
		gboolean complete_sync,
		gboolean sync_applets,
		gboolean sync_panels)
{
	int num;
#if PER_SESSION_CONFIGURATION
	gchar *new_args[] = { "rm", "-r", NULL };
#endif /* PER_SESSION_CONFIGURATION */

	/* If in commie mode, then no saving is needed, the user
	 * could have changed anything anyway */
	if (commie_mode)
		return;

	if (panel_cfg_path)
		g_free (panel_cfg_path);

#ifdef PER_SESSION_CONFIGURATION
	if (gnome_client_get_flags(client) & GNOME_CLIENT_IS_CONNECTED &&
	    GNOME_CLIENT (client)->restart_style != GNOME_RESTART_NEVER)
		panel_cfg_path = g_strdup (gnome_client_get_config_prefix (client));
	else
#endif /* PER_SESSION_CONFIGURATION */		
		panel_cfg_path = g_strdup (PANEL_CONFIG_PATH);

#ifdef PER_SESSION_CONFIGURATION
	new_args[2] = gnome_config_get_real_path (panel_cfg_path);
	gnome_client_set_discard_command (client, 3, new_args);
#endif /* PER_SESSION_CONFIGURATION */

#ifdef SESSION_DEBUG	
	printf("Saving to [%s]\n",PANEL_CONFIG_PATH);

	printf("Saving session: 1"); fflush(stdout);

	printf(" 2"); fflush(stdout);
#endif

	gnome_config_push_prefix (PANEL_CONFIG_PATH "panel/Config/");

	if(complete_sync || sync_applets)
		gnome_config_set_int ("applet_count", applet_count);
#ifdef SESSION_DEBUG
	printf(" 3"); fflush(stdout);
#endif
	if(complete_sync || sync_panels) {
		num = 1;
		g_slist_foreach(panel_list, save_panel_configuration, &num);
		gnome_config_set_int("panel_count",num-1);
	}
#ifdef SESSION_DEBUG
	printf(" 4\n"); fflush(stdout);
#endif
	gnome_config_pop_prefix ();

	if(complete_sync)
		save_tornoff();

	gnome_config_sync();
	
	if(complete_sync || sync_applets) {
		ss_cur_applet = -1;
		ss_done_save = FALSE;

		session_unlink_dead_launchers ();

		/* start saving applets */
		save_next_applet ();
	}
}

static guint sync_handler = 0;
static gboolean sync_handler_needed = FALSE;

void
panel_config_sync(void)
{
	int ncs = need_complete_save;
	int ats = applets_to_sync;
	int pts = panels_to_sync;

	if (sync_handler != 0) {
		gtk_timeout_remove (sync_handler);
		sync_handler = 0;
	}

	sync_handler_needed = FALSE;

	if(need_complete_save ||
	   applets_to_sync ||
	   panels_to_sync) {
#ifdef PER_SESSION_CONFIGURATION
		if (!(gnome_client_get_flags(client) & 
		      GNOME_CLIENT_IS_CONNECTED)) {
#endif
			need_complete_save = FALSE;
			applets_to_sync = FALSE;
			panels_to_sync = FALSE;
			do_session_save (client, ncs, ats, pts);
#ifdef PER_SESSION_CONFIGURATION
		} else {
			/*prevent possible races by doing this before requesting
			  save*/
			need_complete_save = FALSE;
			applets_to_sync = FALSE;
			panels_to_sync = FALSE;
			gnome_client_request_save (client, GNOME_SAVE_LOCAL, FALSE,
						   GNOME_INTERACT_NONE, FALSE, FALSE);
		}
#endif /* PER_SESSION_CONFIGURATION */
	}
}

static gboolean
sync_handler_timeout (gpointer data)
{
	sync_handler = 0;

	if (sync_handler_needed)
		panel_config_sync ();

	return FALSE;
}

void
panel_config_sync_schedule (void)
{
	if (sync_handler == 0) {
		/* don't sync for another 30 secs */
		sync_handler = gtk_timeout_add (30000, sync_handler_timeout, NULL);
		sync_handler_needed = FALSE;
		panel_config_sync ();
	} else {
		/* a timeout is running */
		sync_handler_needed = TRUE;
	}
}


/* This is called when the session manager requests a shutdown.  It
   can also be run directly when we don't detect a session manager.
   We assume no interaction is done by the applets.  And we ignore the
   other arguments for now.  Yes, this is lame.  */
/* update: some SM stuff implemented but we still ignore most of the
   arguments now*/
int
panel_session_save (GnomeClient *client,
		    int phase,
		    GnomeSaveStyle save_style,
		    int is_shutdown,
		    GnomeInteractStyle interact_style,
		    int is_fast,
		    gpointer client_data)
{
	if (is_shutdown) {
		ss_timeout = 1500;
		ss_interactive = TRUE;
	}
	do_session_save(client,TRUE,FALSE,FALSE);
	if (is_shutdown) {
		while(!ss_done_save)
			gtk_main_iteration_do(TRUE);
	}
	/* Always successful.  */
	return TRUE;
}

int
panel_session_die (GnomeClient *client,
		   gpointer client_data)
{
	GSList *li;

	gtk_timeout_remove (config_sync_timeout);
	config_sync_timeout = 0;
  
	status_inhibit = TRUE;
	status_spot_remove_all ();

	for (li = applets; li != NULL; li = li->next) {
		AppletInfo *info = li->data;

		if (info->type == APPLET_EXTERN) {
			extern_save_last_position ((Extern)info->data, FALSE);

			gtk_widget_destroy (info->widget);
		}
		else if (info->type == APPLET_SWALLOW) {
			Swallow *swallow = info->data;

			swallow->clean_remove = TRUE;

			if(GTK_SOCKET(swallow->socket)->plug_window)
				XKillClient(GDK_DISPLAY(),
					    GDK_WINDOW_XWINDOW(GTK_SOCKET(swallow->socket)->plug_window));
		}
	}

	gnome_config_sync ();

	xstuff_unsetup_desktop_area ();
			
	extern_shutdown ();
	
	gtk_main_quit();
	return TRUE;
}

/* the logout function */
void
panel_quit (void)
{
	gnome_client_request_save (client, GNOME_SAVE_BOTH, 1,
				   GNOME_INTERACT_ANY, 0, 1);
}

#ifdef FIXME
/* try evil hacks to rewrite panel config from old applets (gnomepager for
 * now.  This is very evil.  But that's the only way to do it, it seems */
static gboolean
try_evil_config_hacks (const char *goad_id, PanelWidget *panel, int pos)
{
	gboolean ret = FALSE;

	if (strcmp (goad_id, "gnomepager_applet") == 0) {
		static gboolean first_time = TRUE;
		static gboolean in_path = FALSE;

		if(first_time) {
			char *tmp;
			tmp = gnome_is_program_in_path("gnomepager_applet");
			in_path = tmp != NULL;
			first_time = FALSE;
			g_free(tmp);
		}

		if (!in_path) {
			extern_load_applet ("deskguide_applet", NULL,
					    panel, pos, TRUE, TRUE);

			extern_load_applet ("tasklist_applet", NULL,
					    panel, pos+1, TRUE, TRUE);
			ret = TRUE;
		}
	}
	return ret;
}
#endif /* FIXME */

char *
get_correct_prefix (char const **sep)
{
	int count;
	char *path;

	gnome_config_push_prefix ("");

	/* if we're being commies, we just use the global path */
	if (commie_mode) {
		gnome_config_pop_prefix ();
		*sep = "=";
		return g_strdup_printf ("=" GLOBAL_CONFDIR PANEL_CONFIG_PATH);
	}

	count = conditional_get_int (PANEL_CONFIG_PATH
				     "panel/Config/panel_count", 0, NULL);
	if (count > 0) {
		gnome_config_pop_prefix ();
		*sep = "";
		return g_strdup (PANEL_CONFIG_PATH);
	}

	path = "=" GLOBAL_CONFDIR PANEL_CONFIG_PATH "panel=/Config/panel_count";
	count = conditional_get_int (path, 0, NULL);

	if (count > 0) {
		gnome_config_pop_prefix ();
		*sep = "=";
		return g_strdup_printf ("=" GLOBAL_CONFDIR PANEL_CONFIG_PATH);
	}

	gnome_config_pop_prefix ();

	/* Eeeeek! return to standard home path and load some slightly sane
	 * setup */
	*sep = "";
	return g_strdup (PANEL_CONFIG_PATH);
}

void
push_correct_global_prefix (void)
{
	if ( ! commie_mode) {
		gboolean foo, def;

		gnome_config_push_prefix ("/panel/Config/");

		foo = conditional_get_bool ("tooltips_enabled", TRUE, &def);
		if ( ! def)
			return;
	}

	/* ahhh, this doesn't exist, but tooltips_enabled should be
	 * in every home, every kitchen and every panel configuration,
	 * so we will load up from the global location */
	gnome_config_pop_prefix ();
	gnome_config_push_prefix ("=" GLOBAL_CONFDIR "/panel=/Config/");
}

static void
init_user_applets(void)
{
	GString *buf;
	int count, num;	
	DistributionType distribution = get_distribution_type ();
	char *prefix;
	const char *sep;

	prefix = get_correct_prefix (&sep);

	buf = g_string_new (NULL);

	g_string_sprintf (buf, "%spanel%s/Config/applet_count",
			  prefix, sep);
	count = conditional_get_int (buf->str, 0, NULL);
	for (num = 1; num <= count; num++) {
		char *applet_name;
		int   pos = 0, panel_num, panel_id;
		PanelWidget *panel;

		g_string_sprintf (buf, "%sApplet_Config%s/Applet_%d/",
				  prefix, sep, num);
		gnome_config_push_prefix (buf->str);

		if ( ! conditional_true ("Conditional")) {
			gnome_config_pop_prefix ();
			continue;
		}

		applet_name = conditional_get_string ("id", "Unknown", NULL);
		
		if (strcmp (applet_name, EMPTY_ID) == 0) {
			g_free (applet_name);
			gnome_config_pop_prefix ();
			continue;
		} else if (strcmp (applet_name, "Unknown") == 0) {
			g_warning ("Unknown applet type!");
			g_free (applet_name);
			gnome_config_pop_prefix ();
			continue;
		}

		pos = conditional_get_int ("position", 0, NULL);
		panel_id = conditional_get_int ("unique_panel_id", -1, NULL);
		if (panel_id < 0) {
			GSList *list;

			panel_num = conditional_get_int ("panel", 0, NULL);

			list = g_slist_nth (panels, panel_num);
			if (list == NULL) {
				g_warning ("Can't find panel, "
					   "putting applet on the first one");
				panel = panels->data;
			} else
				panel = list->data;
		} else {
			panel = panel_widget_get_by_id (panel_id);
			if (panel == NULL) {
				g_warning ("Can't find panel, "
					   "putting applet on the first one");
				panel = panels->data;
			}
		}

		/* if this isn't true, then we are smoking some serious crack */
		g_assert (panel != NULL);
		
		/*if we are to right stick this, make the number large, 
		 G_MAXINT/2 should allways be large enough */
		pos += (conditional_get_bool ("right_stick", FALSE, NULL)
			? G_MAXINT/2 : 0);

#ifdef FIXME 
		if (strcmp (applet_name, EXTERN_ID) == 0) {
			char *goad_id = conditional_get_string ("goad_id",
								NULL, NULL);
			if ( ! string_empty (goad_id) &&
			    /* if we try an evil hack such as loading tasklist
			     * and deskguide instead of the desguide don't
			     * try to load this applet in the first place */
			    ! try_evil_config_hacks (goad_id, panel, pos)) {
				/*this is the config path to be passed to the
				  applet when it loads*/
				g_string_sprintf (buf, "%sApplet_%d_Extern/",
						  PANEL_CONFIG_PATH, num);

				extern_load_applet (goad_id, buf->str, 
						    panel, pos, TRUE, TRUE);
			}
			g_free (goad_id);

		} else 
#endif /* FIXME */

		if (strcmp (applet_name, LAUNCHER_ID) == 0) { 
			gboolean hoard = FALSE;
			Launcher *launcher;
			char *file;

			file = conditional_get_string ("base_location", NULL,
						       NULL);
			if (string_empty (file)) {
				g_free (file);
				file = conditional_get_string ("parameters",
							       NULL, NULL);
				hoard = TRUE;
			} 
			if (*file != '/') {
				char *tmp;
#ifdef SESSION_DEBUG
	printf ("init_user_applets for non-starting slashes [%s]\n", file);
#endif
				tmp = launcher_file_name (file);
				g_free (file);
				file = tmp;
			} 

			launcher = load_launcher_applet(file, panel, pos, TRUE);
			g_free(file);

			/* If this was an old style launcher, hoard it now */
			if (hoard && launcher != NULL)
				launcher_hoard (launcher);

		} else if (strcmp (applet_name, LOGOUT_ID) == 0) { 
			load_logout_applet (panel, pos, TRUE);

		} else if (strcmp (applet_name, LOCK_ID) == 0) {
			load_lock_applet (panel, pos, TRUE);

		} else if (strcmp (applet_name, STATUS_ID) == 0) {
			load_status_applet (panel, pos, TRUE);
			
		} else if (strcmp (applet_name, RUN_ID) == 0) {
			load_run_applet (panel, pos, TRUE);

		} else if (strcmp (applet_name, SWALLOW_ID) == 0) {
			char *path = conditional_get_string ("execpath",
							     NULL, NULL);
			char *params = conditional_get_string ("parameters",
							       NULL, NULL);
			int width = conditional_get_int ("width", 0, NULL);
			int height = conditional_get_int ("height", 0, NULL);
			load_swallow_applet (path, params, width, height,
					     panel, pos, TRUE);
			g_free (path);
			g_free (params);

		} else if (strcmp (applet_name, MENU_ID) == 0) {
			char *params = conditional_get_string ("parameters",
							       ".", NULL);
			int type = conditional_get_int ("main_menu_type", -1,
							NULL);
			/* this defaults to false, because we want old menus to
			 * work in the old style */
			gboolean global_main_was_default;
			gboolean global_main =
				conditional_get_bool ("global_main", FALSE,
						      &global_main_was_default);
			int flags;
			gboolean old_style = 
				conditional_get_bool ("old_style_main", TRUE,
						      NULL);
			gboolean custom_icon = 
				conditional_get_bool ("custom_icon", FALSE,
						      NULL);
			char *custom_icon_file =
				conditional_get_string ("custom_icon_file",
							NULL, NULL);

			flags = conditional_get_int ("main_menu_flags",
						     get_default_menu_flags (),
						     NULL);
			if (flags < 0)
				flags = get_default_menu_flags ();
			
			/* Hack to try to do the right conversion while trying
			 * to turn on the feature on as many setups as
			 * possible */
			if (global_main_was_default &&
			    flags == global_config.menu_flags) {
				global_main = TRUE;
			}

			if(type >= 0) {
				flags = 0;
				if (type == X_MAIN_MENU_BOTH) {
					flags |= MAIN_MENU_SYSTEM|MAIN_MENU_USER;
				} else if(type == X_MAIN_MENU_SYSTEM) {
					flags |= MAIN_MENU_SYSTEM|
						MAIN_MENU_USER_SUB;
				} else {
					flags |= MAIN_MENU_SYSTEM_SUB|
						MAIN_MENU_USER;
				}
				/*guess distribution menus*/
				if (distribution != DISTRIBUTION_UNKNOWN)
					flags |= MAIN_MENU_DISTRIBUTION_SUB;
				/*guess KDE menus */
				if (g_file_test (kde_menudir, G_FILE_TEST_IS_DIR))
					flags |= MAIN_MENU_KDE_SUB;
			}
			if(old_style) {
				/*this is needed to make panel properly
				  read older style configs */
				if(flags&MAIN_MENU_SYSTEM &&
				   flags&MAIN_MENU_SYSTEM_SUB)
					flags &=~ MAIN_MENU_SYSTEM;
				if(flags&MAIN_MENU_USER &&
				   flags&MAIN_MENU_USER_SUB)
					flags &=~ MAIN_MENU_USER;
				if(flags&MAIN_MENU_DISTRIBUTION &&
				   flags&MAIN_MENU_DISTRIBUTION_SUB)
					flags &=~ MAIN_MENU_DISTRIBUTION;
				/*keep this for compatibility with older
				  config files */
				if(flags&MAIN_MENU_OBSOLETE_DEBIAN &&
				   flags&MAIN_MENU_OBSOLETE_DEBIAN_SUB)
					flags &=~ MAIN_MENU_DISTRIBUTION;
				if(flags&MAIN_MENU_KDE &&
				   flags&MAIN_MENU_KDE_SUB)
					flags &=~ MAIN_MENU_KDE;
				flags |= MAIN_MENU_APPLETS_SUB |
					MAIN_MENU_PANEL_SUB |
					MAIN_MENU_DESKTOP;
			}

			if (old_style)
				load_menu_applet(params, flags, FALSE,
						 custom_icon, custom_icon_file,
						 panel, pos, TRUE);
			else
				load_menu_applet(params, flags, global_main,
						 custom_icon, custom_icon_file,
						 panel, pos, TRUE);

			g_free (custom_icon_file);
			g_free (params);

		} else if (strcmp (applet_name, DRAWER_ID) == 0) {
			int mypanel = conditional_get_int ("parameters", -1,
							   NULL);
			int mypanel_id = conditional_get_int
				("unique_drawer_panel_id", -1, NULL);
			char *pixmap = conditional_get_string ("pixmap",
							       NULL, NULL);
			char *tooltip = conditional_get_string ("tooltip",
								NULL, NULL);
			if (mypanel_id < 0 && mypanel >= 0) {
				PanelWidget *pw = g_slist_nth_data (panels, mypanel);
				if (pw != NULL)
					mypanel_id = pw->unique_id;
			}
			load_drawer_applet (mypanel_id, pixmap, tooltip,
					    panel, pos, TRUE);
			g_free (pixmap);
			g_free (tooltip);
		} else {
			g_warning ("Unknown applet type!");
		}
		gnome_config_pop_prefix ();
		g_free (applet_name);
	}
	g_string_free (buf, TRUE);
	g_free (prefix);
}

static void
init_user_panels(void)
{
	GString *buf;
	int count, num;	
	char *s;
	GtkWidget *panel = NULL;
	char *prefix;
	const char *sep;

	prefix = get_correct_prefix (&sep);

	buf = g_string_new (NULL);

	g_string_sprintf (buf, "%spanel%s/Config/panel_count",
			  prefix, sep);

	count = conditional_get_int (buf->str, 0, NULL);

	/*load a default snapped panel on the bottom of the screen,
	  it is required to have at least one panel for this all
	  to work, so this is the way we find out if there was no
	  config from last time*/
	if (count <= 0)  {
		/* Eeeeeeeek! no default config, no user config, this is
		 * bad bad bad, load a single panel */
		panel = edge_widget_new (0 /* screen */,
					 BORDER_BOTTOM,
					 BASEP_EXPLICIT_HIDE /* mode */,
					 BASEP_SHOWN /* state */,
					 BASEP_LEVEL_DEFAULT /* level */,
					 TRUE /* avoid_on_maximize */,
					 SIZE_STANDARD /* size */,
					 TRUE /* hidebuttons_enabled */,
					 TRUE /* hidebutton_pixmaps */,
					 PANEL_BACK_NONE /* back type */,
					 NULL,
					 TRUE,
					 FALSE,
					 TRUE,
					 NULL);
		panel_setup(panel);
		gtk_widget_show(panel);

		/* load up the foot menu */
		load_menu_applet(NULL, get_default_menu_flags (),
				 TRUE, FALSE, NULL,
				 PANEL_WIDGET(BASEP_WIDGET(panel)->panel),
				 0, TRUE);

		g_free (prefix);
		g_string_free (buf, TRUE);

		return;
	}

	for (num = 1; num <= count; num++) {
		PanelType type;
		PanelBackType back_type;
		int sz;
		BasePState state;
		BasePMode mode;
		BasePLevel level;
		gboolean avoid_on_maximize;
		BorderEdge edge;
		char *back_pixmap, *color;
		GdkColor back_color = {0,0,0,1};
		gboolean fit_pixmap_bg;
		gboolean strech_pixmap_bg;
		gboolean rotate_pixmap_bg;
		int hidebuttons_enabled;
		int hidebutton_pixmaps_enabled;
		int unique_id;
		int screen;

		g_string_sprintf (buf, "%spanel%s/Panel_%d/",
				  prefix, sep, num);
		gnome_config_push_prefix (buf->str);

		if ( ! conditional_true ("Conditional")) {
			gnome_config_pop_prefix ();
			continue;
		}

		unique_id = conditional_get_int ("unique_id", -1, NULL);
		
		back_pixmap = conditional_get_string ("backpixmap", NULL, NULL);
		if (string_empty (back_pixmap)) {
			g_free (back_pixmap);
			back_pixmap = NULL;
		}

		color = conditional_get_string ("backcolor", "#ffffff", NULL);
		if ( ! string_empty (color))
			gdk_color_parse (color, &back_color);

		back_type=conditional_get_int ("back_type",
					       PANEL_BACK_NONE, NULL);
		fit_pixmap_bg = conditional_get_bool ("fit_pixmap_bg",
						      TRUE, NULL);
		strech_pixmap_bg = conditional_get_bool ("strech_pixmap_bg",
							 FALSE, NULL);
		rotate_pixmap_bg = conditional_get_bool ("rotate_pixmap_bg",
							 FALSE, NULL);

		sz=conditional_get_int ("sz", SIZE_STANDARD, NULL);
		if(sz < 0)
			sz = 0;

		/*a hack to allow for old config files to be read correctly*/
		/*don't update this if new sizes (SIZE_SMALL) are added*/
		if(sz < 4) {
			switch(sz) {
			case 0: sz = SIZE_TINY; break;
			case 1: sz = SIZE_STANDARD; break;
			case 2: sz = SIZE_LARGE; break;
			case 3: sz = SIZE_HUGE; break;
			default: break;
			}
		}
		
		/*now for type specific config*/

		type = conditional_get_int ("type", EDGE_PANEL, NULL);

		hidebuttons_enabled =
			conditional_get_bool ("hidebuttons_enabled", TRUE, NULL);
		hidebutton_pixmaps_enabled =
			conditional_get_bool ("hidebutton_pixmaps_enabled", TRUE, NULL);

		state = conditional_get_int ("state", 0, NULL);
		mode = conditional_get_int ("mode", 0, NULL);
		level = conditional_get_int ("level", 0, NULL);
		screen = conditional_get_int ("screen", 0, NULL);
		if (screen < 0)
			screen = 0;
#if 0 /* i guess we can't easily do this for now */
		pos = basep_widget_load_pos_settings();
#endif
		switch (type) {
			
		case EDGE_PANEL:
			edge = conditional_get_int ("edge", BORDER_BOTTOM,
						    NULL);

			avoid_on_maximize = conditional_get_bool
				("avoid_on_maximize", TRUE, NULL);

			panel = edge_widget_new (screen,
						 edge, 
						 mode, state,
						 level, avoid_on_maximize,
						 sz,
						 hidebuttons_enabled,
						 hidebutton_pixmaps_enabled,
						 back_type, back_pixmap,
						 fit_pixmap_bg,
						 strech_pixmap_bg,
						 rotate_pixmap_bg,
						 &back_color);
			break;
		case ALIGNED_PANEL: {
			AlignedAlignment align;
			edge = conditional_get_int ("edge", BORDER_BOTTOM,
						    NULL);
			
			align = conditional_get_int ("align", ALIGNED_LEFT,
						     NULL);

			avoid_on_maximize = conditional_get_bool
				("avoid_on_maximize", TRUE, NULL);

			panel = aligned_widget_new (screen,
						    align,
						    edge,
						    mode,
						    state,
						    level,
						    avoid_on_maximize,
						    sz,
						    hidebuttons_enabled,
						    hidebutton_pixmaps_enabled,
						    back_type,
						    back_pixmap,
						    fit_pixmap_bg,
						    strech_pixmap_bg,
						    rotate_pixmap_bg,
						    &back_color);
			break;
		}
		case SLIDING_PANEL: {
			gint16 offset;
			SlidingAnchor anchor;
			edge = conditional_get_int ("edge", BORDER_BOTTOM,
						    NULL);
			
			anchor = conditional_get_int ("anchor",
						      SLIDING_ANCHOR_LEFT,
						      NULL);

			offset = conditional_get_int ("offset", 0, NULL);

			avoid_on_maximize = conditional_get_bool
				("avoid_on_maximize", TRUE, NULL);

			panel = sliding_widget_new (screen,
						    anchor,
						    offset,
						    edge,
						    mode,
						    state,
						    level,
						    avoid_on_maximize,
						    sz,
						    hidebuttons_enabled,
						    hidebutton_pixmaps_enabled,
						    back_type,
						    back_pixmap,
						    fit_pixmap_bg,
						    strech_pixmap_bg,
						    rotate_pixmap_bg,
						    &back_color);
			break;
		}
		case DRAWER_PANEL: {
			PanelOrientType orient;
			/*BasePState temp_state;*/

			orient = conditional_get_int ("orient", ORIENT_UP,
						      NULL);

			/* FIXME: there are some issues with auto hiding drawers */

			avoid_on_maximize = conditional_get_bool
				("avoid_on_maximize", FALSE, NULL);

			panel = drawer_widget_new (orient,
						   BASEP_EXPLICIT_HIDE, 
						   state,
						   level,
						   avoid_on_maximize,
						   sz,
						   hidebuttons_enabled,
						   hidebutton_pixmaps_enabled,
						   back_type,
						   back_pixmap,
						   fit_pixmap_bg,
						   strech_pixmap_bg,
						   rotate_pixmap_bg,
						   &back_color);
#if 0
			g_string_sprintf (buf, "temp_state=%d", BASEP_SHOWN);
			temp_state = conditional_get_int (buf->str);
			DRAWER_POS (BASEP_WIDGET (panel)->pos)->temp_state = temp_state;
#endif
			break;
		}
		case FLOATING_PANEL: {
			PanelOrientation orient;
			int x, y;
			
			orient = conditional_get_int ("orient",
						      PANEL_HORIZONTAL,
						      NULL);

			x = conditional_get_int ("x", 0, NULL);
			y = conditional_get_int ("y", 0, NULL);

			avoid_on_maximize = conditional_get_bool
				("avoid_on_maximize", FALSE, NULL);

			panel = floating_widget_new (screen,
						     x,
						     y,
						     orient,
						     mode,
						     state,
						     level,
						     avoid_on_maximize,
						     sz,
						     hidebuttons_enabled,
						     hidebutton_pixmaps_enabled,
						     back_type,
						     back_pixmap,
						     fit_pixmap_bg,
						     strech_pixmap_bg,
						     rotate_pixmap_bg,
						     &back_color);
			break;
		}
		case FOOBAR_PANEL:
			panel = foobar_widget_new (screen);

			push_correct_global_prefix ();
			s = conditional_get_string ("clock_format",
						    _("%I:%M:%S %p"),
						    NULL);
			gnome_config_pop_prefix ();

			if (s != NULL)
				foobar_widget_set_clock_format (FOOBAR_WIDGET (panel), s);
			g_free (s);

			break;
		default:
			panel = NULL;
			g_warning ("Unkown panel type: %d; ignoring.", type);
			break;
		}

		gnome_config_pop_prefix ();
		
		g_free (color);
		g_free (back_pixmap);

		if (panel != NULL) {
			if (unique_id > 0)
				panel_set_id (panel, unique_id);
			panel_setup (panel);
			gtk_widget_show (panel);
		}
	}
	g_string_free (buf, TRUE);

	g_free (prefix);
}

static gboolean
is_among_users (const char *username, const char *users)
{
	char *copy;
	char *p;

	if (users == NULL)
		return FALSE;

	copy = g_strchug (g_strdup (users));

	if (strcmp (copy, "*") == 0) {
		g_free (copy);
		return TRUE;
	}

	p = strtok (copy, ", \t;:");
	while (p != NULL) {
		if (strcasecmp_no_locale (username, p) == 0) {
			g_free (copy);
			return TRUE;
		}
		p = strtok (NULL, ", \t;:");
	}

	g_free (copy);
	return FALSE;
}

void
load_system_wide (void)
{
	char *users;
	const char *username = g_get_user_name ();

	gnome_config_push_prefix ("=" GLOBAL_CONFDIR "/System=/Restrictions/");

	commie_mode = gnome_config_get_bool ("LockDown=FALSE");
	no_run_box = gnome_config_get_bool ("NoRunBox=FALSE");

	users = gnome_config_get_string ("RestrictedUsers=*");
	if (is_among_users (username, users)) {
		g_free (users);
		users = gnome_config_get_string ("UnrestrictedUsers=");
		if (is_among_users (username, users)) {
			commie_mode = FALSE;
			no_run_box = FALSE;
		}
	} else {
		commie_mode = FALSE;
		no_run_box = FALSE;
	}
	g_free (users);

	/* Root shall always be allowed to do whatever */
	if (getuid () == 0) {
		commie_mode = FALSE;
		no_run_box = FALSE;
	}

	gnome_config_pop_prefix ();
}


void
load_up_globals (void)
{
	global_config.hide_speed = 
		panel_gconf_global_config_get_int ("panel-hide-speed");

	global_config.minimized_size = 
		panel_gconf_global_config_get_int ("panel-minimized-size");

	global_config.hide_delay = 
		panel_gconf_global_config_get_int ("panel-hide-delay");

	global_config.show_delay =
		panel_gconf_global_config_get_int ("panel-show-delay");

	global_config.tooltips_enabled =
		panel_gconf_global_config_get_bool ("tooltips-enabled");

	global_config.keep_menus_in_memory =
		panel_gconf_global_config_get_bool ("keep-menus-in-memory");

	global_config.disable_animations =
		panel_gconf_global_config_get_bool ("disable-animations");

	global_config.autoraise = 
		panel_gconf_global_config_get_bool ("autoraise-panel");

	global_config.layer = 
		panel_gconf_global_config_get_int ("panel-window-layer");
	
	global_config.drawer_auto_close =
		panel_gconf_global_config_get_bool ("drawer-auto-close");

	global_config.highlight_when_over =
		panel_gconf_global_config_get_bool ("highlight-launchers-on-mouseover");
	
	global_config.confirm_panel_remove =
		panel_gconf_global_config_get_bool ("confirm-panel-remove");

	global_config.keys_enabled = 
		panel_gconf_global_config_get_bool ("enable-key-bindings");

	
	global_config.menu_key = 
		panel_gconf_global_config_get_string ("menu-key");
	
	convert_string_to_keysym_state(global_config.menu_key,
				       &global_config.menu_keysym,
				       &global_config.menu_state);
	
	global_config.run_key = 
		panel_gconf_global_config_get_string ("run-key");

	convert_string_to_keysym_state(global_config.run_key,
				       &global_config.run_keysym,
				       &global_config.run_state);

	global_config.screenshot_key =
		panel_gconf_global_config_get_string ("screenshot-key");

	convert_string_to_keysym_state (global_config.screenshot_key,
					&global_config.screenshot_keysym,
					&global_config.screenshot_state);

	global_config.window_screenshot_key = 
		panel_gconf_global_config_get_string ("window-screenshot-key");

	convert_string_to_keysym_state (global_config.window_screenshot_key,
					&global_config.window_screenshot_keysym,
					&global_config.window_screenshot_state);
	
	global_config.use_large_icons =
		panel_gconf_global_config_get_bool ("use_large_icons");

	global_config.avoid_collisions =
		panel_gconf_global_config_get_bool ("avoid-panel-overlap");

	/* FIXME STUFF THAT IS BORKED */
	global_config.merge_menus = TRUE;
	global_config.menu_check = TRUE;
	global_config.menu_flags = get_default_menu_flags();
/*
	global_config.merge_menus =
		conditional_get_bool ("merge_menus", TRUE, NULL);

	global_config.menu_check =
		conditional_get_bool ("menu_check", TRUE, NULL);

	
	global_config.menu_flags = conditional_get_int
		("menu_flags", get_default_menu_flags (), NULL);
	if (global_config.menu_flags < 0) {
		global_config.menu_flags = get_default_menu_flags ();
	}
*/
	gnome_config_sync ();

	apply_global_config ();
	
}

void
write_global_config (void)
{
	gnome_config_push_prefix ("/panel/Config/");
/* FIXME
	gnome_config_set_int ("hide_speed",
			      global_config.hide_speed);
	gnome_config_set_int ("minimized_size",
			      global_config.minimized_size);
	gnome_config_set_int ("hide_delay",
			      global_config.hide_delay);
	gnome_config_set_int ("show_delay",
			      global_config.show_delay);
	gnome_config_set_bool ("tooltips_enabled",
			       global_config.tooltips_enabled);
	gnome_config_set_bool ("use_large_icons",
			       global_config.use_large_icons);
	gnome_config_set_bool ("merge_menus",
			       global_config.merge_menus);
	gnome_config_set_bool ("menu_check",
			       global_config.menu_check);
	gnome_config_set_bool ("off_panel_popups",
			       global_config.off_panel_popups);
	gnome_config_set_bool ("disable_animations",
			       global_config.disable_animations);
	gnome_config_set_bool ("autoraise",
			       global_config.autoraise);
	gnome_config_set_bool ("keep_bottom",
			       global_config.keep_bottom);
	gnome_config_set_bool ("normal_layer",
			       global_config.normal_layer);
	gnome_config_set_bool ("drawer_auto_close",
			       global_config.drawer_auto_close);
	gnome_config_set_bool ("saturate_when_over",
			       global_config.saturate_when_over);
	gnome_config_set_bool ("confirm_panel_remove",
			       global_config.confirm_panel_remove);
	gnome_config_set_int ("menu_flags", global_config.menu_flags);
	gnome_config_set_bool ("keys_enabled", global_config.keys_enabled);
	gnome_config_set_string ("menu_key", global_config.menu_key);
	gnome_config_set_string ("run_key", global_config.run_key);
	gnome_config_set_string ("screenshot_key",
				 global_config.screenshot_key);
	gnome_config_set_string ("window_screenshot_key",
				 global_config.window_screenshot_key);
	gnome_config_set_bool ("avoid_collisions",
			       global_config.avoid_collisions);
			     
*/	
	gnome_config_pop_prefix();
	gnome_config_sync();
	
}

void load_session (void) {
  init_user_panels ();
  init_user_applets ();
}
