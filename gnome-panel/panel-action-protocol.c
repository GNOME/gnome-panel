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

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "applet.h"
#include "gp-module-manager.h"
#include "panel-toplevel.h"
#include "panel-util.h"

struct _GpActionProtocol
{
  GObject        parent;

  GpApplication *application;
};

G_DEFINE_TYPE (GpActionProtocol, gp_action_protocol, G_TYPE_OBJECT)

static Atom atom_gnome_panel_action            = None;
static Atom atom_gnome_panel_action_main_menu  = None;
static Atom atom_gnome_panel_action_run_dialog = None;

static void
handle_action (GpActionProtocol *self,
               GpActionFlags     action,
               guint32           time)
{
  GpModuleManager *module_manager;

  if (action == GP_ACTION_MAIN_MENU &&
      panel_applet_activate_main_menu (time))
    return;

  module_manager = gp_application_get_module_manager (self->application);

  gp_module_manager_handle_action (module_manager, action, time);
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
	GpActionFlags action;

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
	action = GP_ACTION_NONE;

	if (atom == atom_gnome_panel_action_main_menu)
		action = GP_ACTION_MAIN_MENU;
	else if (atom == atom_gnome_panel_action_run_dialog)
		action = GP_ACTION_RUN_DIALOG;

	if (action == GP_ACTION_NONE)
		return GDK_FILTER_CONTINUE;

	handle_action (GP_ACTION_PROTOCOL (data), action, xevent->xclient.data.l[1]);

	return GDK_FILTER_REMOVE;
}

static void
gp_action_protocol_finalize (GObject *object)
{
  GpActionProtocol *self;

  self = GP_ACTION_PROTOCOL (object);

  gdk_window_remove_filter (NULL, panel_action_protocol_filter, self);

  G_OBJECT_CLASS (gp_action_protocol_parent_class)->finalize (object);
}

static void
gp_action_protocol_class_init (GpActionProtocolClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->finalize = gp_action_protocol_finalize;
}

static void
gp_action_protocol_init (GpActionProtocol *self)
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
	gdk_window_add_filter (NULL, panel_action_protocol_filter, self);
}

GpActionProtocol *
gp_action_protocol_new (GpApplication *application)
{
  GpActionProtocol *action_protocol;

  action_protocol = g_object_new (GP_TYPE_ACTION_PROTOCOL, NULL);
  action_protocol->application = application;

  return action_protocol;
}
