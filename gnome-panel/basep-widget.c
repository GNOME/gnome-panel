/* Gnome panel: basep widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#include <math.h>
#include <config.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include "panel-widget.h"
#include "basep-widget.h"
#include "panel-util.h"
#include "panel_config_global.h"
#include "gdkextra.h"

extern int panel_applet_in_drag;

static void basep_widget_class_init	(BasePWidgetClass *klass);
static void basep_widget_init		(BasePWidget      *basep);

static GtkWindowClass *parent_class = NULL;

/*global settings*/
extern int pw_explicit_step;
extern int pw_drawer_step;
extern int pw_auto_step;
extern int pw_minimized_size;
extern int pw_minimize_delay;
extern int pw_disable_animations;
extern PanelMovementType pw_movement_type;

extern GlobalConfig global_config;

/************************
 widget core
 ************************/

guint
basep_widget_get_type ()
{
	static guint basep_widget_type = 0;

	if (!basep_widget_type) {
		GtkTypeInfo basep_widget_info = {
			"BasePWidget",
			sizeof (BasePWidget),
			sizeof (BasePWidgetClass),
			(GtkClassInitFunc) basep_widget_class_init,
			(GtkObjectInitFunc) basep_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		basep_widget_type = gtk_type_unique (gtk_window_get_type (),
						     &basep_widget_info);
	}

	return basep_widget_type;
}

static void
basep_widget_realize(GtkWidget *w)
{
	BasePWidget *basep = BASEP_WIDGET(w);
	GTK_WIDGET_CLASS(parent_class)->realize(w);

	set_frame_colors(PANEL_WIDGET(basep->panel),
			 basep->frame,
			 basep->hidebutton_n,
			 basep->hidebutton_e,
			 basep->hidebutton_w,
			 basep->hidebutton_s);
}

static void
basep_widget_class_init (BasePWidgetClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass*) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;

        parent_class = gtk_type_class (gtk_window_get_type ());

	class->set_hidebuttons = NULL;
	
	widget_class->realize = basep_widget_realize;
}


static int
basep_enter_notify(GtkWidget *basep,
		   GdkEventCrossing *event,
		   gpointer data)
{
	if (!gnome_win_hints_wm_exists() &&
	    global_config.autoraise)
		gdk_window_raise(basep->window);
	return FALSE;
}

void
basep_widget_get_position(BasePWidget *basep, PanelOrientType hide_orient,
			  gint16 *x, gint16 *y, gint16 w, gint16 h)
{
	GtkWidget *widget = GTK_WIDGET(basep);

	*x = *y = 0;
	switch(hide_orient) {
	case ORIENT_UP:
		if(h < basep->shown_alloc.height)
			*y -= basep->shown_alloc.height - h;
		break;
	case ORIENT_DOWN:
		break;
	case ORIENT_LEFT:
		if(w < basep->shown_alloc.width)
			*x -= basep->shown_alloc.width - w;
		break;
	case ORIENT_RIGHT:
		break;
	default:
		break;
	}
}

static void
basep_widget_set_ebox_orient(BasePWidget *basep,
			     PanelOrientType hide_orient)
{
	XSetWindowAttributes xattributes;

	switch(hide_orient) {
	case ORIENT_UP:
	case ORIENT_LEFT:
		xattributes.win_gravity = SouthEastGravity;
		break;
	case ORIENT_DOWN:
	case ORIENT_RIGHT:
		xattributes.win_gravity = NorthWestGravity;
		break;
	default:
		xattributes.win_gravity = NorthWestGravity;
		break;
	}

	XChangeWindowAttributes (GDK_WINDOW_XDISPLAY(basep->ebox->window),
				 GDK_WINDOW_XWINDOW(basep->ebox->window),
				 CWWinGravity,  &xattributes);
	
}

static int
move_step(int src, int dest, long start_time, long end_time, long cur_time)
{
	double percentage;
	
	if(src == dest)
		return dest;

	percentage = ((double)(cur_time-start_time))/(end_time-start_time);
	
	if(percentage>1.0)
		percentage = 1.0;
	
	return  src + ((dest - src)*percentage);
}

