/* Gnome panel: basep widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Jacob Berkman
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
#include "foobar-widget.h"
#include "drawer-widget.h"
#include "border-widget.h"
#include "edge-widget.h"
#include "aligned-widget.h"
#include "xstuff.h"
#include "multiscreen-stuff.h"

extern gboolean panel_applet_in_drag;
extern GSList *panel_list;

extern int applets_to_sync;
extern int panels_to_sync;
extern int need_complete_save;

/*global settings*/
extern int pw_explicit_step;
extern int pw_drawer_step;
extern int pw_auto_step;
extern int pw_minimized_size;
extern int pw_minimize_delay;
extern int pw_maximize_delay;
extern gboolean pw_disable_animations;
extern PanelMovementType pw_movement_type;

extern GtkTooltips *panel_tooltips;

extern GlobalConfig global_config;

static void basep_widget_class_init	(BasePWidgetClass *klass);
static void basep_widget_init		(BasePWidget      *basep);

static void basep_pos_class_init (BasePPosClass *klass);
static void basep_pos_init (BasePPos *pos);
static gboolean basep_leave_notify (GtkWidget *widget, GdkEventCrossing *event);
static gboolean basep_enter_notify (GtkWidget *widget, GdkEventCrossing *event);
static void basep_style_set (GtkWidget *widget, GtkStyle *previous_style);

static void basep_widget_destroy (GtkObject *o);

static GtkWindowClass *basep_widget_parent_class = NULL;
static GtkObjectClass *basep_pos_parent_class = NULL;

/************************
 widget core
 ************************/

GtkType
basep_widget_get_type (void)
{
	static GtkType basep_widget_type = 0;

	if (!basep_widget_type) {
		GtkTypeInfo basep_widget_info = {
			"BasePWidget",
			sizeof (BasePWidget),
			sizeof (BasePWidgetClass),
			(GtkClassInitFunc) basep_widget_class_init,
			(GtkObjectInitFunc) basep_widget_init,
			NULL,
			NULL,
			NULL
		};

		basep_widget_type = gtk_type_unique (gtk_window_get_type (),
						     &basep_widget_info);
	}

	return basep_widget_type;
}

enum {
	/*TYPE_CHANGE_SIGNAL,*/
	MODE_CHANGE_SIGNAL,
	STATE_CHANGE_SIGNAL,
	SCREEN_CHANGE_SIGNAL,
	WIDGET_LAST_SIGNAL
};

static guint basep_widget_signals[WIDGET_LAST_SIGNAL] = { 0 };

static BasePPosClass *
basep_widget_get_pos_class (BasePWidget *basep) {
	BasePPosClass *klass;

	g_return_val_if_fail (IS_BASEP_WIDGET(basep), NULL);
	g_return_val_if_fail (IS_BASEP_POS(basep->pos), NULL);

	klass = BASEP_POS_CLASS(GTK_OBJECT(basep->pos)->klass);

	g_return_val_if_fail (IS_BASEP_POS_CLASS (klass), NULL);

	return klass;
}

static void
basep_widget_realize (GtkWidget *w)
{
	BasePWidget *basep = BASEP_WIDGET (w);
	BasePPosClass *klass;

	g_return_if_fail (IS_BASEP_WIDGET (basep));

	gtk_window_set_wmclass (GTK_WINDOW (basep),
				"panel_window", "Panel");

	GTK_WIDGET_CLASS (basep_widget_parent_class)->realize (w);

	basep_widget_update_winhints (basep);
	xstuff_set_no_group_and_no_input (w->window);

	set_frame_colors (PANEL_WIDGET (basep->panel),
			  basep->frame,
			  basep->hidebutton_n,
			  basep->hidebutton_e,
			  basep->hidebutton_w,
			  basep->hidebutton_s);

	
	klass = basep_widget_get_pos_class (basep);
	g_return_if_fail (klass);
	if (klass->realize != NULL)
		klass->realize (w);
}

static void
basep_widget_map (GtkWidget *w)
{
        BasePWidget *basep = BASEP_WIDGET (w);

        g_return_if_fail (IS_BASEP_WIDGET (basep));

        if (GTK_WIDGET_CLASS (basep_widget_parent_class)->map != NULL)
		GTK_WIDGET_CLASS (basep_widget_parent_class)->map (w);

        basep_widget_update_winhints (basep);
}

static void
basep_widget_size_request (GtkWidget *widget,
			   GtkRequisition *requisition)
{
	GtkRequisition chreq;

	BasePWidget *basep = BASEP_WIDGET(widget);
	BasePPosClass *klass = basep_widget_get_pos_class (basep);

	g_assert (klass);

	if (basep->request_cube) {
		requisition->width = requisition->height =
			PANEL_MINIMUM_WIDTH;
		basep->request_cube = FALSE;
		return;
	}

	gtk_widget_size_request (basep->ebox, &chreq);

	/* this typically only does stuff on edge panels */
	if (klass->get_size) {
		int w,h;
		w = chreq.width;
		h = chreq.height;
		klass->get_size(basep, &w, &h);
		chreq.width = w;
		chreq.height = h;
	}

	if (basep->state != BASEP_SHOWN) {
		int w,h;
		PanelOrientType hide_orient =
			klass->get_hide_orient (basep);
		w = chreq.width;
		h = chreq.height;
		klass->get_hide_size (basep, hide_orient, &w, &h);
		chreq.width = w;
		chreq.height = h;
	}

	requisition->width = chreq.width;
	requisition->height = chreq.height;
}

static void
basep_widget_size_allocate (GtkWidget *widget,
			    GtkAllocation *allocation)
{
	GtkAllocation challoc;
	GtkRequisition chreq;
	
	BasePWidget *basep = BASEP_WIDGET(widget);
	BasePPosClass *klass = basep_widget_get_pos_class (basep);
	
	/*we actually want to ignore the size_reqeusts since they
	  are sometimes a cube for the flicker prevention*/
#ifdef PANEL_DEBUG
	if (basep->state == BASEP_MOVING)
		g_warning ("size_allocate whilst moving");
#endif

	gtk_widget_size_request (basep->ebox, &chreq);

	if (klass->get_size) {
		int w,h;
		w = chreq.width;
		h = chreq.height;
		klass->get_size(basep, &w, &h);
		chreq.width = w;
		chreq.height = h;
	}
	if (klass->get_pos) {
		int x,y;
		klass->get_pos (basep, &x, &y,
				chreq.width,
				chreq.height);
		allocation->x = x;
		allocation->y = y;
	}

	allocation->width = challoc.width = chreq.width;
	allocation->height = challoc.height = chreq.height;
	challoc.x = challoc.y = 0;

	basep->shown_alloc = *allocation;

	if (basep->state != BASEP_SHOWN) {
		int w,h,x,y;
		PanelOrientType hide_orient = 
			klass->get_hide_orient (basep);

		w = allocation->width;
		h = allocation->height;
		klass->get_hide_size (basep, hide_orient, &w,&h);
		allocation->width = w;
		allocation->height = h;

		x = allocation->x;
		y = allocation->y;
		klass->get_hide_pos (basep, hide_orient,
				     &x, &y,
				     basep->shown_alloc.width,
				     basep->shown_alloc.height);
		allocation->x = x;
		allocation->y = y;

		basep_widget_get_position (basep, hide_orient,
					   &x, &y,
					   allocation->width,
					   allocation->height);
		challoc.x = x;
		challoc.y = y;
	}

	if (basep->keep_in_screen) {
		gint16 max;

		max = multiscreen_width (basep->screen) -
			allocation->width +
			multiscreen_x (basep->screen);

		if (allocation->x < multiscreen_x (basep->screen))
			allocation->x = multiscreen_x (basep->screen);
		else if (allocation->x > max)
			allocation->x = max;


		max = multiscreen_height (basep->screen) -
			allocation->height +
			multiscreen_y (basep->screen);

		if (allocation->y < multiscreen_y (basep->screen))
			allocation->y = multiscreen_y (basep->screen);
		else if (allocation->y > max)
			allocation->y = max;
				       
	}
	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED(widget)) {
		xstuff_set_pos_size (widget->window,
				     allocation->x, 
				     allocation->y,
				     allocation->width,
				     allocation->height);
	}

	gtk_widget_size_allocate (basep->ebox, &challoc);
}

