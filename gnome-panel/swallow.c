/*
 * GNOME panel swallow module.
 * (C) 1997,1998,1999,2000 The Free Software Foundation
 * (C) 2000 Eazel, Inc.
 *
 * Author: George Lebl
 */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include <libgnomeui/libgnomeui.h>
#include <libgnome/libgnome.h>

#include "gnome-desktop-item.h"

#include "swallow.h"

#include "applet.h"
#include "panel.h"
#include "panel-util.h"
#include "panel-widget.h"
#include "xstuff.h"

/* from gtkhandlebox.c */
#define DRAG_HANDLE_SIZE 10

enum {
	RELOAD_BUTTON
};

GList *check_swallows = NULL;

static void socket_destroyed (GtkWidget *w, gpointer data);
static void socket_realized (GtkWidget *w, gpointer data);

static void
socket_realized (GtkWidget *w, gpointer data)
{
	Swallow *swallow = data;

	g_return_if_fail (swallow->title != NULL);
	
	gdk_error_trap_push ();

	check_swallows = g_list_prepend (check_swallows, swallow);

	xstuff_go_through_client_list ();

	gdk_flush();
	gdk_error_trap_pop ();
}

static void
swallow_launch (Swallow *swallow)
{
	if (swallow->path && swallow->path [0] != '\0') {
		char *p = strrchr (swallow->path, '.');
		GnomeDesktopItem *item;

		/*only if such a file exists and ends in a .desktop, should
		  we try to launch it as such*/
		if(p != NULL &&
		   (strcmp (p, ".desktop") == 0 ||
		    strcmp (p, ".kdelnk") == 0) &&
		   g_file_test (swallow->path, G_FILE_TEST_EXISTS) &&
		   (item = gnome_desktop_item_new_from_file (swallow->path, 0, NULL)) != NULL) {
			char *curdir = g_get_current_dir ();
			chdir (g_get_home_dir ());

			gnome_desktop_item_launch (item,
						   NULL /* file_list */,
						   0 /* flags */,
						   NULL /* error */);
			/* FIXME: handle_errors */
			gnome_desktop_item_unref (item);

			chdir (curdir);
			g_free (curdir);
		} else {
			gnome_execute_shell (g_get_home_dir (), swallow->path);
		}
	}
}

static gboolean
before_remove (Swallow *swallow)
{
	GtkWidget *dlg;

	if (swallow->clean_remove)
		return TRUE;

	dlg = gtk_message_dialog_new (NULL /* parent */,
				      0 /* flags */,
				      GTK_MESSAGE_QUESTION,
				      GTK_BUTTONS_NONE,
				      _("A swallowed application appears to "
				       "have died unexpectedly.\n"
				       "Attempt to reload it?"));

	gtk_dialog_add_button (GTK_DIALOG (dlg),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dlg),
			       _("Reload"),
			       RELOAD_BUTTON);
	gtk_dialog_set_default_response (GTK_DIALOG (dlg),
					 RELOAD_BUTTON);
	gtk_window_set_wmclass (GTK_WINDOW (dlg),
				"swallow_crashed", "Panel");

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == 0) {

		/* make the socket again */
		swallow->socket = gtk_socket_new ();

		if (swallow->width > 0 || swallow->height > 0)
			gtk_widget_set_size_request (swallow->socket,
						     swallow->width, swallow->height);

		g_signal_connect_after (G_OBJECT (swallow->socket),
					"realize",
					G_CALLBACK (socket_realized),
					swallow);
		g_signal_connect (G_OBJECT (swallow->socket), "destroy",
				  G_CALLBACK (socket_destroyed),
				  swallow);

		gtk_container_add (GTK_CONTAINER (swallow->frame),
				   swallow->socket);
		gtk_widget_show (swallow->socket);

		/* launch the command if some exists */
		swallow_launch (swallow);

		check_swallows = g_list_prepend (check_swallows, swallow);
		xstuff_go_through_client_list ();
		
		return FALSE;
	}

	return TRUE;
}

static void
unref_swallow (Swallow *swallow)
{
	swallow->ref_count --;

	if (swallow->ref_count == 0) {
		g_free (swallow->title);
		swallow->title = NULL;
		g_free (swallow->path);
		swallow->path = NULL;

		g_free (swallow);
	}
}

static Swallow *
ref_swallow (Swallow *swallow)
{
	swallow->ref_count ++;

	return swallow;
}

static void
socket_destroyed (GtkWidget *w, gpointer data)
{
	Swallow *swallow = data;

	swallow->wid = -1;
	
	if ( ! before_remove (swallow))
		return;

	gtk_widget_destroy (swallow->ebox);
	swallow->ebox = NULL;

	check_swallows = g_list_remove (check_swallows, swallow);

	/* here is where the swallow really dies */
	unref_swallow (swallow);
}

