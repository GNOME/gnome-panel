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
#include "panel-widget.h"
#include "panel.h"
#include "panel_config_global.h"
#include "swallow.h"
#include "mico-glue.h"

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

	gtk_table_attach(GTK_TABLE(swallow->table),swallow->socket,
			 1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	gtk_widget_show(swallow->socket);

	/*FIXME: add the right window or wait for it or something here*/
	gtk_widget_set_usize(swallow->socket,48,48);
	/*{
		long wid;
		puts("window ID to get:");
		scanf("%lX",&wid);
		gtk_socket_steal(GTK_SOCKET(swallow->socket),wid);
	}*/

	gtk_object_set_user_data(GTK_OBJECT(swallow->socket),swallow);

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
