/*   gnome-panel-add-launcher: Thingie to add a launcher
 *
 *   Copyright 2000 Eazel, Inc.
 *   Authors: George Lebl <jirka@5z.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <libgnomeui/gnome-window-icon.h>
#include <gdk/gdkx.h>
#include "gnome-panel.h"

static int panel = 0;
static int pos = 0;
static gboolean url = FALSE;

static const struct poptOption options[] = {
	{ "panel", 0, POPT_ARG_INT, &panel, 0, N_("Panel to add the launcher to"), N_("NUMBER") },
	{ "pos", 0, POPT_ARG_INT, &pos, 0, N_("Position to add the launcher to"), N_("NUMBER") },
	{ "url", 0, POPT_ARG_NONE, &url, 0, N_("The argument is a url to add, not a .desktop file"), NULL },
	{ NULL } 
};

int
main (int argc, char **argv)
{
	CORBA_Environment ev;
	poptContext ctx;
	const char **args;
	const char *arg;
	GNOME_Panel2 panel_client = CORBA_OBJECT_NIL;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	CORBA_exception_init(&ev);

	if (gnome_CORBA_init_with_popt_table ("gnome-panel-add-launcher", VERSION,
					      &argc, argv,
					      options, 0, &ctx,
					      GNORBA_INIT_SERVER_FUNC, &ev) == NULL)
		return 1;

	args = poptGetArgs (ctx);

	if (args == NULL ||
	    args[0] == NULL ||
	    args[1] != NULL) {
		fprintf (stderr,
			 _("You must supply a single argument with the "
			   ".desktop file or url to use\n"));
		return 1;
	}

	arg = args[0];

	panel_client =
		goad_server_activate_with_repo_id (NULL,
						   "IDL:GNOME/Panel2:1.0",
						   GOAD_ACTIVATE_EXISTING_ONLY,
						   NULL);
	
	if (panel_client == NULL) {
		fprintf (stderr, _("No panel found\n"));
		return 1;
	}

	if (url)
		GNOME_Panel2_add_launcher_from_info_url (panel_client, arg, arg, arg, "", panel, pos, &ev);
	else
		GNOME_Panel2_add_launcher (panel_client, arg, panel, pos, &ev);

	CORBA_exception_free(&ev);

	return 0;
}
