/*
 * GNOME panel swallow module.
 * (C) 1997 The Free Software Foundation
 *
 * Author: George Lebl
 */

#include <config.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "panel-include.h"

extern PanelWidget *current_panel;

static int
ignore_x_error(Display* d, XErrorEvent* e)
{
	fprintf(stderr, "ignore_x_error called\n");
	return 0;
}

static int
get_window_id(Window win, char *title, guint32 *wid)
{
	Window root_return;
	Window parent_return;
	Window *children;
	unsigned int nchildren;
	unsigned int i;
	char *tit;
	int ret = FALSE;
	int (*oldErrorHandler)(Display*, XErrorEvent*);

		
	XQueryTree(GDK_DISPLAY(),
		   win,
		   &root_return,
		   &parent_return,
		   &children,
		   &nchildren);

	oldErrorHandler = XSetErrorHandler(ignore_x_error);
	for(i=0;i<nchildren;i++) {
		if (!XFetchName(GDK_DISPLAY(),
				children[i],
				&tit)) {
			continue;
		}
		if(tit) {
			if(strcmp(tit,title)==0) {
				XFree(tit);
				*wid = children[i];
				ret = TRUE;
				break;
			}
			XFree(tit);
		}
	}
	XSetErrorHandler(oldErrorHandler);
	for(i=0;!ret && i<nchildren;i++)
		ret=get_window_id(children[i],title,wid);
	if(children)
		XFree(children);
	return ret;
}

/*we should really do this differently but for now this is good enough*/
static int
socket_getwindow_timeout(Swallow *swallow)
{
	if(!get_window_id(GDK_ROOT_WINDOW(),swallow->title, &(swallow->wid)))
		return TRUE;
	gtk_socket_steal(GTK_SOCKET(swallow->socket),swallow->wid);
	return FALSE;
}

static int
socket_realized(GtkWidget *w, gpointer data)
{
	Swallow *swallow = gtk_object_get_user_data(GTK_OBJECT(w));

	g_return_val_if_fail(swallow->title!=NULL,FALSE);

	if(!get_window_id(GDK_ROOT_WINDOW(),swallow->title, &swallow->wid))
		gtk_timeout_add(500,(GtkFunction)socket_getwindow_timeout,
				swallow);
	else
		gtk_socket_steal(GTK_SOCKET(swallow->socket),swallow->wid);

	return FALSE;
}

/*FIXME: I dunno how I should handle the destruction of the applet itself,
  the problem is that the socket doesn't get destroyed nor does it get a
  delete_event when it's child dies ...*/
static int
socket_delete_event(GtkWidget *w, gpointer data)
{
	Swallow *swallow = data;
	gtk_widget_destroy(swallow->ebox);
	return TRUE;
}

static int
do_the_destroy(gpointer data)
{
	Swallow *swallow = data;
	
	gtk_widget_destroy(swallow->ebox);

	g_free(swallow->title);
	g_free(swallow->path);
	g_free(swallow);
	return FALSE;
}

static int
socket_destroyed(GtkWidget *w, gpointer data)
{
	gtk_idle_add(do_the_destroy,data);
	return TRUE;
}


static void
really_add_swallow(GtkWidget *d,int button, gpointer data)
{
	GtkWidget *title_e = gtk_object_get_data(GTK_OBJECT(d),"title_e");
	GtkWidget *exec_e = gtk_object_get_data(GTK_OBJECT(d),"exec_e");
	GtkWidget *width_s = gtk_object_get_data(GTK_OBJECT(d),"width_s");
	GtkWidget *height_s = gtk_object_get_data(GTK_OBJECT(d),"height_s");


	if(button!=0) {
		gtk_widget_destroy(d);
		return;
	}
	
	load_swallow_applet(gtk_entry_get_text(GTK_ENTRY(exec_e)),
			    gtk_entry_get_text(GTK_ENTRY(title_e)),
			    gtk_spin_button_get_value_as_int(
						GTK_SPIN_BUTTON(width_s)),
			    gtk_spin_button_get_value_as_int(
						GTK_SPIN_BUTTON(height_s)),
			    current_panel, 0);
	gtk_widget_destroy(d);
}

static void
act_really_add_swallow(GtkWidget *w, gpointer data)
{
	GtkWidget *d = data;
	
	/*just call the above handler for the dialog*/
	really_add_swallow(d,0,NULL);
}