#if 0
static int
move_step(int src, int dest, int pos, int step)
{
	int range;
	int diff;
	int percentage;
	int ret;
	
	if(src == dest)
		return 0;

	range = abs(src-dest);
	diff = abs(range-abs(pos-src));
	percentage = (diff*100)/range;

	if(percentage>50)
		percentage = 100-percentage;
	
	ret = (src>dest?-1:1)*(((step>>1)*log((percentage/10.0)+1))+1);
	if(src>dest) {
		if(ret+pos<dest) ret = dest - pos;
	} else {
		if(ret+pos>dest) ret = dest - pos;
	}
	return ret;
}
#endif

void
basep_widget_do_hiding(BasePWidget *basep, PanelOrientType hide_orient,
		       int leftover, int step)
{
	GtkWidget *wid;
	int ox,oy,ow,oh;
	int x,y,w,h;
	int dx,dy,dw,dh;
	int diff;
	
	g_return_if_fail(basep != NULL);
	g_return_if_fail(IS_BASEP_WIDGET(basep));

	wid = GTK_WIDGET(basep);
	
	ox = x = basep->shown_alloc.x;
	oy = y = basep->shown_alloc.y;
	ow = w = basep->shown_alloc.width;
	oh = h = basep->shown_alloc.height;
	
	switch(hide_orient) {
	case ORIENT_UP:
		diff = h-leftover;
		dx = x;
		dy = y;
		dw = w;
		dh = leftover;
		break;
	case ORIENT_DOWN:
		diff = h-leftover;
		dx = x;
		dy = y+h-leftover;
		dw = w;
		dh = leftover;
		break;
	case ORIENT_LEFT:
		diff = w-leftover;
		dx = x;
		dy = y;
		dw = leftover;
		dh = h;
		break;
	case ORIENT_RIGHT:
		diff = w-leftover;
		dx = x+w-leftover;
		dy = y;
		dw = leftover;
		dh = h;
		break;
	default:
		/*fix warning*/ dx = dy = dw = dh = 0;
		diff = 1;
		g_assert_not_reached();
		break;
	}

	if(!pw_disable_animations && step != 0) {
		int i;
		GTimeVal tval;
		long start_secs;
		long start_time;
		long end_time;
		long cur_time;

		g_get_current_time(&tval);
		
		start_secs = tval.tv_sec;
		start_time = tval.tv_usec;
		
		end_time = start_time +
			(diff/1000.0)*200*(10001-(step*step));

		basep_widget_set_ebox_orient(basep, hide_orient);
		
		i = 0;
		while(x != dx ||
		      y != dy ||
		      w != dw ||
		      h != dh) {
			g_get_current_time(&tval);
			
			cur_time = ((tval.tv_sec-start_secs)*1000000) +
				tval.tv_usec;

			x = move_step(ox,dx,start_time,end_time,cur_time);
			y = move_step(oy,dy,start_time,end_time,cur_time);
			w = move_step(ow,dw,start_time,end_time,cur_time);
			h = move_step(oh,dh,start_time,end_time,cur_time);
			gdk_window_move_resize(wid->window, x,y,w,h);
			/*drawing the entire table flickers, so don't
			  do it often*/
			if(i++%10)
				gtk_widget_draw(basep->panel, NULL);
			else
				gtk_widget_draw(basep->table, NULL);
			gdk_flush();
		}

		gdk_window_resize(wid->window,dw,dh);
		gdk_window_set_hints (wid->window,
				      dx,dy,0,0,0,0,
				      GDK_HINT_POS);
		basep_widget_set_ebox_orient(basep, -1);
	}
	
	gtk_widget_draw(basep->table, NULL);
}