static void
basep_widget_mode_change (BasePWidget *basep, BasePMode mode)
{
	if (IS_BORDER_WIDGET (basep))
		basep_border_queue_recalc (basep->screen);
}

static void
basep_widget_state_change (BasePWidget *basep, BasePState state)
{
	if (IS_BORDER_WIDGET (basep))
		basep_border_queue_recalc (basep->screen);
}

static void
basep_widget_real_screen_change (BasePWidget *basep, int screen)
{
	if (basep->screen != screen) {
		basep_border_queue_recalc (basep->screen);
		basep->screen = screen;
		/* this will queue border recalc in the new screen */
		gtk_widget_queue_resize (GTK_WIDGET (basep));
		panels_to_sync = TRUE;

		update_config_screen (basep);
	}
}

static void
basep_widget_class_init (BasePWidgetClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        basep_widget_parent_class = gtk_type_class (gtk_window_get_type ());

	/*basep_widget_signals[TYPE_CHANGE_SIGNAL] = 
		gtk_signal_new("type_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(BasePWidgetClass,
						 type_change),
			       gtk_marshal_NONE__ENUM,
			       GTK_TYPE_NONE,
			       1, GTK_TYPE_ENUM);*/

	basep_widget_signals[MODE_CHANGE_SIGNAL] = 
		gtk_signal_new("mode_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(BasePWidgetClass,
						 mode_change),
			       gtk_marshal_NONE__ENUM,
			       GTK_TYPE_NONE,
			       1, GTK_TYPE_ENUM);

	basep_widget_signals[STATE_CHANGE_SIGNAL] = 
		gtk_signal_new("state_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(BasePWidgetClass,
						 state_change),
			       gtk_marshal_NONE__ENUM,
			       GTK_TYPE_NONE,
			       1, GTK_TYPE_ENUM);

	basep_widget_signals[SCREEN_CHANGE_SIGNAL] = 
		gtk_signal_new("screen_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(BasePWidgetClass,
						 screen_change),
			       gtk_marshal_NONE__INT,
			       GTK_TYPE_NONE,
			       1,
			       GTK_TYPE_INT);

	gtk_object_class_add_signals(object_class, 
				     basep_widget_signals,
				     WIDGET_LAST_SIGNAL);

	klass->mode_change = basep_widget_mode_change;
	klass->state_change = basep_widget_state_change;
	klass->screen_change = basep_widget_real_screen_change;

	widget_class->size_request = basep_widget_size_request;
	widget_class->size_allocate = basep_widget_size_allocate;
	widget_class->realize = basep_widget_realize;
	widget_class->map = basep_widget_map;
	widget_class->enter_notify_event = basep_enter_notify;
	widget_class->leave_notify_event = basep_leave_notify;
	widget_class->style_set = basep_style_set;

	object_class->destroy = basep_widget_destroy;
}

/* pos core */
GtkType
basep_pos_get_type (void)
{
	static GtkType basep_pos_type = 0;
	
	if (!basep_pos_type) {
		GtkTypeInfo basep_pos_info = {
			"BasePPos",
			sizeof (BasePPos),
			sizeof (BasePPosClass),
			(GtkClassInitFunc) basep_pos_class_init,
			(GtkObjectInitFunc) basep_pos_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		basep_pos_type = gtk_type_unique (gtk_object_get_type (),
						  &basep_pos_info);
	}

	return basep_pos_type;
}

#if 0
enum {
	POS_CHANGE_SIGNAL,
	POS_LAST_SIGNAL
}
static guint basep_pos_signals[POS_LAST_SIGNAL] = { 0 };
#endif

static void
basep_pos_get_hide_size (BasePWidget *basep, 
			 PanelOrientType hide_orient,
			 int *w, int *h)
{
	switch (hide_orient) {
	case ORIENT_UP:
	case ORIENT_DOWN:
		*h = (basep->state == BASEP_AUTO_HIDDEN)
			? pw_minimized_size
			: get_requisition_height (basep->hidebutton_n);
		break;
	case ORIENT_RIGHT:
	case ORIENT_LEFT:
		*w = (basep->state == BASEP_AUTO_HIDDEN)
			? pw_minimized_size
			: get_requisition_width (basep->hidebutton_e);
		break;
	}
	*w = MAX (*w, 1);
	*h = MAX (*h, 1);
}

static void
basep_pos_get_hide_pos (BasePWidget *basep,
			PanelOrientType hide_orient,
			int *x, int *y,
			int w, int h)
{
	switch (hide_orient) {
	case ORIENT_UP:
	case ORIENT_LEFT:
		break;
	case ORIENT_RIGHT:
		*x += w - ((basep->state == BASEP_AUTO_HIDDEN)
			   ? pw_minimized_size
			   : get_requisition_width (basep->hidebutton_w));
		break;
	case ORIENT_DOWN:
		*y += h - ((basep->state == BASEP_AUTO_HIDDEN)
			   ? pw_minimized_size
			   : get_requisition_height (basep->hidebutton_s));
		break;
	}
}
		
static void
basep_pos_class_init (BasePPosClass *klass)
{
	/*GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);*/

	basep_pos_parent_class = gtk_type_class(gtk_object_get_type ());

	klass->get_hide_size = basep_pos_get_hide_size;
	klass->get_hide_pos = basep_pos_get_hide_pos;
}	

/* nothing to see here... */
static void
basep_pos_init (BasePPos *pos)
{
	return;
}

static gboolean
basep_leave_notify (GtkWidget *widget,
		    GdkEventCrossing *event)
{
	BasePWidget *basep = BASEP_WIDGET (widget);

#ifdef PANEL_DEBUG
	if (basep->state == BASEP_MOVING)
		g_warning ("moving in leave_notify");

	if (basep->leave_notify_timer_tag != 0)
		g_warning ("timeout already queued");
#endif
	if (event->detail == GDK_NOTIFY_INFERIOR)
		return FALSE;
	
	basep_widget_queue_autohide (basep);

	if (GTK_WIDGET_CLASS (basep_widget_parent_class)->leave_notify_event)
		return GTK_WIDGET_CLASS (basep_widget_parent_class)->leave_notify_event (widget, event);
	else
		return FALSE;
}

static gboolean
basep_enter_notify (GtkWidget *widget,
		    GdkEventCrossing *event)
{
	BasePWidget *basep = BASEP_WIDGET (widget);

	if (basep->state == BASEP_AUTO_HIDDEN &&
	    event->detail != GDK_NOTIFY_INFERIOR) {

		g_assert (basep->mode == BASEP_AUTO_HIDE);
#ifdef PANEL_DEBUG
		g_print ("detail: %d\n", event->detail);
#endif
		if (basep->leave_notify_timer_tag != 0) {
			gtk_timeout_remove (basep->leave_notify_timer_tag);
			basep->leave_notify_timer_tag = 0;
		}

		basep_widget_queue_autoshow (basep);
	}  

	if (global_config.autoraise)
		gdk_window_raise (GTK_WIDGET(basep)->window);

	if (GTK_WIDGET_CLASS (basep_widget_parent_class)->enter_notify_event)
		return GTK_WIDGET_CLASS (basep_widget_parent_class)->enter_notify_event (widget, event);
	else
		return FALSE;
}

void
basep_widget_get_position (BasePWidget *basep, PanelOrientType hide_orient,
			   int *x, int *y, int w, int h)
{
	*x = *y = 0;
	switch(hide_orient) {
	case ORIENT_UP:
		if(h < basep->shown_alloc.height)
			*y = h - basep->shown_alloc.height;
		break;
	case ORIENT_LEFT:
		if(w < basep->shown_alloc.width)
			*x = w - basep->shown_alloc.width;
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

	if(global_config.simple_movement) {
		percentage = ((double)(cur_time-start_time))/(end_time-start_time);
		if(percentage>1.0)
			percentage = 1.0;
		return  src + ((dest - src)*percentage);
	} else {
		double n = cur_time-start_time;
		double d = end_time-start_time;

		if(n<d) {
			/*blah blah blah just a simple function to make the
			  movement more "sin" like ... we run it twice to
			  pronounce make it more pronounced*/
			percentage = sin(M_PI*(n/d)-M_PI/2)/2+0.5;
			percentage = sin(M_PI*percentage-M_PI/2)/2+0.5;
			if(percentage<0.0)
				percentage = 0.0;
			else if(percentage>1.0)
				percentage = 1.0;
		} else
			percentage = 1.0;

		return  src + ((dest - src)*percentage);
	}
}

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

	if (basep->state != BASEP_MOVING) {
#ifdef PANEL_DEBUG
		g_warning ("do_hiding whilst not moving");
#endif
		return;
	}

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
			gdk_flush();
			usleep(1000);
		}

		xstuff_set_pos_size (wid->window,
				     dx, dy, dw, dh);
		basep_widget_set_ebox_orient(basep, -1);
	}
	
	gtk_widget_queue_resize(wid);
	gtk_widget_draw(basep->table, NULL);
}