static void
really_add_swallow (GtkWidget *d, int response, gpointer data)
{
	GtkWidget *title_e = g_object_get_data (G_OBJECT (d), "title_e");
	GtkWidget *exec_e = g_object_get_data (G_OBJECT (d), "exec_e");
	GtkWidget *width_s = g_object_get_data (G_OBJECT (d), "width_s");
	GtkWidget *height_s = g_object_get_data (G_OBJECT (d), "height_s");
	PanelWidget *panel = g_object_get_data (G_OBJECT (d), "panel");
	int pos = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (d), "pos"));
	gboolean exactpos =
		GPOINTER_TO_INT (g_object_get_data (G_OBJECT (d), "exactpos"));

	switch (response) {
	case GTK_RESPONSE_CANCEL:
		gtk_widget_destroy (d);
		return;
	case GTK_RESPONSE_HELP:
		panel_show_help ("specialobjects", "SWALLOWEDAPPS");
		return;
	default:
		break;
	}
	
	load_swallow_applet(gtk_entry_get_text(GTK_ENTRY(exec_e)),
			    gtk_entry_get_text(GTK_ENTRY(title_e)),
			    gtk_spin_button_get_value_as_int(
						GTK_SPIN_BUTTON(width_s)),
			    gtk_spin_button_get_value_as_int(
						GTK_SPIN_BUTTON(height_s)),
			    panel, pos, exactpos);
	gtk_widget_destroy(d);
}

static void
act_really_add_swallow(GtkWidget *w, gpointer data)
{
	GtkWidget *d = data;
	
	/*just call the above handler for the dialog*/
	really_add_swallow (d, 0, NULL);
}

/*I couldn't resist the naming of this function*/
void
ask_about_swallowing(PanelWidget *panel, int pos, gboolean exactpos)
{
	GtkWidget *d;
	
	GtkWidget *title_e;
	GtkWidget *exec_e;
	GtkWidget *width_s;
	GtkWidget *height_s;
	GtkWidget *w;
	GtkWidget *box, *i_box;
	GtkAdjustment *adj;
	
	d = gtk_dialog_new_with_buttons (_("Create swallow applet"),
					 NULL /* parent */,
					 0 /* flags */,
					 GTK_STOCK_HELP,
					 GTK_RESPONSE_HELP,
					 GTK_STOCK_CANCEL,
					 GTK_RESPONSE_CANCEL,
					 GTK_STOCK_OK,
					 GTK_RESPONSE_OK,
					 NULL);
	gtk_window_set_wmclass (GTK_WINDOW (d),
				"create_swallow", "Panel");

	g_object_set_data (G_OBJECT (d), "panel", panel);
	g_object_set_data (G_OBJECT (d), "pos", GINT_TO_POINTER (pos));
	g_object_set_data (G_OBJECT (d), "exactpos", GINT_TO_POINTER (exactpos));

	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox),box, TRUE,TRUE,0);
	w = gtk_label_new(_("Title of application to swallow"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	w = gnome_entry_new("swallow_title");
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);
	title_e = gnome_entry_gtk_entry(GNOME_ENTRY(w));
	g_signal_connect (G_OBJECT(title_e),"activate",
			  G_CALLBACK(act_really_add_swallow),d);

	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox),box, TRUE,TRUE,0);
	w = gtk_label_new(_("Command (optional)"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	w = gnome_file_entry_new("execute",_("Browse"));
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);
	exec_e = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
	g_signal_connect (G_OBJECT(exec_e),"activate",
			  G_CALLBACK(act_really_add_swallow),d);

	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox),box, TRUE,TRUE,0);

	w = gtk_label_new(_("Width"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 
						    (float)gdk_screen_width(),
						    1.0, 5.0, 0.0);
	gtk_adjustment_set_value(adj, 48); 
	width_s = gtk_spin_button_new(adj,0,0);
	gtk_box_pack_start(GTK_BOX(box),width_s,FALSE,FALSE,0);


	i_box =gtk_hbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(box), i_box, FALSE, FALSE, GNOME_PAD_SMALL);
	
	w = gtk_label_new(_("Height"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 
						    (float)gdk_screen_height(),
						    1.0, 5.0, 0.0);
	gtk_adjustment_set_value(adj, 48); 
	height_s = gtk_spin_button_new(adj,0,0);
	gtk_box_pack_start(GTK_BOX(box),height_s,FALSE,FALSE,0);
	
	g_signal_connect (G_OBJECT (d), "response",
			  G_CALLBACK (really_add_swallow), NULL);
	g_object_set_data (G_OBJECT (d), "title_e", title_e);
	g_object_set_data (G_OBJECT (d), "exec_e", exec_e);
	g_object_set_data (G_OBJECT (d), "width_s", width_s);
	g_object_set_data (G_OBJECT (d), "height_s", height_s);


	gtk_dialog_set_default_response (GTK_DIALOG (d), GTK_RESPONSE_OK);

	gtk_widget_grab_focus(title_e);

	gtk_widget_show_all (d);
}

