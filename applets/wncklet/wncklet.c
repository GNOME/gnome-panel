/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

#include "config.h"

#include <gtk/gtk.h>
#include <libwnck/libwnck.h>
#include <panel-applet.h>
#include <string.h>

#include "wncklet.h"
#include "window-menu.h"
#include "workspace-switcher.h"
#include "window-list.h"
#include "showdesktop.h"

void
wncklet_connect_while_alive (gpointer    object,
			     const char *signal,
			     GCallback   func,
			     gpointer    func_data,
			     gpointer    alive_object)
{
	GClosure *closure;

	closure = g_cclosure_new (func, func_data, NULL);
	g_object_watch_closure (G_OBJECT (alive_object), closure);
	g_signal_connect_closure_by_id (
			object	,
			g_signal_lookup (signal, G_OBJECT_TYPE (object)), 0,
			closure,
			FALSE);
}

static gboolean 
wncklet_factory (PanelApplet *applet,
		 const char  *iid,
		 gpointer     data)
{
	gboolean retval = FALSE;
	static gboolean type_registered = FALSE;

	if (!type_registered) {
		wnck_set_client_type (WNCK_CLIENT_TYPE_PAGER);
		type_registered = TRUE;
	}

	if (!strcmp (iid, "WindowMenuApplet"))
		retval = window_menu_applet_fill (applet);

	else if (!strcmp (iid, "WorkspaceSwitcherApplet")||
	         !strcmp (iid, "PagerApplet"))
		retval = workspace_switcher_applet_fill (applet);

	else if (!strcmp (iid, "WindowListApplet") ||
	         !strcmp (iid, "TasklistApplet"))
		retval = window_list_applet_fill (applet);

	else if (!strcmp (iid, "ShowDesktopApplet"))
		retval = show_desktop_applet_fill (applet);

	return retval;
}

PANEL_APPLET_IN_PROCESS_FACTORY ("WnckletFactory",
				 PANEL_TYPE_APPLET,
				 wncklet_factory,
				 NULL)
