/* Gnome panel: panel functionality
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Federico Mena
 *           Miguel de Icaza
 */

/* FIXME:
 *
 *  A lot of this is redundant. As far as I can
 *  make out the only code here that still makes
 *  sense is the setting of the restart command,
 *  the unlinking of dead launchers and destroying
 *  all panels on destroy.
 */

#include <config.h>

#include <libgnome/libgnome.h>

#include "session.h"

#include "applet.h"
#include "menu.h"
#include "panel.h"
#include "panel-applet-frame.h"
#include "panel-config-global.h"
#include "panel-gconf.h"
#include "panel-shell.h"
#include "xstuff.h"

#undef SESSION_DEBUG

extern GSList          *panels;
extern GSList          *applets;
extern GnomeClient     *client;
extern GSList          *panel_list;
static int              config_sync_timeout;

gboolean                applets_to_sync = FALSE;
gboolean                panels_to_sync = FALSE;
gboolean                need_complete_save = FALSE;
gboolean                commie_mode = FALSE;
gboolean                no_run_box = FALSE;
GlobalConfig            global_config;

static void
panel_session_save_applets (GSList *applets_list)
{
	GSList *l;

	for (l = applets_list; l; l = l->next) {
		AppletInfo *info = l->data;
	
		g_return_if_fail (info && info->widget);

		panel_applet_save_position (info, info->gconf_key, TRUE);
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
			panel_save_to_gconf ((PanelData *) l->data);

	if (save_applets) {
		session_unlink_dead_launchers ();

		panel_session_save_applets (applets);
	}
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
		g_source_remove (sync_handler);
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
		sync_handler = g_timeout_add (30000, sync_handler_timeout, NULL);
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
	config_sync_timeout = g_timeout_add (10*1000, panel_session_do_sync, NULL);
}

void
panel_session_set_restart_command (GnomeClient *client,
				   char        *exec)
{
	char *argv [4];
	int   argc;

	argc = 3;

	argv [0] = exec;
	argv [1] = "--profile";
	argv [2] = (char *) panel_gconf_get_profile ();
	argv [3] = NULL;

	gnome_client_set_restart_command (client, argc, argv);
	gnome_client_set_priority (client, 40);

	if (!getenv ("GNOME_PANEL_DEBUG"))
		gnome_client_set_restart_style (
			client, GNOME_RESTART_IMMEDIATELY);
}

gboolean
panel_session_save (GnomeClient        *client,
		    int                 phase,
		    GnomeSaveStyle      save_style,
		    int                 is_shutdown,
		    GnomeInteractStyle  interact_style,
		    int                 is_fast,
		    gpointer            client_data)
{
	panel_session_set_restart_command (client, client_data);

	panel_session_do_save (client, TRUE, FALSE, FALSE);

	return TRUE;
}

int
panel_session_die (GnomeClient *client,
		   gpointer client_data)
{
	GSList *panels_to_destroy;
	GSList *l;

	g_source_remove (config_sync_timeout);
	config_sync_timeout = 0;
  
	for (l = applets; l; l = l->next) {
		AppletInfo *info = l->data;

		panel_applet_save_position (info, info->gconf_key, TRUE);
	}

	panels_to_destroy = g_slist_copy (panel_list);

	for (l = panels_to_destroy; l; l = l->next) {
		PanelData *pd = l->data;

		if (pd->panel)
			gtk_widget_destroy (pd->panel);
	}

	g_slist_free (panels_to_destroy);

	gnome_config_sync ();

	panel_shell_unregister ();
	
	gtk_main_quit ();

	return TRUE;
}

/* the logout function */
void
panel_quit (void)
{
	/* Only request a Global save. We only want a Local
	 * save if the user selects 'Save current setup'
	 * from the dialog.
	 */
	gnome_client_request_save (
		client, GNOME_SAVE_GLOBAL, 1, GNOME_INTERACT_ANY, 0, 1);
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

	panel_load_global_config ();
	init_menus ();
	panel_load_panels_from_gconf ();

	panel_applet_load_applets_from_gconf ();
}
