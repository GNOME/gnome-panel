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
#include "panel_config_global.h"
#include "gdkextra.h"

extern int panel_applet_in_drag;

static void snapped_widget_class_init	(SnappedWidgetClass *klass);
static void snapped_widget_init		(SnappedWidget      *snapped);
static void snapped_widget_size_request	(GtkWidget          *widget,
					 GtkRequisition     *requisition);
static void snapped_widget_size_allocate(GtkWidget          *widget,
					 GtkAllocation      *allocation);
static void snapped_widget_set_hidebuttons(BasePWidget *basep);

static BasePWidgetClass *parent_class = NULL;

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

		snapped_widget_type = gtk_type_unique (basep_widget_get_type (),
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
snapped_widget_realize(GtkWidget *w)
{
	SnappedWidget *snapped = SNAPPED_WIDGET(w);
	GTK_WIDGET_CLASS(parent_class)->realize(w);

	gnome_win_hints_init();
	if (gnome_win_hints_wm_exists()) {
		gnome_win_hints_set_hints(w, 
					  WIN_HINTS_SKIP_FOCUS |
					  WIN_HINTS_SKIP_WINLIST |
					  WIN_HINTS_SKIP_TASKBAR);
		gnome_win_hints_set_state(w, 
					  WIN_STATE_STICKY |
					  WIN_STATE_FIXED_POSITION);
		if(snapped->mode == SNAPPED_AUTO_HIDE) {
			gnome_win_hints_set_layer(w, WIN_LAYER_ABOVE_DOCK);
		} else {
			gnome_win_hints_set_layer(w, WIN_LAYER_DOCK);
		}
		gnome_win_hints_set_expanded_size(w, 0, 0, 0, 0);
		gdk_window_set_decorations(w->window, 0);
	}    
}

static void
snapped_widget_class_init (SnappedWidgetClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass*) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;
	BasePWidgetClass *basep_class = (BasePWidgetClass*) class;

        parent_class = gtk_type_class (basep_widget_get_type ());
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
	
	basep_class->set_hidebuttons = snapped_widget_set_hidebuttons;
	
	widget_class->size_request = snapped_widget_size_request;
	widget_class->size_allocate = snapped_widget_size_allocate;
	widget_class->realize = snapped_widget_realize;
}

/*if this is true the size request will request a 48x48 cube, this is used
  during orientation changes to make no flicker*/
static int snapped_widget_request_cube = FALSE;
static void
snapped_widget_size_request(GtkWidget *widget,
			    GtkRequisition *requisition)
{
	SnappedWidget *snapped = SNAPPED_WIDGET(widget);
	BasePWidget *basep = BASEP_WIDGET(widget);

	if(snapped_widget_request_cube) {
		requisition->width = 48;
		requisition->height = 48;
		snapped_widget_request_cube = FALSE;
		return;
	}

	gtk_widget_size_request (basep->table, &basep->table->requisition);
	
	switch(snapped->pos) {
		case SNAPPED_BOTTOM:
		case SNAPPED_TOP:
			requisition->width = gdk_screen_width();
			requisition->height = basep->table->requisition.height;
			break;
		case SNAPPED_LEFT:
		case SNAPPED_RIGHT:
			requisition->height = gdk_screen_height();
			requisition->width = basep->table->requisition.width;
			break;
	}
}

static void
snapped_widget_get_pos(SnappedWidget *snapped, gint16 *x, gint16 *y,
		       int width, int height)
{
	int xcor = 0;
	int ycor = 0;
	int thick;
	PanelWidget *panel = PANEL_WIDGET(BASEP_WIDGET(snapped)->panel);
	
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
			       BASEP_WIDGET(snapped)->hidebutton_w->allocation.width;
		else if(snapped->state == SNAPPED_HIDDEN_RIGHT)
			xcor = gdk_screen_width() -
			       BASEP_WIDGET(snapped)->hidebutton_w->allocation.width;
	} else { /*vertical*/
		if(snapped->state == SNAPPED_HIDDEN_LEFT)
			xcor = - gdk_screen_height() +
			       BASEP_WIDGET(snapped)->hidebutton_s->allocation.height;
		else if(snapped->state == SNAPPED_HIDDEN_RIGHT)
			xcor = gdk_screen_height() -
			       BASEP_WIDGET(snapped)->hidebutton_n->allocation.height;
	}

	switch(snapped->pos) {
		case SNAPPED_TOP:
			*x = xcor;
			*y = -ycor;
			break;
		case SNAPPED_BOTTOM:
			*x = xcor;
			*y = gdk_screen_height() - thick + ycor;
			break;
		case SNAPPED_LEFT:
			*x = -ycor;
			*y = xcor;
			break;
		case SNAPPED_RIGHT:
			*x = gdk_screen_width() - thick + ycor;
			*y = xcor;
			break;
	}
}

