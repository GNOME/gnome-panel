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
#include "panel_config_global.h"
#include "gdkextra.h"

extern int panel_applet_in_drag;

static void corner_widget_class_init	(CornerWidgetClass *klass);
static void corner_widget_init		(CornerWidget      *corner);
static void corner_widget_size_request	(GtkWidget         *widget,
					 GtkRequisition    *requisition);
static void corner_widget_size_allocate	(GtkWidget         *widget,
					 GtkAllocation     *allocation);
static void corner_widget_set_hidebuttons(BasePWidget      *basep);

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

		corner_widget_type = gtk_type_unique (basep_widget_get_type (),
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
	CornerWidget *corner = CORNER_WIDGET(w);
	GTK_WIDGET_CLASS(parent_class)->realize(w);

	gnome_win_hints_init();
	if (gnome_win_hints_wm_exists()) {
		gnome_win_hints_set_hints(w,
					  WIN_HINTS_SKIP_FOCUS |
					  WIN_HINTS_SKIP_WINLIST |
					  WIN_HINTS_SKIP_TASKBAR |
					  WIN_HINTS_DO_NOT_COVER);
		gnome_win_hints_set_state(w,
					  WIN_STATE_STICKY |
					  WIN_STATE_FIXED_POSITION);
		gnome_win_hints_set_layer(w, global_config.keep_bottom?
					  WIN_LAYER_BELOW:
					  WIN_LAYER_DOCK);
		gnome_win_hints_set_expanded_size(w, 0, 0, 0, 0);
		gdk_window_set_decorations(w->window, 0);
	}
}

static void
corner_widget_class_init (CornerWidgetClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass*) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;
	BasePWidgetClass *basep_class = (BasePWidgetClass*) class;

        parent_class = gtk_type_class (basep_widget_get_type ());
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

	basep_class->set_hidebuttons = corner_widget_set_hidebuttons;
	
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
	BasePWidget *basep = BASEP_WIDGET(widget);
	if(corner_widget_request_cube) {
		requisition->width = 48;
		requisition->height = 48;
		corner_widget_request_cube = FALSE;
		return;
	}

	gtk_widget_size_request (basep->ebox, &basep->ebox->requisition);
	
	requisition->width = basep->ebox->requisition.width;
	requisition->height = basep->ebox->requisition.height;
}

static void
corner_widget_get_hidepos(CornerWidget *corner, PanelOrientType *hide_orient,
			  int *w, int *h)
{
	BasePWidget *basep = BASEP_WIDGET(corner);
	PanelWidget *panel = PANEL_WIDGET(basep->panel);
	*w = GTK_WIDGET(corner)->allocation.width;
	*h = GTK_WIDGET(corner)->allocation.height;
	*hide_orient = -1;
	if(corner->state == CORNER_SHOWN)
		return;
	
	switch(corner->pos) {
	case CORNER_NE:
		if(panel->orient == PANEL_HORIZONTAL) {
			*w = basep->hidebutton_w->allocation.width;
			*hide_orient = ORIENT_RIGHT;
		} else {
			*h = basep->hidebutton_s->allocation.height;
			*hide_orient = ORIENT_UP;
		}
		break;
	case CORNER_SE:
		if(panel->orient == PANEL_HORIZONTAL) {
			*w = basep->hidebutton_w->allocation.width;
			*hide_orient = ORIENT_RIGHT;
		} else {
			*h = basep->hidebutton_n->allocation.height;
			*hide_orient = ORIENT_DOWN;
		}
		break;
	case CORNER_SW:
		if(panel->orient == PANEL_HORIZONTAL) {
			*w = basep->hidebutton_e->allocation.width;
			*hide_orient = ORIENT_LEFT;
		} else {
			*h = basep->hidebutton_n->allocation.height;
			*hide_orient = ORIENT_DOWN;
		}
		break;
	case CORNER_NW:
		if(panel->orient == PANEL_HORIZONTAL) {
			*w = basep->hidebutton_e->allocation.width;
			*hide_orient = ORIENT_LEFT;
		} else {
			*h = basep->hidebutton_s->allocation.height;
			*hide_orient = ORIENT_UP;
		}
		break;
	}
}

