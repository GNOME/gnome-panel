/* Gnome panel: drawer widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#include <math.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include "panel-widget.h"
#include "drawer-widget.h"
#include "panel-util.h"
#include "gdkextra.h"

/*there  can universally be only one applet being dragged since we assume
we only have one mouse :) */
extern int panel_applet_in_drag;

static void drawer_widget_class_init	(DrawerWidgetClass *klass);
static void drawer_widget_init		(DrawerWidget      *drawer);

extern GdkCursor *fleur_cursor;

/*global settings*/
extern int pw_explicit_step;
extern int pw_drawer_step;
extern int pw_auto_step;
extern int pw_minimized_size;
extern int pw_minimize_delay;
extern int pw_disable_animations;
extern PanelMovementType pw_movement_type;

typedef void (*StateSignal) (GtkObject * object,
			     DrawerState state,
			     gpointer data);

/************************
 widget core
 ************************/

guint
drawer_widget_get_type ()
{
	static guint drawer_widget_type = 0;

	if (!drawer_widget_type) {
		GtkTypeInfo drawer_widget_info = {
			"DrawerWidget",
			sizeof (DrawerWidget),
			sizeof (DrawerWidgetClass),
			(GtkClassInitFunc) drawer_widget_class_init,
			(GtkObjectInitFunc) drawer_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		drawer_widget_type = gtk_type_unique (gtk_window_get_type (),
						      &drawer_widget_info);
	}

	return drawer_widget_type;
}

enum {
	STATE_CHANGE_SIGNAL,
	LAST_SIGNAL
};

static int drawer_widget_signals[LAST_SIGNAL] = {0};

static void
marshal_signal_state (GtkObject * object,
		      GtkSignalFunc func,
		      gpointer func_data,
		      GtkArg * args)
{
	StateSignal rfunc;

	rfunc = (StateSignal) func;

	(*rfunc) (object, GTK_VALUE_ENUM (args[0]),
		  func_data);
}

static void
drawer_widget_class_init (DrawerWidgetClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) class;

	drawer_widget_signals[STATE_CHANGE_SIGNAL] =
		gtk_signal_new("state_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(DrawerWidgetClass,
			       			 state_change),
			       marshal_signal_state,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);

	gtk_object_class_add_signals(object_class,drawer_widget_signals,
				     LAST_SIGNAL);

	class->state_change = NULL;
}

static void
drawer_widget_set_drop_zone(DrawerWidget *drawer)
{
	if(PANEL_WIDGET(drawer->panel)->orient == PANEL_HORIZONTAL) {
		if(drawer->drop_zone_pos == DROP_ZONE_LEFT) {
			gtk_widget_hide(drawer->handle_n);
			gtk_widget_show(drawer->handle_e);
			gtk_widget_hide(drawer->handle_w);
			gtk_widget_hide(drawer->handle_s);
		} else {
			gtk_widget_hide(drawer->handle_n);
			gtk_widget_hide(drawer->handle_e);
			gtk_widget_show(drawer->handle_w);
			gtk_widget_hide(drawer->handle_s);
		}
	} else {
		if(drawer->drop_zone_pos == DROP_ZONE_LEFT) {
			gtk_widget_show(drawer->handle_n);
			gtk_widget_hide(drawer->handle_e);
			gtk_widget_hide(drawer->handle_w);
			gtk_widget_hide(drawer->handle_s);
		} else {
			gtk_widget_hide(drawer->handle_n);
			gtk_widget_hide(drawer->handle_e);
			gtk_widget_hide(drawer->handle_w);
			gtk_widget_show(drawer->handle_s);
		}
	}
}

static int
move_step(int src, int dest, int pos, int step)
{
	int range = abs(src-dest);
	int diff = abs(range-abs(pos-src));
	int percentage = (diff*100)/range;

	if(percentage>50)
		percentage = 100-percentage;

	return ((step>>1)*log((percentage/10.0)+1))+1;
}

