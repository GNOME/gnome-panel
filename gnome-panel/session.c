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
#include "floating-widget.h"
#include "foobar-widget.h"
#include "launcher.h"
#include "logout.h"
#include "menu-fentry.h"
#include "menu-util.h"
#include "menu.h"
#include "panel-util.h"
#include "panel-gconf.h"
#include "panel-main.h"
#include "panel-config-global.h"
#include "session.h"
#include "sliding-widget.h"
#include "status.h"
#include "swallow.h"
#include "gnome-run.h"
#include "global-keys.h"
#include "xstuff.h"
#include "multiscreen-stuff.h"
#include "conditional.h"
#include "panel-applet-frame.h"
#include "panel-shell.h"

#undef SESSION_DEBUG

extern GSList          *panels;
extern GSList          *applets;
extern int              applet_count;
extern GtkTooltips     *panel_tooltips;
extern GnomeClient     *client;
extern GSList          *panel_list;
extern char            *kde_menudir;
static int              config_sync_timeout;

gboolean                applets_to_sync = FALSE;
gboolean                panels_to_sync = FALSE;
gboolean                need_complete_save = FALSE;
gboolean                commie_mode = FALSE;
gboolean                no_run_box = FALSE;
GlobalConfig            global_config;

static gchar *panel_profile_name = NULL;

gchar *
session_get_current_profile (void) {
	return g_strdup (panel_profile_name);
}

void
session_set_current_profile (const gchar *profile_name) {

	g_return_if_fail (profile_name != NULL);

	if (panel_profile_name != NULL);
		g_free (panel_profile_name);
	panel_profile_name = g_strdup (profile_name);
}

void
apply_global_config (void)
{
	static int layer_old = -1;
	static int menu_flags_old = -1;
	static int old_menu_check = -1;
	GSList *li;

	if (global_config.tooltips_enabled)
		gtk_tooltips_enable (panel_tooltips);
	else
		gtk_tooltips_disable (panel_tooltips);
	/* not incredibly efficent way to do this, we just make
	 * sure that all directories are reread */
	if (old_menu_check != global_config.menu_check) {
		fr_force_reread ();
	}
	if(old_menu_check != global_config.menu_check) {
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
	old_menu_check = global_config.menu_check;

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
			else if (FOOBAR_IS_WIDGET (pd->panel))
				foobar_widget_update_winhints (FOOBAR_WIDGET (pd->panel));
		}
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

	panel_global_keys_setup();
}

static void
panel_session_save_applets (GSList *applets_list)
{
	GSList *l;

	for (l = applets_list; l; l = l->next) {
		AppletInfo *info = l->data;
	
		g_return_if_fail (info);

		switch (info->type) {
		case APPLET_BONOBO:
			panel_applet_frame_save_position (
					PANEL_APPLET_FRAME (info->data));
			break;
#ifdef FIXME
		case APPLET_EMPTY:
			/*
			 * just save id
			 */
			break;
		case APPLET_DRAWER: {
			int i;
			Drawer *drawer = info->data;

			gnome_config_set_string("id", DRAWER_ID);

			i = g_slist_index(panels,
					  BASEP_WIDGET(drawer->drawer)->panel);
			if(i>=0)
				gnome_config_set_int("parameters",i);
			gnome_config_set_string ("unique_drawer_panel_id", 
					      PANEL_WIDGET (BASEP_WIDGET(drawer->drawer)->panel)->unique_id);
			gnome_config_set_string("pixmap",
						drawer->pixmap);
			gnome_config_set_string("tooltip",
						drawer->tooltip);
			}
			break;
		case APPLET_SWALLOW: {
			Swallow *swallow = info->data;
			gnome_config_set_string("id", SWALLOW_ID);
			gnome_config_set_string("parameters",
						swallow->title);
			gnome_config_set_string("execpath",
						swallow->path);
			gnome_config_set_int("width",swallow->width);
			gnome_config_set_int("height",swallow->height);
			}
			break;
		case APPLET_MENU: {
			Menu *menu = info->data;
			gnome_config_set_string("id", MENU_ID);
			gnome_config_set_string("parameters",
						menu->path);
			gnome_config_set_int("main_menu_flags",
					     menu->main_menu_flags);
			gnome_config_set_bool("global_main",
					      menu->global_main);
			gnome_config_set_bool("custom_icon",
					      menu->custom_icon);
			gnome_config_set_string("custom_icon_file",
						menu->custom_icon_file);
			}
			break;
		case APPLET_LAUNCHER: {
			Launcher *launcher = info->data;
			const char *location;

			gnome_config_set_string ("id", LAUNCHER_ID);

			/* clean old launcher info */
			gnome_config_clean_key ("parameters");

			launcher_save (launcher);
			location = gnome_desktop_item_get_location (launcher->ditem);
			gnome_config_set_string ("base_location", location);
			}
			break;
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
#endif /* FIXME */
		default:
			break;
		}
	}
}