static void
snapped_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	SnappedWidget *snapped = SNAPPED_WIDGET(widget);
	BasePWidget *basep = BASEP_WIDGET(widget);
	GtkAllocation challoc;

	/*get us a size request (we ignored the one on allocation because
	  we don't want to change our size, ever*/
	gtk_widget_size_request (basep->table, &basep->table->requisition);
	
	/*ignore the allocation we get, we want to be this large*/
	switch(snapped->pos) {
		case SNAPPED_BOTTOM:
		case SNAPPED_TOP:
			allocation->width = gdk_screen_width();
			allocation->height = basep->table->requisition.height;
			break;
		case SNAPPED_LEFT:
		case SNAPPED_RIGHT:
			allocation->height = gdk_screen_height();
			allocation->width = basep->table->requisition.width;
			break;
	}
	
	snapped_widget_get_pos(snapped,
			       &allocation->x,
			       &allocation->y,
			       allocation->width,
			       allocation->height);


	widget->allocation = *allocation;
	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (widget->window,
					allocation->x, 
					allocation->y,
					allocation->width, 
					allocation->height);

	challoc.x = challoc.y = 0;
	challoc.width = allocation->width;
	challoc.height = allocation->height;
	gtk_widget_size_allocate(basep->table,&challoc);
	
}