static void
move_horiz_d(DrawerWidget *drawer, int src_x, int dest_x, int step, int hide)
{
	int orig_x, x, y;
	int orig_w, w, h;
	GtkWidget *wid = GTK_WIDGET(drawer);

	x = drawer->x;
	y = drawer->y;
	gdk_window_get_size(wid->window,&w,&h);
	if(!hide)
		w = 0;

	orig_x = x;
	orig_w = w;

	if (!pw_disable_animations && step != 0) {
		if (src_x < dest_x) {
			for( x = src_x; x < dest_x;
			     x+= move_step(src_x,dest_x,x,step)) {
				if(hide) {
					move_resize_window(wid,
							   x, y, w, h);
					w-=move_step(src_x,dest_x,x,step);
				} else {
					move_resize_window(wid,
							   orig_x, y, w, h);
					w+=move_step(src_x,dest_x,x,step);
				}
			}
		} else {
			for (x = src_x; x > dest_x;
			     x-= move_step(src_x,dest_x,x,step)) {
				if(hide) {
					move_resize_window(wid,
							   orig_x, y, w, h);
					w-=move_step(src_x,dest_x,x,step);
				} else {
					move_resize_window(wid,
							   x, y, w, h);
					w+=move_step(src_x,dest_x,x,step);
				}
			}
		}
	}
	
	if(hide)
		w = orig_w - abs(src_x-dest_x);
	else
		w = orig_w + abs(src_x-dest_x);

	move_resize_window(wid, dest_x, drawer->y,w,h);
}

static void
move_vert_d(DrawerWidget *drawer, int src_y, int dest_y, int step, int hide)
{
	int orig_y, x, y;
	int orig_h, w, h;
	GtkWidget *wid = GTK_WIDGET(drawer);

	x = drawer->x;
	y = drawer->y;
	gdk_window_get_size(wid->window,&w,&h);
	if(!hide)
		h = 0;

	orig_y = y;
	orig_h = h;

	if (!pw_disable_animations && step != 0) {
		if (src_y < dest_y) {
			for( y = src_y; y < dest_y;
			     y+= move_step(src_y,dest_y,y,step)) {
				if(hide) {
					move_resize_window(wid,
							   x, y, w, h);
					h-=move_step(src_y,dest_y,y,step);
				} else {
					move_resize_window(wid,
							   x, orig_y, w, h);
					h+=move_step(src_y,dest_y,y,step);
				}
			}
		} else {
			for (y = src_y; y > dest_y;
			     y-= move_step(src_y,dest_y,y,step)) {
				if(hide) {
					move_resize_window(wid,
							   x, orig_y, w, h);
					h-=move_step(src_y,dest_y,y,step);
				} else {
					move_resize_window(wid,
							   x, y, w, h);
					h+=move_step(src_y,dest_y,y,step);
				}
			}
		}
	}
	
	if(hide)
		h = orig_h - abs(src_y-dest_y);
	else
		h = orig_h + abs(src_y-dest_y);

	move_resize_window(wid, drawer->x, dest_y, w,h);
}

void
drawer_widget_open_drawer(DrawerWidget *drawer)
{
	int x,y;
	int width, height;

	if((drawer->state == DRAWER_SHOWN) ||
	   (drawer->state == DRAWER_MOVING))
		return;
	
	if(!GTK_WIDGET(drawer)->window) {
		gtk_widget_set_uposition(GTK_WIDGET(drawer),drawer->x,drawer->y);
		drawer->state = DRAWER_SHOWN;
		return;
	}

	drawer->state = DRAWER_MOVING;

	width   = GTK_WIDGET(drawer)->allocation.width;
	height  = GTK_WIDGET(drawer)->allocation.height;
	x = drawer->x;
	y = drawer->y;

	gdk_window_move(GTK_WIDGET(drawer)->window, -3000,-3000);
	gtk_widget_show(GTK_WIDGET(drawer));
	if(PANEL_WIDGET(drawer->panel)->orient == PANEL_HORIZONTAL) {
		if(drawer->drop_zone_pos==DROP_ZONE_LEFT) {
			move_resize_window(GTK_WIDGET(drawer),x+width,y,
					   0,height);
			move_horiz_d(drawer, x+width, x,
				     pw_drawer_step,FALSE);
		} else {
			move_resize_window(GTK_WIDGET(drawer),x-width,y,
					   0,height);
			move_horiz_d(drawer, x-width, x,
				     pw_drawer_step,FALSE);
		}
	} else {
		if(drawer->drop_zone_pos==DROP_ZONE_LEFT) {
			move_resize_window(GTK_WIDGET(drawer),x,y+height,
					   width,0);
			move_vert_d(drawer, y+height, y,
				  pw_drawer_step,FALSE);
		} else {
			move_resize_window(GTK_WIDGET(drawer),x,y-height,
					   width,0);
			move_vert_d(drawer, y-height, y,
				    pw_drawer_step,FALSE);
		}
	}
	/*move_resize_window(GTK_WIDGET(drawer),x,y,width,height);*/

	drawer->state = DRAWER_SHOWN;

	gtk_signal_emit(GTK_OBJECT(drawer),
			drawer_widget_signals[STATE_CHANGE_SIGNAL],
			DRAWER_SHOWN);

}

