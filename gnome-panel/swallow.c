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
#include "panel-widget.h"
#include "panel.h"
#include "panel_config_global.h"
#include "swallow.h"
#include "mico-glue.h"

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

Swallow *
create_swallow_applet(char *arguments, SwallowOrient orient)
{
	Swallow *swallow;
	GtkWidget *w;

	swallow = g_new(Swallow,1);

	swallow->table = gtk_table_new(2,2,FALSE);
	gtk_widget_show(swallow->table);

	swallow->handle_n = gtk_vbox_new(FALSE,0);
	gtk_table_attach(GTK_TABLE(swallow->table),swallow->handle_n,
			 1,2,0,1,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	w = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_n),w,TRUE,TRUE,0);
	gtk_widget_show(w);
	w = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_n),w,TRUE,TRUE,0);
	gtk_widget_show(w);
	w = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_n),w,TRUE,TRUE,0);
	gtk_widget_show(w);

	swallow->handle_w = gtk_hbox_new(FALSE,0);
	gtk_table_attach(GTK_TABLE(swallow->table),swallow->handle_w,
			 0,1,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	w = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_w),w,TRUE,TRUE,0);
	gtk_widget_show(w);
	w = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_w),w,TRUE,TRUE,0);
	gtk_widget_show(w);
	w = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(w),GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(swallow->handle_w),w,TRUE,TRUE,0);
	gtk_widget_show(w);
	
	swallow->socket=gtk_socket_new();
	gtk_signal_connect_after(GTK_OBJECT(swallow->socket),"realize",
			         GTK_SIGNAL_FUNC(socket_realized), NULL);

	gtk_table_attach(GTK_TABLE(swallow->table),swallow->socket,
			 1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);
	
	gtk_widget_show(swallow->socket);

	gtk_object_set_user_data(GTK_OBJECT(swallow->socket),swallow);

	if(arguments)
		swallow->title=g_strdup(arguments);
	else
		swallow->title=NULL;
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