void
basep_widget_do_showing(BasePWidget *basep, PanelOrientType hide_orient,
			int leftover, int step)
{
	GtkWidget *wid;
	int ox,oy,ow,oh;
	int x,y,w,h;
	int dx,dy,dw,dh;
	int diff;

	g_return_if_fail(basep != NULL);
	g_return_if_fail(IS_BASEP_WIDGET(basep));

	wid = GTK_WIDGET(basep);
	
	ox = x = basep->shown_alloc.x;
	oy = y = basep->shown_alloc.y;
	dw = basep->shown_alloc.width;
	dh = basep->shown_alloc.height;
	
	switch(hide_orient) {
	case ORIENT_UP:
		dx = x;
		dy = y;
		ow = w = dw;
		oh = h = leftover;
		diff = dh-leftover;
		break;
	case ORIENT_DOWN:
		dx = x;
		dy = y - dh + leftover;
		ow = w = dw;
		oh = h = leftover;
		diff = dh-leftover;
		break;
	case ORIENT_LEFT:
		dx = x;
		dy = y;
		ow = w = leftover;
		oh = h = dh;
		diff = dw-leftover;
		break;
	case ORIENT_RIGHT:
		dx = x - dw + leftover;
		dy = y;
		ow = w = leftover;
		oh = h = dh;
		diff = dw-leftover;
		break;
	default:
		/*fix warning*/ dx = dy = ow = oh = w = h = 0;
		diff = 1;
		g_assert_not_reached();
		break;
	}
	
	if(!pw_disable_animations && step != 0) {
		int i;
		GTimeVal tval;
		long start_secs;
		long start_time;
		long end_time;
		long cur_time;

		g_get_current_time(&tval);
		
		start_secs = tval.tv_sec;
		start_time = tval.tv_usec;
		
		end_time = start_time +
			(diff/1000.0)*200*(10001-(step*step));
		
		basep_widget_set_ebox_orient(basep, hide_orient);
		gdk_window_resize(wid->window,ow,oh);
		gdk_window_set_hints (wid->window,
				      ox,oy,0,0,0,0,
				      GDK_HINT_POS);
		gdk_window_show(wid->window);
		gdk_window_move(wid->window,ox,oy);

		gtk_widget_show_now(wid);

		gdk_window_show(wid->window);
		i = 0;
		while(x != dx ||
		      y != dy ||
		      w != dw ||
		      h != dh) {
			g_get_current_time(&tval);
			
			cur_time = ((tval.tv_sec-start_secs)*1000000) +
				tval.tv_usec;

			x = move_step(ox,dx,start_time,end_time,cur_time);
			y = move_step(oy,dy,start_time,end_time,cur_time);
			w = move_step(ow,dw,start_time,end_time,cur_time);
			h = move_step(oh,dh,start_time,end_time,cur_time);
			gdk_window_move_resize(wid->window, x,y,w,h);
			/*drawing the entire table flickers, so don't
			  do it often*/
			if(i++%10)
				gtk_widget_draw(basep->panel, NULL);
			else
				gtk_widget_draw(basep->table, NULL);
			gdk_flush();
		}

		gdk_window_resize(wid->window,dw,dh);
		gdk_window_set_hints (wid->window,
				      dx,dy,0,0,0,0,
				      GDK_HINT_POS);

		basep_widget_set_ebox_orient(basep, -1);
	}
	
	gtk_widget_draw(basep->table, NULL);
	gtk_widget_queue_resize(wid);
}


