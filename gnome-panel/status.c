/* Gnome panel: status applet
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  George Lebl
 */

#include <config.h>
#include <string.h>
#include <signal.h>
#include <gnome.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "panel-include.h"
#include "gnome-panel.h"

#include "xstuff.h"

static StatusApplet *the_status = NULL; /*"there can only be one" status
					  applet*/
static GtkWidget *offscreen = NULL; /*offscreen window for putting status
				      spots if there is no status applet*/
static GtkWidget *fixed = NULL; /*the fixed container in which the docklets reside*/
static GSList *spots = NULL;
static int nspots = 0;

int status_inhibit = FALSE; /*inhibit adding and updating for the purpose
			      of quitting*/

extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

extern PortableServer_POA thepoa;

#define DOCKLET_SPOT 22

/*this will show debug output and put the offscreen window on 10 10 to
  view it*/
/*#define DEBUG_STATUS 1*/

#ifdef DEBUG_STATUS
#define DPUTS(x) puts(x)
#define DPRINTD(d) printf("%s: %d\n",#d,d)
#else
#define DPUTS(x)
#define DPRINTD(d)
#endif

gboolean
got_spot_with_winid(guint32 winid)
{
	GSList *li;
	for(li = spots; li; li = li->next) {
		StatusSpot *ss = li->data;
		GtkSocket *s = GTK_SOCKET(ss->socket);
		if(s->plug_window &&
		   GDK_WINDOW_XWINDOW(s->plug_window) == winid)
			return TRUE;
	}
	return FALSE;
}


void
status_applet_update(StatusApplet *s)
{
	GSList *li;
	int w,h;
	int sz;
	int rows;
	int i,j;
	
	if(status_inhibit) return;
	
	DPUTS("STATUS_APPLET_UPDATE");
	DPRINTD(nspots);

	if(s->orient == PANEL_HORIZONTAL)
		GTK_HANDLE_BOX(s->handle)->handle_position = GTK_POS_LEFT;
	else
		GTK_HANDLE_BOX(s->handle)->handle_position = GTK_POS_TOP;
	
	switch(s->size) {
	case SIZE_TINY: sz = 24; break;
	case SIZE_SMALL: sz = 36; break;
	case SIZE_STANDARD: sz = 48; break;
	case SIZE_LARGE: sz = 64; break;
	case SIZE_HUGE: sz = 80; break;
	default: sz = 48; break;
	}
	
	rows = sz/DOCKLET_SPOT;

	if(nspots%rows == 0)
		w = DOCKLET_SPOT*(nspots/rows);
	else
		w = DOCKLET_SPOT*((nspots/rows)+1);
	
	/*make minimum size*/
	if(w<10) w = 10;	

	h = DOCKLET_SPOT*rows;

	/*if we are vertical just switch stuff around*/
	if(s->orient == PANEL_VERTICAL) {
		int t = w;
		w = h;
		h = t;
	}
	
	gtk_widget_set_usize(fixed,w,h);

	DPRINTD(w);
	DPRINTD(h);
	
	i = j = 0;
	for(li = spots; li; li = li->next) {
		StatusSpot *ss = li->data;
		gtk_fixed_move(GTK_FIXED(fixed),ss->socket,i,j);
		i+=DOCKLET_SPOT;
		if(i>=w) {
			i = 0;
			j+=DOCKLET_SPOT;
		}
	}
	if(s->handle && s->handle->parent)
		gtk_widget_queue_resize(s->handle->parent);
}

static void
status_socket_destroyed(GtkWidget *w, StatusSpot *ss)
{
	/*so that we don't get called recursively, we set the ->socket to
	  null inside status_spot_remove*/
	if(ss->socket)
		status_spot_remove(ss, FALSE);
}

StatusSpot *
new_status_spot(void)
{
	StatusSpot *ss;
	
	if(status_inhibit)
		return NULL;
	
	DPUTS("NEW_STATUS_SPOT");

	ss = g_new0(StatusSpot,1);
	ss->wid = 0;
	ss->sspot = CORBA_OBJECT_NIL;

	spots = g_slist_prepend(spots,ss);
	nspots++;

	ss->socket = gtk_socket_new();
	gtk_widget_set_usize(ss->socket,DOCKLET_SPOT,DOCKLET_SPOT);
#if 0	
	if(!the_status && !offscreen) {
		offscreen = gtk_window_new(GTK_WINDOW_POPUP);
#ifdef DEBUG_STATUS
		gtk_widget_set_uposition(offscreen,10,10);
#else
		gtk_widget_set_uposition(offscreen,gdk_screen_width()+10,
					 gdk_screen_height()+10);
#endif

		/*it should be null at this point*/
		g_assert(!fixed);
		
		fixed = gtk_fixed_new();
		gtk_widget_show(fixed);

		gtk_container_add(GTK_CONTAINER(offscreen),fixed);

		gtk_fixed_put(GTK_FIXED(fixed),ss->socket,0,0);
		gtk_widget_show_now(offscreen);
	} else {
#endif
		g_assert(fixed);
		gtk_fixed_put(GTK_FIXED(fixed),ss->socket,0,0);
		if(the_status)
			status_applet_update(the_status);
#if 0
	}
#endif
	gtk_widget_show_now(ss->socket);
	gtk_signal_connect(GTK_OBJECT(ss->socket),"destroy",
			   GTK_SIGNAL_FUNC(status_socket_destroyed),
			   ss);
	
	ss->wid = GDK_WINDOW_XWINDOW(ss->socket->window);
	return ss;
}

