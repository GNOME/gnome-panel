/* Gnome panel: snapped widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#include <math.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include "panel-widget.h"
#include "snapped-widget.h"
#include "panel-util.h"
#include "gdkextra.h"

extern int panel_applet_in_drag;

static void snapped_widget_class_init	(SnappedWidgetClass *klass);
static void snapped_widget_init		(SnappedWidget      *snapped);

extern GdkCursor *fleur_cursor;

/*global settings*/
extern int pw_explicit_step;
extern int pw_drawer_step;
extern int pw_auto_step;
extern int pw_minimized_size;
extern int pw_minimize_delay;
extern int pw_disable_animations;
extern PanelMovementType pw_movement_type;

extern int panel_widget_inhibit_allocates;

/************************
 widget core
 ************************/

guint
snapped_widget_get_type ()
{
	static guint snapped_widget_type = 0;

	if (!snapped_widget_type) {
		GtkTypeInfo snapped_widget_info = {
			"SnappedWidget",
			sizeof (SnappedWidget),
			sizeof (SnappedWidgetClass),
			(GtkClassInitFunc) snapped_widget_class_init,
			(GtkObjectInitFunc) snapped_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		snapped_widget_type = gtk_type_unique (gtk_window_get_type (),
						       &snapped_widget_info);
	}

	return snapped_widget_type;
}

enum {
	POS_CHANGE_SIGNAL,
	STATE_CHANGE_SIGNAL,
	LAST_SIGNAL
};

static int snapped_widget_signals[LAST_SIGNAL] = {0,0};

/*int is used for anums anyhow*/
typedef void (*IntSignal) (GtkObject * object,
			   int i,
			   gpointer data);


static void
marshal_signal_int (GtkObject * object,
		    GtkSignalFunc func,
		    gpointer func_data,
		    GtkArg * args)
{
	IntSignal rfunc;

	rfunc = (IntSignal) func;

	(*rfunc) (object, GTK_VALUE_ENUM (args[0]),
		  func_data);
}

static void
snapped_widget_class_init (SnappedWidgetClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) class;

	snapped_widget_signals[POS_CHANGE_SIGNAL] =
		gtk_signal_new("pos_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(SnappedWidgetClass,
			       			 pos_change),
			       marshal_signal_int,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);
	snapped_widget_signals[STATE_CHANGE_SIGNAL] =
		gtk_signal_new("state_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(SnappedWidgetClass,
			       			 state_change),
			       marshal_signal_int,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);

	gtk_object_class_add_signals(object_class,snapped_widget_signals,
				     LAST_SIGNAL);

	class->pos_change = NULL;
	class->state_change = NULL;
}

static void
snapped_widget_set_position(SnappedWidget *snapped)
{
	int xcor = 0;
	int ycor = 0;
	int x,y;
	int newx,newy;
	int width,height;
	int thick;
	PanelWidget *panel = PANEL_WIDGET(snapped->panel);
	
	if(snapped->state == SNAPPED_MOVING)
		return;

	if(GTK_WIDGET(snapped)->window)
		gdk_window_get_geometry(GTK_WIDGET(snapped)->window,&x,&y,
					&width,&height,NULL);
	else {
		x = y = -3000;
		width = height = 48;
	}
	newx = x;
	newy = y;
	
	if(snapped->pos == SNAPPED_BOTTOM ||
	   snapped->pos == SNAPPED_TOP)
		thick = height;
	else
		thick = width;

	if(snapped->mode == SNAPPED_AUTO_HIDE &&
	   snapped->state == SNAPPED_HIDDEN)
		ycor = thick - pw_minimized_size;
	if(panel->orient == PANEL_HORIZONTAL) {
		if(snapped->state == SNAPPED_HIDDEN_LEFT)
			xcor = - gdk_screen_width() +
			       snapped->hidebutton_w->allocation.width;
		else if(snapped->state == SNAPPED_HIDDEN_RIGHT)
			xcor = gdk_screen_width() -
			       snapped->hidebutton_w->allocation.width;
	} else { /*vertical*/
		if(snapped->state == SNAPPED_HIDDEN_LEFT)
			xcor = - gdk_screen_height() +
			       snapped->hidebutton_s->allocation.height;
		else if(snapped->state == SNAPPED_HIDDEN_RIGHT)
			xcor = gdk_screen_height() -
			       snapped->hidebutton_n->allocation.height;
	}

	switch(snapped->pos) {
		case SNAPPED_TOP:
			newx = xcor;
			newy = -ycor;
			break;
		case SNAPPED_BOTTOM:
			newx = xcor;
			newy = gdk_screen_height() - thick + ycor;
			break;
		case SNAPPED_LEFT:
			newx = -ycor;
			newy = xcor;
			break;
		case SNAPPED_RIGHT:
			newx = gdk_screen_width() - thick + ycor;
			newy = xcor;
			break;
	}
	if(!GTK_WIDGET(snapped)->window)
		gtk_widget_set_uposition(GTK_WIDGET(snapped),newx,newy);
	else if(newx != x || newy != y)
		move_window(GTK_WIDGET(snapped),newx,newy);
}