static void
snapped_widget_set_initial_pos(SnappedWidget *snapped)
{
	gint16 x,y;

	switch(snapped->pos) {
		case SNAPPED_BOTTOM:
		case SNAPPED_TOP:
			snapped_widget_get_pos(snapped,
					       &x,
					       &y,
					       gdk_screen_width(),
					       48);
			break;
		case SNAPPED_LEFT:
		case SNAPPED_RIGHT:
			snapped_widget_get_pos(snapped,
					       &x,
					       &y,
					       48,
					       gdk_screen_height());
			break;
	}
	gtk_widget_set_uposition(GTK_WIDGET(snapped),x,y);
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
	   panel_widget_is_cursor(PANEL_WIDGET(BASEP_WIDGET(snapped)->panel),0)) {
		snapped->leave_notify_timer_tag = 0;
		return FALSE;
	}

	/*we are moving, or have drawers open, so wait with the
	  pop_down*/
	if(panel_applet_in_drag ||
	   snapped->drawers_open>0)
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
	static const char *supinfo[] = {"panel", "collapse", NULL};

	if ((snapped->state == SNAPPED_MOVING) ||
	    (snapped->state == SNAPPED_SHOWN))
		return;

	gnome_triggers_vdo("", NULL, supinfo);

	snapped->state = SNAPPED_MOVING;

	width   = GTK_WIDGET(snapped)->allocation.width;
	height  = GTK_WIDGET(snapped)->allocation.height;

	if(PANEL_WIDGET(BASEP_WIDGET(snapped)->panel)->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(snapped, -width +
				   BASEP_WIDGET(snapped)->hidebutton_w->allocation.width, 0,
				   pw_explicit_step);
		else
			move_horiz(snapped, width -
				   BASEP_WIDGET(snapped)->hidebutton_e->allocation.width, 0,
				   pw_explicit_step);
	} else {
		if(fromright)
			move_vert(snapped, -height +
				  BASEP_WIDGET(snapped)->hidebutton_s->allocation.height, 0,
				  pw_explicit_step);
		else
			move_vert(snapped, height -
				  BASEP_WIDGET(snapped)->hidebutton_n->allocation.height, 0,
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
	static const char *supinfo[] = {"panel", "collapse", NULL};

	if((snapped->state != SNAPPED_SHOWN))
		return;
	
	gnome_triggers_vdo("", NULL, supinfo);

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

	if(PANEL_WIDGET(BASEP_WIDGET(snapped)->panel)->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(snapped, 0, -width +
				   BASEP_WIDGET(snapped)->hidebutton_w->allocation.width,
				   pw_explicit_step);
		else
			move_horiz(snapped, 0, width -
				   BASEP_WIDGET(snapped)->hidebutton_e->allocation.width,
				   pw_explicit_step);
	} else {
		if(fromright)
			move_vert(snapped, 0, -height +
				  BASEP_WIDGET(snapped)->hidebutton_s->allocation.height,
				  pw_explicit_step);
		else
			move_vert(snapped, 0, height -
				  BASEP_WIDGET(snapped)->hidebutton_n->allocation.height,
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
snapped_widget_set_hidebuttons(BasePWidget *basep)
{
	SnappedWidget *snapped = SNAPPED_WIDGET(basep);

	/*hidebuttons are disabled*/
	if(!basep->hidebuttons_enabled) {
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);
		/*in case the panel was hidden, show it, since otherwise
		  we wouldn't see it anymore*/
		if(snapped->state == SNAPPED_HIDDEN_RIGHT) {
			snapped_widget_pop_show(snapped,FALSE);
			snapped_widget_queue_pop_down(snapped);
		} else if(snapped->state == SNAPPED_HIDDEN_LEFT) {
			snapped_widget_pop_show(snapped,TRUE);
			snapped_widget_queue_pop_down(snapped);
		}
	/* horizontal and enabled */
	} else if(PANEL_WIDGET(basep->panel)->orient == PANEL_HORIZONTAL) {
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_show(basep->hidebutton_e);
		gtk_widget_show(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);
	} else { /*vertical*/
		gtk_widget_show(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_show(basep->hidebutton_s);
	}
}


static void
snapped_widget_init (SnappedWidget *snapped)
{
	gtk_signal_connect(GTK_OBJECT(snapped), "enter_notify_event",
			   GTK_SIGNAL_FUNC(snapped_enter_notify),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(snapped), "leave_notify_event",
			   GTK_SIGNAL_FUNC(snapped_leave_notify),
			   NULL);
	snapped->pos = SNAPPED_BOTTOM;
	snapped->mode = SNAPPED_EXPLICIT_HIDE;
	snapped->state = SNAPPED_SHOWN;

	snapped->leave_notify_timer_tag = 0;
	snapped->autohide_inhibit = FALSE;
	snapped->drawers_open = 0;
}

GtkWidget*
snapped_widget_new (SnappedPos pos,
		    SnappedMode mode,
		    SnappedState state,
		    int hidebuttons_enabled,
		    int hidebutton_pixmaps_enabled,
		    PanelBackType back_type,
		    char *back_pixmap,
		    int fit_pixmap_bg,
		    GdkColor *back_color)
{
	SnappedWidget *snapped;
	PanelOrientation orient;
	GtkWidget *frame;
	BasePWidget *basep;

	snapped = gtk_type_new(snapped_widget_get_type());

	basep = BASEP_WIDGET(snapped);

	if(pos == SNAPPED_TOP ||
	   pos == SNAPPED_BOTTOM)
		orient = PANEL_HORIZONTAL;
	else
		orient = PANEL_VERTICAL;
	
	basep_widget_construct(basep,
			       FALSE,
			       FALSE,
			       orient,
			       hidebuttons_enabled,
			       hidebutton_pixmaps_enabled,
			       back_type,
			       back_pixmap,
			       fit_pixmap_bg,
			       back_color);
	/*EAST*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_e),"clicked",
			   GTK_SIGNAL_FUNC(snapped_show_hide_right),
			   snapped);
	/*NORTH*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_n),"clicked",
			   GTK_SIGNAL_FUNC(snapped_show_hide_right),
			   snapped);
	/*WEST*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_w),"clicked",
			   GTK_SIGNAL_FUNC(snapped_show_hide_left),
			   snapped);
	/*SOUTH*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_s),"clicked",
			   GTK_SIGNAL_FUNC(snapped_show_hide_left),
			   snapped);

	snapped->pos = pos;
	snapped->mode = mode;
	if(state != SNAPPED_MOVING)
		snapped->state = state;

	snapped->autohide_inhibit = FALSE;
	
	/*sanity check ... this is a case which should never happen*/
	if(snapped->mode == SNAPPED_EXPLICIT_HIDE &&
	   snapped->state == SNAPPED_HIDDEN)
		snapped->state = SNAPPED_SHOWN;

	snapped_widget_set_initial_pos(snapped);

	if(snapped->mode == SNAPPED_AUTO_HIDE)
		snapped_widget_queue_pop_down(snapped);

	return GTK_WIDGET(snapped);
}

void
snapped_widget_change_params(SnappedWidget *snapped,
			     SnappedPos pos,
			     SnappedMode mode,
			     SnappedState state,
			     int hidebuttons_enabled,
			     int hidebutton_pixmaps_enabled,
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
	if(mode != snapped->mode) {
		snapped->mode = mode;
		if (gnome_win_hints_wm_exists()) {
			if(snapped->mode == SNAPPED_AUTO_HIDE) {
				gnome_win_hints_set_layer(GTK_WIDGET(snapped),
							  WIN_LAYER_ABOVE_DOCK);
			} else {
				gnome_win_hints_set_layer(GTK_WIDGET(snapped),
							  WIN_LAYER_DOCK);
			}
		}
	}

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

	/*avoid flicker during size_request*/
	if(oldorient != orient)
		snapped_widget_request_cube = TRUE;

	basep_widget_change_params(BASEP_WIDGET(snapped),
				   orient,
				   hidebuttons_enabled,
				   hidebutton_pixmaps_enabled,
				   back_type,
				   pixmap_name,
				   fit_pixmap_bg,
				   back_color);
				   
	gtk_widget_queue_resize(GTK_WIDGET(snapped));

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
	BasePWidget *basep = BASEP_WIDGET(snapped);
	PanelWidget *panel = PANEL_WIDGET(basep->panel);
	snapped_widget_change_params(snapped,
				     pos,
				     snapped->mode,
				     snapped->state,
				     basep->hidebuttons_enabled,
				     basep->hidebutton_pixmaps_enabled,
				     panel->back_type,
				     panel->back_pixmap,
				     panel->fit_pixmap_bg,
				     &panel->back_color);
}