static void
panel_session_save_panel (PanelData *pd)
{
	BasePWidget *basep = NULL; 
	PanelWidget *panel = NULL;
	GString	*buf;
	GSList *panel_id_list;
	GSList *temp;
	gboolean exists= FALSE;
	gchar *panel_profile;
	gchar *panel_id_key;
	
	buf = g_string_new (NULL);

	panel_profile = session_get_current_profile ();
#ifdef SESSION_DEBUG
	printf ("Saving to %s profile\n", panel_profile);
#endif
	panel_id_key = panel_gconf_general_profile_get_full_key (panel_profile, "panel-id-list");

	if (BASEP_IS_WIDGET (pd->panel)) {
		basep = BASEP_WIDGET (pd->panel);
		panel = PANEL_WIDGET (basep->panel);
	} else if (FOOBAR_IS_WIDGET (pd->panel)) {
		panel = PANEL_WIDGET (FOOBAR_WIDGET(pd->panel)->panel);
	}

#ifdef SESSION_DEBUG
	printf ("Saving unique panel id %s\n", panel->unique_id);
#endif
	/* Get the current list from gconf */	
	panel_id_list = gconf_client_get_list (panel_gconf_get_client (),
					   panel_id_key,
					   GCONF_VALUE_STRING,
					   NULL);

	
	/* We need to go through the panel-id-list and add stuff to the panel */
	
	for (temp = panel_id_list; temp; temp = temp->next) {
#ifdef SESSION_DEBUG
	printf ("Found ID: %s\n", (gchar *)temp->data);
#endif
		if (strcmp (panel->unique_id, (gchar *)temp->data) == 0)
			exists = TRUE;
	}	
	
	/* We need to add this key */
	if (exists == FALSE) {
#ifdef SESSION_DEBUG
	printf ("Adding a new panel to id-list: %s\n", panel->unique_id);
#endif
		panel_id_list = g_slist_append (panel_id_list, g_strdup (panel->unique_id));	
	}

	gconf_client_set_list (panel_gconf_get_client (),
			       panel_id_key,
			       GCONF_VALUE_STRING,
			       panel_id_list,
			       NULL);

	g_free (panel_id_key);
	g_slist_foreach (panel_id_list, (GFunc)g_free, NULL);
	g_slist_free (panel_id_list);

#ifdef SESSION_DEBUG
	printf ("Saving to panel profile : %s\n", panel_profile);
#endif

	panel_gconf_panel_profile_set_int (panel_profile,
					   panel->unique_id,
					   "panel-type", pd->type);

	if (basep != NULL) {
		panel_gconf_panel_profile_set_bool (panel_profile, panel->unique_id,
						    "hide-buttons-enabled", basep->hidebuttons_enabled);
		panel_gconf_panel_profile_set_bool (panel_profile, panel->unique_id,
						    "hide-button-pixmaps-enabled", basep->hidebutton_pixmaps_enabled);
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-hide-mode", basep->mode);
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-hide-state", basep->state);
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "screen-id", basep->screen);
	}

	panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
					   "panel-size", panel->sz);
	panel_gconf_panel_profile_set_bool (panel_profile, panel->unique_id,
					    "panel-backgroun-pixmap-fit", panel->fit_pixmap_bg);
	panel_gconf_panel_profile_set_bool (panel_profile, panel->unique_id,
					    "panel-background-pixmap-stretch", panel->stretch_pixmap_bg);
	panel_gconf_panel_profile_set_bool (panel_profile, panel->unique_id,
					    "panel-background-pixmap-rotate", panel->rotate_pixmap_bg);
	panel_gconf_panel_profile_set_string (panel_profile, panel->unique_id,
					      "panel-background-pixmap", sure_string (panel->back_pixmap));


	g_string_printf (buf, "#%02x%02x%02x",
			 (guint)panel->back_color.red/256,
			 (guint)panel->back_color.green/256,
			 (guint)panel->back_color.blue/256);
	
	panel_gconf_panel_profile_set_string (panel_profile, panel->unique_id,
					      "panel-background-color", buf->str);

	panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
					   "panel-background-type", panel->back_type);
	
	/* now do different types */
	if (BORDER_IS_WIDGET(pd->panel))
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "screen-edge", BORDER_POS (basep->pos)->edge);
	switch (pd->type) {
	case ALIGNED_PANEL:
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-align", ALIGNED_POS (basep->pos)->align);
		break;
	case SLIDING_PANEL:
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-offset", SLIDING_POS (basep->pos)->offset);
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-anchor", SLIDING_POS (basep->pos)->anchor);
		break;
	case FLOATING_PANEL:
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-orient", PANEL_WIDGET (basep->pos)->orient);
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-x-position", FLOATING_POS (basep->pos)->x);
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-y-position", FLOATING_POS (basep->pos)->y);
		break;
	case DRAWER_PANEL:
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-orient", DRAWER_POS (basep->pos)->orient);
		break;
	case FOOBAR_PANEL:
		panel_gconf_panel_profile_set_string (panel_profile, panel->unique_id,
						      "clock-format", FOOBAR_WIDGET (pd->panel)->clock_format);
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id, 
						   "screen_id", FOOBAR_WIDGET (pd->panel)->screen);
		break;
	default:
		break;
	}
