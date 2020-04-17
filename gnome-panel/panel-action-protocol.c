/*
 * panel-action-protocol.h: _GNOME_PANEL_ACTION protocol impl.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-action-protocol.h"
#include "panel-applets-manager.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "applet.h"
#include "panel-toplevel.h"
#include "panel-util.h"
#include "panel-run-dialog.h"

static Atom atom_gnome_panel_action            = None;
static Atom atom_gnome_panel_action_main_menu  = None;
static Atom atom_gnome_panel_action_run_dialog = None;

static void
menu_destroy_cb (GtkWidget   *widget,
                 PanelWidget *panel_widget)
{
	panel_toplevel_pop_autohide_disabler (panel_widget->toplevel);
}

static void
menu_loaded_cb (GtkWidget   *widget,
                PanelWidget *panel_widget)
{
	GdkWindow *window;
	GdkRectangle rect;
	GdkDisplay *display;
	GdkSeat *seat;
	GdkDevice *device;

	g_signal_connect (widget, "destroy", G_CALLBACK (menu_destroy_cb), panel_widget);
	panel_toplevel_push_autohide_disabler (panel_widget->toplevel);

	window = gtk_widget_get_window (GTK_WIDGET (panel_widget));

	rect.x = 0;
	rect.y = 0;
	rect.width = 1;
	rect.height = 1;

	display = gdk_display_get_default ();
	seat = gdk_display_get_default_seat (display);
	device = gdk_seat_get_pointer (seat);

	gdk_window_get_device_position (window, device,
	                                &rect.x, &rect.y,
	                                NULL);

	gtk_menu_popup_at_rect (GTK_MENU (widget), window, &rect,
	                        GDK_GRAVITY_SOUTH_EAST,
	                        GDK_GRAVITY_NORTH_WEST,
	                        NULL);
}

static void
panel_action_protocol_main_menu (GdkScreen *screen,
				 guint32    activate_time)
{
	GSList *panels;
	GtkWidget *menu;

	if (panel_applet_activate_main_menu (activate_time))
		return;

	panels = panel_widget_get_panels ();
	menu = panel_applets_manager_get_standalone_menu ();

	g_signal_connect (menu, "loaded", G_CALLBACK (menu_loaded_cb), panels->data);
}

static void
panel_action_protocol_run_dialog (GdkScreen *screen,
				  guint32    activate_time)
{
	panel_run_dialog_present (screen, activate_time);
}

static GdkFilterReturn
panel_action_protocol_filter (GdkXEvent *gdk_xevent,
			      GdkEvent  *event,
			      gpointer   data)
{
	GdkWindow *window;
	GdkScreen *screen;
	GdkDisplay *display;
	XEvent    *xevent = (XEvent *) gdk_xevent;
	Atom       atom;

	if (xevent->type != ClientMessage)
		return GDK_FILTER_CONTINUE;

	if (xevent->xclient.message_type != atom_gnome_panel_action)
		return GDK_FILTER_CONTINUE;

	screen = gdk_event_get_screen (event);
	display = gdk_screen_get_display (screen);
	window = gdk_x11_window_lookup_for_display (display, xevent->xclient.window);
	if (!window)
		return GDK_FILTER_CONTINUE;

	if (window != gdk_screen_get_root_window (screen))
		return GDK_FILTER_CONTINUE;

	atom = xevent->xclient.data.l[0];

	if (atom == atom_gnome_panel_action_main_menu)
		panel_action_protocol_main_menu (screen, xevent->xclient.data.l [1]);
	else if (atom == atom_gnome_panel_action_run_dialog)
		panel_action_protocol_run_dialog (screen, xevent->xclient.data.l [1]);
	else
		return GDK_FILTER_CONTINUE;

	return GDK_FILTER_REMOVE;
}

void
panel_action_protocol_init (void)
{
	GdkDisplay *display;

	display = gdk_display_get_default ();

	atom_gnome_panel_action =
		XInternAtom (GDK_DISPLAY_XDISPLAY (display),
			     "_GNOME_PANEL_ACTION",
			     FALSE);
	atom_gnome_panel_action_main_menu =
		XInternAtom (GDK_DISPLAY_XDISPLAY (display),
			     "_GNOME_PANEL_ACTION_MAIN_MENU",
			     FALSE);
	atom_gnome_panel_action_run_dialog =
		XInternAtom (GDK_DISPLAY_XDISPLAY (display),
			     "_GNOME_PANEL_ACTION_RUN_DIALOG",
			     FALSE);

	/* We'll filter event sent on non-root windows later */
	gdk_window_add_filter (NULL, panel_action_protocol_filter, NULL);
}