static GtkWidget *
make_hidebutton(BasePWidget *basep,
		char *pixmaparrow,
		int horizontal)
{
	GtkWidget *w;
	GtkWidget *pixmap;
	char *pixmap_name;

	w=gtk_button_new();
	GTK_WIDGET_UNSET_FLAGS(w,GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
	if(horizontal)
		gtk_widget_set_usize(w,0,PANEL_MINIMUM_WIDTH);
	else
		gtk_widget_set_usize(w,PANEL_MINIMUM_WIDTH,0);

	pixmap_name=gnome_pixmap_file(pixmaparrow);
	if(pixmap_name) {
		pixmap = gnome_pixmap_new_from_file(pixmap_name);
		g_free(pixmap_name);
	} else {
		pixmap = gtk_label_new("*");
	}
	gtk_widget_show(pixmap);

	gtk_container_add(GTK_CONTAINER(w),pixmap);
	gtk_object_set_user_data(GTK_OBJECT(w), pixmap);
	return w;
}

/*static void
basep_widget_destroy (BasePWidget *basep)
{
	if(basep->fake)
		gdk_window_destroy(basep->fake);
}*/


static void
basep_widget_init (BasePWidget *basep)
{
	/*if we set the gnomewm hints it will have to be changed to TOPLEVEL*/
	gnome_win_hints_init();
	if (gnome_win_hints_wm_exists())
		GTK_WINDOW(basep)->type = GTK_WINDOW_TOPLEVEL;
	else
		GTK_WINDOW(basep)->type = GTK_WINDOW_POPUP;
	GTK_WINDOW(basep)->allow_shrink = TRUE;
	GTK_WINDOW(basep)->allow_grow = TRUE;
	GTK_WINDOW(basep)->auto_shrink = TRUE;
	
	basep->shown_alloc.x = basep->shown_alloc.y =
		basep->shown_alloc.width = basep->shown_alloc.height = 0;
	
	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(basep),
			      gtk_widget_get_events(GTK_WIDGET(basep)) |
			      GDK_BUTTON_RELEASE_MASK);

	basep->ebox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(basep),basep->ebox);
	gtk_widget_show(basep->ebox);

	basep->table = gtk_table_new(3,3,FALSE);
	gtk_container_add(GTK_CONTAINER(basep->ebox),basep->table);
	gtk_widget_show(basep->table);

	basep->frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(basep->frame),GTK_SHADOW_OUT);

	gtk_table_attach(GTK_TABLE(basep->table),basep->frame,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);


	/*gtk_signal_connect(GTK_OBJECT(basep), "destroy",
			   GTK_SIGNAL_FUNC(basep_widget_destroy),
			   NULL);*/

	gtk_signal_connect(GTK_OBJECT(basep), "enter_notify_event",
			   GTK_SIGNAL_FUNC(basep_enter_notify),
			   NULL);

	basep->hidebuttons_enabled = TRUE;
	basep->hidebutton_pixmaps_enabled = TRUE;
}



static void
show_hidebutton_pixmap(GtkWidget *hidebutton, int show)
{
	GtkWidget *pixmap;

	pixmap = gtk_object_get_user_data(GTK_OBJECT(hidebutton));

	if (!pixmap) return;

	if (show)
		gtk_widget_show(pixmap);
	else
		gtk_widget_hide(pixmap);
}

static void
basep_widget_show_hidebutton_pixmaps(BasePWidget *basep)
{
	int show = basep->hidebutton_pixmaps_enabled;
	show_hidebutton_pixmap(basep->hidebutton_n, show);
	show_hidebutton_pixmap(basep->hidebutton_e, show);
	show_hidebutton_pixmap(basep->hidebutton_w, show);
	show_hidebutton_pixmap(basep->hidebutton_s, show);
}

static void
b_back_change(PanelWidget *panel,
	      PanelBackType type,
	      char *pixmap,
	      GdkColor *color,
	      BasePWidget *basep)
{
	if(type == PANEL_BACK_PIXMAP &&
	   basep->panel->parent == basep->frame) {
		gtk_widget_hide(basep->frame);
		gtk_widget_ref(basep->panel);
		gtk_container_remove(GTK_CONTAINER(basep->frame),
				     basep->panel);
		gtk_table_attach(GTK_TABLE(basep->table),basep->panel,
				 1,2,1,2,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 0,0);
		gtk_widget_unref(basep->panel);
	} else if(type != PANEL_BACK_PIXMAP &&
		  basep->panel->parent == basep->table) {
		gtk_widget_ref(basep->panel);
		gtk_container_remove(GTK_CONTAINER(basep->table),
				     basep->panel);
		gtk_container_add(GTK_CONTAINER(basep->frame),
				  basep->panel);
		gtk_widget_unref(basep->panel);
		gtk_widget_show(basep->frame);
	}

	set_frame_colors(panel,
			 basep->frame,
			 basep->hidebutton_n,
			 basep->hidebutton_e,
			 basep->hidebutton_w,
			 basep->hidebutton_s);
}