void
basep_widget_do_showing(BasePWidget *basep, PanelOrientType hide_orient,
			int leftover, int step)
{
	GtkWidget *wid;
	int x,y, dx,dy, ox,oy;
	int w,h, dw,dh, ow,oh;
	int diff;

	g_return_if_fail(basep != NULL);
	g_return_if_fail(IS_BASEP_WIDGET(basep));

	if (basep->state != BASEP_MOVING) {
#ifdef PANEL_DEBUG
		g_warning ("do_showing whilst not moving");
#endif
		return;
	}

	wid = GTK_WIDGET(basep);
	
	ox = dx = x = basep->shown_alloc.x;
	oy = dy = y = basep->shown_alloc.y;
	dw = basep->shown_alloc.width;
	dh = basep->shown_alloc.height;
			      
	switch(hide_orient) {
	case ORIENT_UP:
		ow = w = dw;
		oh = h = leftover;
		diff = dh-leftover;
		break;
	case ORIENT_DOWN:
		oy = y + dh - leftover;
		ow = w = dw;
		oh = h = leftover;
		diff = dh-leftover;
		break;
	case ORIENT_LEFT:
		ow = w = leftover;
		oh = h = dh;
		diff = dw-leftover;
		break;
	case ORIENT_RIGHT:
		ox = x + dw - leftover;
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
		gdk_window_show(wid->window);
		xstuff_set_pos_size (wid->window,
				     ox, oy, ow, oh);

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
			gdk_flush();
			
			/*drawing the entire table flickers, so don't
			  do it often*/
			if(i++%10)
				gtk_widget_draw(basep->panel, NULL);
			else
				gtk_widget_draw(basep->table, NULL);
			gdk_flush();
			usleep(1000);
		}

		xstuff_set_pos_size (wid->window,
				     dx, dy, dw, dh);

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

	w = gtk_button_new();
	GTK_WIDGET_UNSET_FLAGS(w, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
	if(horizontal)
		gtk_widget_set_usize(w, 0, PANEL_MINIMUM_WIDTH);
	else
		gtk_widget_set_usize(w, PANEL_MINIMUM_WIDTH, 0);

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
	gtk_object_set_data(GTK_OBJECT(w), "gnome_disable_sound_events",
			    GINT_TO_POINTER(1));

	gtk_tooltips_set_tip (panel_tooltips, w,
			      _("Hide this panel"), NULL);

	return w;
}

static void
basep_widget_destroy (GtkObject *o)
{
	BasePWidget *basep = BASEP_WIDGET (o);
        /* check if there's a timeout set, and delete it if 
	 * there was */
	if (basep->leave_notify_timer_tag != 0)
		gtk_timeout_remove (basep->leave_notify_timer_tag);
	basep->leave_notify_timer_tag = 0;

	if (IS_BORDER_WIDGET (basep))
		basep_border_queue_recalc (basep->screen);

	gtk_object_unref (GTK_OBJECT (basep->pos));
	basep->pos = NULL;

	if (GTK_OBJECT_CLASS (basep_widget_parent_class)->destroy)
		GTK_OBJECT_CLASS (basep_widget_parent_class)->destroy (o);
}	

static void
reparent_button_widgets(GtkWidget *w, gpointer data)
{
	GdkWindow *newwin = data;
	if(IS_BUTTON_WIDGET(w)) {
		ButtonWidget *button = BUTTON_WIDGET(w);
		/* we can just reparent them all to 0,0 as the next thing
		 * that will happen is a queue_resize and on size allocate
		 * they will be put into their proper place */
		gdk_window_reparent(button->event_window, newwin, 0, 0);
	}
}

void
basep_widget_redo_window(BasePWidget *basep)
{
	GtkWindow *window;
	GtkWidget *widget;
	GdkWindowAttr attributes;
	gint attributes_mask;
	GdkWindow *oldwin;
	GdkWindow *newwin;
	gboolean comp;

	comp = xstuff_is_compliant_wm();
	if(comp == basep->compliant_wm)
		return;

	window = GTK_WINDOW(basep);
	widget = GTK_WIDGET(basep);

	basep->compliant_wm = comp;
	if(basep->compliant_wm) {
		window->type = GTK_WINDOW_TOPLEVEL;
		attributes.window_type = GDK_WINDOW_TOPLEVEL;
	} else {
		window->type = GTK_WINDOW_POPUP;
		attributes.window_type = GDK_WINDOW_TEMP;
	}

	if(!widget->window)
		return;

	/* this is mostly copied from gtkwindow.c realize method */
	attributes.title = window->title;
	attributes.wmclass_name = window->wmclass_name;
	attributes.wmclass_class = window->wmclass_class;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= (GDK_EXPOSURE_MASK |
				  GDK_KEY_PRESS_MASK |
				  GDK_ENTER_NOTIFY_MASK |
				  GDK_LEAVE_NOTIFY_MASK |
				  GDK_FOCUS_CHANGE_MASK |
				  GDK_STRUCTURE_MASK);

	attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
	attributes_mask |= (window->title ? GDK_WA_TITLE : 0);
	attributes_mask |= (window->wmclass_name ? GDK_WA_WMCLASS : 0);
   
	oldwin = widget->window;

	newwin = gdk_window_new(NULL, &attributes, attributes_mask);
	gdk_window_set_user_data(newwin, window);

	xstuff_set_no_group_and_no_input (newwin);

	/* reparent our main panel window */
	gdk_window_reparent(basep->ebox->window, newwin, 0, 0);
	/* reparent all the base event windows as they are also children of
	 * the basep */
	gtk_container_foreach(GTK_CONTAINER(basep->panel),
			      reparent_button_widgets,
			      newwin);


	widget->window = newwin;

	gdk_window_set_user_data(oldwin, NULL);
	gdk_window_destroy(oldwin);

	widget->style = gtk_style_attach(widget->style, widget->window);
	gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	gtk_widget_queue_resize(widget);

	basep_widget_update_winhints (basep);

	gtk_drag_dest_set (widget, 0, NULL, 0, 0);

	gtk_widget_map(widget);
}


static void
basep_widget_init (BasePWidget *basep)
{
	basep->screen = 0;

	/*if we set the gnomewm hints it will have to be changed to TOPLEVEL*/
	basep->compliant_wm = xstuff_is_compliant_wm();
	if(basep->compliant_wm)
		GTK_WINDOW(basep)->type = GTK_WINDOW_TOPLEVEL;
	else
		GTK_WINDOW(basep)->type = GTK_WINDOW_POPUP;

	GTK_WINDOW(basep)->allow_shrink = TRUE;
	GTK_WINDOW(basep)->allow_grow = TRUE;
	GTK_WINDOW(basep)->auto_shrink = TRUE;

	/*don't let us close the window*/                                       
	
	gtk_signal_connect(GTK_OBJECT(basep),"delete_event",                    
			   GTK_SIGNAL_FUNC(gtk_true),NULL);                     

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

	basep->innerebox = gtk_event_box_new();

	gtk_table_attach(GTK_TABLE(basep->table),basep->innerebox,1,2,1,2,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 GTK_FILL|GTK_EXPAND|GTK_SHRINK,
			 0,0);

	basep->mode = BASEP_EXPLICIT_HIDE;
	basep->state = BASEP_SHOWN;
	basep->level = BASEP_LEVEL_DEFAULT;
	basep->avoid_on_maximize = TRUE;
	basep->leave_notify_timer_tag = 0;
	basep->autohide_inhibit = FALSE;
	basep->drawers_open = 0;

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

void
basep_widget_update_winhints (BasePWidget *basep)
{
	GtkWidget *w = GTK_WIDGET (basep);
	GnomeWinLayer layer;
	guint coverhint;

        /* Do this before compliance check, the compliance
         * check is for the legacy GNOME spec, this is for the
         * new WM spec. Also, there's no harm to setting the hint
         * for incompliant WM's, they'll just ignore it.
         */
        xstuff_set_wmspec_dock_hints (w->window);
        
	if ( ! basep->compliant_wm)
		return;

	gnome_win_hints_set_expanded_size (w, 0, 0, 0, 0);
	gdk_window_set_decorations (w->window, 0);
	gnome_win_hints_set_state (w, WIN_STATE_STICKY |
				   WIN_STATE_FIXED_POSITION);

	if (basep->avoid_on_maximize) {
		coverhint = WIN_HINTS_DO_NOT_COVER;
	} else {
		coverhint = 0;
	}

	if (basep->level == BASEP_LEVEL_DEFAULT) {
		if (global_config.normal_layer) {
			layer = WIN_LAYER_NORMAL;
		} else if (global_config.keep_bottom) {
			layer = WIN_LAYER_BELOW;
		} else {
			layer = WIN_LAYER_DOCK;
		}

		/* drawers are always in DOCK, or NORMAL */
		if ( IS_DRAWER_WIDGET(w) &&
		     ! global_config.normal_layer)
			layer = WIN_LAYER_DOCK;

		/* if in autohiding mode, then we always want dock by default
		 */
		if (basep->mode == BASEP_AUTO_HIDE)
			layer = WIN_LAYER_DOCK;

	} else if (basep->level == BASEP_LEVEL_BELOW) {
		layer = WIN_LAYER_BELOW;
	} else if (basep->level == BASEP_LEVEL_NORMAL) {
		layer = WIN_LAYER_NORMAL;
	} else /* if (basep->level == BASEP_LEVEL_ABOVE) */ {
		layer = WIN_LAYER_DOCK;
	} 

	switch (basep->state) {
	case BASEP_SHOWN:
	case BASEP_MOVING:

		gnome_win_hints_set_layer (w, layer);
		gnome_win_hints_set_hints (w, GNOME_PANEL_HINTS |
					   coverhint);
		break;
	default: /* all of the hidden states */
		gnome_win_hints_set_hints (w, GNOME_PANEL_HINTS);
		gnome_win_hints_set_layer (w, (global_config.keep_bottom ||
					       global_config.normal_layer)
					   ? WIN_LAYER_ONTOP 
					   : WIN_LAYER_ABOVE_DOCK);
		break;
	}
}

void
basep_update_frame (BasePWidget *basep)
{
	gboolean hide_frame = global_config.hide_panel_frame ||
		PANEL_WIDGET (basep->panel)->back_type == 
						PANEL_BACK_TRANSLUCENT ||
		PANEL_WIDGET (basep->panel)->back_type == PANEL_BACK_PIXMAP;

	if (hide_frame && GTK_WIDGET_VISIBLE (basep->frame)) {
		gtk_widget_show (basep->innerebox);
		gtk_widget_reparent (basep->panel, basep->innerebox);
		gtk_widget_hide (basep->frame);
	} else if (!hide_frame && !GTK_WIDGET_VISIBLE (basep->frame)) {
		gtk_widget_show (basep->frame);
		gtk_widget_reparent (basep->panel, basep->frame);
		gtk_widget_hide (basep->innerebox);
	}
}

static void
basep_back_change (PanelWidget *panel,
		   PanelBackType type,
		   char *pixmap,
		   GdkColor *color,
		   BasePWidget *basep)
{
	basep_update_frame (basep);

	set_frame_colors (panel,
			  basep->frame,
			  basep->hidebutton_n,
			  basep->hidebutton_e,
			  basep->hidebutton_w,
			  basep->hidebutton_s);
}

static void
basep_style_set (GtkWidget *widget, GtkStyle *previous_style)
{
	BasePWidget *basep;
	PanelWidget *panel;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_BASEP_WIDGET (widget));

	basep = BASEP_WIDGET (widget);

	g_return_if_fail (basep->panel != NULL);
	g_return_if_fail (IS_PANEL_WIDGET (basep->panel));

	panel = PANEL_WIDGET (basep->panel);

	if (GTK_WIDGET_CLASS (basep_widget_parent_class)->style_set)
		GTK_WIDGET_CLASS (basep_widget_parent_class)->style_set (widget, previous_style);

	set_frame_colors (panel,
			  basep->frame,
			  basep->hidebutton_n,
			  basep->hidebutton_e,
			  basep->hidebutton_w,
			  basep->hidebutton_s);
}

static void
basep_widget_north_clicked (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep = data;
	BasePPosClass *klass =
		basep_widget_get_pos_class (basep);

	gtk_widget_set_state (widget, GTK_STATE_NORMAL);
	gtk_widget_queue_draw (widget);

	if (klass && klass->north_clicked)
		klass->north_clicked(basep);
}

static void
basep_widget_south_clicked (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep = data;
	BasePPosClass *klass =
		basep_widget_get_pos_class (basep);

	gtk_widget_set_state (widget, GTK_STATE_NORMAL);
	gtk_widget_queue_draw (widget);
	
	if (klass && klass->south_clicked)
		klass->south_clicked(basep);
}

static void
basep_widget_east_clicked (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep = data;
	BasePPosClass *klass =
		basep_widget_get_pos_class (basep);

	gtk_widget_set_state (widget, GTK_STATE_NORMAL);
	gtk_widget_queue_draw (widget);
	
	if (klass && klass->east_clicked)
		klass->east_clicked(basep);
}

static void
basep_widget_west_clicked (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep = data;
	BasePPosClass *klass =
		basep_widget_get_pos_class (basep);

	gtk_widget_set_state (widget, GTK_STATE_NORMAL);
	gtk_widget_queue_draw (widget);
	
	if (klass && klass->west_clicked)
		klass->west_clicked(basep);
}

GtkWidget*
basep_widget_construct (BasePWidget *basep,
			gboolean packed,
			gboolean reverse_arrows,
			int screen,
			PanelOrientation orient,
			int sz,
			BasePMode mode,
			BasePState state,
			BasePLevel level,
			gboolean avoid_on_maximize,
			gboolean hidebuttons_enabled,
			gboolean hidebutton_pixmaps_enabled,
			PanelBackType back_type,
			char *back_pixmap,
			gboolean fit_pixmap_bg,
			gboolean strech_pixmap_bg,
			gboolean rotate_pixmap_bg,
			GdkColor *back_color)
{
	BasePPosClass *klass = basep_widget_get_pos_class (basep);
	int x = 0, y = 0;
	basep->panel = panel_widget_new(packed,
					orient,
					sz,
					back_type,
					back_pixmap,
					fit_pixmap_bg,
					strech_pixmap_bg,
					rotate_pixmap_bg,
					/*if hidebuttons are enabled, then
					  do no padding on the sides */
					!hidebuttons_enabled,
					back_color);

	gtk_signal_connect_after(GTK_OBJECT(basep->panel), "back_change",
				 GTK_SIGNAL_FUNC(basep_back_change),
				 basep);

	PANEL_WIDGET(basep->panel)->panel_parent = GTK_WIDGET(basep);
	PANEL_WIDGET(basep->panel)->drop_widget = GTK_WIDGET(basep);

	gtk_widget_show(basep->panel);

	if(back_type != PANEL_BACK_PIXMAP && 
			back_type != PANEL_BACK_TRANSLUCENT &&
			!global_config.hide_panel_frame) {
		gtk_widget_show(basep->frame);
		gtk_container_add(GTK_CONTAINER(basep->frame),basep->panel);
	} else {
		gtk_widget_show(basep->innerebox);
		gtk_container_add(GTK_CONTAINER(basep->innerebox),basep->panel);
	}

	/*we add all the hide buttons to the table here*/
	/*WEST*/
	basep->hidebutton_w = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-right.png":
					      "panel-arrow-left.png",
					      TRUE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_w,
			 0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	gtk_signal_connect (GTK_OBJECT(basep->hidebutton_w), "clicked",
			    GTK_SIGNAL_FUNC (basep_widget_west_clicked),
			    basep);
	/*NORTH*/
	basep->hidebutton_n = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-down.png":
					      "panel-arrow-up.png",
					      FALSE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_n,
			 1,2,0,1,GTK_FILL,GTK_FILL,0,0);
	gtk_signal_connect (GTK_OBJECT(basep->hidebutton_n), "clicked",
			    GTK_SIGNAL_FUNC (basep_widget_north_clicked),
			    basep);
	/*EAST*/
	basep->hidebutton_e = make_hidebutton(basep,
					      reverse_arrows?
					      "panel-arrow-left.png":
					      "panel-arrow-right.png",
					      TRUE);
	gtk_table_attach(GTK_TABLE(basep->table),basep->hidebutton_e,
			 2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	gtk_signal_connect (GTK_OBJECT(basep->hidebutton_e), "clicked",
			    GTK_SIGNAL_FUNC (basep_widget_east_clicked),
			    basep);
	/*SOUTH*/
	basep->hidebutton_s = make_hidebutton (basep,
					      reverse_arrows ?
					      "panel-arrow-up.png" :
					      "panel-arrow-down.png",
					      FALSE);
	gtk_table_attach(GTK_TABLE(basep->table), basep->hidebutton_s,
			 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_signal_connect (GTK_OBJECT(basep->hidebutton_s), "clicked",
			    GTK_SIGNAL_FUNC (basep_widget_south_clicked),
			    basep);

	basep->screen = screen;

	basep->level = level;
	basep->avoid_on_maximize = avoid_on_maximize;

	basep->hidebuttons_enabled = hidebuttons_enabled;
	basep->hidebutton_pixmaps_enabled = hidebutton_pixmaps_enabled;

	basep_widget_set_hidebuttons (basep);
	basep_widget_show_hidebutton_pixmaps (basep);

	basep->mode = mode;
	basep->state = state;

	basep->pos->basep = basep;

	if (state == BASEP_AUTO_HIDDEN &&
	    mode != BASEP_AUTO_HIDE)
		basep->state = BASEP_SHOWN;
	       
	if (klass->get_pos)
		klass->get_pos (basep, &x, &y, 
				PANEL_MINIMUM_WIDTH,
				PANEL_MINIMUM_WIDTH);
	gtk_widget_set_uposition (GTK_WIDGET (basep), x, y);
	
	return GTK_WIDGET (basep);
}

void
basep_widget_change_params (BasePWidget *basep,
			    int screen,
			    PanelOrientation orient,
			    int sz,
			    BasePMode mode,
			    BasePState state,
			    BasePLevel level,
			    gboolean avoid_on_maximize,
			    gboolean hidebuttons_enabled,
			    gboolean hidebutton_pixmaps_enabled,
			    PanelBackType back_type,
			    char *pixmap_name,
			    gboolean fit_pixmap_bg,
			    gboolean strech_pixmap_bg,
			    gboolean rotate_pixmap_bg,
			    GdkColor *back_color)
{
	g_return_if_fail(basep);
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET(basep)));

	if (PANEL_WIDGET (basep->panel)->orient != orient)
		basep->request_cube = TRUE;
	
	basep->hidebuttons_enabled = hidebuttons_enabled;
	basep->hidebutton_pixmaps_enabled = hidebutton_pixmaps_enabled;

#if 0
	if (type != basep->type)
		basep_widget_convert_to (basep, type);
#endif

	if (state == BASEP_AUTO_HIDDEN &&
	    mode != BASEP_AUTO_HIDE)
		state = BASEP_SHOWN;

	if (mode != basep->mode) {
		basep->mode = mode;
		if (mode == BASEP_AUTO_HIDE)
			basep_widget_queue_autohide (basep);
		gtk_signal_emit(GTK_OBJECT(basep),
				basep_widget_signals[MODE_CHANGE_SIGNAL],
				mode);
	}
	
	if (state != basep->state) {
		basep->state = state;
		if (state != BASEP_AUTO_HIDDEN)
			basep_widget_autoshow (basep);
		gtk_signal_emit(GTK_OBJECT(basep),
				basep_widget_signals[STATE_CHANGE_SIGNAL],
				state);
		panels_to_sync = TRUE;
	}

	panel_widget_change_params(PANEL_WIDGET(basep->panel),
				   orient,
				   sz,
				   back_type,
				   pixmap_name,
				   fit_pixmap_bg,
				   strech_pixmap_bg,
				   rotate_pixmap_bg,
				   /*if hidebuttons are enabled, then
				     do no padding on the sides */
				   !hidebuttons_enabled,
				   back_color);

	if (basep->level != level ||
	    basep->avoid_on_maximize != avoid_on_maximize) {
		basep->level = level;
		basep->avoid_on_maximize = avoid_on_maximize;
		basep_widget_update_winhints (basep);
	}

	basep_widget_set_hidebuttons (basep);
	basep_widget_show_hidebutton_pixmaps (basep);

	basep_widget_screen_change (basep, screen);

	gtk_widget_queue_resize (GTK_WIDGET (basep));
}

