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
#include "panel_config_global.h"
#include "gdkextra.h"
#include "panel-include.h" 

/*there  can universally be only one applet being dragged since we assume
we only have one mouse :) */
extern int panel_applet_in_drag;

static void drawer_widget_class_init	(DrawerWidgetClass *klass);
static void drawer_widget_init		(DrawerWidget      *drawer);
static void drawer_widget_size_request	(GtkWidget          *widget,
					 GtkRequisition     *requisition);
static void drawer_widget_size_allocate	(GtkWidget          *widget,
					 GtkAllocation      *allocation);

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
drawer_widget_realize(GtkWidget *w)
{
	DrawerWidget *drawer = DRAWER_WIDGET(w);
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
		gnome_win_hints_set_layer(w, WIN_LAYER_DOCK);
		gnome_win_hints_set_expanded_size(w, 0, 0, 0, 0);
		gdk_window_set_decorations(w->window, 0);
	}
	set_frame_colors(PANEL_WIDGET(drawer->panel),
			 drawer->frame,
			 drawer->handle_n,
			 drawer->handle_e,
			 drawer->handle_w,
			 drawer->handle_s);
}

static void
drawer_widget_class_init (DrawerWidgetClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass*) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;

        parent_class = gtk_type_class (gtk_window_get_type ());
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
	
	widget_class->size_request = drawer_widget_size_request;
	widget_class->size_allocate = drawer_widget_size_allocate;
	widget_class->realize = drawer_widget_realize;
}

/*if this is set, the size request and size alloc calls are ignored*/
static int ignore_allocate = FALSE;

/*if this is true the size request will request a 48x48 cube, this is used
  during orientation changes to make no flicker*/
static int drawer_widget_request_cube = FALSE;
static void
drawer_widget_size_request(GtkWidget *widget,
			    GtkRequisition *requisition)
{
	DrawerWidget *drawer = DRAWER_WIDGET(widget);
	int w,h;
	if(ignore_allocate)
		return;
	if(drawer_widget_request_cube) {
		requisition->width = 48;
		requisition->height = 48;
		drawer_widget_request_cube = FALSE;
		return;
	}

	gtk_widget_size_request (drawer->table, &drawer->table->requisition);

	w = drawer->table->requisition.width;
	h = drawer->table->requisition.height;

	/* do a minimal 48 size*/
	if(PANEL_WIDGET(drawer->panel)->orient == PANEL_HORIZONTAL) {
		if(w<48) w=48;
	} else {
		if(h<48) h=48;
	}
	requisition->width = w;
	requisition->height = h;
}

static void
drawer_widget_get_pos(DrawerWidget *drawer, gint16 *x, gint16 *y,
		      int width, int height)
{
	PanelWidget *panel = PANEL_WIDGET(drawer->panel);

	if (panel->master_widget &&
	    GTK_WIDGET_REALIZED (panel->master_widget)) {
		int bx, by, bw, bh;
		int px, py, pw, ph;
		GtkWidget *ppanel; /*parent panel*/

		/*get the parent of the applet*/
		ppanel = panel->master_widget->parent;
		/*go the the toplevel panel widget*/
		while(ppanel->parent)
			ppanel = ppanel->parent;
		if (GTK_WIDGET_REALIZED (ppanel)) {
			gdk_window_get_origin (panel->master_widget->window, &bx, &by);
			if(GTK_WIDGET_NO_WINDOW(panel->master_widget)) {
				bx+=panel->master_widget->allocation.x;
				by+=panel->master_widget->allocation.y;
			}
			bw = panel->master_widget->allocation.width;
			bh = panel->master_widget->allocation.height;
			gdk_window_get_origin (ppanel->window, &px, &py);
			gdk_window_get_size (ppanel->window, &pw, &ph);

			switch(drawer->orient) {
			case ORIENT_UP:
				*x = bx+(bw-width)/2;
				*y = py - height;
				break;
			case ORIENT_DOWN:
				*x = bx+(bw-width)/2;
				*y = py + ph;
				break;
			case ORIENT_LEFT:
				*x = px - width;
				*y = by+(bh-height)/2;
				break;
			case ORIENT_RIGHT:
				*x = px + pw;
				*y = by+(bh-height)/2;
				break;
			}
			return;
		}
	}
	/*if we fail*/
	*x = *y = -3000;
}

