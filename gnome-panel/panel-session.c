/*
 * panel-session.c:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-session.h"

#include <stdlib.h>

#include "panel-profile.h"
#include "panel-shell.h"

void
panel_session_set_restart_command (GnomeClient *client,
				   const char  *argv0)
{
#define N_ARGS 4
        char *argv [N_ARGS];

	g_return_if_fail (GNOME_IS_CLIENT (client));

	argv [0] = (char *) argv0;
	argv [1] = "--profile";
	argv [2] = (char *) panel_profile_get_name ();
	argv [3] = NULL;

        gnome_client_set_restart_command (client, N_ARGS, argv);
        gnome_client_set_priority (client, 40);

        if (!getenv ("GNOME_PANEL_DEBUG"))
                gnome_client_set_restart_style (client, GNOME_RESTART_IMMEDIATELY);
#undef N_ARGS
}

void
panel_session_request_logout (GnomeClient *client)
{
	g_return_if_fail (GNOME_IS_CLIENT (client));

	/* Only request a Global save. We only want a Local
	 * save if the user selects 'Save current setup'
	 * from the dialog.
	 */
	gnome_client_request_save (client,
				   GNOME_SAVE_GLOBAL,
				   TRUE,
				   GNOME_INTERACT_ANY,
				   FALSE,
				   TRUE);
}

void
panel_session_handle_die_request (GnomeClient *client)
{
	GSList *toplevels_to_destroy, *l;

	g_return_if_fail (GNOME_IS_CLIENT (client));

        toplevels_to_destroy = g_slist_copy (panel_toplevel_list_toplevels ());
        for (l = toplevels_to_destroy; l; l = l->next)
		gtk_widget_destroy (l->data);
        g_slist_free (toplevels_to_destroy);

	panel_shell_unregister ();

	gtk_main_quit ();
}
