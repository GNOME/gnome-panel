/* Gnome panel: corner widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#include <math.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include "panel-widget.h"
#include "corner-widget.h"
#include "panel-util.h"
#include "gdkextra.h"

extern int panel_applet_in_drag;

static void corner_widget_class_init	(CornerWidgetClass *klass);
static void corner_widget_init		(CornerWidget      *corner);
static void corner_widget_size_request	(GtkWidget         *widget,
					 GtkRequisition    *requisition);
static void corner_widget_size_allocate	(GtkWidget         *widget,
					 GtkAllocation     *allocation);

static GtkWindowClass *parent_class = NULL;

extern GdkCursor *fleur_cursor;

/*global settings*/
extern int pw_explicit_step;
extern int pw_drawer_step;
extern int pw_auto_step;
extern int pw_minimized_size;
extern int pw_minimize_delay;
extern int pw_disable_animations;
extern PanelMovementType pw_movement_type;

/************************
 widget core
 ************************/

guint
corner_widget_get_type ()
{
	static guint corner_widget_type = 0;

	if (!corner_widget_type) {
		GtkTypeInfo corner_widget_info = {
			"CornerWidget",
			sizeof (CornerWidget),
			sizeof (CornerWidgetClass),
			(GtkClassInitFunc) corner_widget_class_init,
			(GtkObjectInitFunc) corner_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		corner_widget_type = gtk_type_unique (gtk_window_get_type (),
						       &corner_widget_info);
	}

	return corner_widget_type;
}

enum {
	POS_CHANGE_SIGNAL,
	STATE_CHANGE_SIGNAL,
	LAST_SIGNAL
};

static int corner_widget_signals[LAST_SIGNAL] = {0,0};

/*int is used for enums anyhow*/
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
corner_widget_realize(GtkWidget *w)
{
  GTK_WIDGET_CLASS(parent_class)->realize(w);
  
  gnome_win_hints_init();
  if (gnome_win_hints_wm_exists())
    {
      gnome_win_hints_set_hints(w,
				WIN_HINTS_SKIP_FOCUS |
				WIN_HINTS_SKIP_WINLIST |
				WIN_HINTS_SKIP_TASKBAR);
      gnome_win_hints_set_state(w,
				WIN_STATE_STICKY |
				WIN_STATE_FIXED_POSITION);
      gnome_win_hints_set_layer(w, WIN_LAYER_DOCK);
      gnome_win_hints_set_expanded_size(w, 0, 0, 0, 0);
      gdk_window_set_decorations(w->window, 0);
    }
}

static void
corner_widget_class_init (CornerWidgetClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass*) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;

        parent_class = gtk_type_class (gtk_window_get_type ());
        corner_widget_signals[POS_CHANGE_SIGNAL] =
		gtk_signal_new("pos_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(CornerWidgetClass,
			       			 pos_change),
			       marshal_signal_int,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);
	corner_widget_signals[STATE_CHANGE_SIGNAL] =
		gtk_signal_new("state_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(CornerWidgetClass,
			       			 state_change),
			       marshal_signal_int,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_ENUM);

	gtk_object_class_add_signals(object_class,corner_widget_signals,
				     LAST_SIGNAL);

	class->pos_change = NULL;
	class->state_change = NULL;
	
	widget_class->size_request = corner_widget_size_request;
	widget_class->size_allocate = corner_widget_size_allocate;
	widget_class->realize = corner_widget_realize;
}

/*if this is true the size request will request a 48x48 cube, this is used
  during orientation changes to make no flicker*/
static int corner_widget_request_cube = FALSE;
static void
corner_widget_size_request(GtkWidget *widget,
			    GtkRequisition *requisition)
{
	CornerWidget *corner = CORNER_WIDGET(widget);
	if(corner_widget_request_cube) {
		requisition->width = 48;
		requisition->height = 48;
		corner_widget_request_cube = FALSE;
		return;
	}

	gtk_widget_size_request (corner->table, &corner->table->requisition);
	
	requisition->width = corner->table->requisition.width;
	requisition->height = corner->table->requisition.height;
}