#ifdef SESSION_DEBUG
	printf ("Done saving\n");
#endif
	g_string_free (buf, TRUE);
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
panel_session_do_save (GnomeClient *client,
		       gboolean     complete_save,
		       gboolean     save_applets,
		       gboolean     save_panels)
{
	GSList *l;

	if (commie_mode)
		return;

	if (complete_save)
		save_panels = save_applets = TRUE;

	if (save_panels)
		for (l = panel_list; l; l = l->next)
			panel_session_save_panel ((PanelData *) l->data);

	if (complete_save)
		panel_menu_session_save_tornoffs ();

	if (save_applets) {
		session_unlink_dead_launchers ();

		panel_session_save_applets (applets);
	}
	/* FIXME : add hack to see if we can flush changes */
	gconf_client_suggest_sync (panel_gconf_get_client (), NULL);
}

static guint sync_handler = 0;
static gboolean sync_handler_needed = FALSE;

void
panel_config_sync (void)
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
			need_complete_save = FALSE;
			applets_to_sync = FALSE;
			panels_to_sync = FALSE;
			panel_session_do_save (client, ncs, ats, pts); 
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

static gboolean
panel_session_do_sync (gpointer data)
{
	panel_config_sync ();

	return TRUE;
}

void
panel_sesssion_setup_config_sync (void)
{
	config_sync_timeout = gtk_timeout_add (10*1000, panel_session_do_sync, NULL);
}

/* This is called when the session manager requests a shutdown.  It
   can also be run directly when we don't detect a session manager.
   We assume no interaction is done by the applets.  And we ignore the
   other arguments for now.  Yes, this is lame.  */
/* update: some SM stuff implemented but we still ignore most of the
   arguments now*/
gboolean
panel_session_save (GnomeClient        *client,
		    int                 phase,
		    GnomeSaveStyle      save_style,
		    int                 is_shutdown,
		    GnomeInteractStyle  interact_style,
		    int                 is_fast,
		    gpointer            client_data)
{
	gchar **argv;
	gint argc;

	argv = g_malloc0(sizeof(gchar*)*4);

	argc = 3;
	argv[0] = client_data;
	argv[1] = "--profile";
      	argv[2] =  session_get_current_profile ();;

	gnome_client_set_restart_command (client, argc, argv);
        gnome_client_set_restart_style (client, GNOME_RESTART_IMMEDIATELY);
        gnome_client_set_priority (client, 40);

	panel_session_do_save (client, TRUE, FALSE, FALSE);

	return TRUE;
}

