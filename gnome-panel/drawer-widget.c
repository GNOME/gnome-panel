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
static void drawer_widget_set_hidebuttons(BasePWidget       *basep);

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

		drawer_widget_type = gtk_type_unique (basep_widget_get_type (),
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
		gnome_win_hints_set_hints(w, GNOME_PANEL_HINTS);
		gnome_win_hints_set_state(w,
					  WIN_STATE_STICKY |
					  WIN_STATE_FIXED_POSITION);
		gnome_win_hints_set_layer(w, global_config.keep_bottom?
					  WIN_LAYER_BELOW:
					  WIN_LAYER_ABOVE_DOCK);
		gnome_win_hints_set_expanded_size(w, 0, 0, 0, 0);
		gdk_window_set_decorations(w->window, 0);
	}
}

static void
drawer_widget_class_init (DrawerWidgetClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass*) class;
	GtkWidgetClass *widget_class = (GtkWidgetClass*) class;
	BasePWidgetClass *basep_class = (BasePWidgetClass*) class;

        parent_class = gtk_type_class (basep_widget_get_type ());
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

	basep_class->set_hidebuttons = drawer_widget_set_hidebuttons;
	
	widget_class->size_request = drawer_widget_size_request;
	widget_class->size_allocate = drawer_widget_size_allocate;
	widget_class->realize = drawer_widget_realize;
}

/*if this is true the size request will request a 48x48 cube, this is used
  during orientation changes to make no flicker*/
static int drawer_widget_request_cube = FALSE;
static void
drawer_widget_size_request(GtkWidget *widget,
			   GtkRequisition *requisition)
{
	DrawerWidget *drawer = DRAWER_WIDGET(widget);
	BasePWidget *basep = BASEP_WIDGET(widget);
	int w,h;
	if(drawer_widget_request_cube) {
		requisition->width = PANEL_MINIMUM_WIDTH;
		requisition->height = PANEL_MINIMUM_WIDTH;
		drawer_widget_request_cube = FALSE;
		return;
	}

	gtk_widget_size_request (basep->ebox, &basep->ebox->requisition);

	w = basep->ebox->requisition.width;
	h = basep->ebox->requisition.height;

	/* do a minimal 48 size*/
	if(PANEL_WIDGET(basep->panel)->orient == PANEL_HORIZONTAL) {
		if(w<PANEL_MINIMUM_WIDTH) w=PANEL_MINIMUM_WIDTH;
	} else {
		if(h<PANEL_MINIMUM_WIDTH) h=PANEL_MINIMUM_WIDTH;
	}
	requisition->width = w;
	requisition->height = h;
}


static void
drawer_widget_get_pos(DrawerWidget *drawer, gint16 *x, gint16 *y,
		      int width, int height)
{
	PanelWidget *panel = PANEL_WIDGET(BASEP_WIDGET(drawer)->panel);

	if (panel->master_widget &&
	    GTK_WIDGET_REALIZED (panel->master_widget) &&
	    /*"allocated" data will be set on each allocation, until then,
	      don't show the actual panel*/
	    gtk_object_get_data(GTK_OBJECT(panel->master_widget),"allocated")) {
		int bx, by, bw, bh;
		int px, py, pw, ph;
		GtkWidget *ppanel; /*parent panel*/

		BasePWidget *basep;
		
		/*get the parent of the applet*/
		ppanel = panel->master_widget->parent;
		bx = panel->master_widget->allocation.x +
			ppanel->allocation.x;
		by = panel->master_widget->allocation.y +
			ppanel->allocation.y;
		/*go the the toplevel panel widget*/
		while(ppanel->parent) {
			ppanel = ppanel->parent;
			bx += ppanel->allocation.x;
			by += ppanel->allocation.y;
		}
		basep = BASEP_WIDGET(ppanel);
		/*a hack as the above just doesn't work perfectly*/
		if(GTK_WIDGET_VISIBLE(basep->hidebutton_n))
			by -= basep->hidebutton_n->allocation.height;
		if(GTK_WIDGET_VISIBLE(basep->hidebutton_w))
			bx -= basep->hidebutton_w->allocation.width;

		bw = panel->master_widget->allocation.width;
		bh = panel->master_widget->allocation.height;
		px = ppanel->allocation.x;
		py = ppanel->allocation.y;
		pw = ppanel->allocation.width;
		ph = ppanel->allocation.height;

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
	/*if we fail*/
	*x = -width-1;
	*y = -height-1;
}

static void
drawer_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	DrawerWidget *drawer = DRAWER_WIDGET(widget);
	BasePWidget *basep = BASEP_WIDGET(widget);
	GtkAllocation challoc;
	int w,h;

	/*we actually want to ignore the size_reqeusts since they
	  are sometimes a cube for the flicker prevention*/
	gtk_widget_size_request (basep->ebox,
				 &basep->ebox->requisition);

	w = basep->ebox->requisition.width;
	h = basep->ebox->requisition.height;

	/* do a minimal 48 size*/
	if(PANEL_WIDGET(basep->panel)->orient == PANEL_HORIZONTAL) {
		if(w<PANEL_MINIMUM_WIDTH) w=PANEL_MINIMUM_WIDTH;
	} else {
		if(h<PANEL_MINIMUM_WIDTH) h=PANEL_MINIMUM_WIDTH;
	}
	allocation->width = w;
	allocation->height = h;

	drawer_widget_get_pos(drawer,
			      &allocation->x,
			      &allocation->y,
			      allocation->width,
			      allocation->height);

	basep->shown_alloc = *allocation;
	
	if(drawer->state != DRAWER_SHOWN ||
	   drawer->temp_hidden) {
		allocation->x = -allocation->width -1;
		allocation->y = -allocation->height -1;
	}
	
	widget->allocation = *allocation;
	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize (widget->window,
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);
		gdk_window_set_hints (widget->window,
				      allocation->x,
				      allocation->y,
				      0,0,0,0, GDK_HINT_POS);
	}

	challoc.x = challoc.y = 0;
	challoc.width = allocation->width;
	challoc.height = allocation->height;
	gtk_widget_size_allocate(basep->ebox,&challoc);
}

