/* Gnome panel: Initialization routines
 * (C) 1997,1998,1999,2000 the Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 *          George Lebl
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include <libbonoboui.h>
#include <bonobo-activation/bonobo-activation.h>

#include "menu-fentry.h"
#include "menu.h"
#include "panel.h"
#include "panel-util.h"
#include "panel-gconf.h"
#include "panel-profile.h"
#include "panel-config-global.h"
#include "panel-shell.h"
#include "panel-multiscreen.h"
#include "panel-session.h"
#include "xstuff.h"
#include "panel-stock-icons.h"
#include "global-keys.h"

#include "nothing.cP"

/* globals */
GlobalConfig global_config;

GSList *panels = NULL;
GSList *applets = NULL;
GSList *panel_list = NULL;

GtkTooltips *panel_tooltips = NULL;

char *kde_menudir = NULL;
char *kde_icondir = NULL;
char *kde_mini_icondir = NULL;

GnomeIconTheme *panel_icon_theme = NULL;

/* FIXME: old lockdown globals */
gboolean commie_mode = FALSE;
gboolean no_run_box = FALSE;

static char *profile_arg;

/* Note: similar function is in gnome-desktop-item !!! */

static void
find_kde_directory (void)
{
	int i;
	const char *kdedir = g_getenv ("KDEDIR");
	char *try_prefixes[] = {
		"/usr",
		"/opt/kde",
		"/usr/local",
		"/kde",
		NULL
	};
	if (kdedir != NULL) {
		kde_menudir = g_build_filename (kdedir, "share", "applnk", NULL);
		kde_icondir = g_build_filename (kdedir, "share", "icons", NULL);
		kde_mini_icondir = g_build_filename (kdedir, "share", "icons", "mini", NULL);
		return;
	}

	/* if what configure gave us works use that */
	if (g_file_test (KDE_MENUDIR, G_FILE_TEST_IS_DIR)) {
		kde_menudir = g_strdup (KDE_MENUDIR);
		kde_icondir = g_strdup (KDE_ICONDIR);
		kde_mini_icondir = g_strdup (KDE_MINI_ICONDIR);
		return;
	}

	for (i = 0; try_prefixes[i] != NULL; i++) {
		char *try;
		try = g_build_filename (try_prefixes[i], "share", "applnk", NULL);
		if (g_file_test (try, G_FILE_TEST_IS_DIR)) {
			kde_menudir = try;
			kde_icondir = g_build_filename (try_prefixes[i], "share", "icons", NULL);
			kde_mini_icondir = g_build_filename (try_prefixes[i], "share", "icons", "mini", NULL);
			return;
		}
		g_free(try);
	}

	/* absolute fallback, these don't exist, but we're out of options
	   here */
	kde_menudir = g_strdup (KDE_MENUDIR);
	kde_icondir = g_strdup (KDE_ICONDIR);
	kde_mini_icondir = g_strdup (KDE_MINI_ICONDIR);
}

static const struct poptOption options[] = {
  {"profile", '\0', POPT_ARG_STRING, &profile_arg, 0, N_("Specify a profile name to load"), NULL},
  POPT_AUTOHELP
  {NULL, '\0', 0, NULL, 0}
};

int
main(int argc, char **argv)
{
	GnomeClient *sm_client;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-panel", VERSION,
			    LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_POPT_TABLE, options,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    NULL);

	bonobo_activate ();

	if (!panel_shell_register ())
		return -1;

	panel_icon_theme = gnome_icon_theme_new ();
	
	find_kde_directory();

	sm_client = gnome_master_client ();
	panel_session_set_restart_command (sm_client, argv [0]);
	g_signal_connect (sm_client, "die",
			  G_CALLBACK (panel_session_handle_die_request), NULL);

	panel_register_window_icon ();

	panel_tooltips = gtk_tooltips_new ();

	xstuff_init ((GdkFilterFunc) panel_global_keys_filter);
	panel_multiscreen_init ();
	panel_init_stock_icons_and_items ();

        init_menus ();

	panel_gconf_add_dir ("/apps/panel/global");
	panel_gconf_add_dir ("/desktop/gnome/interface");
	panel_gconf_notify_add ("/apps/panel/global", panel_global_config_notify, NULL);

	panel_load_global_config ();
	panel_profile_load (profile_arg);

	/*add forbidden lists to ALL panels*/
	g_slist_foreach (panels,
			 (GFunc)panel_widget_add_forbidden,
			 NULL);

	gtk_main ();

	return 0;
}