static void
corner_widget_get_pos(CornerWidget *corner, gint16 *x, gint16 *y, int width, int height)
{
	PanelWidget *panel = PANEL_WIDGET(corner->panel);
	
	switch(corner->pos) {
	case CORNER_NE:
		if(panel->orient == PANEL_HORIZONTAL) {
			*y = 0;
			if(corner->state == CORNER_HIDDEN)
				*x = gdk_screen_width() -
					corner->hidebutton_w->requisition.width;
			else /*shown*/
				*x = gdk_screen_width() - width;
		} else { /*vertical*/
			*x = gdk_screen_width() - width;
			if(corner->state == CORNER_HIDDEN)
				*y = corner->hidebutton_s->requisition.height -
					height;
			else /*shown*/
				*y = 0;
		}
		break;
	case CORNER_SE:
		if(panel->orient == PANEL_HORIZONTAL) {
			*y = gdk_screen_height() - height;
			if(corner->state == CORNER_HIDDEN)
				*x = gdk_screen_width() -
					corner->hidebutton_w->requisition.width;
			else /*shown*/
				*x = gdk_screen_width() - width;
		} else { /*vertical*/
			*x = gdk_screen_width() - width;
			if(corner->state == CORNER_HIDDEN)
				*y = gdk_screen_height() -
					corner->hidebutton_n->requisition.height;
			else /*shown*/
				*y = gdk_screen_height() - height;
		}
		break;
	case CORNER_SW:
		if(panel->orient == PANEL_HORIZONTAL) {
			*y = gdk_screen_height() - height;
			if(corner->state == CORNER_HIDDEN)
				*x = corner->hidebutton_e->requisition.width -
					width;
			else /*shown*/
				*x = 0;
		} else { /*vertical*/
			*x = 0;
			if(corner->state == CORNER_HIDDEN)
				*y = gdk_screen_height() -
					corner->hidebutton_n->requisition.height;
			else /*shown*/
				*y = gdk_screen_height() - height;
		}
		break;
	case CORNER_NW:
		if(panel->orient == PANEL_HORIZONTAL) {
			*y = 0;
			if(corner->state == CORNER_HIDDEN)
				*x = corner->hidebutton_e->requisition.width -
					width;
			else /*shown*/
				*x = 0;
		} else { /*vertical*/
			*x = 0;
			if(corner->state == CORNER_HIDDEN)
				*y = corner->hidebutton_s->requisition.height -
					height;
			else /*shown*/
				*y = 0;
		}
		break;
	}
}

static void
corner_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	CornerWidget *corner = CORNER_WIDGET(widget);
	GtkAllocation challoc;

	/*we actually want to ignore the size_reqeusts since they are sometimes
	  a cube for the flicker prevention*/
	gtk_widget_size_request (corner->table, &corner->table->requisition);
	
	allocation->width = corner->table->requisition.width;
	allocation->height = corner->table->requisition.height;

	corner_widget_get_pos(corner,
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
	else
		gtk_widget_set_uposition(widget,allocation->x,allocation->y);

	challoc.x = challoc.y = 0;
	challoc.width = allocation->width;
	challoc.height = allocation->height;
	gtk_widget_size_allocate(corner->table,&challoc);
}

