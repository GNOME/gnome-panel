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
#include "panel-config-global.h"
#include "session.h"
#include "sliding-widget.h"
#include "status.h"
#include "swallow.h"
#include "gnome-run.h"
#include "global-keys.h"
#include "xstuff.h"
#include "multiscreen-stuff.h"
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

/* FIXME : Need to do some good error checking on all of these variables */

static GConfEnumStringPair panel_type_type_enum_map [] = {
	{ EDGE_PANEL,      "edge-panel" },
	{ DRAWER_PANEL,    "drawer-panel" },
	{ ALIGNED_PANEL,   "aligned-panel" },
	{ SLIDING_PANEL,   "sliding-panel" },
	{ FLOATING_PANEL,  "floating-panel" },
	{ FOOBAR_PANEL,	   "menu-panel" },
};

static GConfEnumStringPair background_type_enum_map [] = {
	{ PANEL_BACK_NONE,   "no-background" },
	{ PANEL_BACK_COLOR,  "color-background" },
	{ PANEL_BACK_PIXMAP, "pixmap-background" },
};

static GConfEnumStringPair panel_size_type_enum_map [] = {
	{ PANEL_SIZE_XX_SMALL, "panel-size-xx-small" },
	{ PANEL_SIZE_X_SMALL,  "panel-size-x-small" },
	{ PANEL_SIZE_SMALL,    "panel-size-small" },
	{ PANEL_SIZE_MEDIUM,   "panel-size-medium" },
	{ PANEL_SIZE_LARGE,    "panel-size-large" },
	{ PANEL_SIZE_X_LARGE,  "panel-size-x-large" },
	{ PANEL_SIZE_XX_LARGE, "panel-size-xx-large" },
};

static GConfEnumStringPair panel_edge_type_enum_map [] = {
	{ BORDER_TOP,    "panel-edge-top" },
	{ BORDER_RIGHT,  "panel-edge-right" },
	{ BORDER_BOTTOM, "panel-edge-bottom" },
	{ BORDER_LEFT,   "panel-edge-left" },
};

static GConfEnumStringPair panel_alignment_type_enum_map [] = {
	{ ALIGNED_LEFT,   "panel-alignment-left" },
	{ ALIGNED_CENTER, "panel-alignment-center" },
	{ ALIGNED_RIGHT,  "panel-alignment-right" },
};

static GConfEnumStringPair panel_anchor_type_enum_map [] = {
	{ SLIDING_ANCHOR_LEFT,  "panel-anchor-left" },
	{ SLIDING_ANCHOR_RIGHT, "panel-anchor-right" },
};

static GConfEnumStringPair panel_orient_type_enum_map [] = {
	{ PANEL_ORIENT_UP, "panel-orient-up" },
	{ PANEL_ORIENT_DOWN, "panel-orient-down" },
	{ PANEL_ORIENT_LEFT, "panel-orient-left" },
	{ PANEL_ORIENT_RIGHT, "panel-orient-right" },
};

static GConfEnumStringPair panel_orientation_type_enum_map [] = {
	{ GTK_ORIENTATION_HORIZONTAL, "panel-orientation-horizontal" },
	{ GTK_ORIENTATION_VERTICAL, "panel-orientation-vertical" },
};

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
	
		g_return_if_fail (info && info->widget);

		panel_applet_save_position (info, info->gconf_key);
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

		gconf_client_set_list (panel_gconf_get_client (),
				       panel_id_key,
				       GCONF_VALUE_STRING,
				       panel_id_list,
				       NULL);
	}

	g_free (panel_id_key);
	g_slist_foreach (panel_id_list, (GFunc)g_free, NULL);
	g_slist_free (panel_id_list);

#ifdef SESSION_DEBUG
	printf ("Saving to panel profile : %s\n", panel_profile);