void
status_spot_remove(StatusSpot *ss, gboolean destroy_socket)
{
	CORBA_Environment ev;
	PortableServer_ObjectId *id;
	GtkWidget *w;

	spots = g_slist_remove(spots,ss);
	nspots--;
	
	/*set socket to NULL, as to indicate that we have taken
	  care of destruction here*/
	if(destroy_socket) {
		w = ss->socket;
		ss->socket = NULL;
		gtk_widget_destroy(w);
	}

	DPUTS("STATUS_SPOT_REMOVE");
	DPRINTD(nspots);
	DPRINTD(g_slist_length(spots));

	if(ss->sspot != CORBA_OBJECT_NIL) {
		CORBA_exception_init(&ev);
		CORBA_Object_release(ss->sspot, &ev);
		id = PortableServer_POA_servant_to_id(thepoa, ss, &ev);
		PortableServer_POA_deactivate_object(thepoa, id, &ev);
		CORBA_free (id);
		POA_GNOME_StatusSpot__fini((PortableServer_Servant) ss, &ev);
		CORBA_exception_free(&ev);
	}

	g_free(ss);
	if(the_status) status_applet_update(the_status);
}

/*kill all status spots*/
void
status_spot_remove_all(void)
{
	DPUTS("STATUS_SPOT_REMOVE_ALL");

	while(spots)
		status_spot_remove(spots->data, TRUE);
	
	DPUTS("DONE REMOVE_ALL");
}

void
status_applet_put_offscreen(StatusApplet *s)
{
	DPUTS("PUT_OFFSCREEN");
	g_assert(offscreen);
#if 0
	if(!offscreen) {
		DPUTS("CREATE OFFSCREEN");
		offscreen = gtk_window_new(GTK_WINDOW_POPUP);
#ifdef DEBUG_STATUS
		gtk_widget_set_uposition(offscreen,10,10);
#else
		gtk_widget_set_uposition(offscreen,gdk_screen_width()+10,
					 gdk_screen_height()+10);
#endif
		gtk_widget_show_now(offscreen);
	}
#endif
	g_assert(GTK_WIDGET_REALIZED(offscreen));
	DPUTS("REPARENT");
	gtk_widget_reparent(fixed,offscreen);
	DPUTS("REPARENT DONE");
}

void
status_applet_create_offscreen(void)
{
	DPUTS("CREATE OFFSCREEN");
	offscreen = gtk_window_new(GTK_WINDOW_POPUP);
#ifdef DEBUG_STATUS
	gtk_widget_set_uposition(offscreen,10,10);
#else
	gtk_widget_set_uposition(offscreen,gdk_screen_width()+10,
				 gdk_screen_height()+10);
#endif
	gtk_widget_show_now(offscreen);

	g_assert(offscreen->window);

	xstuff_setup_kde_dock_thingie(offscreen->window);
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

static void
applet_destroy(GtkWidget *w, StatusApplet *s)
{
	g_free(s);
	the_status = NULL;
}

static void
reparent_fixed(GtkWidget *frame)
{
	DPUTS("REPARENT");
	gtk_widget_reparent(fixed,frame);
	DPUTS("REPARENT DONE");
}

int
load_status_applet(PanelWidget *panel, int pos)
{
	GtkWidget *ebox;
	GtkWidget *frame;
	if(the_status)
		return FALSE;

	DPUTS("LOAD_STATUS_APPLET");
	
	the_status = g_new0(StatusApplet,1);
	frame = gtk_frame_new(NULL);
	the_status->orient = panel->orient;
	the_status->size = panel->sz;
	gtk_frame_set_shadow_type(GTK_FRAME(frame),
				  GTK_SHADOW_IN);
	the_status->handle = gtk_handle_box_new();
	gtk_signal_connect(GTK_OBJECT(the_status->handle), "event",
			   GTK_SIGNAL_FUNC(ignore_1st_click), NULL);
	gtk_container_add(GTK_CONTAINER(the_status->handle),
			  frame);

	if(!fixed) {
		DPUTS("NO FIXED");
		fixed = gtk_fixed_new();
		gtk_widget_show(fixed);

		gtk_container_add(GTK_CONTAINER(frame),fixed);
	} else {
		gtk_signal_connect_after(GTK_OBJECT(frame),"realize",
					 GTK_SIGNAL_FUNC(reparent_fixed),
					 NULL);
	}
	
	status_applet_update(the_status);
	
	ebox = gtk_event_box_new();
	gtk_widget_show(ebox);
	gtk_signal_connect(GTK_OBJECT(ebox), "destroy",
			   GTK_SIGNAL_FUNC(applet_destroy), the_status);
	gtk_container_add(GTK_CONTAINER(ebox),the_status->handle);

	register_toy(ebox,the_status, panel, pos, APPLET_STATUS);
	the_status->info = applets_last->data;

	return TRUE;
}