#if 0
gboolean
basep_widget_convert_to (BasePWidget *basep,
			 PanelType type)
{
	BasePPosClass *klass =
		basep_widget_get_pos_class (basep);
	BasePPos *old_pos, *new_pos;
	gint16 x=0, y=0;
	gboolean temp_keep;

	g_return_val_if_fail (IS_BASEP_WIDGET(basep), FALSE);

	g_return_val_if_fail (create_panel_type[type], FALSE);

	basep_widget_get_pos(basep, &x, &y);

	old_pos = basep->pos;
	new_pos = gtk_type_new (create_panel_type[type] ());

	if (!new_pos)
		return FALSE;

	basep->pos = new_pos;
	new_pos->basep = basep;

	gtk_object_unref (GTK_OBJECT (old_pos));

	klass = basep_widget_get_pos_class (basep);
	if (klass->pre_convert_hook)
		klass->pre_convert_hook (basep);

	temp_keep = basep->keep_in_screen;
	basep->keep_in_screen = FALSE;
	gtk_widget_set_uposition (GTK_WIDGET (basep), -100, -100);
	gdk_flush ();
	basep_widget_set_pos (basep, -100, -100);
	gdk_flush ();
	g_print ("-------------------------------------\n");
	basep_widget_set_pos (basep, x, y);
	basep->keep_in_screen = temp_keep;
	gtk_signal_emit (GTK_OBJECT(basep),
			 basep_widget_signals[TYPE_CHANGE_SIGNAL],
			 type);

	/*gtk_widget_queue_resize (GTK_WIDGET (basep));*/
	return TRUE;
}
#endif