static void
drawer_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	DrawerWidget *drawer = DRAWER_WIDGET(widget);
	GtkAllocation challoc;
	int w,h;

	if(ignore_allocate)
		return;

	/*we actually want to ignore the size_reqeusts since they are sometimes
	  a cube for the flicker prevention*/
	gtk_widget_size_request (drawer->table, &drawer->table->requisition);

	w = drawer->table->requisition.width;
	h = drawer->table->requisition.height;

	/* do a minimal 48 size*/
	if(PANEL_WIDGET(drawer->panel)->orient == PANEL_HORIZONTAL) {
		if(w<48) w=48;
	} else {
		if(h<48) h=48;
	}
	allocation->width = w;
	allocation->height = h;

	drawer_widget_get_pos(drawer,
			      &allocation->x,
			      &allocation->y,
			      allocation->width,
			      allocation->height);
	
	/*ugly optimisation*/
	if(memcmp(allocation,&widget->allocation,sizeof(GtkAllocation))==0)
		return;

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
	gtk_widget_size_allocate(drawer->table,&challoc);
}

static void
show_handle_pixmap(GtkWidget *handle, int show_pixmap, int show_button)
{
	GtkWidget *pixmap;

	pixmap = GTK_BIN(handle)->child;

	if (!handle) return;	
	if (show_button)
	        gtk_widget_show(handle);
	else 
	        gtk_widget_hide(handle);

	if (!pixmap) return;
	if (show_pixmap)
		gtk_widget_show(pixmap);
	else
		gtk_widget_hide(pixmap);
}

static void
drawer_hidebutton_set(DrawerWidget *drawer)
{
  int pixmap_enabled = drawer->hidebutton_pixmap_enabled;
  int button_enabled = drawer->hidebutton_enabled;
  show_handle_pixmap(drawer->handle_n, pixmap_enabled,
		     button_enabled);
  show_handle_pixmap(drawer->handle_s, pixmap_enabled,
		     button_enabled); 
  show_handle_pixmap(drawer->handle_w, pixmap_enabled,
		     button_enabled); 
  show_handle_pixmap(drawer->handle_e, pixmap_enabled,
		     button_enabled);
}