GtkWidget*
basep_widget_construct (BasePWidget *basep,
			int packed,
			int reverse_arrows,
			PanelOrientation orient,
			int hidebuttons_enabled,
			int hidebutton_pixmaps_enabled,
			PanelBackType back_type,
			char *back_pixmap,
			int fit_pixmap_bg,
			GdkColor *back_color)
{
	basep->panel = panel_widget_new(packed,
					orient,
					back_type,
					back_pixmap,
					fit_pixmap_bg,
					back_color);
	gtk_signal_connect_after(GTK_OBJECT(basep->panel), "back_change",
				 GTK_SIGNAL_FUNC(b_back_change),
				 basep);
	gtk_object_set_data(GTK_OBJECT(basep->panel),PANEL_PARENT,
			    basep);
	PANEL_WIDGET(basep->panel)->drop_widget = GTK_WIDGET(basep);

	gtk_widget_show(basep->panel);

	if(back_type != PANEL_BACK_PIXMAP) {
		gtk_widget_show(basep->frame);
		gtk_container_add(GTK_CONTAINER(basep->frame),basep->panel);
	} else {
		gtk_table_attach(GTK_TABLE(basep->table),basep->panel,
				 1,2,1,2,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 0,0);
	}

	/*we add all the hide buttons to the table here*/
	/*EAST*/
	basep->hidebutton_w = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-right.png":
					      "panel-arrow-left.png",
					      TRUE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_w,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	/*NORTH*/
	basep->hidebutton_n = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-down.png":
					      "panel-arrow-up.png",
					      FALSE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	/*WEST*/
	basep->hidebutton_e = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-left.png":
					      "panel-arrow-right.png",
					      TRUE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_e,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	/*SOUTH*/
	basep->hidebutton_s = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-up.png":
					      "panel-arrow-down.png",
					      FALSE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_s,
			 1,2,2,3,GTK_FILL,GTK_FILL,0,0);

	basep->hidebuttons_enabled = hidebuttons_enabled;
	basep->hidebutton_pixmaps_enabled = hidebutton_pixmaps_enabled;

	basep_widget_set_hidebuttons(basep);
	basep_widget_show_hidebutton_pixmaps(basep);

	return GTK_WIDGET(basep);
}

void
basep_widget_change_params(BasePWidget *basep,
			   PanelOrientation orient,
			   int hidebuttons_enabled,
			   int hidebutton_pixmaps_enabled,
			   PanelBackType back_type,
			   char *pixmap_name,
			   int fit_pixmap_bg,
			   GdkColor *back_color)
{
	g_return_if_fail(basep);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(basep)));
	
	basep->hidebuttons_enabled = hidebuttons_enabled;
	basep->hidebutton_pixmaps_enabled = hidebutton_pixmaps_enabled;

	panel_widget_change_params(PANEL_WIDGET(basep->panel),
				   orient,
				   back_type,
				   pixmap_name,
				   fit_pixmap_bg,
				   back_color);

	basep_widget_set_hidebuttons(basep);
	basep_widget_show_hidebutton_pixmaps(basep);
}

void
basep_widget_enable_buttons(BasePWidget *basep)
{
	gtk_widget_set_sensitive(basep->hidebutton_n,TRUE);
	gtk_widget_set_sensitive(basep->hidebutton_e,TRUE);
	gtk_widget_set_sensitive(basep->hidebutton_w,TRUE);
	gtk_widget_set_sensitive(basep->hidebutton_s,TRUE);
}


void
basep_widget_disable_buttons(BasePWidget *basep)
{
	gtk_widget_set_sensitive(basep->hidebutton_n,FALSE);
	gtk_widget_set_sensitive(basep->hidebutton_e,FALSE);
	gtk_widget_set_sensitive(basep->hidebutton_w,FALSE);
	gtk_widget_set_sensitive(basep->hidebutton_s,FALSE);
}

void
basep_widget_set_hidebuttons(BasePWidget *basep)
{
	BasePWidgetClass *class = BASEP_WIDGET_CLASS(GTK_OBJECT(basep)->klass);
	if(class->set_hidebuttons)
		(*class->set_hidebuttons)(basep);
}