void
basep_widget_enable_buttons_ (BasePWidget *basep, gboolean enabled)
{
	gtk_widget_set_sensitive(basep->hidebutton_n, enabled);
	gtk_widget_set_sensitive(basep->hidebutton_e, enabled);
	gtk_widget_set_sensitive(basep->hidebutton_w, enabled);
	gtk_widget_set_sensitive(basep->hidebutton_s, enabled);
}

void
basep_widget_set_hidebuttons (BasePWidget *basep)
{
	BasePPosClass *klass = basep_widget_get_pos_class (basep);
	if (!basep->hidebuttons_enabled) {
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);	

		/* if we removed hidebuttons we need to show ourselves,
		 * except for the drawers case that is */
		if ((basep->state == BASEP_HIDDEN_LEFT ||
		     basep->state == BASEP_HIDDEN_RIGHT) &&
		    ! IS_DRAWER_WIDGET (basep))
			basep_widget_explicit_show (basep);
	} else {
		g_return_if_fail (klass && klass->set_hidebuttons);
		klass->set_hidebuttons(basep);
	}
}

void
basep_widget_explicit_hide (BasePWidget *basep, BasePState state)
{
	static const char *supinfo[] = {"panel", "collapse", NULL};

	g_assert ( (state == BASEP_HIDDEN_RIGHT) ||
		   (state == BASEP_HIDDEN_LEFT) );

	if((basep->state != BASEP_SHOWN))
		return;

	if (basep->state == BASEP_MOVING) {
#ifdef PANEL_DEBUG
		g_warning ("explicit_hide whilst moving");
#endif
		return;
	}

	gnome_triggers_vdo("", NULL, supinfo);

	gtk_signal_emit(GTK_OBJECT(basep),
			basep_widget_signals[STATE_CHANGE_SIGNAL],
			state);
	panels_to_sync = TRUE;

	/* if the app did any updating of the interface, flush that for us*/
	gdk_flush();
	
	if (GTK_WIDGET_REALIZED(GTK_WIDGET(basep))) {
		BasePPosClass *klass = basep_widget_get_pos_class (basep);
		PanelOrientType hide_orient;
		int w, h, size;

		basep->state = state;
		
		hide_orient = klass->get_hide_orient (basep);
		basep_widget_get_size (basep, &w, &h);
		klass->get_hide_size (basep,
				      hide_orient,
				      &w, &h);

		size = (hide_orient == ORIENT_UP ||
			hide_orient == ORIENT_DOWN) ?
			h : w;
		
		basep->state = BASEP_MOVING;
		basep_widget_update_winhints (basep);
		basep_widget_do_hiding (basep, hide_orient,
					size, pw_explicit_step);
	}

	basep->state = state;
	basep_widget_update_winhints (basep);
}

