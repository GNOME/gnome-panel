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
#include "panel-action-protocol.h"

#include "nothing.cP"

/* globals */
GSList *panels = NULL;
GSList *panel_list = NULL;

GtkTooltips *panel_tooltips = NULL;

GnomeIconTheme *panel_icon_theme = NULL;

static char *profile_arg;

static const struct poptOption options[] = {
  {"profile", '\0', POPT_ARG_STRING, &profile_arg, 0, N_("Specify a profile name to load"), NULL},
  {NULL, '\0', 0, NULL, 0}
};

int
main (int argc, char **argv)
{
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-panel", VERSION,
			    LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_POPT_TABLE, options,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    NULL);

	if (!panel_shell_register ())
		return -1;

	panel_icon_theme = gnome_icon_theme_new ();
	gnome_icon_theme_set_allow_svg (panel_icon_theme, TRUE);
	
	panel_register_window_icon ();

	panel_tooltips = gtk_tooltips_new ();

	panel_action_protocol_init ();
	panel_multiscreen_init ();
	panel_init_stock_icons_and_items ();

        init_menus ();

	panel_gconf_add_dir ("/desktop/gnome/interface");

	panel_global_config_load ();
	panel_profile_load (profile_arg);

	panel_session_init (argv [0]);


	/*add forbidden lists to ALL panels*/
	g_slist_foreach (panels,
			 (GFunc)panel_widget_add_forbidden,
			 NULL);

	gtk_main ();

	return 0;
}