int
panel_session_die (GnomeClient *client,
		   gpointer client_data)
{
	GSList *l;

	gtk_timeout_remove (config_sync_timeout);
	config_sync_timeout = 0;
  
	status_inhibit = TRUE;
	status_spot_remove_all ();

	for (l = applets; l; l = l->next) {
		AppletInfo *info = l->data;

		switch (info->type) {
		case APPLET_BONOBO:
			panel_applet_frame_save_position (
				PANEL_APPLET_FRAME (info->data));
			break;
		case APPLET_SWALLOW: {
			Swallow   *swallow = info->data;
			GtkSocket *socket;

			swallow->clean_remove = TRUE;

			socket = GTK_SOCKET (swallow->socket);

			if (socket->plug_window)
                                XKillClient (GDK_DISPLAY (),
					     GDK_WINDOW_XWINDOW(socket->plug_window));

			}
			break;
		default:
			break;
		}
	}

	gnome_config_sync ();

	xstuff_unsetup_desktop_area ();
			
	panel_shell_unregister ();
	
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

#ifdef FIXME /* see session_load */
static void
session_init_applets (void)
{
	GString *buf;
	int count, num;	
	char *prefix;
	const char *sep;

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

		if (!strcmp (applet_name, BONOBO_ID)) {
			char *iid;

			iid = conditional_get_string ("iid", NULL, NULL);

			if (iid && iid [0] )
				panel_applet_frame_load (iid, panel, pos);

			g_free (iid);

		} else if (!strcmp (applet_name, LAUNCHER_ID)) { 
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
			char *path = conditional_get_string ("path",
							     "applications:/", NULL);
			gboolean main_menu =
				conditional_get_bool ("main_menu", TRUE, NULL);
			gboolean global_main =
				conditional_get_bool ("global_main", TRUE, NULL);
			gboolean custom_icon = 
				conditional_get_bool ("custom_icon", FALSE,
						      NULL);
			char *custom_icon_file =
				conditional_get_string ("custom_icon_file",
							NULL, NULL);
			int flags = conditional_get_int ("main_menu_flags",
							 get_default_menu_flags (),
							 NULL);
			if (flags < 0)
				flags = get_default_menu_flags ();
			
			load_menu_applet (path, main_menu,
					  flags, global_main,
					  custom_icon, custom_icon_file,
					  panel, pos, TRUE);

			g_free (custom_icon_file);
			g_free (path);

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
			/* FIXME - temporarily to get the id stuff right
				load_drawer_applet (mypanel_id, pixmap, tooltip,
						    panel, pos, TRUE);
 			*/ 
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
#endif /* FIXME */

static void
session_init_panels(void)
{
	gchar *panel_default_profile;

	gchar *panel_profile_key;
	gboolean use_default = FALSE;

	GtkWidget *panel = NULL;
	GSList *panel_ids;
	GSList *temp;
	gchar *timestring;

	
	panel_profile_key = g_strdup_printf ("/apps/panel/profiles/%s", session_get_current_profile ());

#ifdef SESSION_DEBUG
	printf ("Current profile is %s\n", session_get_current_profile ());
	printf ("We are checking for dir %s\n", panel_profile_key);
#endif

	if (panel_gconf_dir_exists (panel_profile_key) == FALSE) {
		/* FIXME: We need to resort to another fallback default panel config 
		          and do some sort of check for screen sizes */
		printf ("We are loading from a default setup - no current profile detected\n");
		use_default = TRUE;
		panel_default_profile = "medium";
		g_free (panel_profile_key);
		panel_profile_key = panel_gconf_general_default_profile_get_full_key (panel_default_profile, "panel-id-list");
	} else {
		g_free (panel_profile_key);
		panel_profile_key = panel_gconf_general_profile_get_full_key (session_get_current_profile (), "panel-id-list");
	}

	panel_ids = gconf_client_get_list (panel_gconf_get_client (),
					   panel_profile_key,
					   GCONF_VALUE_STRING,
					   NULL);
	g_free (panel_profile_key);
					  
	for (temp = panel_ids; temp; temp = temp->next) {
		PanelType type;
		PanelBackType back_type;
		gchar *panel_id;
		int sz;
		BasePState state;
		BasePMode mode;
		BorderEdge edge;
		char *back_pixmap, *color;
		GdkColor back_color = {0,0,0,1};
		gboolean fit_pixmap_bg;
		gboolean stretch_pixmap_bg;
		gboolean rotate_pixmap_bg;
		int hidebuttons_enabled;
		int hidebutton_pixmaps_enabled;
		int screen;

		panel_id = temp->data;

#ifdef SESSION_DEBUG
	printf ("Loading panel id %s\n", panel_id);
#endif
		back_pixmap = panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (), 
										panel_id,
										"panel-background-pixmap",
										use_default, NULL);
		if (string_empty (back_pixmap)) {
			g_free (back_pixmap);
			back_pixmap = NULL;
		}

		color = panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
								   	  panel_id,
									  "panel-background-color",
									  use_default, NULL);
		if ( ! string_empty (color))
			gdk_color_parse (color, &back_color);

		
		back_type = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
								  	   panel_id,
									   "panel-background-type",
									   use_default, PANEL_BACK_NONE);
		
		fit_pixmap_bg = panel_gconf_panel_profile_get_conditional_bool (session_get_current_profile (),
										panel_id,
										"panel-background-pixmap-fit",
										use_default, FALSE);

		stretch_pixmap_bg = panel_gconf_panel_profile_get_conditional_bool (session_get_current_profile (),
										    panel_id,
										    "panel-background-pixmap-stretch",
										    use_default, FALSE);

		rotate_pixmap_bg = panel_gconf_panel_profile_get_conditional_bool (session_get_current_profile (),
										  panel_id,
								       		  "panel-background-pixmap-rotate",
								       		  use_default, FALSE);
		
		sz = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
								    panel_id,
								    "panel-size",
								    use_default, PANEL_SIZE_SMALL);
		

		/* Now for type specific config */

		type = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
								      panel_id,
								      "panel-type",
								      use_default, EDGE_PANEL);
		
		hidebuttons_enabled = panel_gconf_panel_profile_get_conditional_bool (session_get_current_profile (),
										      panel_id,
										      "hide-buttons-enabled",
										      use_default, TRUE);
		
		hidebutton_pixmaps_enabled = panel_gconf_panel_profile_get_conditional_bool (session_get_current_profile (),
										   	     panel_id,
										   	     "hide-button-pixmaps-enabled",
										   	     use_default, TRUE);

		state = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
								       panel_id,
									"panel-hide-state",
									use_default, 0);

		mode = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
								      panel_id,
								      "panel-hide-mode",
								      use_default, 0);

		screen = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
								        panel_id,
									"screen-id",
									use_default, 0);

		switch (type) {
			
		case EDGE_PANEL:
			edge = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
									      panel_id,
									      "screen-edge",
									      use_default, BORDER_BOTTOM);
			
			panel = edge_widget_new (panel_id,
						 screen,
						 edge, 
						 mode, state,
						 sz,
						 hidebuttons_enabled,
						 hidebutton_pixmaps_enabled,
						 back_type, back_pixmap,
						 fit_pixmap_bg,
						 stretch_pixmap_bg,
						 rotate_pixmap_bg,
						 &back_color);
			break;
		case ALIGNED_PANEL: {
			AlignedAlignment align;
			
			edge = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
									      panel_id,
									      "screen-edge",
									      use_default, BORDER_BOTTOM);
			
			align = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
									       panel_id,
									       "panel-align",
									       use_default, ALIGNED_LEFT);

			panel = aligned_widget_new (panel_id,
						    screen,
						    align,
						    edge,
						    mode,
						    state,
						    sz,
						    hidebuttons_enabled,
						    hidebutton_pixmaps_enabled,
						    back_type,
						    back_pixmap,
						    fit_pixmap_bg,
						    stretch_pixmap_bg,
						    rotate_pixmap_bg,
						    &back_color);
			break;
		}
		case SLIDING_PANEL: {
			gint16 offset;
			SlidingAnchor anchor;
			
			edge = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
									      panel_id,
									      "screen-edge",
									      use_default, BORDER_BOTTOM);

			anchor = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
										panel_id,
									        "panel-anchor",
										use_default, SLIDING_ANCHOR_LEFT);
			
			offset = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
										panel_id,
										"panel-offset",
										use_default, 0);
	
			panel = sliding_widget_new (panel_id,
						    screen,
						    anchor,
						    offset,
						    edge,
						    mode,
						    state,
						    sz,
						    hidebuttons_enabled,
						    hidebutton_pixmaps_enabled,
						    back_type,
						    back_pixmap,
						    fit_pixmap_bg,
						    stretch_pixmap_bg,
						    rotate_pixmap_bg,
						    &back_color);
			break;
		}
		case DRAWER_PANEL: {
			PanelOrient orient;
			
			orient = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
										panel_id,
										"panel-orient",
										use_default, PANEL_ORIENT_UP);


			panel = drawer_widget_new (panel_id,
						   orient,
						   BASEP_EXPLICIT_HIDE, 
						   state,
						   sz,
						   hidebuttons_enabled,
						   hidebutton_pixmaps_enabled,
						   back_type,
						   back_pixmap,
						   fit_pixmap_bg,
						   stretch_pixmap_bg,
						   rotate_pixmap_bg,
						   &back_color);
			break;
		}
		case FLOATING_PANEL: {
			GtkOrientation orient;
			int x, y;
			
			orient = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
										panel_id,
										"panel-orient",
										use_default, GTK_ORIENTATION_HORIZONTAL);

			x = panel_gconf_panel_profile_get_conditional_int (session_get_current_profile (),
									   panel_id,
									   "panel-x-position",
									   use_default, 0);

			y = panel_gconf_panel_profile_get_conditional_int  (session_get_current_profile (),
									    panel_id,
									    "panel-y-position",
									    use_default, 0);
			
			panel = floating_widget_new (panel_id,
						     screen,
						     x,
						     y,
						     orient,
						     mode,
						     state,
						     sz,
						     hidebuttons_enabled,
						     hidebutton_pixmaps_enabled,
						     back_type,
						     back_pixmap,
						     fit_pixmap_bg,
						     stretch_pixmap_bg,
						     rotate_pixmap_bg,
						     &back_color);
			break;
		}
		case FOOBAR_PANEL:
			panel = foobar_widget_new (panel_id, screen);

			timestring = panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
										       panel_id,
										       "clock-format",
										       use_default, _("%I:%M:%S %p"));

			if (timestring != NULL)
				foobar_widget_set_clock_format (FOOBAR_WIDGET (panel), timestring);
			g_free (timestring);

			break;
		default:
			panel = NULL;
			g_warning ("Unkown panel type: %d; ignoring.", type);
			break;
		}

		g_free (color);
		g_free (back_pixmap);

		if (panel != NULL) {
			panel_setup (panel);
			gtk_widget_show (panel);
		}
	}

	/* Failsafe! */
	if (panel_ids == NULL) {
		panel_error_dialog ("no_panels_found",
				    _("No panels were found in your "
				      "configuration.  I will create "
				      "a menu panel for you"));

		panel = foobar_widget_new (NULL, 0);
		panel_setup (panel);
		gtk_widget_show (panel);
	}

	g_slist_foreach (panel_ids, (GFunc)g_free, NULL);
	g_slist_free (panel_ids);
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
		if (g_ascii_strcasecmp (username, p) == 0) {
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
session_read_global_config (void)
{
	GSList *li, *list;

	list = panel_gconf_all_global_entries ();

	for (li = list; li != NULL ; li = li->next) {
		GConfEntry *entry;
		GConfValue *value;
		gchar      *key;

		entry = (GConfEntry *)li->data;

		value = gconf_entry_get_value (entry);

		key = g_path_get_basename (gconf_entry_get_key (entry));
		/* FIXME: We need to do something more user friendly here */
		if (!strcmp (key, "panel-animation-speed")) {
			switch (gconf_value_get_int (value)) {
				case 0:
					global_config.animation_speed = 80;
					break;
				case 1:
					global_config.animation_speed = 60;
					break;
				case 2:
					global_config.animation_speed = 10;
					break;
				default:
					global_config.animation_speed = 60;
			}
		}

		else if (!strcmp (key, "panel-minimized-speed"))
			global_config.minimized_size =
				gconf_value_get_int (value);

		else if (!strcmp (key, "panel-minimized-size"))
			global_config.minimized_size =
				gconf_value_get_int (value);

		else if (!strcmp (key, "panel-hide-delay"))
			global_config.hide_delay =
				gconf_value_get_int (value);

		else if (!strcmp (key, "panel-show-delay"))
			global_config.show_delay =
				gconf_value_get_int (value);

		else if (!strcmp (key, "tooltips-enabled"))
			global_config.tooltips_enabled =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "keep-menus-in-memory"))
			global_config.keep_menus_in_memory =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "enable-animations"))
			global_config.enable_animations =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "autoraise-panel"))
			global_config.autoraise =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "panel-window-layer"))
			global_config.layer =
				gconf_value_get_int (value);

		else if (!strcmp (key, "drawer-autoclose"))
			global_config.drawer_auto_close =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "auto-update-menus"))
			g_warning ("\nGman: So guys, are we actually gonne do something with this in some sort of GNOME 2.0 timeframe?\n");

		else if (!strcmp (key, "highlight-launchers-on-mouseover"))
			global_config.highlight_when_over =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "confirm-panel-remove"))
			global_config.confirm_panel_remove =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "enable-key-bindings"))
			global_config.keys_enabled =
				gconf_value_get_bool (value);

		else if (!strcmp (key, "menu-key")) { 
			if (global_config.menu_key )
				g_free (global_config.menu_key );

			global_config.menu_key =
				g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.menu_key,
							&global_config.menu_keysym,
							&global_config.menu_state);
		} else if (!strcmp (key, "run-key")) {
			if (global_config.run_key )
				g_free (global_config.run_key );

			global_config.run_key =
				 g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.run_key,
							&global_config.run_keysym,
							&global_config.run_state);
		} else if (!strcmp (key, "screenshot-key")) {
			if (global_config.screenshot_key )
				g_free (global_config.screenshot_key );

			global_config.screenshot_key =
				g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.screenshot_key,
							&global_config.screenshot_keysym,
							&global_config.screenshot_state);

		} else if (!strcmp (key, "window-screenshot-key")) {
			if (global_config.window_screenshot_key )
				g_free (global_config.window_screenshot_key );

			global_config.window_screenshot_key =
				g_strdup (gconf_value_get_string (value));

			convert_string_to_keysym_state (global_config.window_screenshot_key,
							&global_config.window_screenshot_keysym,
							&global_config.window_screenshot_state);

		} else  {
			g_warning ("%s not handled", key);
		}

		g_free (key);
		gconf_entry_free (entry);
	}

	g_slist_free (list);

	/* FIXME STUFF THAT IS BORKED */
	global_config.menu_check = TRUE;
	global_config.menu_flags = get_default_menu_flags();

	apply_global_config ();
}