static void
corner_widget_get_pos(CornerWidget *corner, gint16 *x, gint16 *y, int width, int height)
{
	PanelWidget *panel = PANEL_WIDGET(BASEP_WIDGET(corner)->panel);
	
	*y = *x = 0;
	switch(corner->pos) {
	case CORNER_NE:
		*x = gdk_screen_width() - width;
		if(panel->orient == PANEL_HORIZONTAL &&
		   corner->state == CORNER_HIDDEN)
			*x = gdk_screen_width() -
				BASEP_WIDGET(corner)->hidebutton_w->requisition.width;
		break;
	case CORNER_SE:
		*x = gdk_screen_width() - width;
		*y = gdk_screen_height() - height;
		if(panel->orient == PANEL_HORIZONTAL &&
		   corner->state == CORNER_HIDDEN)
			*x = gdk_screen_width() -
				BASEP_WIDGET(corner)->hidebutton_w->requisition.width;
		else if(panel->orient == PANEL_VERTICAL &&
			corner->state == CORNER_HIDDEN)
			*y = gdk_screen_height() -
				BASEP_WIDGET(corner)->hidebutton_n->requisition.height;
		break;
	case CORNER_SW:
		*y = gdk_screen_height() - height;
		if(panel->orient == PANEL_VERTICAL &&
		   corner->state == CORNER_HIDDEN)
			*y = gdk_screen_height() -
				BASEP_WIDGET(corner)->hidebutton_n->requisition.height;
		break;
	case CORNER_NW:
		break;
	}
}

static void
corner_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	CornerWidget *corner = CORNER_WIDGET(widget);
	BasePWidget *basep = BASEP_WIDGET(widget);
	GtkAllocation challoc;
	
	/*we actually want to ignore the size_reqeusts since they
	  are sometimes a cube for the flicker prevention*/
	gtk_widget_size_request (basep->ebox,
				 &basep->ebox->requisition);
	
	allocation->width = basep->ebox->requisition.width;
	allocation->height = basep->ebox->requisition.height;

	corner_widget_get_pos(corner,
			      &allocation->x,
			      &allocation->y,
			      allocation->width,
			      allocation->height);


	challoc.x = challoc.y = 0;
	challoc.width = allocation->width;
	challoc.height = allocation->height;

	basep->shown_alloc = *allocation;

	if(corner->state != CORNER_SHOWN) {
		PanelOrientType hide_orient;
		int x,y,w,h;
		widget->allocation = *allocation;
		corner_widget_get_hidepos(corner, &hide_orient,
					  &w, &h);
		basep_widget_get_position(basep, hide_orient,
					  &x, &y, w, h);
		challoc.x = x;
		challoc.y = y;

		allocation->width = w;
		allocation->height = h;
	}

	widget->allocation = *allocation;
	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_set_hints (widget->window,
				      allocation->x, allocation->y,
				      0,0,0,0, GDK_HINT_POS);
		gdk_window_move_resize (widget->window,
					allocation->x, 
					allocation->y,
					allocation->width,
					allocation->height);
	}

	gtk_widget_size_allocate(basep->ebox,&challoc);
}

static void
corner_widget_set_initial_pos(CornerWidget *corner)
{
	gint16 x,y;
	corner_widget_get_pos(corner, &x, &y, 48, 48);
	gtk_widget_set_uposition(GTK_WIDGET(corner),x,y);
}

static void
corner_widget_pop_show(CornerWidget *corner, int fromright)
{
	static const char *supinfo[] = {"panel", "collapse", NULL};

	if ((corner->state == CORNER_MOVING) ||
	    (corner->state == CORNER_SHOWN))
		return;

	gnome_triggers_vdo("", NULL, supinfo);

	corner->state = CORNER_MOVING;

	if(PANEL_WIDGET(BASEP_WIDGET(corner)->panel)->orient == PANEL_HORIZONTAL) {
		if(fromright)
			basep_widget_do_showing(BASEP_WIDGET(corner),
						ORIENT_LEFT,
						BASEP_WIDGET(corner)->hidebutton_e->allocation.width,
						pw_explicit_step);
		else
			basep_widget_do_showing(BASEP_WIDGET(corner),
						ORIENT_RIGHT,
						BASEP_WIDGET(corner)->hidebutton_w->allocation.width,
						pw_explicit_step);
	} else {
		if(fromright)
			basep_widget_do_showing(BASEP_WIDGET(corner),
						ORIENT_UP,
						BASEP_WIDGET(corner)->hidebutton_s->allocation.height,
						pw_explicit_step);
		else
			basep_widget_do_showing(BASEP_WIDGET(corner),
						ORIENT_DOWN,
						BASEP_WIDGET(corner)->hidebutton_n->allocation.height,
						pw_explicit_step);
	}

	gnome_win_hints_set_layer(GTK_WIDGET(corner),
				  global_config.keep_bottom?
				  WIN_LAYER_BELOW:
				  WIN_LAYER_DOCK);

	corner->state = CORNER_SHOWN;

	gtk_signal_emit(GTK_OBJECT(corner),
			corner_widget_signals[STATE_CHANGE_SIGNAL],
			CORNER_SHOWN);
}

