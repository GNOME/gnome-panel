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
#include <gnome.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "panel-include.h"
#include "panel-widget.h"

#include "xstuff.h"

/* from gtkhandlebox.c */
#define DRAG_HANDLE_SIZE 10

GList *check_swallows = NULL;

extern GlobalConfig global_config;

extern GSList *applets_last;

#if 0
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

	gdk_error_trap_push ();
		
	XQueryTree(GDK_DISPLAY(),
		   win,
		   &root_return,
		   &parent_return,
		   &children,
		   &nchildren);

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
				break ;
			}
			XFree(tit);
		}
	}
	gdk_flush();
	gdk_error_trap_pop ();
	for(i=0;!ret && i<nchildren;i++)
		ret=get_window_id(children[i],title,wid);
	if(children)
		XFree(children);
	return ret;
}
#endif

static int
socket_realized(GtkWidget *w, gpointer data)
{
	Swallow *swallow = gtk_object_get_user_data(GTK_OBJECT(w));

	g_return_val_if_fail(swallow->title!=NULL,FALSE);
	
	gdk_error_trap_push ();

	if(!get_window_id(GDK_ROOT_WINDOW(), swallow->title,
			  &swallow->wid, TRUE)) {
		check_swallows = g_list_prepend(check_swallows,swallow);
		xstuff_reset_need_substructure();
	} else
		gtk_socket_steal(GTK_SOCKET(swallow->socket), swallow->wid);

	gdk_flush();
	gdk_error_trap_pop ();

	return FALSE;
}

static void
socket_destroyed(GtkWidget *w, gpointer data)
{
	Swallow *swallow = data;
	
	gtk_widget_destroy(swallow->ebox);
	swallow->ebox = NULL;

	check_swallows = g_list_remove(check_swallows,swallow);
}

static void
free_swallow (gpointer data)
{
	Swallow *swallow = data;
	
	g_free(swallow->title);
	swallow->title = NULL;
	g_free(swallow->path);
	swallow->path = NULL;

	g_free(swallow);
}