#endif

	panel_gconf_panel_profile_set_string (panel_profile,
					      panel->unique_id,
					      "panel-type", 
					      gconf_enum_to_string (panel_type_type_enum_map, pd->type));

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

	panel_gconf_panel_profile_set_string (panel_profile, panel->unique_id,
					   "panel-size", 
					   gconf_enum_to_string (panel_size_type_enum_map, panel->sz));
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

	panel_gconf_panel_profile_set_string (panel_profile, panel->unique_id,
					      "panel-background-type", 
					      gconf_enum_to_string (background_type_enum_map, panel->back_type));
	
	/* now do different types */
	if (BORDER_IS_WIDGET(pd->panel))
		panel_gconf_panel_profile_set_string (panel_profile, panel->unique_id,
						      "screen-edge", 
						      gconf_enum_to_string (panel_edge_type_enum_map, BORDER_POS (basep->pos)->edge));
	switch (pd->type) {
	case ALIGNED_PANEL:
		panel_gconf_panel_profile_set_string (panel_profile, panel->unique_id,
						      "panel-align", 
						      gconf_enum_to_string (panel_alignment_type_enum_map, ALIGNED_POS (basep->pos)->align));
		break;
	case SLIDING_PANEL:
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-offset", SLIDING_POS (basep->pos)->offset);
		panel_gconf_panel_profile_set_string (panel_profile, panel->unique_id,
						      "panel-anchor", 
						      gconf_enum_to_string (panel_anchor_type_enum_map, SLIDING_POS (basep->pos)->anchor));
		break;
	case FLOATING_PANEL:
		panel_gconf_panel_profile_set_string (panel_profile, panel->unique_id,
						      "panel-orient", 
						      gconf_enum_to_string (panel_orientation_type_enum_map, PANEL_WIDGET (basep->pos)->orient));

		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-x-position", FLOATING_POS (basep->pos)->x);
		panel_gconf_panel_profile_set_int (panel_profile, panel->unique_id,
						   "panel-y-position", FLOATING_POS (basep->pos)->y);
		break;
	case DRAWER_PANEL:
		panel_gconf_panel_profile_set_string (panel_profile, panel->unique_id,
						      "panel-orient", 
						      gconf_enum_to_string (panel_orient_type_enum_map, DRAWER_POS (basep->pos)->orient));
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
	g_free (panel_profile);
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
#ifdef FIXME
	/* We need to do tornoff's still */
	if (complete_save)
		panel_menu_session_save_tornoffs ();
#endif
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
panel_session_setup_config_sync (void)
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

		panel_applet_save_position (info, info->gconf_key);

		if (info->type == APPLET_SWALLOW) {
			Swallow   *swallow = info->data;
			GtkSocket *socket;

			swallow->clean_remove = TRUE;

			socket = GTK_SOCKET (swallow->socket);

			if (socket->plug_window)
                                XKillClient (GDK_DISPLAY (),
					     GDK_WINDOW_XWINDOW(socket->plug_window));

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

		gconf_string_to_enum (background_type_enum_map,
				      panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
											panel_id,
											"panel-background-type",
											use_default, "no-background"),
				      (gint *) &back_type);
		
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
		
		gconf_string_to_enum (panel_size_type_enum_map,
				      panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
										     	panel_id,
										     	"panel-size",
										     	use_default, "panel-size-small"),
				      (gint *) &sz);
		

		/* Now for type specific config */

		gconf_string_to_enum (panel_type_type_enum_map, 
	       			     panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
	       									       panel_id,
	       									       "panel-type",
	       									       use_default, "edge-panel"),
	       			     (gint *) &type);
		
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
			gconf_string_to_enum (panel_edge_type_enum_map,
					      panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
												panel_id,
												"screen-edge",
												use_default, "panel-edge-bottom"),
					      (gint *) &edge);
			
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
			
			gconf_string_to_enum (panel_edge_type_enum_map,
					      panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
											     	panel_id,
											     	"screen-edge",
											     	use_default, "panel-edge-bottom"),
					      (gint *) &edge);
			
			gconf_string_to_enum (panel_alignment_type_enum_map,
					      panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
											     	panel_id,
											     	"panel-align",
											     	use_default, "panel-alignment-left"),
					      (gint *) &align);

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
			
			gconf_string_to_enum (panel_edge_type_enum_map,
					      panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
												panel_id,
												"screen-edge",
												use_default, "panel-edge_bottom"),
					      (gint *) &edge);

			gconf_string_to_enum (panel_anchor_type_enum_map,
					      panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
											        panel_id,
											        "panel-anchor",
											        use_default, "panel-anchor-left"),
					      (gint *) &anchor);
			
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
			
			gconf_string_to_enum (panel_orient_type_enum_map,
					      panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
												panel_id,
												"panel-orient",
												use_default, "panel-orient-up"),
					      (gint *) &orient);


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
			
			gconf_string_to_enum (panel_orientation_type_enum_map,
					      panel_gconf_panel_profile_get_conditional_string (session_get_current_profile (),
												panel_id,
												"panel-orient ",
												use_default, "panel-orientation-horizontal"),
					      (gint *) &orient);

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