static void
drawer_widget_set_hidebuttons(BasePWidget *basep)
{
	DrawerWidget *drawer = DRAWER_WIDGET(basep);

	/*hidebuttons are disabled*/
	if(!basep->hidebuttons_enabled) {
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);
		return;
	}
	
	switch(drawer->orient) {
	case ORIENT_UP:
		gtk_widget_show(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);
		break;
	case ORIENT_DOWN:
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_show(basep->hidebutton_s);
		break;
	case ORIENT_LEFT:
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_show(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);
		break;
	case ORIENT_RIGHT:
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_show(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);
		break;
	}
}

void
drawer_widget_open_drawer(DrawerWidget *drawer)
{
	static const char *supinfo[] = {"panel", "collapse", NULL};
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

	gnome_triggers_vdo("", NULL, supinfo);

	width   = GTK_WIDGET(drawer)->allocation.width;
	height  = GTK_WIDGET(drawer)->allocation.height;
	drawer_widget_get_pos(drawer,&x,&y,width,height);
	GTK_WIDGET(drawer)->allocation.x = x;
	GTK_WIDGET(drawer)->allocation.y = y;

	drawer->state = DRAWER_MOVING;

	switch(drawer->orient) {
	case ORIENT_UP:
		BASEP_WIDGET(drawer)->shown_alloc.y += 
			BASEP_WIDGET(drawer)->shown_alloc.height;
		basep_widget_do_showing(BASEP_WIDGET(drawer), ORIENT_DOWN,
					0,pw_drawer_step);
		break;
	case ORIENT_DOWN:
		basep_widget_do_showing(BASEP_WIDGET(drawer), ORIENT_UP,
					0,pw_drawer_step);
		break;
	case ORIENT_LEFT:
		BASEP_WIDGET(drawer)->shown_alloc.x += 
			BASEP_WIDGET(drawer)->shown_alloc.width;
		basep_widget_do_showing(BASEP_WIDGET(drawer), ORIENT_RIGHT,
					0,pw_drawer_step);
		break;
	case ORIENT_RIGHT:
		basep_widget_do_showing(BASEP_WIDGET(drawer), ORIENT_LEFT,
					0,pw_drawer_step);
		break;
	default: break;
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
	static const char *supinfo[] = {"panel", "collapse", NULL};
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

	gnome_triggers_vdo("", NULL, supinfo);

	gtk_signal_emit(GTK_OBJECT(drawer),
			drawer_widget_signals[STATE_CHANGE_SIGNAL],
			DRAWER_HIDDEN);

	width   = GTK_WIDGET(drawer)->allocation.width;
	height  = GTK_WIDGET(drawer)->allocation.height;
	drawer_widget_get_pos(drawer,&x,&y,width,height);
	GTK_WIDGET(drawer)->allocation.x = x;
	GTK_WIDGET(drawer)->allocation.y = y;

	drawer->state = DRAWER_MOVING;
	
	switch(drawer->orient) {
	case ORIENT_UP:
		basep_widget_do_hiding(BASEP_WIDGET(drawer), ORIENT_DOWN,
				       0,pw_drawer_step);
		break;
	case ORIENT_DOWN:
		basep_widget_do_hiding(BASEP_WIDGET(drawer), ORIENT_UP,
				       0,pw_drawer_step);
		break;
	case ORIENT_LEFT:
		basep_widget_do_hiding(BASEP_WIDGET(drawer), ORIENT_RIGHT,
				       0,pw_drawer_step);
		break;
	case ORIENT_RIGHT:
		basep_widget_do_hiding(BASEP_WIDGET(drawer), ORIENT_LEFT,
				       0,pw_drawer_step);
		break;
	default: break;
	}

	drawer->state = DRAWER_HIDDEN;
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
	drawer->state = DRAWER_SHOWN;
	drawer->temp_hidden = FALSE;
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
	BasePWidget *basep;
	
	drawer = gtk_type_new(drawer_widget_get_type());

	basep = BASEP_WIDGET(drawer);

	switch(orient) {
	case ORIENT_UP: porient = PANEL_VERTICAL; break;
	case ORIENT_DOWN: porient = PANEL_VERTICAL; break;
	case ORIENT_LEFT: porient = PANEL_HORIZONTAL; break;
	case ORIENT_RIGHT: porient = PANEL_HORIZONTAL; break;
	default:
		porient = PANEL_HORIZONTAL;
	}

	basep_widget_construct(basep,
			       TRUE,
			       TRUE,
			       porient,
			       hidebutton_enabled,
			       hidebutton_pixmap_enabled,
			       back_type,
			       back_pixmap,
			       fit_pixmap_bg,
			       back_color);

	/*EAST*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_e),"clicked",
			   GTK_SIGNAL_FUNC(drawer_handle_click),
			   drawer);
	/*NORTH*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_n),"clicked",
			   GTK_SIGNAL_FUNC(drawer_handle_click),
			   drawer);
	/*WEST*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_w),"clicked",
			   GTK_SIGNAL_FUNC(drawer_handle_click),
			   drawer);
	/*SOUTH*/
	gtk_signal_connect(GTK_OBJECT(basep->hidebutton_s),"clicked",
			   GTK_SIGNAL_FUNC(drawer_handle_click),
			   drawer);


	drawer->state = state;

	gtk_widget_set_uposition(GTK_WIDGET(drawer),-100,-100);

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
	case ORIENT_UP:
	case ORIENT_DOWN:
		porient = PANEL_VERTICAL;
		break;
	case ORIENT_LEFT:
	case ORIENT_RIGHT:
	default:
		porient = PANEL_HORIZONTAL;
		break;
	}

	oldstate = drawer->state;

	drawer->state = state;

	drawer->orient = orient;

	/*avoid flicker during size_request*/
	if(PANEL_WIDGET(BASEP_WIDGET(drawer)->panel)->orient != porient)
		drawer_widget_request_cube = TRUE;

	basep_widget_change_params(BASEP_WIDGET(drawer),
				   porient,
				   hidebutton_enabled,
				   hidebutton_pixmap_enabled,
				   back_type,
				   pixmap,
				   fit_pixmap_bg,
				   back_color);

	if(oldstate != drawer->state)
	   	gtk_signal_emit(GTK_OBJECT(drawer),
	   			drawer_widget_signals[STATE_CHANGE_SIGNAL],
	   			drawer->state);
}

void
drawer_widget_change_orient(DrawerWidget *drawer,
			    PanelOrientType orient)
{
	BasePWidget *basep = BASEP_WIDGET(drawer);
	PanelWidget *panel = PANEL_WIDGET(basep->panel);
	drawer_widget_change_params(drawer,
				    orient,
				    drawer->state,
				    panel->back_type,
				    panel->back_pixmap,
				    panel->fit_pixmap_bg,
				    &panel->back_color,
				    basep->hidebutton_pixmaps_enabled,
				    basep->hidebuttons_enabled); 
}

void
drawer_widget_restore_state(DrawerWidget *drawer)
{
	drawer->temp_hidden = FALSE;
	gtk_widget_queue_resize(GTK_WIDGET(drawer));
	gtk_widget_show(GTK_WIDGET(drawer));
}