void
drawer_widget_close_drawer(DrawerWidget *drawer)
{
	int x,y;
	int width, height;

	if((drawer->state != DRAWER_SHOWN) ||
	   (drawer->state == DRAWER_MOVING))
		return;

	if(!GTK_WIDGET(drawer)->window) {
		gtk_widget_hide(GTK_WIDGET(drawer));
		drawer->state = DRAWER_HIDDEN;
		return;
	}

	gtk_signal_emit(GTK_OBJECT(drawer),
			drawer_widget_signals[STATE_CHANGE_SIGNAL],
			DRAWER_HIDDEN);

	drawer->state = DRAWER_MOVING;

	width   = GTK_WIDGET(drawer)->allocation.width;
	height  = GTK_WIDGET(drawer)->allocation.height;
	gdk_window_get_position(GTK_WIDGET(drawer)->window,&x,&y);

	if(!pw_disable_animations) {
		if(PANEL_WIDGET(drawer->panel)->orient == PANEL_HORIZONTAL) {
			if(drawer->drop_zone_pos==DROP_ZONE_LEFT)
				move_horiz_d(drawer, x, x+width,
					     pw_drawer_step, TRUE);
			else
				move_horiz_d(drawer, x, x-width,
					     pw_drawer_step, TRUE);
		} else {
			if(drawer->drop_zone_pos==DROP_ZONE_LEFT)
				move_vert_d(drawer, y, y+height,
					    pw_drawer_step, TRUE);
			else
				move_vert_d(drawer, y, y-height,
					    pw_drawer_step, TRUE);
		}
	}

	gtk_widget_hide(GTK_WIDGET(drawer));

	move_window(GTK_WIDGET(drawer),x,y);

	drawer->state = DRAWER_HIDDEN;

	move_resize_window(GTK_WIDGET(drawer),x,y,width,height);
}


static int
drawer_enter_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	/*FIXME: do we want this autoraise piece?*/
	gdk_window_raise(widget->window);

	return FALSE;
}


static GtkWidget *
make_hidebutton(char *pixmaphandle,
		int wi, int he)
{
	GtkWidget *w;
	GtkWidget *pixmap;
	char *pixmap_name;

	GtkWidget *frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
	gtk_widget_show(frame);
	pixmap_name=gnome_unconditional_pixmap_file(pixmaphandle);
	pixmap = gnome_pixmap_new_from_file(pixmap_name);
	gtk_widget_show(pixmap);
	g_free(pixmap_name);
	gtk_container_add(GTK_CONTAINER(frame),pixmap);
	w = gtk_event_box_new();
	gtk_widget_show(w);
	gtk_container_add(GTK_CONTAINER(w),frame);
	gtk_widget_set_usize(w,wi,he);

	return w;
}

static int
drawer_widget_destroy(GtkWidget *w, gpointer data)
{
	return FALSE;
}