static void
corner_widget_pop_hide(CornerWidget *corner, int fromright)
{
	static const char *supinfo[] = {"panel", "collapse", NULL};
	int width, height;

	if((corner->state != CORNER_SHOWN))
		return;

	gnome_triggers_vdo("", NULL, supinfo);
	
	gnome_win_hints_set_layer(GTK_WIDGET(corner),
				  global_config.keep_bottom?
				  WIN_LAYER_ONTOP:
				  WIN_LAYER_ABOVE_DOCK);

	gtk_signal_emit(GTK_OBJECT(corner),
			corner_widget_signals[STATE_CHANGE_SIGNAL],
			CORNER_HIDDEN);

	corner->state = CORNER_MOVING;

	if(PANEL_WIDGET(BASEP_WIDGET(corner)->panel)->orient == PANEL_HORIZONTAL) {
		if(fromright)
			basep_widget_do_hiding(BASEP_WIDGET(corner),
					       ORIENT_LEFT,
					       BASEP_WIDGET(corner)->hidebutton_e->allocation.width,
					       pw_explicit_step);
		else
			basep_widget_do_hiding(BASEP_WIDGET(corner),
					       ORIENT_RIGHT,
					       BASEP_WIDGET(corner)->hidebutton_w->allocation.width,
					       pw_explicit_step);
	} else {
		if(fromright)
			basep_widget_do_hiding(BASEP_WIDGET(corner),
					       ORIENT_UP,
					       BASEP_WIDGET(corner)->hidebutton_s->allocation.height,
					       pw_explicit_step);
		else
			basep_widget_do_hiding(BASEP_WIDGET(corner),
					       ORIENT_DOWN,
					       BASEP_WIDGET(corner)->hidebutton_n->allocation.height,
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
	PanelWidget *panel = PANEL_WIDGET(BASEP_WIDGET(corner)->panel);
	if((panel->orient == PANEL_HORIZONTAL && !is_west(corner)) ||
	   (panel->orient == PANEL_VERTICAL && !is_north(corner)))
		return TRUE;
	return FALSE;
}

static void
jump_to_opposite(CornerWidget *corner)
{
	CornerPos newpos = CORNER_NW;
	PanelWidget *panel = PANEL_WIDGET(BASEP_WIDGET(corner)->panel);
	
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
corner_widget_set_hidebuttons(BasePWidget *basep)
{
	CornerWidget *corner = CORNER_WIDGET(basep);

	/*hidebuttons are disabled*/
	if(!basep->hidebuttons_enabled) {
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);
		/*in case the panel was hidden, show it, since otherwise
		  we wouldn't see it anymore*/
		if(is_right(corner))
			corner_widget_pop_show(corner,FALSE);
		else
			corner_widget_pop_show(corner,TRUE);
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
corner_widget_init (CornerWidget *corner)
{
	corner->pos = CORNER_NE;
	corner->state = CORNER_SHOWN;
}


GtkWidget*
corner_widget_new (CornerPos pos,
		   PanelOrientation orient,
		   CornerState state,
		   int hidebuttons_enabled,
		   int hidebutton_pixmaps_enabled,
		   PanelBackType back_type,
		   char *back_pixmap,
		   int fit_pixmap_bg,
		   GdkColor *back_color)
{
	CornerWidget *corner;
	BasePWidget *basep;

	corner = gtk_type_new(corner_widget_get_type());

	basep = BASEP_WIDGET(corner);

	basep_widget_construct(basep,
			       TRUE,
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
			   GTK_SIGNAL_FUNC(corner_show_hide_left),
			   corner);
	/*NORTH*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_n),"clicked",
			   GTK_SIGNAL_FUNC(corner_show_hide_right),
			   corner);
	/*WEST*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_w),"clicked",
			   GTK_SIGNAL_FUNC(corner_show_hide_right),
			   corner);
	/*SOUTH*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_s),"clicked",
			   GTK_SIGNAL_FUNC(corner_show_hide_left),
			   corner);

	corner->pos = pos;
	if(state != CORNER_MOVING)
		corner->state = state;

	corner_widget_set_initial_pos(corner);

	return GTK_WIDGET(corner);
}

void
corner_widget_change_params(CornerWidget *corner,
			    CornerPos pos,
			    PanelOrientation orient,
			    CornerState state,
			    int hidebuttons_enabled,
			    int hidebutton_pixmaps_enabled,
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
	if(PANEL_WIDGET(BASEP_WIDGET(corner)->panel)->orient != orient)
		corner_widget_request_cube = TRUE;

	basep_widget_change_params(BASEP_WIDGET(corner),
				   orient,
				   hidebuttons_enabled,
				   hidebutton_pixmaps_enabled,
				   back_type,
				   pixmap_name,
				   fit_pixmap_bg,
				   back_color);
	
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
	BasePWidget *basep = BASEP_WIDGET(corner);
	PanelWidget *panel = PANEL_WIDGET(basep->panel);
	corner_widget_change_params(corner,
				    pos,
				    orient,
				    corner->state,
				    basep->hidebuttons_enabled,
				    basep->hidebutton_pixmaps_enabled,
				    panel->back_type,
				    panel->back_pixmap,
				    panel->fit_pixmap_bg,
				    &panel->back_color);
}