static void
really_add_swallow(GtkWidget *d,int button, gpointer data)
{
	GtkWidget *title_e = gtk_object_get_data(GTK_OBJECT(d),"title_e");
	GtkWidget *exec_e = gtk_object_get_data(GTK_OBJECT(d),"exec_e");
	GtkWidget *width_s = gtk_object_get_data(GTK_OBJECT(d),"width_s");
	GtkWidget *height_s = gtk_object_get_data(GTK_OBJECT(d),"height_s");
	PanelWidget *panel = gtk_object_get_data(GTK_OBJECT(d),"panel");
	int pos = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(d),"pos"));
	gboolean exactpos =
		GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(d),"exactpos"));

	switch (button) {
	case 1: /* cancel */
		gtk_widget_destroy(d);
		return;
	case 2: /* help */
		panel_pbox_help_cb (NULL, 0, "specialobjects.html#SWALLOWEDAPPS");
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
	really_add_swallow(d,0,NULL);
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
	
	d = gnome_dialog_new(_("Create swallow applet"),
			     GNOME_STOCK_BUTTON_OK,
			     GNOME_STOCK_BUTTON_CANCEL,
			     GNOME_STOCK_BUTTON_HELP,
			     NULL);
	gtk_window_set_wmclass(GTK_WINDOW(d),
			       "create_swallow","Panel");
	/*gtk_window_set_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER);*/
	gtk_window_set_policy(GTK_WINDOW(d), FALSE, FALSE, TRUE);

	gtk_object_set_data(GTK_OBJECT(d),"panel",panel);
	gtk_object_set_data(GTK_OBJECT(d),"pos",GINT_TO_POINTER(pos));
	gtk_object_set_data(GTK_OBJECT(d),"exactpos",GINT_TO_POINTER(exactpos));

	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),box, TRUE,TRUE,0);
	w = gtk_label_new(_("Title of application to swallow"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	w = gnome_entry_new("swallow_title");
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);
	title_e = gnome_entry_gtk_entry(GNOME_ENTRY(w));
	gtk_signal_connect(GTK_OBJECT(title_e),"activate",
			   GTK_SIGNAL_FUNC(act_really_add_swallow),d);

	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),box, TRUE,TRUE,0);
	w = gtk_label_new(_("Command (optional)"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	w = gnome_file_entry_new("execute",_("Browse"));
	gtk_box_pack_start(GTK_BOX(box),w,TRUE,TRUE,0);
	exec_e = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
	gtk_signal_connect(GTK_OBJECT(exec_e),"activate",
			   GTK_SIGNAL_FUNC(act_really_add_swallow),d);

	box = gtk_hbox_new(FALSE,GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox),box, TRUE,TRUE,0);

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
	
	gtk_signal_connect(GTK_OBJECT(d),"clicked",
			   GTK_SIGNAL_FUNC(really_add_swallow),NULL);
	gtk_object_set_data(GTK_OBJECT(d),"title_e",title_e);
	gtk_object_set_data(GTK_OBJECT(d),"exec_e",exec_e);
	gtk_object_set_data(GTK_OBJECT(d),"width_s",width_s);
	gtk_object_set_data(GTK_OBJECT(d),"height_s",height_s);


	gnome_dialog_close_hides(GNOME_DIALOG(d),FALSE);

	gnome_dialog_set_default(GNOME_DIALOG(d),0);

	gtk_widget_grab_focus(title_e);

	gtk_widget_show_all (d);
	panel_set_dialog_layer (d);
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
create_swallow_applet(char *title, char *path, int width, int height, SwallowOrient orient)
{
	Swallow *swallow;
	GtkWidget *w;
	
	swallow = g_new(Swallow,1);

	swallow->ebox = gtk_event_box_new();
	gtk_widget_show(swallow->ebox);

	swallow->socket=gtk_socket_new();
	if(width != 0 || height != 0)
		gtk_widget_set_usize(swallow->socket, width, height);
	gtk_signal_connect_after(GTK_OBJECT(swallow->socket),"realize",
			         GTK_SIGNAL_FUNC(socket_realized), NULL);
	gtk_signal_connect(GTK_OBJECT(swallow->socket),"destroy",
			   GTK_SIGNAL_FUNC(socket_destroyed), swallow);
	
	
	swallow->handle_box = gtk_handle_box_new();
	gtk_signal_connect(GTK_OBJECT(swallow->handle_box),"event",
			   GTK_SIGNAL_FUNC(ignore_1st_click),NULL);
	
	gtk_container_add ( GTK_CONTAINER(swallow->ebox),
			    swallow->handle_box );
	
	w = gtk_frame_new(NULL);
	
	gtk_frame_set_shadow_type( GTK_FRAME(w), GTK_SHADOW_IN);
	gtk_container_add ( GTK_CONTAINER(swallow->handle_box), w );
	
	gtk_widget_show ( swallow->handle_box );
	/*
	  FIXME: I want to add the title of the swallowed window.
	  E.g. a Pager is "sticky", but having a pager in a handlebox
	  is not -> clicking will change the desktop, but leave the
	  pager behind :-( Below is one of the non working versions...
	*/
	/*gdk_window_set_title(GTK_HANDLE_BOX(swallow->handle_box)->float_window,
	  g_strdup(title));*/
	gtk_container_add ( GTK_CONTAINER(w),
			    swallow->socket );

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
	if (GTK_HANDLE_BOX(swallow->handle_box)->child_detached) {
		if(orient==SWALLOW_VERTICAL) {
			GTK_HANDLE_BOX(swallow->handle_box)->handle_position = GTK_POS_TOP;
			gtk_widget_set_usize( swallow->handle_box, swallow->width,
					      DRAG_HANDLE_SIZE);
		} else {
			GTK_HANDLE_BOX(swallow->handle_box)->handle_position = GTK_POS_LEFT;
			gtk_widget_set_usize( swallow->handle_box, DRAG_HANDLE_SIZE,
					      swallow->height );
		}
	} else {
		if(orient==SWALLOW_VERTICAL) {
			GTK_HANDLE_BOX(swallow->handle_box)->handle_position = GTK_POS_TOP;
			gtk_widget_set_usize( swallow->handle_box, swallow->width,
					      swallow->height + DRAG_HANDLE_SIZE);
		} else {
			GTK_HANDLE_BOX(swallow->handle_box)->handle_position = GTK_POS_LEFT;
			gtk_widget_set_usize( swallow->handle_box,
					      swallow->width + DRAG_HANDLE_SIZE,
					      swallow->height );
		}
	}
	gtk_object_set_user_data(GTK_OBJECT(swallow->handle_box),
				 GINT_TO_POINTER(orient));
}

void
load_swallow_applet(char *path, char *params, int width, int height,
		    PanelWidget *panel, int pos, gboolean exactpos)
{
	Swallow *swallow;

	swallow = create_swallow_applet(params, path, width, height,
					SWALLOW_HORIZONTAL);
	if(!swallow)
		return;

	if(!register_toy(swallow->ebox,
			 swallow, free_swallow,
			 panel, pos,
			 exactpos, APPLET_SWALLOW))
		return;
	applet_add_callback(applets_last->data, "help",
			    GNOME_STOCK_PIXMAP_HELP,
			    _("Help"));
	if(path && *path) {
		char *p = strrchr(path,'.');
		GnomeDesktopEntry *item;
		/*only if such a file exists and ends in a .desktop, should
		  we try to launch it as such*/
		if(p &&
		   (strcmp(p,".desktop")==0 ||
		    strcmp(p,".kdelnk")==0) &&
		   g_file_exists(path) &&
		   (item = gnome_desktop_entry_load(path))) {
			gnome_desktop_entry_launch(item);
			gnome_desktop_entry_free(item);
		} else {
			gnome_execute_shell(NULL, path);
		}
	}
}
