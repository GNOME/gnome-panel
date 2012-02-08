/* wncklet.c: A collection of window navigation applets
 *
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <panel-applet.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>

#include "wncklet.h"
#include "window-menu.h"
#include "workspace-switcher.h"
#include "window-list.h"
#include "showdesktop.h"

WnckScreen *
wncklet_get_screen (GtkWidget *applet)
{
	int screen_num;

	if (!gtk_widget_has_screen (applet))
		return wnck_screen_get_default ();

	screen_num = gdk_screen_get_number (gtk_widget_get_screen (applet));

	return wnck_screen_get (screen_num);
}

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


#ifdef WNCKLET_INPROCESS
PANEL_APPLET_IN_PROCESS_FACTORY ("WnckletFactory",
				 PANEL_TYPE_APPLET,
				 wncklet_factory,
				 NULL)
#else
PANEL_APPLET_OUT_PROCESS_FACTORY ("WnckletFactory",
				  PANEL_TYPE_APPLET,
				  wncklet_factory,
				  NULL)
#endif