static void
corner_widget_set_initial_pos(CornerWidget *corner)
{
	gint16 x,y;
	corner_widget_get_pos(corner, &x, &y, 48, 48);
	gtk_widget_set_uposition(GTK_WIDGET(corner),x,y);
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
move_horiz(CornerWidget *corner, int src_x, int dest_x, int step)
{
	int x, y;
	GtkWidget *w = GTK_WIDGET(corner);

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
move_vert(CornerWidget *corner, int src_y, int dest_y, int step)
{
	int x, y;

	GtkWidget *w = GTK_WIDGET(corner);

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

static void
corner_widget_pop_show(CornerWidget *corner, int fromright)
{
	int width, height;

	if ((corner->state == CORNER_MOVING) ||
	    (corner->state == CORNER_SHOWN))
		return;

	corner->state = CORNER_MOVING;

	width   = GTK_WIDGET(corner)->allocation.width;
	height  = GTK_WIDGET(corner)->allocation.height;

	if(PANEL_WIDGET(corner->panel)->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(corner, -width +
				   corner->hidebutton_w->allocation.width, 0,
				   pw_explicit_step);
		else
			move_horiz(corner, gdk_screen_width() -
				   corner->hidebutton_e->allocation.width, 
				   gdk_screen_width() - width,
				   pw_explicit_step);
	} else {
		if(fromright)
			move_vert(corner, -height +
				  corner->hidebutton_s->allocation.height, 0,
				  pw_explicit_step);
		else
			move_vert(corner, gdk_screen_height() -
				  corner->hidebutton_n->allocation.height, 
				  gdk_screen_height() - height,
				  pw_explicit_step);
	}

	corner->state = CORNER_SHOWN;

	gtk_signal_emit(GTK_OBJECT(corner),
			corner_widget_signals[STATE_CHANGE_SIGNAL],
			CORNER_SHOWN);
}

static void
corner_widget_pop_hide(CornerWidget *corner, int fromright)
{
	int width, height;

	if((corner->state != CORNER_SHOWN))
		return;
	

	gtk_signal_emit(GTK_OBJECT(corner),
			corner_widget_signals[STATE_CHANGE_SIGNAL],
			CORNER_HIDDEN);

	corner->state = CORNER_MOVING;

	width   = GTK_WIDGET(corner)->allocation.width;
	height  = GTK_WIDGET(corner)->allocation.height;

	if(PANEL_WIDGET(corner->panel)->orient == PANEL_HORIZONTAL) {
		if(fromright)
			move_horiz(corner, 0, -width +
				   corner->hidebutton_w->allocation.width,
				   pw_explicit_step);
		else
			move_horiz(corner, gdk_screen_width() - width,
				   gdk_screen_width() -
				   corner->hidebutton_e->allocation.width,
				   pw_explicit_step);
	} else {
		if(fromright)
			move_vert(corner, 0, -height +
				  corner->hidebutton_s->allocation.height,
				  pw_explicit_step);
		else
			move_vert(corner, gdk_screen_height() - height,
				  gdk_screen_height() -
				  corner->hidebutton_n->allocation.height,
				  pw_explicit_step);
	}

	corner->state = CORNER_HIDDEN;
}

static int
is_west(CornerWidget *corner)
{
	if(corner->pos == CORNER_NW ||
	   corner->pos == CORNER_SW)
		return TRUE;
	return FALSE;
}

static int
is_north(CornerWidget *corner)
{
	if(corner->pos == CORNER_NE ||
	   corner->pos == CORNER_NW)
		return TRUE;
	return FALSE;
}

static int
is_right(CornerWidget *corner)
{
	PanelWidget *panel = PANEL_WIDGET(corner->panel);
	if((panel->orient == PANEL_HORIZONTAL && !is_west(corner)) ||
	   (panel->orient == PANEL_VERTICAL && !is_north(corner)))
		return TRUE;
	return FALSE;
}

static void
jump_to_opposite(CornerWidget *corner)
{
	CornerPos newpos = CORNER_NW;
	PanelWidget *panel = PANEL_WIDGET(corner->panel);
	
	switch(corner->pos) {
	case CORNER_NE:
		if(panel->orient == PANEL_HORIZONTAL)
			newpos = CORNER_NW;
		else /*vertical*/
			newpos = CORNER_SE;
		break;
	case CORNER_SE:
		if(panel->orient == PANEL_HORIZONTAL)
			newpos = CORNER_SW;
		else /*vertical*/
			newpos = CORNER_NE;
		break;
	case CORNER_SW:
		if(panel->orient == PANEL_HORIZONTAL)
			newpos = CORNER_SE;
		else /*vertical*/
			newpos = CORNER_NW;
		break;
	case CORNER_NW:
		if(panel->orient == PANEL_HORIZONTAL)
			newpos = CORNER_NE;
		else /*vertical*/
			newpos = CORNER_SW;
		break;
	}
	corner_widget_change_pos_orient(corner,newpos,panel->orient);
}

static int
corner_show_hide_right(GtkWidget *widget, gpointer data)
{
	CornerWidget *corner = data;
	gtk_widget_set_state(widget,GTK_STATE_NORMAL);
	gtk_widget_queue_draw(widget);
	if(corner->state == CORNER_MOVING) 
		return FALSE;
	else if(corner->state == CORNER_SHOWN) {
		if(!is_right(corner))
			corner_widget_pop_hide(corner,TRUE);
		else
			jump_to_opposite(corner);
	} else {
		if(is_right(corner))
			corner_widget_pop_show(corner,FALSE);
		else
			jump_to_opposite(corner);
	}
	return FALSE;
}

static int
corner_show_hide_left(GtkWidget *widget, gpointer data)
{
	CornerWidget *corner = data;
	gtk_widget_set_state(widget,GTK_STATE_NORMAL);
	gtk_widget_queue_draw(widget);
	if(corner->state == CORNER_MOVING) 
		return FALSE;
	else if(corner->state == CORNER_SHOWN) {
		if(is_right(corner))
			corner_widget_pop_hide(corner,FALSE);
		else
			jump_to_opposite(corner);
	} else {
		if(!is_right(corner))
			corner_widget_pop_show(corner,TRUE);
		else
			jump_to_opposite(corner);
	}
	return FALSE;
}

static void
corner_enter_notify(CornerWidget *corner,
		     GdkEventCrossing *event,
		     gpointer data)
{
  if (!gnome_win_hints_wm_exists())
    gdk_window_raise(GTK_WIDGET(corner)->window);
}

static void
corner_widget_set_hidebuttons(CornerWidget *corner)
{
	if(PANEL_WIDGET(corner->panel)->orient == PANEL_HORIZONTAL) {
		gtk_widget_hide(corner->hidebutton_n);
		gtk_widget_show(corner->hidebutton_e);
		gtk_widget_show(corner->hidebutton_w);
		gtk_widget_hide(corner->hidebutton_s);
	} else {
		gtk_widget_show(corner->hidebutton_n);
		gtk_widget_hide(corner->hidebutton_e);
		gtk_widget_hide(corner->hidebutton_w);
		gtk_widget_show(corner->hidebutton_s);
	}
}

static GtkWidget *
make_hidebutton(CornerWidget *corner,
		char *pixmaparrow,
		GtkSignalFunc hidefunc,
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

	pixmap_name=gnome_unconditional_pixmap_file(pixmaparrow);
	pixmap = gnome_pixmap_new_from_file(pixmap_name);
	g_free(pixmap_name);
	gtk_widget_show(pixmap);

	gtk_container_add(GTK_CONTAINER(w),pixmap);
	gtk_signal_connect(GTK_OBJECT(w), "clicked",
			   GTK_SIGNAL_FUNC(hidefunc),
			   corner);
	return w;
}

static void
corner_widget_init (CornerWidget *corner)
{
  /*if we set the gnomewm hints it will have to be changed to TOPLEVEL*/

  gnome_win_hints_init();
  if (gnome_win_hints_wm_exists())
    GTK_WINDOW(corner)->type = GTK_WINDOW_TOPLEVEL;
  else
    GTK_WINDOW(corner)->type = GTK_WINDOW_POPUP;
	GTK_WINDOW(corner)->allow_shrink = TRUE;
	GTK_WINDOW(corner)->allow_grow = TRUE;
	GTK_WINDOW(corner)->auto_shrink = TRUE;

	/*this makes the popup "pop down" once the button is released*/
	gtk_widget_set_events(GTK_WIDGET(corner),
			      gtk_widget_get_events(GTK_WIDGET(corner)) |
			      GDK_BUTTON_RELEASE_MASK);

	corner->table = gtk_table_new(3,3,FALSE);
	gtk_container_add(GTK_CONTAINER(corner),corner->table);
	gtk_widget_show(corner->table);

	/*we add all the hide buttons to the table here*/
	/*EAST*/
	corner->hidebutton_e =
		make_hidebutton(corner,
				"panel-arrow-left.png",
				GTK_SIGNAL_FUNC(corner_show_hide_right),
				TRUE);
	gtk_table_attach(GTK_TABLE(corner->table),corner->hidebutton_e,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	/*NORTH*/
	corner->hidebutton_n =
		make_hidebutton(corner,
				"panel-arrow-up.png",
				GTK_SIGNAL_FUNC(corner_show_hide_right),
				FALSE);
	gtk_table_attach(GTK_TABLE(corner->table),corner->hidebutton_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	/*WEST*/
	corner->hidebutton_w =
		make_hidebutton(corner,
				"panel-arrow-right.png",
				GTK_SIGNAL_FUNC(corner_show_hide_left),
				TRUE);
	gtk_table_attach(GTK_TABLE(corner->table),corner->hidebutton_w,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	/*SOUTH*/
	corner->hidebutton_s =
		make_hidebutton(corner,
				"panel-arrow-down.png",
				GTK_SIGNAL_FUNC(corner_show_hide_left),
				FALSE);
	gtk_table_attach(GTK_TABLE(corner->table),corner->hidebutton_s,
			 1,2,2,3,GTK_FILL,GTK_FILL,0,0);

	gtk_signal_connect(GTK_OBJECT(corner), "enter_notify_event",
			   GTK_SIGNAL_FUNC(corner_enter_notify),
			   NULL);
	corner->pos = CORNER_NE;
	corner->state = CORNER_SHOWN;
}



GtkWidget*
corner_widget_new (CornerPos pos,
		   PanelOrientation orient,
		   CornerState state,
		   PanelBackType back_type,
		   char *back_pixmap,
		   int fit_pixmap_bg,
		   GdkColor *back_color)
{
	CornerWidget *corner;
	GtkWidget *frame;

	corner = gtk_type_new(corner_widget_get_type());

	corner->panel = panel_widget_new(TRUE,
					 orient,
					 back_type,
					 back_pixmap,
					 fit_pixmap_bg,
					 back_color);
	gtk_object_set_data(GTK_OBJECT(corner->panel),PANEL_PARENT,
			    corner);
	PANEL_WIDGET(corner->panel)->drop_widget = GTK_WIDGET(corner);

	gtk_widget_show(corner->panel);

	frame = gtk_frame_new(NULL);
	gtk_widget_show(frame);
	gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(frame),corner->panel);

	gtk_table_attach(GTK_TABLE(corner->table),frame,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	corner->pos = pos;
	if(state != CORNER_MOVING)
		corner->state = state;

	corner_widget_set_hidebuttons(corner);

	corner_widget_set_initial_pos(corner);

	return GTK_WIDGET(corner);
}

void
corner_widget_change_params(CornerWidget *corner,
			    CornerPos pos,
			    PanelOrientation orient,
			    CornerState state,
			    PanelBackType back_type,
			    char *pixmap_name,
			    int fit_pixmap_bg,
			    GdkColor *back_color)
{
	CornerPos oldpos;
	CornerState oldstate;
	
	g_return_if_fail(corner);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(corner)));

	oldpos = corner->pos;
	corner->pos = pos;
	oldstate = corner->state;
	corner->state = state;

	/*avoid flicker during size_request*/
	if(PANEL_WIDGET(corner->panel)->orient != orient)
		corner_widget_request_cube = TRUE;
	
	panel_widget_change_params(PANEL_WIDGET(corner->panel),
				   orient,
				   back_type,
				   pixmap_name,
				   fit_pixmap_bg,
				   back_color);

	corner_widget_set_hidebuttons(corner);

	if(oldpos != corner->pos)
	   	gtk_signal_emit(GTK_OBJECT(corner),
	   			corner_widget_signals[POS_CHANGE_SIGNAL],
	   			corner->pos);
	if(oldstate != corner->state)
	   	gtk_signal_emit(GTK_OBJECT(corner),
	   			corner_widget_signals[STATE_CHANGE_SIGNAL],
	   			corner->state);
	
	gtk_widget_queue_resize(GTK_WIDGET(corner));
}


void
corner_widget_change_pos_orient(CornerWidget *corner,
				CornerPos pos,
				PanelOrientation orient)
{
	PanelWidget *panel = PANEL_WIDGET(corner->panel);
	corner_widget_change_params(corner,
				    pos,
				    orient,
				    corner->state,
				    panel->back_type,
				    panel->back_pixmap,
				    panel->fit_pixmap_bg,
				    &panel->back_color);
}

void
corner_widget_enable_buttons(CornerWidget *corner)
{
	gtk_widget_set_sensitive(corner->hidebutton_n,TRUE);
	gtk_widget_set_sensitive(corner->hidebutton_e,TRUE);
	gtk_widget_set_sensitive(corner->hidebutton_w,TRUE);
	gtk_widget_set_sensitive(corner->hidebutton_s,TRUE);
}


void
corner_widget_disable_buttons(CornerWidget *corner)
{
	gtk_widget_set_sensitive(corner->hidebutton_n,FALSE);
	gtk_widget_set_sensitive(corner->hidebutton_e,FALSE);
	gtk_widget_set_sensitive(corner->hidebutton_w,FALSE);
	gtk_widget_set_sensitive(corner->hidebutton_s,FALSE);
}
