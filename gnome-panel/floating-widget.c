/* Gnome panel: floating widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 *
 */

#include "config.h"

#include "floating-widget.h"
#include "border-widget.h"
#include "panel_config_global.h"
#include "foobar-widget.h"
#include "panel-util.h"
#include "multiscreen-stuff.h"

extern GlobalConfig global_config;
extern int pw_minimized_size;

static void floating_pos_class_init (FloatingPosClass *klass);
static void floating_pos_init (FloatingPos *pos);

static void floating_pos_set_hidebuttons (BasePWidget *basep);
static PanelOrientType floating_pos_get_applet_orient (BasePWidget *basep);

static PanelOrientType floating_pos_get_hide_orient (BasePWidget *basep);
static void floating_pos_get_hide_pos (BasePWidget *basep,
				     PanelOrientType hide_orient,
				     int *x, int *y,
				     int w, int h);

static void floating_pos_get_pos(BasePWidget *basep,
				 int *x, int *y,
				 int w, int h);

static void floating_pos_set_pos (BasePWidget *basep,
				  int x, int y,
				  int w, int h,
				  gboolean force);

static void floating_pos_get_hide_size (BasePWidget *basep,
					PanelOrientType hide_orient,
					int *x, int *y);

static void floating_pos_get_menu_pos (BasePWidget *basep,
				       GtkWidget *widget,
				       GtkRequisition *mreq,
				       int *x, int *y,
				       int wx, int wy,
				       int ww, int wh);

static void floating_pos_pre_convert_hook (BasePWidget *basep);

static void floating_pos_show_hide_left (BasePWidget *basep);
static void floating_pos_show_hide_right (BasePWidget *basep);

static BasePPosClass *parent_class;

GtkType
floating_pos_get_type (void)
{
	static GtkType floating_pos_type = 0;

	if (floating_pos_type == 0) {
		GtkTypeInfo floating_pos_info = {
			"FloatingPos",
			sizeof (FloatingPos),
			sizeof (FloatingPosClass),
			(GtkClassInitFunc) floating_pos_class_init,
			(GtkObjectInitFunc) floating_pos_init,
			NULL,
			NULL,
			NULL
		};

		floating_pos_type = gtk_type_unique (TYPE_BASEP_POS,
						     &floating_pos_info);
	}

	return floating_pos_type;
}

enum {
	COORDS_CHANGE_SIGNAL,
	LAST_SIGNAL
};
static guint floating_pos_signals[LAST_SIGNAL] = { 0 };

static void
floating_pos_class_init (FloatingPosClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
	BasePPosClass *pos_class = BASEP_POS_CLASS(klass);

	parent_class = gtk_type_class(TYPE_BASEP_POS);

	floating_pos_signals[COORDS_CHANGE_SIGNAL] =
		gtk_signal_new ("floating_coords_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (FloatingPosClass,
						   coords_change),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE,
				2, GTK_TYPE_INT, GTK_TYPE_INT);
	
	gtk_object_class_add_signals (object_class,
				      floating_pos_signals,
				      LAST_SIGNAL);
	
	/* fill out the virtual funcs */
	pos_class->set_hidebuttons = floating_pos_set_hidebuttons;
	pos_class->get_applet_orient = floating_pos_get_applet_orient;
	pos_class->get_hide_orient = floating_pos_get_hide_orient;
	pos_class->get_hide_pos = floating_pos_get_hide_pos;
	pos_class->get_hide_size = floating_pos_get_hide_size;
	pos_class->get_pos = floating_pos_get_pos;
	pos_class->set_pos = floating_pos_set_pos;
	pos_class->get_menu_pos = floating_pos_get_menu_pos;
	pos_class->pre_convert_hook = floating_pos_pre_convert_hook;
	pos_class->north_clicked = pos_class->west_clicked = 
		floating_pos_show_hide_left;
	pos_class->south_clicked = pos_class->east_clicked =
		floating_pos_show_hide_right;
}

static void
floating_pos_init (FloatingPos *pos) { }

