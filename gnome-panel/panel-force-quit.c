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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "panel-force-quit.h"

#include <libgnome/gnome-i18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "panel-stock-icons.h"

GdkFilterReturn popup_filter (GdkXEvent *gdk_xevent,
			      GdkEvent  *event,
			      GtkWidget *popup);

static Atom wm_state_atom = None;

static GtkWidget *
display_popup_window (GdkScreen *screen)
{
	GtkWidget *retval;
	GtkWidget *vbox;
	GtkWidget *image;
	GtkWidget *frame;
	GtkWidget *label;
	int        screen_width, screen_height;

	retval = gtk_window_new (GTK_WINDOW_POPUP);
	atk_object_set_role (gtk_widget_get_accessible (retval), ATK_ROLE_ALERT);
	gtk_window_set_screen (GTK_WINDOW (retval), screen);
	gtk_window_stick (GTK_WINDOW (retval));
	gtk_widget_add_events (retval, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
	gtk_container_add (GTK_CONTAINER (retval), frame);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_widget_show (vbox);

	image = gtk_image_new_from_stock (PANEL_STOCK_FORCE_QUIT, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), image, TRUE, TRUE, 4);
	gtk_widget_show (image);

	label = gtk_label_new (_("Click on a window to force the application to quit. "
				 "To cancel press <ESC>."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);
	gtk_widget_show (label);

	gtk_widget_realize (retval);

	screen_width  = gdk_screen_get_width  (screen);
	screen_height = gdk_screen_get_height (screen);

	gtk_window_move (GTK_WINDOW (retval),
			 (screen_width  - retval->allocation.width) / 2,
			 (screen_height - retval->allocation.height) / 2);

	gtk_widget_show (GTK_WIDGET (retval));

	return retval;
}

static void
remove_popup (GtkWidget *popup)
{
	GdkWindow *root;

	root = gdk_screen_get_root_window (
			gtk_window_get_screen (GTK_WINDOW (popup)));
	gdk_window_remove_filter (root, (GdkFilterFunc) popup_filter, popup);

	gtk_widget_destroy (popup);
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_keyboard_ungrab (GDK_CURRENT_TIME);
}

static gboolean
wm_state_set (Window window)
{
	gulong  nitems;
	gulong  bytes_after;
	gulong *prop;
	Atom    ret_type = None;
	int     ret_format;
	int     result;

	gdk_error_trap_push ();
	result = XGetWindowProperty (gdk_display, window, wm_state_atom,
				     0, G_MAXLONG, False, wm_state_atom,
				     &ret_type, &ret_format, &nitems,
				     &bytes_after, (gpointer) &prop);

	if (gdk_error_trap_pop ())
		return FALSE;

	if (result != Success)
		return FALSE;

	XFree (prop);

	if (ret_type != wm_state_atom)
		return FALSE;

	return TRUE;
}

static Window 
find_managed_window (Window window)
{
	Window  root;
	Window  parent;
	Window *kids = NULL;
	Window  retval;
	int     nkids, i;
	int     result;

	if (wm_state_set (window))
		return window;

	gdk_error_trap_push ();
	result = XQueryTree (gdk_display, window, &root, &parent, &kids, &nkids);
	if (gdk_error_trap_pop () || !result)
		return None;

	retval = None;

	for (i = 0; i < nkids; i++) {
		if (wm_state_set (kids [i])) {
			retval = kids [i];
			break;
		}

		retval = find_managed_window (kids [i]);
		if (retval != None)
			break;
	}

	if (kids)
		XFree (kids);

	return retval;
}

/* From metacity */
static gboolean
kill_window_question (void)
{
	GtkWidget *dialog;
	gboolean   retval;
 
	dialog = gtk_message_dialog_new (NULL, 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 _("Force this application to exit?\n"
					   "(Any open documents will be lost.)"));
 
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL,
				PANEL_STOCK_FORCE_QUIT,
				GTK_RESPONSE_ACCEPT,
				NULL);
 
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	retval = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT;

	gtk_widget_destroy (dialog);

	return retval;
}

static void 
handle_button_press_event (GtkWidget *popup,
			   XKeyEvent *event)
{
	Window window;

	remove_popup (popup);

	if (event->subwindow == None)
		return;

	if (wm_state_atom == None)
		wm_state_atom = XInternAtom (gdk_display, "WM_STATE", FALSE);

	window = find_managed_window (event->subwindow);

	if (window != None && kill_window_question ())
		XKillClient (gdk_display, window);
}

GdkFilterReturn
popup_filter (GdkXEvent *gdk_xevent,
	      GdkEvent  *event,
	      GtkWidget *popup)
{
	XEvent *xevent = (XEvent *) gdk_xevent;

	switch (xevent->type) {
	case ButtonPress:
		handle_button_press_event (popup, &xevent->xkey);
		return GDK_FILTER_REMOVE;
	case KeyPress:
		if (xevent->xkey.keycode == XKeysymToKeycode (gdk_display, XK_Escape)) {
			remove_popup (popup);
			return GDK_FILTER_REMOVE;
		}
		break;
	default:
		break;
	}

	return GDK_FILTER_CONTINUE;
}

void
panel_force_quit (GdkScreen *screen)
{
	GdkGrabStatus  status;
	GdkCursor     *cross;
	GtkWidget     *popup;
	GdkWindow     *root;

	popup = display_popup_window (screen);

	root = gdk_screen_get_root_window (screen);

	gdk_window_add_filter (root, (GdkFilterFunc) popup_filter, popup);

	cross = gdk_cursor_new (GDK_CROSS);
	status = gdk_pointer_grab (root, FALSE, GDK_BUTTON_PRESS_MASK,
				   NULL, cross, GDK_CURRENT_TIME);
	gdk_cursor_unref (cross);
	if (status != GDK_GRAB_SUCCESS) {
		g_warning ("Pointer grab failed\n");
		remove_popup (popup);
		return;
	}

	status = gdk_keyboard_grab (root, FALSE, GDK_CURRENT_TIME);
	if (status != GDK_GRAB_SUCCESS) {
		g_warning ("Keyboard grab failed\n");
		remove_popup (popup);
		return;
	}

	gdk_flush ();
}