void
basep_widget_explicit_show (BasePWidget *basep)
{
	static const char *supinfo[] = {"panel", "expand", NULL};

	if ( (basep->state != BASEP_HIDDEN_LEFT &&
	      basep->state != BASEP_HIDDEN_RIGHT))
		return;
 
	if (basep->state == BASEP_MOVING) {
#ifdef PANEL_DEBUG
		g_warning ("explicit_show whilst moving");
#endif
		return;
	}

	gnome_triggers_vdo("", NULL, supinfo);

	if (GTK_WIDGET_REALIZED(GTK_WIDGET(basep))) {
		BasePPosClass *klass = basep_widget_get_pos_class (basep);
		PanelOrientType hide_orient;
		int w, h, size;

		hide_orient = klass->get_hide_orient (basep);
		basep_widget_get_size (basep, &w, &h);
		klass->get_hide_size (basep,
				      hide_orient,
				      &w, &h);

		size = (hide_orient == ORIENT_UP ||
			hide_orient == ORIENT_DOWN) ?
			h : w;

		basep->state = BASEP_MOVING;
		basep_widget_update_winhints (basep);
		basep_widget_do_showing (basep, hide_orient,
					 size, pw_explicit_step);
	}
	
	basep->state = BASEP_SHOWN;
	basep_widget_update_winhints (basep);

	gtk_signal_emit(GTK_OBJECT(basep),
			basep_widget_signals[STATE_CHANGE_SIGNAL],
			BASEP_SHOWN);
	panels_to_sync = TRUE;
}

gboolean
basep_widget_autoshow (gpointer data)
{
	BasePWidget *basep = data;
	static const char *supinfo[] = {"panel", "expand", NULL};

	g_return_val_if_fail (IS_BASEP_WIDGET(basep), FALSE);

	if (basep->state == BASEP_MOVING) {
#ifdef PANEL_DEBUG
		g_warning ("autoshow whilst moving");
#endif
		return TRUE;
	}
	
	if ( (basep->mode != BASEP_AUTO_HIDE) ||
	     (basep->state != BASEP_AUTO_HIDDEN))
		return TRUE;

	if (GTK_WIDGET_REALIZED(basep)) {
		BasePPosClass *klass = basep_widget_get_pos_class (basep);
		PanelOrientType hide_orient;
		int w, h, size;

		gnome_triggers_vdo("", NULL, supinfo);

		hide_orient = klass->get_hide_orient (basep);
		basep_widget_get_size (basep, &w, &h);
		klass->get_hide_size (basep, 
				      hide_orient,
				      &w, &h);

		size = (hide_orient == ORIENT_UP ||
			hide_orient == ORIENT_DOWN) ?
			h : w;

		basep->state = BASEP_MOVING;
		basep_widget_update_winhints (basep);
		basep_widget_do_showing (basep,
					 hide_orient,
					 size,
					 pw_auto_step);
	}

	basep->state = BASEP_SHOWN;
	basep_widget_update_winhints (basep);

	gtk_signal_emit (GTK_OBJECT(basep),
			 basep_widget_signals[STATE_CHANGE_SIGNAL],
			 BASEP_SHOWN);

	basep->enter_notify_timer_tag = 0;
	return FALSE;
}

void
basep_widget_queue_autoshow (BasePWidget *basep)
{
        /* check if there's already a timeout set, and delete it if 
         * there was */
	if (basep->state == BASEP_MOVING) {
#ifdef PANEL_DEBUG
		g_print ("return 2");
#endif
		return; 
	}

        if (basep->leave_notify_timer_tag != 0) {
                gtk_timeout_remove (basep->leave_notify_timer_tag);
                basep->leave_notify_timer_tag = 0;
	}

        if (basep->enter_notify_timer_tag != 0) {
                gtk_timeout_remove (basep->enter_notify_timer_tag);
#ifdef PANEL_DEBUG
		g_print ("<timeout removed>\n");
#endif
	}

        if ((basep->mode != BASEP_AUTO_HIDE) ||
            (basep->state == BASEP_SHOWN)) {
#ifdef PANEL_DEBUG
		g_print ("return 1\n");
#endif
                return;
	}

	if (pw_minimize_delay == 0) {
		basep_widget_autoshow (basep);
	} else {
		/* set up our delay for popup. */
		basep->enter_notify_timer_tag =
			gtk_timeout_add (pw_maximize_delay,
					 basep_widget_autoshow, basep);
	}
}