static void
snapped_widget_set_size(SnappedWidget *snapped)
{
	PanelWidget *panel = PANEL_WIDGET(snapped->panel);
	if(panel->orient == PANEL_HORIZONTAL) {
		gtk_widget_set_usize(GTK_WIDGET(snapped),
				     gdk_screen_width(),
				     0);
		if(GTK_WIDGET(snapped)->window)
			resize_window(GTK_WIDGET(snapped),
				      gdk_screen_width(),
				      panel->thick);
	} else { /*vertical*/
		gtk_widget_set_usize(GTK_WIDGET(snapped),
				     0,
				     gdk_screen_height());
		if(GTK_WIDGET(snapped)->window)
			resize_window(GTK_WIDGET(snapped),
				      panel->thick,
				      gdk_screen_height());
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
move_horiz(SnappedWidget *snapped, int src_x, int dest_x, int step)
{
	int x, y;
	GtkWidget *w = GTK_WIDGET(snapped);

	gdk_window_get_origin(w->window,&x,&y);

	if (!pw_disable_animations && step != 0) {
		if (src_x < dest_x) {
			for( x = src_x; x < dest_x;
			     x+= move_step(src_x,dest_x,x,step))
				move_window(w,x,y);
		} else {
			for (x = src_x; x > dest_x;
			     x-= move_step(src_x,dest_x,x,step))
				move_window(w,x,y);
		}
	}
	
	move_window(w, dest_x, y);
}


static void
move_vert(SnappedWidget *snapped, int src_y, int dest_y, int step)
{
	int x, y;

	GtkWidget *w = GTK_WIDGET(snapped);

	gdk_window_get_origin(w->window,&x,&y);

	if (!pw_disable_animations && step != 0) {
		if (src_y < dest_y) {
                        for (y = src_y; y < dest_y;
			     y+= move_step(src_y,dest_y,y,step))
				move_window(w,x,y);
		} else {
                        for (y = src_y; y > dest_y;
			     y-= move_step(src_y,dest_y,y,step))
				move_window(w,x,y);
		}
	}

	move_window(w, x, dest_y);
}

void
snapped_widget_pop_up(SnappedWidget *snapped)
{
	int width, height;
	int swidth, sheight;

	if ((snapped->state == SNAPPED_MOVING) ||
	    (snapped->state == SNAPPED_SHOWN))
		return;

	snapped->state = SNAPPED_MOVING;

	width   = GTK_WIDGET(snapped)->allocation.width;
	height  = GTK_WIDGET(snapped)->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (snapped->pos) {
		case SNAPPED_TOP:
		        move_vert(snapped, -height + pw_minimized_size, 0,
				  pw_auto_step);
			break;

		case SNAPPED_BOTTOM:
			move_vert(snapped, sheight - pw_minimized_size, 
				  sheight - height,pw_auto_step);
			break;

		case SNAPPED_LEFT:
			move_horiz(snapped, -width + pw_minimized_size, 0,
				   pw_auto_step);
			break;

		case SNAPPED_RIGHT:
			move_horiz(snapped, swidth - pw_minimized_size, 
				   swidth - width,pw_auto_step);
			break;
	}

	snapped->state = SNAPPED_SHOWN;

	gtk_signal_emit(GTK_OBJECT(snapped),
			snapped_widget_signals[STATE_CHANGE_SIGNAL],
			SNAPPED_SHOWN);
}


static int
snapped_widget_pop_down(gpointer data)
{
	SnappedWidget *snapped = data;
	int width, height;
	int swidth, sheight;

	if(snapped->autohide_inhibit)
		return TRUE;

	if(snapped->state != SNAPPED_SHOWN ||
	   panel_widget_is_cursor(PANEL_WIDGET(snapped->panel),0)) {
		snapped->leave_notify_timer_tag = 0;
		return FALSE;
	}

	/*we are moving, or have drawers open, so wait with the
	  pop_down*/
	if(panel_applet_in_drag ||
	   PANEL_WIDGET(snapped->panel)->drawers_open>0)
		return TRUE;

	gtk_signal_emit(GTK_OBJECT(snapped),
			snapped_widget_signals[STATE_CHANGE_SIGNAL],
			SNAPPED_HIDDEN);

	snapped->state = SNAPPED_MOVING;

	width   = GTK_WIDGET(snapped)->allocation.width;
	height  = GTK_WIDGET(snapped)->allocation.height;
	swidth  = gdk_screen_width();
	sheight = gdk_screen_height();

	switch (snapped->pos) {
		case SNAPPED_TOP:
			move_vert(snapped, 0, -height + pw_minimized_size,
				  pw_auto_step);
			break;

		case SNAPPED_BOTTOM:
			move_vert(snapped, sheight - height, 
				  sheight - pw_minimized_size,
				  pw_auto_step);
			break;

		case SNAPPED_LEFT:
			move_horiz(snapped, 0, -width + pw_minimized_size,
				   pw_auto_step);
			break;

		case SNAPPED_RIGHT:
			move_horiz(snapped, swidth - width, 
				   swidth - pw_minimized_size,
				   pw_auto_step);
			break;
	}

	snapped->state = SNAPPED_HIDDEN;

	snapped->leave_notify_timer_tag = 0;
  
	return FALSE;
}


static void
snapped_widget_pop_show(SnappedWidget *snapped, int fromright)
{
	int width, height;

	if ((snapped->state == SNAPPED_MOVING) ||
	    (snapped->state == SNAPPED_SHOWN))
		return;

	snapped->state = SNAPPED_MOVING;

	width   = GTK_WIDGET(snapped)->allocation.width;
	height  = GTK_WIDGET(snapped)->allocation.height;

	if(PANEL_WIDGET(snapped->panel)->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(snapped, -width +
				   snapped->hidebutton_w->allocation.width, 0,
				   pw_explicit_step);
		else
			move_horiz(snapped, width -
				   snapped->hidebutton_e->allocation.width, 0,
				   pw_explicit_step);
	} else {
		if(fromright)
			move_vert(snapped, -height +
				  snapped->hidebutton_s->allocation.height, 0,
				  pw_explicit_step);
		else
			move_vert(snapped, height -
				  snapped->hidebutton_n->allocation.height, 0,
				  pw_explicit_step);
	}

	snapped->state = SNAPPED_SHOWN;

	gtk_signal_emit(GTK_OBJECT(snapped),
			snapped_widget_signals[STATE_CHANGE_SIGNAL],
			SNAPPED_SHOWN);
}

static void
snapped_widget_pop_hide(SnappedWidget *snapped, int fromright)
{
	int width, height;

	if((snapped->state != SNAPPED_SHOWN))
		return;
	

	if (snapped->leave_notify_timer_tag != 0) {
		gtk_timeout_remove (snapped->leave_notify_timer_tag);
		snapped->leave_notify_timer_tag = 0;
	}

	if(fromright)
	   	gtk_signal_emit(GTK_OBJECT(snapped),
	   			snapped_widget_signals[STATE_CHANGE_SIGNAL],
	   			SNAPPED_HIDDEN_LEFT);
	else
	   	gtk_signal_emit(GTK_OBJECT(snapped),
	   			snapped_widget_signals[STATE_CHANGE_SIGNAL],
	   			SNAPPED_HIDDEN_RIGHT);

	snapped->state = SNAPPED_MOVING;

	width   = GTK_WIDGET(snapped)->allocation.width;
	height  = GTK_WIDGET(snapped)->allocation.height;

	if(PANEL_WIDGET(snapped->panel)->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(snapped, 0, -width +
				   snapped->hidebutton_w->allocation.width,
				   pw_explicit_step);
		else
			move_horiz(snapped, 0, width -
				   snapped->hidebutton_e->allocation.width,
				   pw_explicit_step);
	} else {
		if(fromright)
			move_vert(snapped, 0, -height +
				  snapped->hidebutton_s->allocation.height,
				  pw_explicit_step);
		else
			move_vert(snapped, 0, height -
				  snapped->hidebutton_n->allocation.height,
				  pw_explicit_step);
	}

	if(fromright)
		snapped->state = SNAPPED_HIDDEN_LEFT;
	else
		snapped->state = SNAPPED_HIDDEN_RIGHT;
}

static int
snapped_show_hide_right(GtkWidget *widget, gpointer data)
{
	SnappedWidget *snapped = data;
	gtk_widget_set_state(widget,GTK_STATE_NORMAL);
	gtk_widget_queue_draw(widget);
	if(snapped->state == SNAPPED_MOVING) 
		return FALSE;
	else if(snapped->state == SNAPPED_SHOWN)
		snapped_widget_pop_hide(snapped,TRUE);
	else
		snapped_widget_pop_show(snapped,FALSE);
	return FALSE;
}

static int
snapped_show_hide_left(GtkWidget *widget, gpointer data)
{
	SnappedWidget *snapped = data;
	gtk_widget_set_state(widget,GTK_STATE_NORMAL);
	gtk_widget_queue_draw(widget);
	if(snapped->state == SNAPPED_MOVING) 
		return FALSE;
	else if(snapped->state == SNAPPED_SHOWN)
		snapped_widget_pop_hide(snapped,FALSE);
	else
		snapped_widget_pop_show(snapped,TRUE);
	return FALSE;
}

static int
snapped_enter_notify(SnappedWidget *snapped,
		     GdkEventCrossing *event,
		     gpointer data)
{
	/*FIXME: do we want this autoraise piece?*/
	gdk_window_raise(GTK_WIDGET(snapped)->window);

	if ((snapped->mode == SNAPPED_EXPLICIT_HIDE) ||
	    (event->detail == GDK_NOTIFY_INFERIOR) ||
	    (snapped->state == SNAPPED_HIDDEN_LEFT) ||
	    (snapped->state == SNAPPED_HIDDEN_RIGHT))
		return FALSE;

	if (snapped->leave_notify_timer_tag != 0) {
		gtk_timeout_remove (snapped->leave_notify_timer_tag);
		snapped->leave_notify_timer_tag = 0;
	}
 
	snapped_widget_pop_up(snapped);
	
	return FALSE;
}

void
snapped_widget_queue_pop_down(SnappedWidget *snapped)
{
	if ((snapped->mode == SNAPPED_EXPLICIT_HIDE) ||
	    (snapped->state == SNAPPED_HIDDEN_LEFT) ||
	    (snapped->state == SNAPPED_HIDDEN_RIGHT))
		return;
	
	/* check if there's already a timeout set, and delete it if 
	 * there was */
	if (snapped->leave_notify_timer_tag != 0)
		gtk_timeout_remove (snapped->leave_notify_timer_tag);
	
	/* set up our delay for popup. */
	snapped->leave_notify_timer_tag =
		gtk_timeout_add (pw_minimize_delay,
				 snapped_widget_pop_down, snapped);
}
	
static int
snapped_leave_notify(SnappedWidget *snapped,
		     GdkEventCrossing *event,
		     gpointer data)
{
	if (event->detail == GDK_NOTIFY_INFERIOR)
		return FALSE;

	snapped_widget_queue_pop_down(snapped);

	return FALSE;
}

static void
snapped_widget_set_hidebuttons(SnappedWidget *snapped)
{
	if(PANEL_WIDGET(snapped->panel)->orient == PANEL_HORIZONTAL) {
		gtk_widget_hide(snapped->hidebutton_n);
		gtk_widget_show(snapped->hidebutton_e);
		gtk_widget_show(snapped->hidebutton_w);
		gtk_widget_hide(snapped->hidebutton_s);
	} else {
		gtk_widget_show(snapped->hidebutton_n);
		gtk_widget_hide(snapped->hidebutton_e);
		gtk_widget_hide(snapped->hidebutton_w);
		gtk_widget_show(snapped->hidebutton_s);
	}
}

static GtkWidget *
make_hidebutton(SnappedWidget *snapped,
		char *pixmaparrow,
		GtkSignalFunc hidefunc)
{
	GtkWidget *w;
	GtkWidget *pixmap;
	char *pixmap_name;

	w=gtk_button_new();
	GTK_WIDGET_UNSET_FLAGS(w,GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

	pixmap_name=gnome_unconditional_pixmap_file(pixmaparrow);
	pixmap = gnome_pixmap_new_from_file(pixmap_name);
	g_free(pixmap_name);
	gtk_widget_show(pixmap);

	gtk_container_add(GTK_CONTAINER(w),pixmap);
	gtk_signal_connect(GTK_OBJECT(w), "clicked",
			   GTK_SIGNAL_FUNC(hidefunc),
			   snapped);
	return w;
}

static int
snapped_widget_destroy(GtkWidget *w, gpointer data)
{
	return FALSE;
}

static void
snapped_widget_configure_event (SnappedWidget *snapped,
				GdkEventConfigure *event,
				gpointer data)
{
	snapped_widget_set_position(snapped);
	if(snapped->panel)
		panel_widget_send_move(PANEL_WIDGET(snapped->panel));
}

static void
snapped_widget_init (SnappedWidget *snapped)
{
	/*if we set the icewm hints it will have to be changed to TOPLEVEL*/
	GTK_WINDOW(snapped)->type = GTK_WINDOW_POPUP;
	GTK_WINDOW(snapped)->allow_shrink = TRUE;
	GTK_WINDOW(snapped)->allow_grow = TRUE;
	GTK_WINDOW(snapped)->auto_shrink = TRUE;

	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(snapped),
			      gtk_widget_get_events(GTK_WIDGET(snapped)) |
			      GDK_BUTTON_RELEASE_MASK);

	snapped->table = gtk_table_new(3,3,FALSE);
	gtk_container_add(GTK_CONTAINER(snapped),snapped->table);
	gtk_widget_show(snapped->table);

	/*we add all the hide buttons to the table here*/
	/*EAST*/
	snapped->hidebutton_e =
		make_hidebutton(snapped,
				"panel-arrow-left.xpm",
				GTK_SIGNAL_FUNC(snapped_show_hide_right));
	gtk_table_attach(GTK_TABLE(snapped->table),snapped->hidebutton_e,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	/*NORTH*/
	snapped->hidebutton_n =
		make_hidebutton(snapped,
				"panel-arrow-up.xpm",
				GTK_SIGNAL_FUNC(snapped_show_hide_right));
	gtk_table_attach(GTK_TABLE(snapped->table),snapped->hidebutton_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	/*WEST*/
	snapped->hidebutton_w =
		make_hidebutton(snapped,
				"panel-arrow-right.xpm",
				GTK_SIGNAL_FUNC(snapped_show_hide_left));
	gtk_table_attach(GTK_TABLE(snapped->table),snapped->hidebutton_w,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	/*SOUTH*/
	snapped->hidebutton_s =
		make_hidebutton(snapped,
				"panel-arrow-down.xpm",
				GTK_SIGNAL_FUNC(snapped_show_hide_left));
	gtk_table_attach(GTK_TABLE(snapped->table),snapped->hidebutton_s,
			 1,2,2,3,GTK_FILL,GTK_FILL,0,0);

	gtk_signal_connect(GTK_OBJECT(snapped), "configure_event",
			   GTK_SIGNAL_FUNC(snapped_widget_configure_event),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(snapped), "enter_notify_event",
			   GTK_SIGNAL_FUNC(snapped_enter_notify),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(snapped), "leave_notify_event",
			   GTK_SIGNAL_FUNC(snapped_leave_notify),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(snapped),
			   "destroy",
			   GTK_SIGNAL_FUNC(snapped_widget_destroy),
			   NULL);
	snapped->pos = SNAPPED_BOTTOM;
	snapped->mode = SNAPPED_EXPLICIT_HIDE;
	snapped->state = SNAPPED_SHOWN;
}



GtkWidget*
snapped_widget_new (SnappedPos pos,
		    SnappedMode mode,
		    SnappedState state,
		    PanelBackType back_type,
		    char *back_pixmap,
		    int fit_pixmap_bg,
		    GdkColor *back_color)
{
	SnappedWidget *snapped;
	PanelOrientation orient;

	snapped = gtk_type_new(snapped_widget_get_type());


	if(pos == SNAPPED_TOP ||
	   pos == SNAPPED_BOTTOM)
		orient = PANEL_HORIZONTAL;
	else
		orient = PANEL_VERTICAL;

	snapped->panel = panel_widget_new(FALSE,
					  orient,
					  back_type,
					  back_pixmap,
					  fit_pixmap_bg,
					  back_color);
	gtk_object_set_data(GTK_OBJECT(snapped->panel),PANEL_PARENT,
			    snapped);
	PANEL_WIDGET(snapped->panel)->drop_widget = GTK_WIDGET(snapped);

	gtk_widget_show(snapped->panel);
	gtk_table_attach(GTK_TABLE(snapped->table),snapped->panel,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	snapped->pos = pos;
	snapped->mode = mode;
	if(state != SNAPPED_MOVING)
		snapped->state = state;

	snapped->autohide_inhibit = FALSE;
	
	/*sanity check ... this is a case which should never happen*/
	if(snapped->mode == SNAPPED_EXPLICIT_HIDE &&
	   snapped->state == SNAPPED_HIDDEN)
		snapped->state = SNAPPED_SHOWN;

	snapped_widget_set_hidebuttons(snapped);

	snapped_widget_set_size(snapped);
	snapped_widget_set_position(snapped);

	if(snapped->mode == SNAPPED_AUTO_HIDE)
		snapped_widget_queue_pop_down(snapped);

	return GTK_WIDGET(snapped);
}

void
snapped_widget_change_params(SnappedWidget *snapped,
			     SnappedPos pos,
			     SnappedMode mode,
			     SnappedState state,
			     PanelBackType back_type,
			     char *pixmap_name,
			     int fit_pixmap_bg,
			     GdkColor *back_color)
{
	PanelOrientation orient;
	PanelOrientation oldorient;
	SnappedPos oldpos;
	SnappedState oldstate;
	
	g_return_if_fail(snapped);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(snapped)));

	oldpos = snapped->pos;
	snapped->pos = pos;
	oldstate = snapped->state;
	snapped->state = state;
	snapped->mode = mode;

	if(snapped->pos == SNAPPED_TOP ||
	   snapped->pos == SNAPPED_BOTTOM)
		orient = PANEL_HORIZONTAL;
	else
		orient = PANEL_VERTICAL;

	if(oldpos == SNAPPED_TOP ||
	   oldpos == SNAPPED_BOTTOM)
		oldorient = PANEL_HORIZONTAL;
	else
		oldorient = PANEL_VERTICAL;

	if(oldorient != orient) {
		int w,h,t;
		GList *list;
		PanelWidget *panel = PANEL_WIDGET(snapped->panel);
		gdk_window_get_size(GTK_WIDGET(snapped)->window,&w,&h);
		t = h>w?w:h;
		resize_window(GTK_WIDGET(snapped),t,t);
		
		for(list = panel->applet_list;
		    list != NULL;
		    list = g_list_next(list)) {
			AppletData *ad = list->data;
			gtk_fixed_move(GTK_FIXED(panel->fixed),ad->applet,0,0);
		}
	}

	panel_widget_change_params(PANEL_WIDGET(snapped->panel),
				   orient,
				   back_type,
				   pixmap_name,
				   fit_pixmap_bg,
				   back_color);

	snapped_widget_set_size(snapped);
	snapped_widget_set_position(snapped);
	snapped_widget_set_hidebuttons(snapped);

	if(snapped->mode == SNAPPED_EXPLICIT_HIDE &&
	   snapped->state == SNAPPED_HIDDEN)
		snapped->state = SNAPPED_SHOWN;

	if(oldpos != snapped->pos)
	   	gtk_signal_emit(GTK_OBJECT(snapped),
	   			snapped_widget_signals[POS_CHANGE_SIGNAL],
	   			snapped->pos);
	if(oldstate != snapped->state)
	   	gtk_signal_emit(GTK_OBJECT(snapped),
	   			snapped_widget_signals[STATE_CHANGE_SIGNAL],
	   			snapped->state);

	if(snapped->mode == SNAPPED_AUTO_HIDE)
		snapped_widget_queue_pop_down(snapped);
}


void
snapped_widget_change_pos(SnappedWidget *snapped,
			  SnappedPos pos)
{
	PanelWidget *panel = PANEL_WIDGET(snapped->panel);
	snapped_widget_change_params(snapped,
				     pos,
				     snapped->mode,
				     snapped->state,
				     panel->back_type,
				     panel->back_pixmap,
				     panel->fit_pixmap_bg,
				     &panel->back_color);
}

void
snapped_widget_enable_buttons(SnappedWidget *snapped)
{
	gtk_widget_set_sensitive(snapped->hidebutton_n,TRUE);
	gtk_widget_set_sensitive(snapped->hidebutton_e,TRUE);
	gtk_widget_set_sensitive(snapped->hidebutton_w,TRUE);
	gtk_widget_set_sensitive(snapped->hidebutton_s,TRUE);
}


void
snapped_widget_disable_buttons(SnappedWidget *snapped)
{
	gtk_widget_set_sensitive(snapped->hidebutton_n,FALSE);
	gtk_widget_set_sensitive(snapped->hidebutton_e,FALSE);
	gtk_widget_set_sensitive(snapped->hidebutton_w,FALSE);
	gtk_widget_set_sensitive(snapped->hidebutton_s,FALSE);
}
