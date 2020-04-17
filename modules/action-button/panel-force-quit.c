/*
 * panel-force-quit.c:
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include "config.h"
#include "panel-force-quit.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <X11/extensions/XInput2.h>

static GdkFilterReturn popup_filter (GdkXEvent *gdk_xevent,
				     GdkEvent  *event,
				     GtkWidget *popup);

static Atom wm_state_atom = None;

static GtkWidget *
display_popup_window (GdkScreen *screen)
{
	GtkWidget     *retval;
	GtkWidget     *vbox;
	GtkWidget     *image;
	GtkWidget     *frame;
	GtkWidget     *label;

	retval = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_window_set_position (GTK_WINDOW (retval), GTK_WIN_POS_CENTER);
	atk_object_set_role (gtk_widget_get_accessible (retval), ATK_ROLE_ALERT);
	gtk_window_set_screen (GTK_WINDOW (retval), screen);
	gtk_window_stick (GTK_WINDOW (retval));
	gtk_widget_add_events (retval, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
	gtk_container_add (GTK_CONTAINER (retval), frame);
	gtk_widget_show (frame);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_widget_show (vbox);

	image = gtk_image_new_from_icon_name ("gnome-panel-force-quit",
					      GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (vbox), image, TRUE, TRUE, 4);
	gtk_widget_show (image);

	label = gtk_label_new (_("Click on a window to force the application to quit. "
				 "To cancel press <ESC>."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);
	gtk_widget_show (label);

	gtk_widget_show (GTK_WIDGET (retval));

	return retval;
}

static void
remove_popup (GtkWidget *popup)
{
	GdkWindow     *root;
	GdkDisplay    *display;
	GdkSeat       *seat;

	root = gdk_screen_get_root_window (
			gtk_window_get_screen (GTK_WINDOW (popup)));
	gdk_window_remove_filter (root, (GdkFilterFunc) popup_filter, popup);

	gtk_widget_destroy (popup);

	display = gdk_window_get_display (root);
	seat = gdk_display_get_default_seat (display);

	gdk_seat_ungrab (seat);
}

static gboolean
wm_state_set (GdkDisplay *display,
              Display    *xdisplay,
              Window      window)
{
	gulong  nitems;
	gulong  bytes_after;
	gulong *prop;
	Atom    ret_type = None;
	int     ret_format;
	int     result;

	gdk_x11_display_error_trap_push (display);
	result = XGetWindowProperty (xdisplay, window, wm_state_atom,
				     0, G_MAXLONG, False, wm_state_atom,
				     &ret_type, &ret_format, &nitems,
				     &bytes_after, (gpointer) &prop);

	if (gdk_x11_display_error_trap_pop (display))
		return FALSE;

	if (result != Success)
		return FALSE;

	XFree (prop);

	if (ret_type != wm_state_atom)
		return FALSE;

	return TRUE;
}

static Window 
find_managed_window (GdkDisplay *display,
                     Display    *xdisplay,
                     Window      window)
{
	Window  root;
	Window  parent;
	Window *kids = NULL;
	Window  retval;
	guint   nkids;
	guint   i;
	int     result;

	if (wm_state_set (display, xdisplay, window))
		return window;

	gdk_x11_display_error_trap_push (display);
	result = XQueryTree (xdisplay, window, &root, &parent, &kids, &nkids);
	if (gdk_x11_display_error_trap_pop (display) || !result)
		return None;

	retval = None;

	for (i = 0; i < nkids; i++) {
		if (wm_state_set (display, xdisplay, kids [i])) {
			retval = kids [i];
			break;
		}

		retval = find_managed_window (display, xdisplay, kids [i]);
		if (retval != None)
			break;
	}

	if (kids)
		XFree (kids);

	return retval;
}

static void
kill_window_response (GtkDialog *dialog,
		      gint       response_id,
		      gpointer   user_data)
{
	if (response_id == GTK_RESPONSE_ACCEPT) {
		GdkDisplay *display;
		Display *xdisplay;
		Window window = (Window) user_data;

		display = gtk_widget_get_display (GTK_WIDGET (dialog));
		xdisplay = gdk_x11_display_get_xdisplay (display);

		gdk_x11_display_error_trap_push (display);
		XKillClient (xdisplay, window);
		gdk_x11_display_error_trap_pop_ignored (display);

		gdk_display_flush (display);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

/* From metacity */
static void
kill_window_question (gpointer window)
{
	GtkWidget *dialog;
 
	dialog = gtk_message_dialog_new (NULL, 0,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 _("Force this application to exit?"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("If you choose to force an application "
						  "to exit, unsaved changes in any open documents "
						  "in it might get lost."));

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("_Cancel"),
				GTK_RESPONSE_CANCEL,
				_("_Force quit"),
				GTK_RESPONSE_ACCEPT,
				NULL);
 
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_CANCEL);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Force Quit"));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (kill_window_response), window);

	gtk_widget_show (dialog);
}

