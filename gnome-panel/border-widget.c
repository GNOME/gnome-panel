/* Gnome panel: border widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#include "config.h"

#include "border-widget.h"
#include "panel_config_global.h"
#include "multiscreen-stuff.h"

extern GlobalConfig global_config;
extern int pw_minimized_size;

static void border_pos_class_init (BorderPosClass *klass);
static void border_pos_init (BorderPos *pos);

static void border_pos_set_hidebuttons (BasePWidget *basep);
static PanelOrientType border_pos_get_applet_orient (BasePWidget *basep);

static PanelOrientType border_pos_get_hide_orient (BasePWidget *basep);

static void border_pos_get_menu_pos (BasePWidget *basep,
				     GtkWidget *widget,
				     GtkRequisition *mreq,
				     int *x, int *y,
				     int wx, int wy,
				     int ww, int wh);

static void border_pos_show_hide_left (BasePWidget *basep);
static void border_pos_show_hide_right (BasePWidget *basep);

static void border_pos_pre_convert_hook (BasePWidget *basep);

static BasePPosClass *parent_class;

GtkType
border_pos_get_type (void)
{
	static GtkType border_pos_type = 0;

	if (!border_pos_type) {
		GtkTypeInfo border_pos_info = {
			"BorderPos",
			sizeof (BorderPos),
			sizeof (BorderPosClass),
			(GtkClassInitFunc) border_pos_class_init,
			(GtkObjectInitFunc) border_pos_init,
			NULL, NULL
		};

		border_pos_type = gtk_type_unique (TYPE_BASEP_POS,
						   &border_pos_info);
	}

	return border_pos_type;
}

enum {
	EDGE_CHANGE_SIGNAL,
	LAST_SIGNAL
};

static guint border_pos_signals[LAST_SIGNAL] = { 0 };

static void
border_pos_class_init (BorderPosClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
	BasePPosClass *pos_class = BASEP_POS_CLASS(klass);

	parent_class = gtk_type_class(TYPE_BASEP_POS);

	/* set up signals */
	border_pos_signals[EDGE_CHANGE_SIGNAL] =
		gtk_signal_new("edge_change",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(BorderPosClass,
						 edge_change),
			       gtk_marshal_NONE__ENUM,
			       GTK_TYPE_NONE, 1,
			       GTK_TYPE_ENUM);

	gtk_object_class_add_signals (object_class, 
				      border_pos_signals,
				      LAST_SIGNAL);

	/* fill out the virtual funcs */
	pos_class->set_hidebuttons = border_pos_set_hidebuttons;
	pos_class->get_applet_orient = border_pos_get_applet_orient;
	pos_class->get_hide_orient = border_pos_get_hide_orient;
	pos_class->get_menu_pos = border_pos_get_menu_pos;

	pos_class->north_clicked = pos_class->west_clicked = 
		border_pos_show_hide_left;
	pos_class->south_clicked = pos_class->east_clicked =
		border_pos_show_hide_right;
	pos_class->pre_convert_hook = border_pos_pre_convert_hook;
}

static void
border_pos_init (BorderPos *pos)
{
	pos->edge = BORDER_TOP;
}

static void
border_pos_set_hidebuttons (BasePWidget *basep)
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
border_pos_get_applet_orient (BasePWidget *basep)
{
	BorderPos *pos = BORDER_POS(basep->pos);
	
	switch (pos->edge) {
	case BORDER_TOP: return ORIENT_DOWN;
	case BORDER_RIGHT: return ORIENT_LEFT;
	case BORDER_BOTTOM: return ORIENT_UP;
	case BORDER_LEFT: return ORIENT_RIGHT;
	default: return ORIENT_UP;		
	}
}

static PanelOrientType 
border_pos_get_hide_orient (BasePWidget *basep)
{
	BorderPos *pos = BORDER_POS (basep->pos);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);

	switch (basep->state) {
	case BASEP_AUTO_HIDDEN:
		switch (pos->edge) {
		case BORDER_TOP: return ORIENT_UP;
		case BORDER_RIGHT: return ORIENT_RIGHT;
		case BORDER_LEFT: return ORIENT_LEFT;
		case BORDER_BOTTOM: return ORIENT_DOWN;
		}
		g_assert_not_reached ();
		break;
	case BASEP_HIDDEN_LEFT:
		return (panel->orient == PANEL_HORIZONTAL)
			? ORIENT_LEFT : ORIENT_UP;
	case BASEP_HIDDEN_RIGHT:
		return (panel->orient == PANEL_HORIZONTAL)
			? ORIENT_RIGHT : ORIENT_DOWN;
	default:
		g_assert_not_reached ();
		break;
	}
	g_assert_not_reached ();
	return ORIENT_LEFT;
}

