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
#include "panel-marshal.h"
#include "panel_config_global.h"
#include "foobar-widget.h"
#include "panel-util.h"
#include "panel-gconf.h"
#include "multiscreen-stuff.h"

static void floating_pos_class_init (FloatingPosClass *klass);
static void floating_pos_instance_init (FloatingPos *pos);

static void floating_pos_set_hidebuttons (BasePWidget *basep);
static PanelOrient floating_pos_get_applet_orient (BasePWidget *basep);

static PanelOrient floating_pos_get_hide_orient (BasePWidget *basep);
static void floating_pos_get_hide_pos (BasePWidget *basep,
				     PanelOrient hide_orient,
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
					PanelOrient hide_orient,
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

static BasePPosClass *floating_pos_parent_class;

GType
floating_pos_get_type (void)
{
	static GType object_type= 0;

	if (object_type == 0) {
		static const GTypeInfo object_info = {
                    sizeof (FloatingPosClass),
                    (GBaseInitFunc)         NULL,
                    (GBaseFinalizeFunc)     NULL,
                    (GClassInitFunc)        floating_pos_class_init,
                    NULL,                   /* class_finalize */
                    NULL,                   /* class_data */
                    sizeof (FloatingPos),
                    0,                      /* n_preallocs */
                    (GInstanceInitFunc)     floating_pos_instance_init
		};

		object_type = g_type_register_static (BASEP_TYPE_POS, "FloatingPos", &object_info, 0);
		floating_pos_parent_class = g_type_class_ref (BASEP_TYPE_POS);
	}

	return object_type;
}

enum {
	COORDS_CHANGE_SIGNAL,
	LAST_SIGNAL
};
static guint floating_pos_signals[LAST_SIGNAL] = { 0 };

static void
floating_pos_class_init (FloatingPosClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BasePPosClass *pos_class = BASEP_POS_CLASS (klass);

	floating_pos_signals[COORDS_CHANGE_SIGNAL] =
		g_signal_new ("floating_coords_change",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (FloatingPosClass, coords_change),
			      NULL,
			      NULL,
			      panel_marshal_INT__INT,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_INT,
			      G_TYPE_INT);
	
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
floating_pos_instance_init (FloatingPos *pos) { }

static void
floating_pos_set_hidebuttons (BasePWidget *basep)
{
	if (PANEL_WIDGET(basep->panel)->orient == GTK_ORIENTATION_HORIZONTAL) {
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

static PanelOrient
floating_pos_get_applet_orient (BasePWidget *basep)
{
	PanelWidget *panel = PANEL_WIDGET (basep->panel);
	if (panel->orient == GTK_ORIENTATION_HORIZONTAL)
		return (FLOATING_POS (basep->pos)->y -
			multiscreen_y (basep->screen) < 
			multiscreen_height (basep->screen) / 2)
			? PANEL_ORIENT_DOWN : PANEL_ORIENT_UP;
	else
		return (FLOATING_POS (basep->pos)->x -
		       	multiscreen_x (basep->screen) < 
			multiscreen_width (basep->screen) /2)
			? PANEL_ORIENT_RIGHT : PANEL_ORIENT_LEFT;
}

static PanelOrient
floating_pos_get_hide_orient (BasePWidget *basep)
{
	FloatingPos *pos = FLOATING_POS (basep->pos);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);

	switch (basep->state) {
	case BASEP_HIDDEN_LEFT:
		return (panel->orient == GTK_ORIENTATION_HORIZONTAL)
			? PANEL_ORIENT_LEFT : PANEL_ORIENT_UP;
	case BASEP_HIDDEN_RIGHT:
		return (panel->orient == GTK_ORIENTATION_HORIZONTAL)
			? PANEL_ORIENT_RIGHT : PANEL_ORIENT_DOWN;
	case BASEP_AUTO_HIDDEN:
		if (panel->orient == GTK_ORIENTATION_HORIZONTAL) {
			return ((pos->x >
				 (multiscreen_width (basep->screen) +
				  multiscreen_x (basep->screen) - pos->x -
				  basep->shown_alloc.width))
				? PANEL_ORIENT_RIGHT : PANEL_ORIENT_LEFT);
		} else {
			return ((pos->y >
				 (multiscreen_height (basep->screen) +
				  multiscreen_y (basep->screen) - pos->y -
				  basep->shown_alloc.height))
				? PANEL_ORIENT_DOWN : PANEL_ORIENT_UP);
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
	PanelOrient menu_orient = floating_pos_get_applet_orient (basep);
	
	switch (menu_orient) {
	case PANEL_ORIENT_DOWN:
		*x += wx;
		*y = wy + wh;
		break;
	case PANEL_ORIENT_LEFT:
		*x = wx - mreq->width;
		*y += wy;
		break;
	default:
	case PANEL_ORIENT_UP:
		*x += wx;
		*y = wy - mreq->height;
		break;
	case PANEL_ORIENT_RIGHT:
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

	if (PANEL_WIDGET (basep->panel)->orient == GTK_ORIENTATION_HORIZONTAL) {
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

	if (PANEL_WIDGET (basep->panel)->orient == GTK_ORIENTATION_VERTICAL) {
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
		g_signal_emit (G_OBJECT (pos),
			       floating_pos_signals[COORDS_CHANGE_SIGNAL],
			       0, pos->x, pos->y);
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
			    PanelOrient hide_orient,
			    int *w, int *h)
{
	if (basep->state == BASEP_AUTO_HIDDEN &&
	    ! basep->hidebuttons_enabled) {
		switch (hide_orient) {
		case PANEL_ORIENT_UP:
		case PANEL_ORIENT_DOWN:
			*h = panel_gconf_global_config_get_int ("panel-minimized-size");
			break;
		case PANEL_ORIENT_LEFT:
		case PANEL_ORIENT_RIGHT:
			*w = panel_gconf_global_config_get_int ("panel-minimized-size");
			break;
		}
	} else {
		switch (hide_orient) {
		case PANEL_ORIENT_UP:
			*h = get_requisition_height (basep->hidebutton_n);
			break;
		case PANEL_ORIENT_RIGHT:
			*w = get_requisition_width (basep->hidebutton_w);
			break;
		case PANEL_ORIENT_DOWN:
			*h = get_requisition_height (basep->hidebutton_s);
			break;
		case PANEL_ORIENT_LEFT:
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
			   PanelOrient hide_orient,
			   int *x, int *y,
			   int w, int h)
{
	switch (hide_orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_LEFT:
		break;
	case PANEL_ORIENT_RIGHT:
		*x += w - ((basep->state == BASEP_AUTO_HIDDEN &&
			    ! basep->hidebuttons_enabled)
			   ? panel_gconf_global_config_get_int ("panel-minimized-size")
			   : get_requisition_width (basep->hidebutton_w));
		break;
	case PANEL_ORIENT_DOWN:
		*y += h - ((basep->state == BASEP_AUTO_HIDDEN &&
			    ! basep->hidebuttons_enabled)
			   ? panel_gconf_global_config_get_int ("panel-minimized-size")
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
			       GtkOrientation orient,
			       BasePMode mode,
			       BasePState state,
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
		g_signal_emit (G_OBJECT (pos),
			       floating_pos_signals[COORDS_CHANGE_SIGNAL],
			       0, x, y);
	}

	basep_widget_change_params (basep,
				    screen,
				    orient,
				    sz,
				    mode,
				    state,
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
			       GtkOrientation orient)
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
		     GtkOrientation orient,
		     BasePMode mode,
		     BasePState state,
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

	floating = gtk_type_new (FLOATING_TYPE_WIDGET);
	floating->pos = gtk_type_new (FLOATING_TYPE_POS);
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
