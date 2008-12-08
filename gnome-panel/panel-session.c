/*
 * panel-session.c: panel session management routines
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include <libgnomeui/gnome-client.h>

#include <stdlib.h>

#include "panel-shell.h"

#include "panel-session.h"

static void
panel_session_handle_die_request (GnomeClient *client)
{
	g_return_if_fail (GNOME_IS_CLIENT (client));

	panel_shell_quit ();
}

void
panel_session_do_not_restart (void)
{
	GnomeClient *client;

	client = gnome_master_client ();

	gnome_client_set_restart_style (client, GNOME_RESTART_IF_RUNNING);
}

void
panel_session_init (void)
{
	GnomeClient *client;

	/* We don't want the WM to try and save/restore our
	 * window position
	 */
	gdk_set_sm_client_id (NULL);

	client = gnome_master_client ();

        if (!getenv ("GNOME_PANEL_DEBUG"))
                gnome_client_set_restart_style (client, GNOME_RESTART_IMMEDIATELY);

        gnome_client_set_priority (client, 40);

	g_signal_connect (client, "die",
			  G_CALLBACK (panel_session_handle_die_request), NULL);
}
