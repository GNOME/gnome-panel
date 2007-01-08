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

#include <glib/gi18n.h>
#include <libgnomeui/gnome-authentication-manager.h>
#include <libgnomeui/gnome-ui-init.h>

#include "panel-gconf.h"
#include "panel-profile.h"
#include "panel-config-global.h"
#include "panel-shell.h"
#include "panel-multiscreen.h"
#include "panel-session.h"
#include "panel-stock-icons.h"
#include "panel-action-protocol.h"
#include "panel-lockdown.h"
#include "xstuff.h"

#include "nothing.cP"

/* globals */
GSList *panels = NULL;
GSList *panel_list = NULL;

GtkTooltips *panel_tooltips = NULL;

static const GOptionEntry options[] = {
  { NULL }
};

int
main (int argc, char **argv)
{
	GOptionContext *context;
	GnomeProgram   *program;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new ("");
	g_option_context_set_ignore_unknown_options (context, TRUE);

	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	program = gnome_program_init ("gnome-panel", VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      NULL);

	gtk_window_set_default_icon_name ("gnome-panel");

	if (!panel_shell_register ())
		return -1;

	gnome_authentication_manager_init ();

	panel_tooltips = gtk_tooltips_new ();

	panel_action_protocol_init ();
	panel_multiscreen_init ();
	panel_init_stock_icons_and_items ();

	panel_session_init ();

	gconf_client_add_dir (panel_gconf_get_client (),
			      "/desktop/gnome/interface",
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);

	panel_global_config_load ();
	panel_lockdown_init ();
	panel_profile_load ();

	/*add forbidden lists to ALL panels*/
	g_slist_foreach (panels,
			 (GFunc)panel_widget_add_forbidden,
			 NULL);

	xstuff_init ();

	gtk_main ();

	panel_lockdown_finalize ();

	gconf_client_remove_dir (panel_gconf_get_client (),
				 "/desktop/gnome/interface",
				 NULL);

	g_object_unref (program);

	return 0;
}