gboolean
basep_widget_autohide (gpointer data)
{
	static const char *supinfo[] = {"panel", "collapse", NULL};
	BasePWidget *basep = data;

	g_return_val_if_fail (IS_BASEP_WIDGET(basep), TRUE);

	if (basep->autohide_inhibit)
		return TRUE;
	
	if (basep->state == BASEP_MOVING) {
#ifdef PANEL_DEBUG
		g_warning ("autohide whilst moving");
#endif
		return TRUE;
	}

	if ( (basep->state != BASEP_SHOWN) ||
	     (basep->mode != BASEP_AUTO_HIDE) ||
	     (panel_widget_is_cursor(PANEL_WIDGET(basep->panel), 0)) ) {
		return TRUE;
	}
	
	if (panel_applet_in_drag || basep->drawers_open>0)
		return TRUE;

	if (!gdk_pointer_is_grabbed ()) {
		if (gdk_pointer_grab (GDK_ROOT_PARENT(), FALSE, 
				      0, NULL, NULL, GDK_CURRENT_TIME)
		    != GrabSuccess) {
			return TRUE;
		} else {
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
		}
	}

	gnome_triggers_vdo("", NULL, supinfo);

	gtk_signal_emit(GTK_OBJECT(basep),
			basep_widget_signals[STATE_CHANGE_SIGNAL],
			BASEP_AUTO_HIDDEN);

	/* if the app did any updating of the interface, flush that for us*/
	gdk_flush();

	if (GTK_WIDGET_REALIZED(basep)) {
		BasePPosClass *klass = basep_widget_get_pos_class (basep);
		PanelOrientType hide_orient;
		int w, h, size;

		basep->state = BASEP_AUTO_HIDDEN;
		
		hide_orient = klass->get_hide_orient (basep);
		basep_widget_get_size (basep, &w, &h);
		klass->get_hide_size (basep, 
				      hide_orient,
				      &w, &h);
		size =  (hide_orient == ORIENT_UP ||
			 hide_orient == ORIENT_DOWN) 
			? h : w;

		basep->state = BASEP_MOVING;
		basep_widget_update_winhints (basep);
		basep_widget_do_hiding (basep,
					hide_orient,
					size,
					pw_auto_step);
	}


	basep->state = BASEP_AUTO_HIDDEN;
	basep_widget_update_winhints (basep);

	basep->leave_notify_timer_tag = 0;
	return FALSE;
}

void
basep_widget_queue_autohide(BasePWidget *basep)
{
        /* check if there's already a timeout set, and delete it if 
         * there was */
	if (basep->state == BASEP_MOVING) {
#ifdef PANEL_DEBUG
		g_print ("return 2");
#endif
		return; 
	}

        if (basep->enter_notify_timer_tag != 0) {
                gtk_timeout_remove (basep->enter_notify_timer_tag);
                basep->enter_notify_timer_tag = 0;
	}

        if (basep->leave_notify_timer_tag != 0) {
                gtk_timeout_remove (basep->leave_notify_timer_tag);
#ifdef PANEL_DEBUG
		g_print ("<timeout removed>\n");
#endif
	}

        if ((basep->mode != BASEP_AUTO_HIDE) ||
            (basep->state != BASEP_SHOWN)) {
#ifdef PANEL_DEBUG
		g_print ("return 1\n");
#endif
                return;
	}
                
       /* set up our delay for popup. */
        basep->leave_notify_timer_tag =
                gtk_timeout_add (pw_minimize_delay,
                                 basep_widget_autohide, basep);
}

void
basep_widget_get_menu_pos (BasePWidget *basep,
			   GtkWidget *menu,
			   int *x, int *y,
			   int wx, int wy,
			   int ww, int wh)
{
	GtkRequisition mreq;
	BasePPosClass *klass = 
		basep_widget_get_pos_class (basep);
	g_return_if_fail (klass && klass->get_menu_pos);

	gtk_widget_get_child_requisition(menu, &mreq);

	klass->get_menu_pos (basep, menu, &mreq,
			     x, y, wx, wy, 
			     ww, wh);

	if (*x + mreq.width >
	    multiscreen_x (basep->screen) + multiscreen_width (basep->screen))
		*x = multiscreen_x (basep->screen) + 
			multiscreen_width (basep->screen) - mreq.width;
	if (*x < multiscreen_x (basep->screen))
		*x = multiscreen_x (basep->screen);

	if (*y + mreq.height >
	    multiscreen_y (basep->screen) + multiscreen_height (basep->screen))
		*y = multiscreen_y (basep->screen) + 
			multiscreen_height (basep->screen) - mreq.height;
	if (*y < multiscreen_y (basep->screen))
		*y = multiscreen_y (basep->screen);
}

PanelOrientType
basep_widget_get_applet_orient (BasePWidget *basep)
{
	BasePPosClass *klass = 
		basep_widget_get_pos_class (basep);

	g_return_val_if_fail (klass &&
			      klass->get_applet_orient, -1);

	return klass->get_applet_orient(basep);
}

#if 0
void
basep_widget_get_hide_size (BasePWidget *basep,
			    PanelOrientType hide_orient,
			    int *w, int *h)
{
	BasePPosClass *klass = basep_widget_get_pos_class(basep);

	basep_widget_get_size(basep, w, h);

	if (basep->state == BASEP_SHOWN) {
		g_warning ("get_hide_size() called on shown BasePWidget");
		return;
	}
	
	g_return_if_fail (klass && klass->get_hide_size);
	klass->get_hide_size (basep, hide_orient, w, h);
}

void
basep_widget_get_hide_orient (BasePWidget *basep,
			      PanelOrientType *hide_orient)
{
	BasePPosClass *klass = basep_widget_get_pos_class(basep);

	*hide_orient = -1;
	if (basep->state == BASEP_SHOWN) {
		g_warning ("get_hide_orient() called on shown BasePWidget");
		return;
	}
	
	g_return_if_fail (klass && klass->get_hide_size);
	klass->get_hide_orient (basep, hide_orient);
}

void
basep_widget_get_hide_pos (BasePWidget *basep,
			   PanelOrientType hide_orient,
			   int *x, int *y)
{
	BasePPosClass *klass = basep_widget_get_pos_class(basep);
	int w, h;

	if (basep->state == BASEP_SHOWN) {
		g_warning ("get_hide_pos() called on shown BasePWidget");
		return;
	}
	
	basep_widget_get_hide_size (basep, hide_orient, &w, &h);
	g_return_if_fail (klass && klass->get_hide_size);
	klass->get_hide_orient (basep, hide_orient);
}
#endif

void
basep_widget_get_size (BasePWidget *basep,
		       int *w, int *h)
{
	GtkRequisition req;
	BasePPosClass *klass = basep_widget_get_pos_class (basep);

	gtk_widget_size_request (basep->ebox, &req);
	*w = req.width;
	*h = req.height;
	
	g_return_if_fail (klass);
	if (klass->get_size)
		klass->get_size(basep, w, h);
}

void
basep_widget_get_pos (BasePWidget *basep,
		      int *x, int *y)
{
	int w, h;
	BasePPosClass *klass = 
		basep_widget_get_pos_class (basep);

	g_return_if_fail (klass && klass->get_pos);

	basep_widget_get_size (basep, &w, &h);
	klass->get_pos(basep, x, y, w, h);
}

void
basep_widget_init_offsets (BasePWidget *basep)
{
	g_return_if_fail (basep != NULL);
	g_return_if_fail (IS_BASEP_WIDGET (basep));

	if (GTK_WIDGET (basep)->window != NULL) {
		int x, y;
		gdk_window_get_pointer (GTK_WIDGET (basep)->window,
					&x, &y, NULL);
		basep->offset_x = x;
		basep->offset_y = y;
	} else {
		basep->offset_x = GTK_WIDGET (basep)->requisition.width / 2;
		basep->offset_y = GTK_WIDGET (basep)->requisition.height / 2;
	}
}

void
basep_widget_set_pos (BasePWidget *basep,
		      int x, int y)
{
	int w, h;
	int newscreen;
	gboolean force = FALSE;
	BasePPosClass *klass = 
		basep_widget_get_pos_class (basep);

	g_return_if_fail (klass && klass->set_pos);

	/* first take care of switching screens */
	newscreen = multiscreen_screen_from_pos (x, y);
	if (newscreen >= 0 &&
	    newscreen != basep->screen) {
		force = TRUE;
		basep_widget_screen_change (basep, newscreen);
	}

	basep_widget_get_size (basep, &w, &h);
	klass->set_pos (basep, x, y, w, h, force);
}

void
basep_widget_pre_convert_hook (BasePWidget *basep)
{
	BasePPosClass  *klass = basep_widget_get_pos_class (basep);
	g_return_if_fail (klass && klass->pre_convert_hook);

	klass->pre_convert_hook (basep);
}

