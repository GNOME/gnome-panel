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

G_CONST_RETURN gchar *
session_get_current_profile (void)
{
	return panel_profile_name;
}

void
session_set_current_profile (const gchar *profile_name) {

	g_return_if_fail (profile_name != NULL);

	if (panel_profile_name != NULL);
		g_free (panel_profile_name);
	panel_profile_name = g_strdup (profile_name);
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

	argv = g_malloc0 (sizeof (gchar *) * 4);

	argc = 3;
	argv[0] = client_data;
	argv[1] = "--profile";
      	argv[2] = (char *) session_get_current_profile ();;

	gnome_client_set_restart_command (client, argc, argv);
        gnome_client_set_restart_style (client, GNOME_RESTART_IMMEDIATELY);
        gnome_client_set_priority (client, 40);

	g_free (argv);

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

#ifdef FIXME
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
#endif

void session_load (void) {
	/* FIXME : We still have to load up system preferences
	 * load_system_wide ();
	 */ 

	panel_session_init_global_config ();
	init_menus ();
	panel_session_init_panels ();

	panel_applet_load_applets_from_gconf ();
}
