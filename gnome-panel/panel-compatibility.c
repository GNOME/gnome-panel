/*
 * panel-compatibility.c:
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

#include "panel-compatibility.h"

#include "edge-widget.h"
#include "panel-menu-bar.h"
#include "panel-applet-frame.h"

GtkWidget *
foobar_widget_new (const char *panel_id,
		   int         screen,
		   int         monitor)
{
	PanelWidget *panel;
	GtkWidget   *retval;
	PanelColor   color = { { 0, 0, 0, 0 }, 0xffff };

	retval = edge_widget_new (panel_id,
				  screen,
				  monitor,
				  BORDER_TOP,
				  BASEP_EXPLICIT_HIDE,
				  BASEP_SHOWN,
				  PANEL_SIZE_X_SMALL,
				  FALSE,
				  FALSE,
				  PANEL_BACK_NONE,
				  NULL,
				  FALSE,
				  FALSE,
				  FALSE,
				  &color);

	panel = PANEL_WIDGET (BASEP_WIDGET (retval)->panel);

	panel_menu_bar_load (panel, 0, TRUE, NULL);

	panel_applet_frame_load ("OAFIID:GNOME_WindowMenuApplet",
				 panel,
				 multiscreen_width (screen, monitor) - 10,
				 FALSE, NULL);

	return retval;
}