void
session_write_global_config (void)
{
	GConfChangeSet *global_config_cs;
	gchar *full_key;

	global_config_cs = gconf_change_set_new ();

	/* FIXME STUFF THAT IS BORKED 
	panel_gconf_global_config_set_int ("menu-flags", global_config.menu_flags);
	panel_gconf_global_config_set_bool ("menu-check", global_config.menu_check);
	*/

	/* FIXME: Make this more generic - this currently sucks */

	full_key = panel_gconf_global_config_get_full_key ("panel-animation-speed");
	gconf_change_set_set_int (global_config_cs, full_key , global_config.animation_speed);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-minimized-speed");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.minimized_size);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-hide-delay");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.hide_delay);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-show-delay");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.show_delay);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("panel-window-layer");
	gconf_change_set_set_int (global_config_cs, full_key, global_config.layer);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("tooltips-enabled");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.tooltips_enabled);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("enable-animations");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.enable_animations);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("autoraise-panel");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.autoraise);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("drawer-autoclose");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.drawer_auto_close);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("highlight-launchers-on-mouseover");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.highlight_when_over);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("confirm-panel-remove");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.confirm_panel_remove);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("keep-menus-in-memory");
	gconf_change_set_set_bool (global_config_cs, full_key, global_config.keep_menus_in_memory);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("menu-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.menu_key);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("run-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.run_key);
	g_free (full_key);

	full_key = panel_gconf_global_config_get_full_key ("screenshot-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.screenshot_key);
	g_free (full_key);
	
	full_key = panel_gconf_global_config_get_full_key ("window-screenshot-key");
	gconf_change_set_set_string (global_config_cs, full_key, global_config.window_screenshot_key);
	g_free (full_key);

	gconf_client_commit_change_set (panel_gconf_get_client (), global_config_cs, FALSE, NULL);

	gconf_change_set_unref (global_config_cs);
			     
}

void session_load (void) {
  session_init_panels ();

#ifdef FIXME /* load the applets */
  session_init_applets ();
#endif
}