static void
drawer_widget_init (DrawerWidget *drawer)
{
	GTK_WINDOW(drawer)->type = GTK_WINDOW_POPUP;
	GTK_WINDOW(drawer)->allow_shrink = TRUE;
	GTK_WINDOW(drawer)->allow_grow = TRUE;
	GTK_WINDOW(drawer)->auto_shrink = TRUE;

	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(drawer),
			      gtk_widget_get_events(GTK_WIDGET(drawer)) |
			      GDK_BUTTON_RELEASE_MASK);

	drawer->table = gtk_table_new(3,3,FALSE);
	gtk_container_add(GTK_CONTAINER(drawer),drawer->table);
	gtk_widget_show(drawer->table);

	/*we add all the handles to the table here*/
	/*EAST*/
	drawer->handle_e = make_hidebutton("panel-menu-main.xpm",40,0);
	gtk_table_attach(GTK_TABLE(drawer->table),drawer->handle_e,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	/*NORTH*/
	drawer->handle_n = make_hidebutton("panel-menu-main.xpm",0,40);
	gtk_table_attach(GTK_TABLE(drawer->table),drawer->handle_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	/*WEST*/
	drawer->handle_w = make_hidebutton( "panel-menu-main.xpm",40,0);
	gtk_table_attach(GTK_TABLE(drawer->table),drawer->handle_w,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	/*SOUTH*/
	drawer->handle_s = make_hidebutton("panel-menu-main.xpm",0,40);
	gtk_table_attach(GTK_TABLE(drawer->table),drawer->handle_s,
			 1,2,2,3,GTK_FILL,GTK_FILL,0,0);

	gtk_signal_connect(GTK_OBJECT(drawer), "enter_notify_event",
			   GTK_SIGNAL_FUNC(drawer_enter_notify),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(drawer),
			   "destroy",
			   GTK_SIGNAL_FUNC(drawer_widget_destroy),
			   NULL);


	drawer->state = DRAWER_SHOWN;
	drawer->drop_zone_pos = DROP_ZONE_LEFT;
}


GtkWidget*
drawer_widget_new (PanelOrientation orient,
		   DrawerState state,
		   DrawerDropZonePos drop_zone_pos,
		   PanelBackType back_type,
		   char *back_pixmap,
		   int fit_pixmap_bg,
		   GdkColor *back_color)
{
	DrawerWidget *drawer;
	GtkWidget *frame;

	drawer = gtk_type_new(drawer_widget_get_type());

	drawer->panel = panel_widget_new(TRUE,
					 orient,
					 back_type,
					 back_pixmap,
					 fit_pixmap_bg,
					 back_color);
	gtk_object_set_data(GTK_OBJECT(drawer->panel),PANEL_PARENT,
			    drawer);
	PANEL_WIDGET(drawer->panel)->drop_widget = GTK_WIDGET(drawer);

	gtk_widget_show(drawer->panel);

	frame = gtk_frame_new(NULL);
	gtk_widget_show(frame);
	gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(frame),drawer->panel);

	gtk_table_attach(GTK_TABLE(drawer->table),frame,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	drawer->state = state;

	drawer->x = 0;
	drawer->y = 0;
	drawer_widget_set_pos(drawer,-100,-100);
	drawer_widget_set_drop_zone(drawer);

	return GTK_WIDGET(drawer);
}

void
drawer_widget_change_params(DrawerWidget *drawer,
			    PanelOrientation orient,
			    DrawerState state,
			    DrawerDropZonePos drop_zone_pos,
			    PanelBackType back_type,
			    char *pixmap,
			    int fit_pixmap_bg,
			    GdkColor *back_color)
{
	DrawerState oldstate;
	
	g_return_if_fail(drawer);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(drawer)));

	oldstate = drawer->state;

	drawer->state = state;

	panel_widget_change_params(PANEL_WIDGET(drawer->panel),
				   orient,
				   back_type,
				   pixmap,
				   fit_pixmap_bg,
				   back_color);
	

	drawer->drop_zone_pos = drop_zone_pos;
	drawer_widget_set_drop_zone(drawer);

	if(oldstate != drawer->state)
	   	gtk_signal_emit(GTK_OBJECT(drawer),
	   			drawer_widget_signals[STATE_CHANGE_SIGNAL],
	   			drawer->state);
}

void
drawer_widget_change_orient(DrawerWidget *drawer,
			    PanelOrientation orient)
{
	PanelWidget *panel = PANEL_WIDGET(drawer->panel);
	drawer_widget_change_params(drawer,
				    orient,
				    drawer->state,
				    drawer->drop_zone_pos,
				    panel->back_type,
				    panel->back_pixmap,
				    panel->fit_pixmap_bg,
				    &panel->back_color);
}

void
drawer_widget_change_drop_zone_pos(DrawerWidget *drawer,
				   DrawerDropZonePos drop_zone_pos)
{
	PanelWidget *panel = PANEL_WIDGET(drawer->panel);
	drawer_widget_change_params(drawer,
				    panel->orient,
				    drawer->state,
				    drop_zone_pos,
				    panel->back_type,
				    panel->back_pixmap,
				    panel->fit_pixmap_bg,
				    &panel->back_color);
}

void
drawer_widget_set_pos(DrawerWidget *drawer, int x, int y)
{
	if(drawer->x!=x || drawer->y!=y) {
		drawer->x = x;
		drawer->y = y;
		if(GTK_WIDGET(drawer)->window)
			move_window(GTK_WIDGET(drawer),x,y);
		gtk_widget_set_uposition(GTK_WIDGET(drawer),x,y);
	}
}

void
drawer_widget_restore_state(DrawerWidget *drawer)
{
	gtk_widget_set_uposition(GTK_WIDGET(drawer),drawer->x,drawer->y);
	gtk_widget_show(GTK_WIDGET(drawer));
}