/*I couldn't resist the naming of this function*/
void
ask_about_swallowing(void)
{
	GtkWidget *d;

	GtkWidget *title_e;
	GtkWidget *exec_e;
	GtkWidget *width_s;
	GtkWidget *height_s;
	GtkWidget *w;
	GtkWidget *box;
	GtkAdjustment *adj;
	d = gnome_dialog_new(_("Create swallow applet"),
			     GNOME_STOCK_BUTTON_OK,
			     GNOME_STOCK_BUTTON_CANCEL,
			     NULL);
	/*gtk_window_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(d), FALSE, FALSE, TRUE);

	box = gtk_hbox_new(FALSE,5);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),box,
			   TRUE,TRUE,5);
	w = gtk_label_new(_("Title of application to swallow"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	w = gnome_entry_new("swallow_title");
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);
	title_e = gnome_entry_gtk_entry(GNOME_ENTRY(w));
	gtk_signal_connect(GTK_OBJECT(title_e),"activate",
			   GTK_SIGNAL_FUNC(act_really_add_swallow),d);

	box = gtk_hbox_new(FALSE,5);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),box,
			   TRUE,TRUE,5);
	w = gtk_label_new(_("Command (optional)"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	w = gnome_file_entry_new("execute",_("Browse"));
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);
	exec_e = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
	gtk_signal_connect(GTK_OBJECT(exec_e),"activate",
			   GTK_SIGNAL_FUNC(act_really_add_swallow),d);

	box = gtk_hbox_new(FALSE,5);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),box,
			   TRUE,TRUE,5);
	w = gtk_label_new(_("Width (optional)"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 255.0, 1.0,
						    5.0, 0.0);
	width_s = gtk_spin_button_new(adj,0,0);
	gtk_box_pack_start(GTK_BOX(box),width_s,TRUE,TRUE,0);
	w = gtk_label_new(_("Height (optional)"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 255.0, 1.0,
						    5.0, 0.0);
	height_s = gtk_spin_button_new(adj,0,0);
	gtk_box_pack_start(GTK_BOX(box),height_s,TRUE,TRUE,0);

	gtk_signal_connect(GTK_OBJECT(d),"clicked",
			   GTK_SIGNAL_FUNC(really_add_swallow),NULL);
	gtk_object_set_data(GTK_OBJECT(d),"title_e",title_e);
	gtk_object_set_data(GTK_OBJECT(d),"exec_e",exec_e);
	gtk_object_set_data(GTK_OBJECT(d),"width_s",width_s);
	gtk_object_set_data(GTK_OBJECT(d),"height_s",height_s);


	gnome_dialog_close_hides(GNOME_DIALOG(d),FALSE);

	gnome_dialog_set_default(GNOME_DIALOG(d),0);

	gtk_widget_grab_focus(title_e);

	gtk_widget_show_all(d);
}


static Swallow *
create_swallow_applet(char *title, char *path, int width, int height, SwallowOrient orient)
{
	Swallow *swallow;
	GtkWidget *w;
	GtkWidget *table;

	swallow = g_new(Swallow,1);

	swallow->ebox = gtk_event_box_new();
	gtk_widget_show(swallow->ebox);

	table = gtk_table_new(2,2,FALSE);
	gtk_container_add(GTK_CONTAINER(swallow->ebox),table);
	gtk_widget_show(table);

	swallow->handle_n = gtk_vbox_new(FALSE,0);
	gtk_table_attach(GTK_TABLE(table),swallow->handle_n,
			 1,2,0,1,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	w = gtk_frame_new(NULL);
	gtk_widget_set_usize(w,48,0);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_n),w,TRUE,TRUE,0);
	gtk_widget_show(w);
	w = gtk_frame_new(NULL);
	gtk_widget_set_usize(w,48,0);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_n),w,TRUE,TRUE,0);
	gtk_widget_show(w);
	w = gtk_frame_new(NULL);
	gtk_widget_set_usize(w,48,0);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_n),w,TRUE,TRUE,0);
	gtk_widget_show(w);

	swallow->handle_w = gtk_hbox_new(FALSE,0);
	gtk_table_attach(GTK_TABLE(table),swallow->handle_w,
			 0,1,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	w = gtk_frame_new(NULL);
	gtk_widget_set_usize(w,0,48);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_w),w,TRUE,TRUE,0);
	gtk_widget_show(w);
	w = gtk_frame_new(NULL);
	gtk_widget_set_usize(w,0,48);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_w),w,TRUE,TRUE,0);
	gtk_widget_show(w);
	w = gtk_frame_new(NULL);
	gtk_widget_set_usize(w,0,48);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_w),w,TRUE,TRUE,0);
	gtk_widget_show(w);
	
	swallow->socket=gtk_socket_new();
	if(width != 0 || height != 0)
		gtk_widget_set_usize(swallow->socket,width,height);
	gtk_signal_connect_after(GTK_OBJECT(swallow->socket),"realize",
			         GTK_SIGNAL_FUNC(socket_realized), NULL);
	gtk_signal_connect_after(GTK_OBJECT(swallow->socket),"destroy",
			         GTK_SIGNAL_FUNC(socket_destroyed), swallow);
	/*gtk_signal_connect_after(GTK_OBJECT(swallow->socket),"delete_event",
			         GTK_SIGNAL_FUNC(socket_delete_event), swallow);*/

	gtk_table_attach(GTK_TABLE(table),swallow->socket,
			 1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);
	
	gtk_widget_show(swallow->socket);

	gtk_object_set_user_data(GTK_OBJECT(swallow->socket),swallow);

	swallow->title = g_strdup(title);
	swallow->path = path?g_strdup(path):NULL;
	swallow->width = width;
	swallow->height = height;
	swallow->wid = -1;

	set_swallow_applet_orient(swallow, orient);

	return swallow;
}

void
set_swallow_applet_orient(Swallow *swallow, SwallowOrient orient)
{
	if(orient==SWALLOW_VERTICAL) {
		gtk_widget_show(swallow->handle_n);
		gtk_widget_hide(swallow->handle_w);
	} else {
		gtk_widget_hide(swallow->handle_n);
		gtk_widget_show(swallow->handle_w);
	}
}

void
load_swallow_applet(char *path, char *params, int width, int height,
		    PanelWidget *panel, int pos)
{
	Swallow *swallow;

	swallow = create_swallow_applet(params, path, width, height,
					SWALLOW_HORIZONTAL);
	if(!swallow)
		return;

	register_toy(swallow->ebox,swallow, panel, pos, APPLET_SWALLOW);

	if(path && *path) {
		char *p = strrchr(path,'.');
		/*only if such a file exists and ends in a .desktop, should
		  we try to launch it as such*/
		if(p && strcmp(p,".desktop")==0 && g_file_exists(path)) {
			GnomeDesktopEntry *item;
			item = gnome_desktop_entry_load(path);
			gnome_desktop_entry_launch(item);
			gnome_desktop_entry_free(item);
		} else {
			char *s = g_copy_strings("(true; ",path," &)",NULL);
			system(s);
			g_free(s);
		}
	}
}
