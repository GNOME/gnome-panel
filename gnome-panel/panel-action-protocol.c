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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-action-protocol.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <libgnome/gnome-i18n.h>

#include "gnome-run.h"
#include "menu.h"
#include "basep-widget.h"
#include "panel-util.h"
#include "egg-screen-exec.h"

static Atom atom_gnome_panel_action            = None;
static Atom atom_gnome_panel_action_main_menu  = None;
static Atom atom_gnome_panel_action_run_dialog = None;

extern GSList *panels;

struct PopupInIdleData {
	GtkMenu *popup_menu;
	guint32  activate_time;
};

static gboolean
popup_menu_in_idle (struct PopupInIdleData *popup_data)
{
	gtk_menu_popup (popup_data->popup_menu,
			NULL, NULL, NULL, NULL, 0,
			popup_data->activate_time);
	g_free (popup_data);

	return FALSE;
}

static void
panel_action_protocol_main_menu (GdkScreen *screen,
				 guint32    activate_time)
{
	PanelWidget            *panel_widget;
	GtkWidget              *panel;
	GtkWidget              *menu;
	struct PopupInIdleData *popup_data;

	panel_widget = panels->data;
	menu = create_panel_root_menu (panel_widget);
	panel = panel_widget->panel_parent;

	BASEP_WIDGET (panel)->autohide_inhibit = TRUE;
	basep_widget_autohide (BASEP_WIDGET (panel));

	gtk_menu_set_screen (GTK_MENU (menu), screen);

	popup_data = g_new0 (struct PopupInIdleData, 1);
	popup_data->popup_menu = GTK_MENU (menu);
	popup_data->activate_time = activate_time;

	/* FIXME: using a timeout to workaround the keyboard
	 *        grab failing with AlreadyGrabbed if we do
	 *        it straight away. Can't track down the other
	 *        grab.
	 */
	g_timeout_add (200, (GSourceFunc) popup_menu_in_idle, popup_data);
}

static void
panel_action_protocol_run_dialog (GdkScreen *screen)
{
	show_run_dialog (screen);
}

static GdkFilterReturn
panel_action_protocol_filter (GdkXEvent *gdk_xevent,
			      GdkEvent  *event,
			      gpointer   data)
{
	GdkWindow *window;
	GdkScreen *screen;
	XEvent    *xevent = (XEvent *) gdk_xevent;

	if (xevent->type != ClientMessage)
		return GDK_FILTER_CONTINUE;

	if (xevent->xclient.message_type != atom_gnome_panel_action)
		return GDK_FILTER_CONTINUE;

	window = gdk_window_lookup (xevent->xclient.window);
	if (!window)
		return GDK_FILTER_CONTINUE;

	screen = gdk_drawable_get_screen (window);

	if (xevent->xclient.data.l [0] == atom_gnome_panel_action_main_menu)
		panel_action_protocol_main_menu (screen, xevent->xclient.data.l [1]);
	else if (xevent->xclient.data.l [0] == atom_gnome_panel_action_run_dialog)
		panel_action_protocol_run_dialog (screen);
	else
		return GDK_FILTER_CONTINUE;

	return GDK_FILTER_REMOVE;
}

void
panel_action_protocol_init (void)
{
	GdkDisplay *display;
	GdkAtom     gdk_atom_gnome_panel_action;

	display = gdk_display_get_default ();

	gdk_atom_gnome_panel_action =
		gdk_atom_intern ("_GNOME_PANEL_ACTION", FALSE);

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

	gdk_display_add_client_message_filter (
		display, gdk_atom_gnome_panel_action,
		panel_action_protocol_filter, NULL);
}