static void 
handle_button_press_event (GtkWidget *popup,
                           Display   *xdisplay,
                           Window     subwindow)
{
	GdkDisplay *display;
	Window window;

	display = gtk_widget_get_display (popup);

	remove_popup (popup);

	if (subwindow == None)
		return;

	if (wm_state_atom == None)
		wm_state_atom = XInternAtom (xdisplay, "WM_STATE", FALSE);

	window = find_managed_window (display, xdisplay, subwindow);

	if (window != None) {
		if (!gdk_x11_window_lookup_for_display (gdk_x11_lookup_xdisplay (xdisplay), window))
			kill_window_question ((gpointer) window);
	}
}

static GdkFilterReturn
popup_filter (GdkXEvent *gdk_xevent,
	      GdkEvent  *event,
	      GtkWidget *popup)
{
	XEvent *xevent = (XEvent *) gdk_xevent;
	XIEvent *xiev;
	XIDeviceEvent *xidev;

	switch (xevent->type) {
	case ButtonPress:
		handle_button_press_event (popup, xevent->xbutton.display, xevent->xbutton.subwindow);
		return GDK_FILTER_REMOVE;
	case KeyPress:
		if (xevent->xkey.keycode == XKeysymToKeycode (xevent->xany.display, XK_Escape)) {
			remove_popup (popup);
			return GDK_FILTER_REMOVE;
		}
		break;
	case GenericEvent:
		xiev = (XIEvent *) xevent->xcookie.data;
		xidev = (XIDeviceEvent *) xiev;
		switch (xiev->evtype) {
		case XI_KeyPress:
			if (xidev->detail == XKeysymToKeycode (xevent->xany.display, XK_Escape)) {
				remove_popup (popup);
				return GDK_FILTER_REMOVE;
			}
			break;
		case XI_ButtonPress:
			handle_button_press_event (popup, xidev->display, xidev->child);
			return GDK_FILTER_REMOVE;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return GDK_FILTER_CONTINUE;
}

static void
prepare_cb (GdkSeat   *seat,
            GdkWindow *window,
            gpointer   user_data)
{
	gdk_window_show_unraised (window);
}

void
panel_force_quit (GdkScreen *screen,
		  guint      time)
{
	GdkGrabStatus  status;
	GdkCursor     *cross;
	GtkWidget     *popup;
	GdkWindow     *root;
	GdkEventMask   event_mask;
	GdkDisplay    *display;
	GdkSeat       *seat;

	popup = display_popup_window (screen);

	root = gdk_screen_get_root_window (screen);

	event_mask = gdk_window_get_events (root);
	gdk_window_set_events (root, event_mask | GDK_KEY_PRESS_MASK);

	gdk_window_add_filter (root, (GdkFilterFunc) popup_filter, popup);

	cross = gdk_cursor_new_for_display (gdk_display_get_default (),
	                                    GDK_CROSS);

	display = gdk_window_get_display (root);
	seat = gdk_display_get_default_seat (display);

	status = gdk_seat_grab (seat,
	                        root,
	                        GDK_SEAT_CAPABILITY_POINTER |
	                        GDK_SEAT_CAPABILITY_KEYBOARD,
	                        TRUE,
	                        cross,
	                        NULL,
	                        prepare_cb,
	                        NULL);

	g_object_unref (cross);

	if (status != GDK_GRAB_SUCCESS) {
		g_warning ("Seat grab failed.");
		remove_popup (popup);
		return;
	}

	gdk_display_flush (display);
}