static void
floating_pos_set_hidebuttons (BasePWidget *basep)
{
	if (PANEL_WIDGET(basep->panel)->orient == PANEL_HORIZONTAL) {
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

static PanelOrientType
floating_pos_get_applet_orient (BasePWidget *basep)
{
	PanelWidget *panel = PANEL_WIDGET (basep->panel);
	if (panel->orient == PANEL_HORIZONTAL)
		return (FLOATING_POS (basep->pos)->y -
			multiscreen_y (basep->screen) < 
			multiscreen_height (basep->screen) / 2)
			? ORIENT_DOWN : ORIENT_UP;
	else
		return (FLOATING_POS (basep->pos)->x -
		       	multiscreen_x (basep->screen) < 
			multiscreen_width (basep->screen) /2)
			? ORIENT_RIGHT : ORIENT_LEFT;
}

static PanelOrientType
floating_pos_get_hide_orient (BasePWidget *basep)
{
	FloatingPos *pos = FLOATING_POS (basep->pos);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);

	switch (basep->state) {
	case BASEP_HIDDEN_LEFT:
		return (panel->orient == PANEL_HORIZONTAL)
			? ORIENT_LEFT : ORIENT_UP;
	case BASEP_HIDDEN_RIGHT:
		return (panel->orient == PANEL_HORIZONTAL)
			? ORIENT_RIGHT : ORIENT_DOWN;
	case BASEP_AUTO_HIDDEN:
		if (panel->orient == PANEL_HORIZONTAL) {
			return ((pos->x >
				 (multiscreen_width (basep->screen) +
				  multiscreen_x (basep->screen) - pos->x -
				  basep->shown_alloc.width))
				? ORIENT_RIGHT : ORIENT_LEFT);
		} else {
			return ((pos->y >
				 (multiscreen_height (basep->screen) +
				  multiscreen_y (basep->screen) - pos->y -
				  basep->shown_alloc.height))
				? ORIENT_DOWN : ORIENT_UP);
		}
	default:
		g_warning ("not hidden");
		return -1;
	}
}

static void
floating_pos_get_menu_pos (BasePWidget *basep,
			   GtkWidget *widget,
			   GtkRequisition *mreq,
			   int *x, int *y,
			   int wx, int wy,
			   int ww, int wh)
{	
	PanelOrientType menu_orient = floating_pos_get_applet_orient (basep);
	
	switch (menu_orient) {
	case ORIENT_DOWN:
		*x += wx;
		*y = wy + wh;
		break;
	case ORIENT_LEFT:
		*x = wx - mreq->width;
		*y += wy;
		break;
	default:
	case ORIENT_UP:
		*x += wx;
		*y = wy - mreq->height;
		break;
	case ORIENT_RIGHT:
		*x = wx + ww;
		*y += wy;
		break;
	}
}

static int
xclamp (int screen, int x, int w)
{
	return CLAMP (x, 0,
		      multiscreen_width (screen) - w);
}

static int
yclamp (int screen, int y, int h)
{
	return CLAMP (y, 0, 
		      multiscreen_height (screen) - h -
		      foobar_widget_get_height (screen));
}

static void
floating_pos_set_pos (BasePWidget *basep,
		      int x, int y,
		      int w, int h,
		      gboolean force)
{
	FloatingPos *pos = FLOATING_POS(basep->pos);
	gint16 newx, newy;

	x -= basep->offset_x;
	y -= basep->offset_y;

	x -= multiscreen_x (basep->screen);
	y -= multiscreen_y (basep->screen);

	if (PANEL_WIDGET (basep->panel)->orient == PANEL_HORIZONTAL) {
		switch (basep->state) {
		case BASEP_SHOWN:
		case BASEP_MOVING:
			break;
		case BASEP_AUTO_HIDDEN:
		case BASEP_HIDDEN_LEFT:
			w = get_requisition_width (basep->hidebutton_w);
			break;
		case BASEP_HIDDEN_RIGHT:
			w = get_requisition_width (basep->hidebutton_e);
			break;
		}
	}

	newx = xclamp (basep->screen, x, w);

	if (PANEL_WIDGET (basep->panel)->orient == PANEL_VERTICAL) {
		switch (basep->state) {
		case BASEP_SHOWN:
		case BASEP_MOVING:
			break;
		case BASEP_AUTO_HIDDEN:
		case BASEP_HIDDEN_LEFT:
			h = get_requisition_height (basep->hidebutton_n);
			break;
		case BASEP_HIDDEN_RIGHT:
			h = get_requisition_height (basep->hidebutton_s);
			break;
		}
	}
	newy = yclamp (basep->screen, y, h);

	if (newy != pos->y || newx != pos->x) {
		pos->x = newx;
		pos->y = newy;
		gtk_signal_emit (GTK_OBJECT (pos),
				 floating_pos_signals[COORDS_CHANGE_SIGNAL],
				 pos->x, pos->y);
		gtk_widget_queue_resize (GTK_WIDGET (basep));
	}
}

static void
floating_pos_get_pos(BasePWidget *basep,
		     int *x, int *y,
		     int w, int h)
{
	*x = xclamp (basep->screen, FLOATING_POS (basep->pos)->x, w);
	*y = yclamp (basep->screen, FLOATING_POS (basep->pos)->y, h);
	*x += multiscreen_x (basep->screen);
	*y += multiscreen_y (basep->screen);
}

static void
floating_pos_get_hide_size (BasePWidget *basep, 
			    PanelOrientType hide_orient,
			    int *w, int *h)
{
	if (basep->state == BASEP_AUTO_HIDDEN &&
	    ! basep->hidebuttons_enabled) {
		switch (hide_orient) {
		case ORIENT_UP:
		case ORIENT_DOWN:
			*h = pw_minimized_size;
			break;
		case ORIENT_LEFT:
		case ORIENT_RIGHT:
			*w = pw_minimized_size;
			break;
		}
	} else {
		switch (hide_orient) {
		case ORIENT_UP:
			*h = get_requisition_height (basep->hidebutton_n);
			break;
		case ORIENT_RIGHT:
			*w = get_requisition_width (basep->hidebutton_w);
			break;
		case ORIENT_DOWN:
			*h = get_requisition_height (basep->hidebutton_s);
			break;
		case ORIENT_LEFT:
			*w = get_requisition_width (basep->hidebutton_e);
			break;
		}
	}
	/* minimum of 3x3 pixels, as 1x1 is impossible to hit in case
	 * something goes wrong */
	*w = MAX (*w, 3);
	*h = MAX (*h, 3);
}

static void
floating_pos_get_hide_pos (BasePWidget *basep,
			   PanelOrientType hide_orient,
			   int *x, int *y,
			   int w, int h)
{
	switch (hide_orient) {
	case ORIENT_UP:
	case ORIENT_LEFT:
		break;
	case ORIENT_RIGHT:
		*x += w - ((basep->state == BASEP_AUTO_HIDDEN &&
			    ! basep->hidebuttons_enabled)
			   ? pw_minimized_size
			   : get_requisition_width (basep->hidebutton_w));
		break;
	case ORIENT_DOWN:
		*y += h - ((basep->state == BASEP_AUTO_HIDDEN &&
			    ! basep->hidebuttons_enabled)
			   ? pw_minimized_size
			   : get_requisition_height (basep->hidebutton_s));
		break;
	}
}

static void
floating_pos_show_hide_left (BasePWidget *basep)
{
	switch (basep->state) {
	case BASEP_SHOWN:
		basep_widget_explicit_hide (basep, BASEP_HIDDEN_LEFT);
		break;
	case BASEP_HIDDEN_RIGHT:
		basep_widget_explicit_show (basep);
		break;
	default:
		break;
	}
}

static void
floating_pos_show_hide_right (BasePWidget *basep)
{
	switch (basep->state) {
	case BASEP_SHOWN:
		basep_widget_explicit_hide (basep, BASEP_HIDDEN_RIGHT);
		break;
	case BASEP_HIDDEN_LEFT:
		basep_widget_explicit_show (basep);
		break;
	default:
		break;
	}
}

static void
floating_pos_pre_convert_hook (BasePWidget *basep)
{
	basep->keep_in_screen = TRUE;
	PANEL_WIDGET (basep->panel)->packed = TRUE;
}

void 
floating_widget_change_params (FloatingWidget *floating,
			       int screen,
			       gint16 x,
			       gint16 y,
			       PanelOrientation orient,
			       BasePMode mode,
			       BasePState state,
			       BasePLevel level,
			       gboolean avoid_on_maximize,
			       int sz,
			       gboolean hidebuttons_enabled,
			       gboolean hidebutton_pixmap_enabled,
			       PanelBackType back_type,
			       char *back_pixmap,
			       gboolean fit_pixmap_bg,
			       gboolean strech_pixmap_bg,
			       gboolean rotate_pixmap_bg,
			       GdkColor *back_color)
{
	BasePWidget *basep = BASEP_WIDGET (floating);
	FloatingPos *pos = FLOATING_POS (basep->pos);

	if (PANEL_WIDGET (basep->panel)->orient != orient)
		BASEP_WIDGET (floating)->request_cube = TRUE;

	x = xclamp (basep->screen, x, basep->shown_alloc.width);
	y = yclamp (basep->screen, y, basep->shown_alloc.height);

	if (y != pos->y || x != pos->x) {
		pos->x = x;
		pos->y = y;
		gtk_signal_emit (GTK_OBJECT (pos),
				 floating_pos_signals[COORDS_CHANGE_SIGNAL],
				 x, y);
	}

	basep_widget_change_params (basep,
				    screen,
				    orient,
				    sz,
				    mode,
				    state,
				    level,
				    avoid_on_maximize,
				    hidebuttons_enabled,
				    hidebutton_pixmap_enabled,
				    back_type,
				    back_pixmap,
				    fit_pixmap_bg,
				    strech_pixmap_bg,
				    rotate_pixmap_bg,
				    back_color);
				    
}

void
floating_widget_change_orient (FloatingWidget *floating,
			       PanelOrientation orient)
{
	FloatingPos *pos = FLOATING_POS (floating->pos);
	if (PANEL_WIDGET (BASEP_WIDGET (floating)->panel)->orient != orient) {
		BasePWidget *basep = BASEP_WIDGET (floating);
		PanelWidget *panel = PANEL_WIDGET (basep->panel);
		floating_widget_change_params (floating, 
					       basep->screen,
					       pos->x,
					       pos->y,
					       orient,
					       basep->mode,
					       basep->state,
					       basep->level,
					       basep->avoid_on_maximize,
					       panel->sz,
					       basep->hidebuttons_enabled,
					       basep->hidebutton_pixmaps_enabled,
					       panel->back_type,
					       panel->back_pixmap,
					       panel->fit_pixmap_bg,
					       panel->strech_pixmap_bg,
					       panel->rotate_pixmap_bg,
					       &panel->back_color);
	}
}

void
floating_widget_change_coords (FloatingWidget *floating,
			       gint16 x, gint16 y)
{
	FloatingPos *pos = FLOATING_POS (floating->pos);
	if (pos->x != x || pos->y != y) {
		BasePWidget *basep = BASEP_WIDGET (floating);
		PanelWidget *panel = PANEL_WIDGET (basep->panel);
		floating_widget_change_params (floating, 
					       basep->screen,
					       x,
					       y,
					       panel->orient,
					       basep->mode,
					       basep->state,
					       basep->level,
					       basep->avoid_on_maximize,
					       panel->sz,
					       basep->hidebuttons_enabled,
					       basep->hidebutton_pixmaps_enabled,
					       panel->back_type,
					       panel->back_pixmap,
					       panel->fit_pixmap_bg,
					       panel->strech_pixmap_bg,
					       panel->rotate_pixmap_bg,
					       &panel->back_color);
	}
}

GtkWidget *
floating_widget_new (int screen,
		     gint16 x,
		     gint16 y,
		     PanelOrientation orient,
		     BasePMode mode,
		     BasePState state,
		     BasePLevel level,
		     gboolean avoid_on_maximize,
		     int sz,
		     gboolean hidebuttons_enabled,
		     gboolean hidebutton_pixmap_enabled,
		     PanelBackType back_type,
		     char *back_pixmap,
		     gboolean fit_pixmap_bg,
		     gboolean strech_pixmap_bg,
		     gboolean rotate_pixmap_bg,
		     GdkColor *back_color)
{
	FloatingWidget *floating;
	FloatingPos *pos;

	floating = gtk_type_new (TYPE_FLOATING_WIDGET);
	floating->pos = gtk_type_new (TYPE_FLOATING_POS);
	pos = FLOATING_POS (floating->pos);

	pos->x = x;
	pos->y = y;

	basep_widget_construct (BASEP_WIDGET (floating),
				TRUE, FALSE,
				screen,
				orient,
				sz,
				mode,
				state,
				level,
				avoid_on_maximize,
				hidebuttons_enabled,
				hidebutton_pixmap_enabled,
				back_type,
				back_pixmap,
				fit_pixmap_bg,
				strech_pixmap_bg,
				rotate_pixmap_bg,
				back_color);

	return GTK_WIDGET (floating);
}