static void
drawer_widget_set_drop_zone(DrawerWidget *drawer)
{
	drawer_hidebutton_set(drawer);
	switch(drawer->orient) {
	case ORIENT_UP:
		gtk_widget_show(drawer->handle_n);
		gtk_widget_hide(drawer->handle_e);
		gtk_widget_hide(drawer->handle_w);
		gtk_widget_hide(drawer->handle_s);
		break;
	case ORIENT_DOWN:
		gtk_widget_hide(drawer->handle_n);
		gtk_widget_hide(drawer->handle_e);
		gtk_widget_hide(drawer->handle_w);
		gtk_widget_show(drawer->handle_s);
		break;
	case ORIENT_LEFT:
		gtk_widget_hide(drawer->handle_n);
		gtk_widget_show(drawer->handle_e);
		gtk_widget_hide(drawer->handle_w);
		gtk_widget_hide(drawer->handle_s);
		break;
	case ORIENT_RIGHT:
		gtk_widget_hide(drawer->handle_n);
		gtk_widget_hide(drawer->handle_e);
		gtk_widget_show(drawer->handle_w);
		gtk_widget_hide(drawer->handle_s);
		break;
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
move_horiz_d(DrawerWidget *drawer, int x, int y, int w, int h,
	    int src_x, int dest_x, int step, int hide)
{
	int orig_x;
	int orig_w;
	GtkWidget *wid = GTK_WIDGET(drawer);

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

	move_resize_window(wid, dest_x, y,w,h);
}

static void
move_vert_d(DrawerWidget *drawer, int x, int y, int w, int h,
	    int src_y, int dest_y, int step, int hide)
{
	int orig_y;
	int orig_h;
	GtkWidget *wid = GTK_WIDGET(drawer);

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

	move_resize_window(wid, x, dest_y, w,h);
}

void
drawer_widget_open_drawer(DrawerWidget *drawer)
{
	gint16 x=0,y=0;
	int width, height;

	if((drawer->state == DRAWER_SHOWN) ||
	   (drawer->state == DRAWER_MOVING))
		return;
	
	if(!GTK_WIDGET(drawer)->window) {
		gtk_widget_queue_resize(GTK_WIDGET(drawer));
		drawer->state = DRAWER_SHOWN;
		return;
	}

	width   = GTK_WIDGET(drawer)->allocation.width;
	height  = GTK_WIDGET(drawer)->allocation.height;
	drawer_widget_get_pos(drawer,&x,&y,width,height);

	drawer->state = DRAWER_MOVING;

	gdk_window_move(GTK_WIDGET(drawer)->window, -3000,-3000);
	ignore_allocate = TRUE;
	gtk_widget_show_now(GTK_WIDGET(drawer));
	ignore_allocate = FALSE;
	switch(drawer->orient) {
	case ORIENT_UP:
		move_vert_d(drawer,x,y,width,height, y+height, y,
			    pw_drawer_step,FALSE);
		break;
	case ORIENT_DOWN:
		move_vert_d(drawer,x,y,width,height, y-height, y,
			    pw_drawer_step,FALSE);
		break;
	case ORIENT_LEFT:
		move_horiz_d(drawer,x,y,width,height, x+width, x,
			     pw_drawer_step,FALSE);
		break;
	case ORIENT_RIGHT:
		move_horiz_d(drawer,x,y,width,height, x-width, x,
			     pw_drawer_step,FALSE);
		break;
	}

	drawer->state = DRAWER_SHOWN;

	gtk_signal_emit(GTK_OBJECT(drawer),
			drawer_widget_signals[STATE_CHANGE_SIGNAL],
			DRAWER_SHOWN);

	gtk_widget_queue_resize(GTK_WIDGET(drawer));
}

void
drawer_widget_close_drawer(DrawerWidget *drawer)
{
	gint16 x=0,y=0;
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

	width   = GTK_WIDGET(drawer)->allocation.width;
	height  = GTK_WIDGET(drawer)->allocation.height;
	drawer_widget_get_pos(drawer,&x,&y,width,height);

	drawer->state = DRAWER_MOVING;

	if(!pw_disable_animations) {
		switch(drawer->orient) {
		case ORIENT_UP:
			move_vert_d(drawer,x,y,width,height, y, y+height,
				    pw_drawer_step, TRUE);
			break;
		case ORIENT_DOWN:
			move_vert_d(drawer,x,y,width,height, y, y-height,
				    pw_drawer_step, TRUE);
			break;
		case ORIENT_LEFT:
			move_horiz_d(drawer,x,y,width,height, x, x+width,
				     pw_drawer_step, TRUE);
			break;
		case ORIENT_RIGHT:
			move_horiz_d(drawer,x,y,width,height, x, x-width,
				     pw_drawer_step, TRUE);
			break;
		}
	}

	gtk_widget_hide(GTK_WIDGET(drawer));

	drawer->state = DRAWER_HIDDEN;
}


static int
drawer_enter_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	if (!gnome_win_hints_wm_exists() &&
	    global_config.autoraise)
		gdk_window_raise(widget->window);
	return FALSE;
}


static GtkWidget *
make_handle(char *pixmaphandle, int wi, int he)
{
	GtkWidget *w;
	GtkWidget *pixmap;
	char *pixmap_name;

	pixmap_name=gnome_unconditional_pixmap_file(pixmaphandle);
	pixmap = gnome_pixmap_new_from_file(pixmap_name);
	gtk_widget_show(pixmap);
	g_free(pixmap_name);

	w = gtk_button_new();
	GTK_WIDGET_UNSET_FLAGS(w,GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
	gtk_widget_show(w);

	gtk_container_add(GTK_CONTAINER(w), pixmap);
	
	gtk_widget_set_usize(w,wi,he);

	return w;
}



static void
drawer_handle_click(GtkWidget *widget, gpointer data)
{
	Drawer *drawer = gtk_object_get_data(GTK_OBJECT(data),
					     DRAWER_PANEL_KEY);
	DrawerWidget *drawerw = DRAWER_WIDGET(drawer->drawer);
	PanelWidget *parent = PANEL_WIDGET(drawer->button->parent);
	GtkWidget *panelw = gtk_object_get_data(GTK_OBJECT(parent),
						PANEL_PARENT);
	drawer_widget_close_drawer(data);
	if(IS_SNAPPED_WIDGET(panelw))
		SNAPPED_WIDGET(panelw)->drawers_open--;
}


static void
drawer_widget_init (DrawerWidget *drawer)
{
	gnome_win_hints_init();
	if (gnome_win_hints_wm_exists())
		GTK_WINDOW(drawer)->type = GTK_WINDOW_TOPLEVEL;
	else
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
	drawer->handle_e = make_handle("panel-arrow-right.png",0,40);
	gtk_table_attach(GTK_TABLE(drawer->table),drawer->handle_e,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	gtk_signal_connect(GTK_OBJECT(drawer->handle_e), "clicked",
			   GTK_SIGNAL_FUNC(drawer_handle_click), drawer);
	/*NORTH*/
	drawer->handle_n = make_handle("panel-arrow-down.png",40,0);
	gtk_table_attach(GTK_TABLE(drawer->table),drawer->handle_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	gtk_signal_connect(GTK_OBJECT(drawer->handle_n), "clicked",
			   GTK_SIGNAL_FUNC(drawer_handle_click), drawer);
	/*WEST*/
	drawer->handle_w = make_handle( "panel-arrow-left.png",0,40);
	gtk_table_attach(GTK_TABLE(drawer->table),drawer->handle_w,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	gtk_signal_connect(GTK_OBJECT(drawer->handle_w), "clicked",
			   GTK_SIGNAL_FUNC(drawer_handle_click), drawer);
	/*SOUTH*/
	drawer->handle_s = make_handle("panel-arrow-up.png",40,0);
	gtk_table_attach(GTK_TABLE(drawer->table),drawer->handle_s,
			 1,2,2,3,GTK_FILL,GTK_FILL,0,0);
	gtk_signal_connect(GTK_OBJECT(drawer->handle_s), "clicked",
			   GTK_SIGNAL_FUNC(drawer_handle_click), drawer);

	gtk_signal_connect(GTK_OBJECT(drawer), "enter_notify_event",
			   GTK_SIGNAL_FUNC(drawer_enter_notify),
			   NULL);
	drawer->hidebutton_enabled = TRUE;
	drawer->hidebutton_pixmap_enabled = TRUE;
	drawer->state = DRAWER_SHOWN;
}

static void
d_back_change(PanelWidget *panel,
	    PanelBackType type,
	    char *pixmap,
	    GdkColor *color,
	    DrawerWidget *drawer)
{
	if(type == PANEL_BACK_PIXMAP &&
	   drawer->panel->parent == drawer->frame) {
		gtk_widget_hide(drawer->frame);
		gtk_widget_ref(drawer->panel);
		gtk_container_remove(GTK_CONTAINER(drawer->frame),
				     drawer->panel);
		gtk_table_attach(GTK_TABLE(drawer->table),drawer->panel,
				 1,2,1,2,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 0,0);
		gtk_widget_unref(drawer->panel);
	} else if(type != PANEL_BACK_PIXMAP &&
		  drawer->panel->parent == drawer->table) {
		gtk_widget_ref(drawer->panel);
		gtk_container_remove(GTK_CONTAINER(drawer->table),
				     drawer->panel);
		gtk_container_add(GTK_CONTAINER(drawer->frame),
				  drawer->panel);
		gtk_widget_unref(drawer->panel);
		gtk_widget_show(drawer->frame);
	}

	set_frame_colors(panel,
			 drawer->frame,
			 drawer->handle_n,
			 drawer->handle_e,
			 drawer->handle_w,
			 drawer->handle_s);
}


 
GtkWidget*
drawer_widget_new (PanelOrientType orient,
		   DrawerState state,
		   PanelBackType back_type,
		   char *back_pixmap,
		   int fit_pixmap_bg,
		   GdkColor *back_color,
		   int hidebutton_pixmap_enabled,
		   int hidebutton_enabled)
{
	DrawerWidget *drawer;
	GtkWidget *frame;
	PanelOrientation porient;
	
	drawer = gtk_type_new(drawer_widget_get_type());

	switch(orient) {
	case ORIENT_UP: porient = PANEL_VERTICAL; break;
	case ORIENT_DOWN: porient = PANEL_VERTICAL; break;
	case ORIENT_LEFT: porient = PANEL_HORIZONTAL; break;
	case ORIENT_RIGHT: porient = PANEL_HORIZONTAL; break;
	default:
		porient = PANEL_HORIZONTAL;
	}

	drawer->panel = panel_widget_new(TRUE,
					 porient,
					 back_type,
					 back_pixmap,
					 fit_pixmap_bg,
					 back_color);
	gtk_signal_connect_after(GTK_OBJECT(drawer->panel), "back_change",
				 GTK_SIGNAL_FUNC(d_back_change),
				 drawer);
	gtk_object_set_data(GTK_OBJECT(drawer->panel),PANEL_PARENT,
			    drawer);
	PANEL_WIDGET(drawer->panel)->drop_widget = GTK_WIDGET(drawer);

	gtk_widget_show(drawer->panel);

	drawer->frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(drawer->frame),GTK_SHADOW_OUT);
	
	if(back_type != PANEL_BACK_PIXMAP) {
		gtk_container_add(GTK_CONTAINER(drawer->frame),drawer->panel);
		gtk_widget_show(drawer->frame);
	} else {
		gtk_table_attach(GTK_TABLE(drawer->table),drawer->panel,
				 1,2,1,2,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
				 0,0);
	}

	gtk_table_attach(GTK_TABLE(drawer->table),drawer->frame,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	drawer->state = state;
        drawer->hidebutton_enabled = hidebutton_enabled;
	drawer->hidebutton_pixmap_enabled = hidebutton_enabled;
	gtk_widget_set_uposition(GTK_WIDGET(drawer),-100,-100);
	drawer_widget_set_drop_zone(drawer);

	return GTK_WIDGET(drawer);
}

void
drawer_widget_change_params(DrawerWidget *drawer,
			    PanelOrientType orient,
			    DrawerState state,
			    PanelBackType back_type,
			    char *pixmap,
			    int fit_pixmap_bg,
			    GdkColor *back_color,
			    int hidebutton_pixmap_enabled,
			    int hidebutton_enabled)
{
	DrawerState oldstate;
	PanelOrientation porient;
	
	g_return_if_fail(drawer);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(drawer)));

	switch(orient) {
	case ORIENT_UP: porient = PANEL_VERTICAL; break;
	case ORIENT_DOWN: porient = PANEL_VERTICAL; break;
	case ORIENT_LEFT: porient = PANEL_HORIZONTAL; break;
	case ORIENT_RIGHT: porient = PANEL_HORIZONTAL; break;
	default: porient = PANEL_HORIZONTAL;
	}

	oldstate = drawer->state;

	drawer->state = state;

	drawer->orient = orient;

	/*avoid flicker during size_request*/
	if(PANEL_WIDGET(drawer->panel)->orient != porient)
		drawer_widget_request_cube = TRUE;

	panel_widget_change_params(PANEL_WIDGET(drawer->panel),
				   porient,
				   back_type,
				   pixmap,
				   fit_pixmap_bg,
				   back_color);
	
	if(oldstate != drawer->state)
	   	gtk_signal_emit(GTK_OBJECT(drawer),
	   			drawer_widget_signals[STATE_CHANGE_SIGNAL],
	   			drawer->state);

	drawer->hidebutton_enabled = hidebutton_enabled;
	drawer->hidebutton_pixmap_enabled = hidebutton_pixmap_enabled;
	drawer_widget_set_drop_zone(drawer);
}

void
drawer_widget_change_orient(DrawerWidget *drawer,
			    PanelOrientType orient)
{
	PanelWidget *panel = PANEL_WIDGET(drawer->panel);
	drawer_widget_change_params(drawer,
				    orient,
				    drawer->state,
				    panel->back_type,
				    panel->back_pixmap,
				    panel->fit_pixmap_bg,
				    &panel->back_color,
				    drawer->hidebutton_pixmap_enabled,
				    drawer->hidebutton_enabled); 
}

void
drawer_widget_restore_state(DrawerWidget *drawer)
{
	gtk_widget_queue_resize(GTK_WIDGET(drawer));
	gtk_widget_show(GTK_WIDGET(drawer));
}