static int
ignore_1st_click(GtkWidget *widget, GdkEvent *event)
{
	GdkEventButton *buttonevent = (GdkEventButton *)event;

	if((event->type == GDK_BUTTON_PRESS &&
	    buttonevent->button == 1) ||
	   (event->type == GDK_BUTTON_RELEASE &&
	    buttonevent->button == 1)) {
		buttonevent->button = 2;
	}
	 
	return FALSE;
}

static Swallow *
create_swallow_applet(const char *title, const char *path, int width, int height, SwallowOrient orient)
{
	Swallow *swallow;

	if (width == 0)
		width = -1;
	if (height == 0)
		height = -1;
	
	swallow = g_new (Swallow, 1);
	swallow->ref_count = 1;

	swallow->clean_remove = TRUE;

	swallow->ebox = gtk_event_box_new ();
	gtk_widget_show (swallow->ebox);

	swallow->socket = gtk_socket_new ();
	gtk_widget_set_size_request (swallow->socket, width, height);
	g_signal_connect_after (G_OBJECT (swallow->socket), "realize",
				G_CALLBACK (socket_realized), swallow);
	g_signal_connect (G_OBJECT (swallow->socket), "destroy",
			  G_CALLBACK (socket_destroyed), swallow);
	
	
	swallow->handle_box = gtk_handle_box_new ();
	g_signal_connect (G_OBJECT (swallow->handle_box), "event",
			  G_CALLBACK (ignore_1st_click), NULL);
	
	gtk_container_add (GTK_CONTAINER(swallow->ebox),
			   swallow->handle_box);
	
	swallow->frame = gtk_frame_new (NULL);
	
	gtk_frame_set_shadow_type (GTK_FRAME(swallow->frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER(swallow->handle_box), swallow->frame);
	
	gtk_widget_show (swallow->handle_box);
	gtk_container_add (GTK_CONTAINER (swallow->frame),
			   swallow->socket);

	gtk_widget_show (swallow->socket);

	swallow->title = g_strdup (title);
	swallow->path = path ? g_strdup (path) : NULL;
	swallow->width = width;
	swallow->height = height;
	swallow->wid = -1;

	set_swallow_applet_orient (swallow, orient);

	return swallow;
}

void
set_swallow_applet_orient(Swallow *swallow, SwallowOrient orient)
{
	if (GTK_HANDLE_BOX(swallow->handle_box)->child_detached) {
		if (orient == SWALLOW_VERTICAL) {
			GTK_HANDLE_BOX(swallow->handle_box)->handle_position = GTK_POS_TOP;
			gtk_widget_set_size_request (swallow->handle_box, swallow->width,
						     DRAG_HANDLE_SIZE);
		} else {
			GTK_HANDLE_BOX(swallow->handle_box)->handle_position = GTK_POS_LEFT;
			gtk_widget_set_size_request (swallow->handle_box, DRAG_HANDLE_SIZE,
						     swallow->height);
		}
	} else {
		if (orient == SWALLOW_VERTICAL) {
			GTK_HANDLE_BOX(swallow->handle_box)->handle_position = GTK_POS_TOP;
			gtk_widget_set_size_request (swallow->handle_box, swallow->width,
						     swallow->height + DRAG_HANDLE_SIZE);
		} else {
			GTK_HANDLE_BOX(swallow->handle_box)->handle_position = GTK_POS_LEFT;
			gtk_widget_set_size_request (swallow->handle_box,
						     swallow->width + DRAG_HANDLE_SIZE,
						     swallow->height);
		}
	}
}

void
load_swallow_applet (const char *path, const char *params, int width, int height,
		     PanelWidget *panel, int pos, gboolean exactpos)
{
	Swallow    *swallow;
	AppletInfo *info;

	swallow = create_swallow_applet (params, path, width, height,
					 SWALLOW_HORIZONTAL);
	if (swallow == NULL)
		return;

	info = panel_applet_register (swallow->ebox, ref_swallow (swallow),
				      (GDestroyNotify) unref_swallow,
				      panel, pos, exactpos, APPLET_SWALLOW);
	if (!info)
		return;

	swallow->clean_remove = FALSE;

	panel_applet_add_callback (info, "help", GTK_STOCK_HELP, _("Help"));

	swallow_launch (swallow);
}