void
basep_widget_set_state (BasePWidget *basep, BasePState state,
			gboolean emit)
{
	if (basep->state == state)
		return;

	basep->state = state;
	
	if (emit)
		gtk_signal_emit(GTK_OBJECT(basep),
				basep_widget_signals[STATE_CHANGE_SIGNAL],
				state);
	panels_to_sync = TRUE;
}

void
basep_widget_screen_change (BasePWidget *basep, int screen)
{
	g_return_if_fail (basep != NULL);
	g_return_if_fail (IS_BASEP_WIDGET (basep));
	g_return_if_fail (screen >= 0);

	if (basep->screen == screen)
		return;

	gtk_signal_emit (GTK_OBJECT (basep),
			 basep_widget_signals[SCREEN_CHANGE_SIGNAL],
			 screen);
}

/*****
 * Collision avoidance stuff
 *****/
/* FIXME: needs to be per screen! */
typedef struct {
	int left;
	int center;
	int right;
} Border;

typedef struct {
	int screen;
	Border borders[4];
	int left;
	int right;
	int top;
	int bottom;
} ScreenBorders;

static GList *border_list = NULL;

static ScreenBorders *
get_borders (int screen)
{
	GList *li;
	ScreenBorders *sb;

	for (li = border_list; li != NULL; li = li->next) {
		sb = li->data;
		if (sb->screen == screen)
			return sb;
	}

	sb = g_new0 (ScreenBorders, 1);
	sb->screen = screen;
	border_list = g_list_prepend (border_list, sb);

	return sb;
}

static void
basep_calculate_borders (int screen)
{
	GSList *li;
	ScreenBorders *sb;

	sb = get_borders (screen);

	for (li = panel_list; li != NULL; li = li->next) {
		PanelData *pd = li->data;
		BasePWidget *basep;
		GtkRequisition chreq;
		BorderEdge edge;

		g_assert (pd != NULL);

		if ( ! IS_EDGE_WIDGET (pd->panel) &&
		     ! IS_ALIGNED_WIDGET (pd->panel))
			continue;

		basep = BASEP_WIDGET (pd->panel);

		if (basep->mode == BASEP_AUTO_HIDE ||
		    basep->screen != screen)
			continue;

		gtk_widget_get_child_requisition (basep->ebox, &chreq);

		edge = BORDER_POS (basep->pos)->edge;

		if (IS_EDGE_WIDGET (basep)) {
			BasePState state = basep->state;
			if (PANEL_WIDGET (basep->panel)->orient ==
			    PANEL_VERTICAL) {
				if (sb->borders[edge].left < chreq.width &&
				    state != BASEP_HIDDEN_RIGHT)
					sb->borders[edge].left = chreq.width;
				if (sb->borders[edge].center < chreq.width &&
				    state != BASEP_HIDDEN_RIGHT &&
				    state != BASEP_HIDDEN_LEFT)
					sb->borders[edge].center = chreq.width;
				if (sb->borders[edge].right < chreq.width &&
				    state != BASEP_HIDDEN_LEFT)
					sb->borders[edge].right = chreq.width;
			} else {
				if (sb->borders[edge].left < chreq.height &&
				    state != BASEP_HIDDEN_RIGHT)
					sb->borders[edge].left = chreq.height;
				if (sb->borders[edge].center < chreq.height &&
				    state != BASEP_HIDDEN_RIGHT &&
				    state != BASEP_HIDDEN_LEFT)
					sb->borders[edge].center = chreq.height;
				if (sb->borders[edge].right < chreq.height &&
				    state != BASEP_HIDDEN_LEFT)
					sb->borders[edge].right = chreq.height;
			}
		} else /* ALIGNED */ {
			AlignedAlignment align = ALIGNED_POS(basep->pos)->align;
			if (PANEL_WIDGET (basep->panel)->orient ==
			    PANEL_VERTICAL) {
				if (align == ALIGNED_LEFT &&
				    sb->borders[edge].left < chreq.width)
					sb->borders[edge].left = chreq.width;
				else if (align == ALIGNED_CENTER &&
					 sb->borders[edge].center < chreq.width)
					sb->borders[edge].center = chreq.width;
				else if (align == ALIGNED_RIGHT &&
					 sb->borders[edge].right < chreq.width)
					sb->borders[edge].right = chreq.width;
			} else {
				if (align == ALIGNED_LEFT &&
				    sb->borders[edge].left < chreq.height)
					sb->borders[edge].left = chreq.height;
				else if (align == ALIGNED_CENTER &&
					 sb->borders[edge].center < chreq.height)
					sb->borders[edge].center = chreq.height;
				else if (align == ALIGNED_RIGHT &&
					 sb->borders[edge].right < chreq.height)
					sb->borders[edge].right = chreq.height;
			}
		}
	}
}

static int
border_max (ScreenBorders *sb, BorderEdge edge)
{
	return MAX (sb->borders[edge].center,
		    MAX (sb->borders[edge].left, sb->borders[edge].right));
}

void
basep_border_recalc (int screen)
{
	int i;
	GSList *li;
	ScreenBorders *sb;
	ScreenBorders old;

	sb = get_borders (screen);

	memcpy (&old, sb, sizeof (ScreenBorders));

	for (i = 0; i < 4; i++) {
		sb->borders[i].left = 0;
		sb->borders[i].center = 0;
		sb->borders[i].right = 0;
	}

	/* if not avoiding collisions, keeping things at 0 is a safe bet */
	if (global_config.avoid_collisions) {
		basep_calculate_borders (screen);
	}

	sb->left = border_max (sb, BORDER_LEFT);
	sb->right = border_max (sb, BORDER_RIGHT);
	sb->top = border_max (sb, BORDER_TOP) +
		foobar_widget_get_height (screen);
	sb->bottom = border_max (sb, BORDER_BOTTOM);

#ifdef PANEL_DEBUG
	g_print ("[basep_border_recalc] SCREEN %d (%d %d %d %d)\n", screen,
		 sb->left, sb->right, sb->top, sb->bottom);
#endif

	if (memcmp (&old, sb, sizeof (ScreenBorders)) != 0) {
		for (li = panel_list; li != NULL; li = li->next) {
			PanelData *pd = li->data;
			GtkWidget *panel;

			g_assert (pd != NULL);

			panel = pd->panel;

			if (IS_BORDER_WIDGET (panel) &&
			    BASEP_WIDGET (panel)->screen == screen) {
				gtk_widget_queue_resize (panel);
			}
		}

		if (sb->left != old.left ||
		    sb->right != old.right ||
		    sb->top != old.top ||
		    sb->bottom != old.bottom)
			xstuff_setup_desktop_area (screen,
						   sb->left,
						   sb->right,
						   sb->top,
						   sb->bottom);
	}
}

static guint queue_recalc_id = 0;
static GList *recalc_list = NULL;

static gboolean
queue_recalc_handler (gpointer data)
{
	GList *list, *li;

	queue_recalc_id = 0;

	list = recalc_list;
	recalc_list = NULL;

	for (li = list; li != NULL; li = li->next) {
		int screen = GPOINTER_TO_INT (li->data);
		basep_border_recalc (screen);
	}

	g_list_free (list);

	return FALSE;
}

void
basep_border_queue_recalc (int screen)
{
	if (g_list_find (recalc_list,
			 GINT_TO_POINTER (screen)) == NULL)
		recalc_list = g_list_prepend (recalc_list,
					      GINT_TO_POINTER (screen));
	if (queue_recalc_id == 0) {
		queue_recalc_id = gtk_idle_add (queue_recalc_handler, NULL);
	}
}

void
basep_border_get (int screen, BorderEdge edge,
		  int *left, int *center, int *right)
{
	ScreenBorders *sb;

	g_assert (screen >=0);
	g_assert (edge >=0 && edge <= 3);

	sb = get_borders (screen);

	if (left != NULL)
		*left = sb->borders[edge].left;
	if (center != NULL)
		*center = sb->borders[edge].center;
	if (right != NULL)
		*right = sb->borders[edge].right;
}