static void
border_pos_get_menu_pos (BasePWidget *basep,
			 GtkWidget *widget,
			 GtkRequisition *mreq,
			 int *x, int *y,
			 int wx, int wy,
			 int ww, int wh)
{
	switch (BORDER_POS(basep->pos)->edge) {
	case BORDER_TOP:
		*x += wx;
		*y = wy + wh;
		break;
	case BORDER_RIGHT:
		*x = wx - mreq->width;
		*y += wy;
		break;
	case BORDER_BOTTOM:
		*x += wx;
		*y = wy - mreq->height;
		break;
	case BORDER_LEFT:
		*x = wx + ww;
		*y += wy;
		break;
	}
}

void
border_widget_change_params (BorderWidget *border,
			     int screen,
			     BorderEdge edge,
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
	PanelOrientation new_orient;
	g_return_if_fail (GTK_WIDGET_REALIZED (GTK_WIDGET (border)));

	new_orient = (edge == BORDER_TOP ||
		      edge == BORDER_BOTTOM)
		? PANEL_HORIZONTAL
		: PANEL_VERTICAL;

	if (edge != BORDER_POS (border->pos)->edge) {	
		BORDER_POS(border->pos)->edge = edge;
		gtk_signal_emit (GTK_OBJECT (border->pos),
				 border_pos_signals[EDGE_CHANGE_SIGNAL],
				 edge);
	}

	basep_widget_change_params (BASEP_WIDGET (border),
				    screen,
				    new_orient,
				    sz,
				    mode,
				    state,
				    level,
				    avoid_on_maximize,
				    hidebuttons_enabled,
				    hidebutton_pixmaps_enabled,
				    back_type,
				    pixmap_name,
				    fit_pixmap_bg,
				    strech_pixmap_bg,
				    rotate_pixmap_bg,
				    back_color);
}

static void
border_pos_show_hide_left (BasePWidget *basep)
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
border_pos_show_hide_right (BasePWidget *basep)
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
border_pos_pre_convert_hook (BasePWidget *basep)
{
	basep->keep_in_screen = TRUE;
	PANEL_WIDGET (basep->panel)->packed = TRUE;
}

void
border_widget_change_edge (BorderWidget *border, BorderEdge edge)
{
	BasePWidget *basep = BASEP_WIDGET (border);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);
	
	if (BORDER_POS (border->pos)->edge == edge)
		return;
	
	border_widget_change_params (border,
				     basep->screen,
				     edge,
				     panel->sz,
				     basep->mode,
				     basep->state,
				     basep->level,
				     basep->avoid_on_maximize,
				     basep->hidebuttons_enabled,
				     basep->hidebutton_pixmaps_enabled,
				     panel->back_type,
				     panel->back_pixmap,
				     panel->fit_pixmap_bg,
				     panel->strech_pixmap_bg,
				     panel->rotate_pixmap_bg,
				     &panel->back_color);
	
}

GtkWidget *
border_widget_construct (BorderWidget *border,
			 int screen,
			 BorderEdge edge,
			 gboolean packed,
			 gboolean reverse_arrows,
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
	BasePWidget *basep = BASEP_WIDGET (border);
	PanelOrientation orient;

	if (edge == BORDER_TOP ||
	    edge == BORDER_BOTTOM)
		orient = PANEL_HORIZONTAL;
	else
		orient = PANEL_VERTICAL;

	BORDER_POS (basep->pos)->edge = edge;
	basep->keep_in_screen = TRUE;

	basep_widget_construct (basep,
				packed,
				reverse_arrows,
				screen,
				orient,
				sz,
				mode,
				state,
				level,
				avoid_on_maximize,
				hidebuttons_enabled,
				hidebutton_pixmaps_enabled,
				back_type,
				back_pixmap,
				fit_pixmap_bg,
				strech_pixmap_bg,
				rotate_pixmap_bg,
				back_color);

	return GTK_WIDGET (basep);
}
