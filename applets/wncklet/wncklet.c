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

#include <config.h>
#include <string.h>
#include <panel-applet.h>

#include "window-menu.h"
#include "workspace-switcher.h"
#include "window-list.h"
#include "showdesktop.h"

static gboolean 
wncklet_factory (PanelApplet *applet,
		 const char  *iid,
		 gpointer     data)
{
	gboolean retval = FALSE;
	
	if (!strcmp (iid, "OAFIID:GNOME_WindowMenuApplet"))
		retval = window_menu_applet_fill (applet);

	else if (!strcmp (iid, "OAFIID:GNOME_WorkspaceSwitcherApplet")||
	         !strcmp (iid, "OAFIID:GNOME_PagerApplet"))
		retval = workspace_switcher_applet_fill (applet);

	else if (!strcmp (iid, "OAFIID:GNOME_WindowListApplet") ||
	         !strcmp (iid, "OAFIID:GNOME_TasklistApplet"))
		retval = window_list_applet_fill (applet);

	else if (!strcmp (iid, "OAFIID:GNOME_ShowDesktopApplet"))
		retval = show_desktop_applet_fill (applet);

	return retval;
}

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_Wncklet_Factory",
                             PANEL_TYPE_APPLET,
                             "Window Navigation Applets",
                             "0",
                             wncklet_factory,
                             NULL);