void
session_remove_panel_from_config (PanelWidget *panel) {
/* We need to take in a panel widget, get it's ID, then remove it from the current profile */
        gchar *panel_profile_key, *panel_profile_directory;
	GSList *panel_ids, *temp, *new_panel_ids = NULL;

#ifdef SESSION_DEBUG
	printf ("We are removing the configuration for panel id : %s\n", panel->unique_id);
#endif
        panel_profile_key = g_strdup_printf ("/apps/panel/profiles/%s", session_get_current_profile ());

        if (panel_gconf_dir_exists (panel_profile_key) == FALSE) {
#ifdef SESSION_DEBUG
	  printf ("We have no configuration to remove!\n");
#endif
                /* We haven't saved any profile, so don't remove anything */
                g_free (panel_profile_key);
                return;
        } else {
                g_free (panel_profile_key);
                panel_profile_key = panel_gconf_general_profile_get_full_key (session_get_current_profile (), "panel-id-list");
        }

        panel_ids = gconf_client_get_list (panel_gconf_get_client (),
                                           panel_profile_key,
                                           GCONF_VALUE_STRING,
                                           NULL);

        /* We now have to go through the GSList and remove panel->unique_id */

        for (temp = panel_ids; temp; temp = temp->next) {
                if (strcmp (panel->unique_id, (gchar *)temp->data) == 0) {
                        /* We now start removing configuration */
                        panel_profile_directory = g_strdup_printf ("/apps/panel/profiles/%s/panels/%s",
                                                                   session_get_current_profile (),
                                                                   PANEL_WIDGET (panel)->unique_id);
#ifdef SESSION_DEBUG
	printf ("We are removing the configuration for gconf key : %s\n", panel_profile_directory);
#endif
                        panel_gconf_directory_recursive_clean (panel_gconf_get_client (),
                                                               (const gchar *) panel_profile_directory);
                        g_free (panel_profile_directory);
                } else {
                        new_panel_ids = g_slist_prepend (new_panel_ids, (gchar *)temp->data);
                }
        }

        gconf_client_set_list (panel_gconf_get_client (),
                               panel_profile_key,
                               GCONF_VALUE_STRING,
                               new_panel_ids,
                               NULL);
	if (new_panel_ids != NULL) {	
		g_slist_foreach (new_panel_ids, (GFunc) g_free, NULL);
		g_slist_free (new_panel_ids);
	}
	gconf_client_suggest_sync (panel_gconf_get_client (), NULL);
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

void session_load (void) {
printf ("Loading up panel profile . . .\n");

	/* FIXME : We still have to load up system preferences
	 * load_system_wide ();
	 */ 

	panel_session_read_global_config ();
	init_menus ();
	session_init_panels ();

	/* FIXME: We still need to load up possible tearoffs */

	panel_applet_load_applets_from_gconf ();
}
