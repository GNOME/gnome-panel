/* Gnome panel: border widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#include "config.h"

#include "border-widget.h"
#include "panel-marshal.h"
#include "panel_config_global.h"
#include "multiscreen-stuff.h"
#include "panel-typebuiltins.h"

#undef BORDER_WIDGET_DEBUG

extern GlobalConfig global_config;

static void border_pos_class_init (BorderPosClass *klass);
static void border_pos_instance_init (BorderPos *pos);

static void border_pos_set_hidebuttons (BasePWidget *basep);
static PanelOrient border_pos_get_applet_orient (BasePWidget *basep);

static PanelOrient border_pos_get_hide_orient (BasePWidget *basep);

static void border_pos_get_menu_pos (BasePWidget *basep,
				     GtkWidget *widget,
				     GtkRequisition *mreq,
				     int *x, int *y,
				     int wx, int wy,
				     int ww, int wh);

static void border_pos_show_hide_left (BasePWidget *basep);
static void border_pos_show_hide_right (BasePWidget *basep);

static void border_pos_pre_convert_hook (BasePWidget *basep);

static BasePPosClass *border_pos_parent_class;

GType
border_pos_get_type (void)
{
	static GType object_type = 0;
	if (object_type == 0) {
		static const GTypeInfo object_info = {
			sizeof (BorderPosClass),
			(GBaseInitFunc)		NULL,
			(GBaseFinalizeFunc) 	NULL,
			(GClassInitFunc)	border_pos_class_init,
			NULL,			/* class_finalize */
			NULL,			/* class_data */
			sizeof (BorderPos),
			0,			/* n_preallocs */
			(GInstanceInitFunc)	border_pos_instance_init
		};

		object_type = g_type_register_static (BASEP_TYPE_POS,
						      "BorderPos", &object_info, 0);
		border_pos_parent_class = g_type_class_ref (BASEP_TYPE_POS);
	}

	return object_type;
}

enum {
	EDGE_CHANGE_SIGNAL,
	LAST_SIGNAL
};

static guint border_pos_signals[LAST_SIGNAL] = { 0 };

static void
border_pos_class_init (BorderPosClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	BasePPosClass *pos_class = BASEP_POS_CLASS(klass);


	/* set up signals */
	border_pos_signals[EDGE_CHANGE_SIGNAL] =
		g_signal_new ("edge_change",
			     G_TYPE_FROM_CLASS (object_class), 
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET (BorderPosClass, edge_change),
			     NULL,
			     NULL,
			     panel_marshal_VOID__ENUM,
			     G_TYPE_NONE,
			     1,
			     PANEL_TYPE_BORDER_EDGE);

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
border_pos_instance_init (BorderPos *pos)
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

static PanelOrient
border_pos_get_applet_orient (BasePWidget *basep)
{
	BorderPos *pos = BORDER_POS(basep->pos);
	
	switch (pos->edge) {
	case BORDER_TOP: return PANEL_ORIENT_DOWN;
	case BORDER_RIGHT: return PANEL_ORIENT_LEFT;
	case BORDER_BOTTOM: return PANEL_ORIENT_UP;
	case BORDER_LEFT: return PANEL_ORIENT_RIGHT;
	default: return PANEL_ORIENT_UP;		
	}
}

static PanelOrient 
border_pos_get_hide_orient (BasePWidget *basep)
{
	BorderPos *pos = BORDER_POS (basep->pos);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);

	switch (basep->state) {
	case BASEP_AUTO_HIDDEN:
		switch (pos->edge) {
		case BORDER_TOP: return PANEL_ORIENT_UP;
		case BORDER_RIGHT: return PANEL_ORIENT_RIGHT;
		case BORDER_LEFT: return PANEL_ORIENT_LEFT;
		case BORDER_BOTTOM: return PANEL_ORIENT_DOWN;
		}
		g_assert_not_reached ();
		break;
	case BASEP_HIDDEN_LEFT:
		return (panel->orient == PANEL_HORIZONTAL)
			? PANEL_ORIENT_LEFT : PANEL_ORIENT_UP;
	case BASEP_HIDDEN_RIGHT:
		return (panel->orient == PANEL_HORIZONTAL)
			? PANEL_ORIENT_RIGHT : PANEL_ORIENT_DOWN;
	default:
		g_assert_not_reached ();
		break;
	}
	g_assert_not_reached ();
	return PANEL_ORIENT_LEFT;
}

#ifdef BORDER_WIDGET_DEBUG
static void
border_pos_debug_print_edge (BorderEdge edge)
{
	g_print ("BorderEdge = ");

	switch (edge) {
	case BORDER_TOP:
		g_print ("(top)\n");
		break;
	case BORDER_RIGHT:
		g_print ("(right)\n");
		break;
	case BORDER_BOTTOM:
		g_print ("(bottom)\n");
		break;
	case BORDER_LEFT:
		g_print ("(left)\n");
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}
#endif /* BORDER_WIDGET_DEBUG */

static void
border_pos_get_menu_pos (BasePWidget *basep,
			 GtkWidget *widget,
			 GtkRequisition *mreq,
			 int *x, int *y,
			 int wx, int wy,
			 int ww, int wh)
{

#ifdef BORDER_WIDGET_DEBUG
	border_pos_debug_print_edge (BORDER_POS(basep->pos)->edge);
#endif /* BORDER_WIDGET_DEBUG */

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
	default:
		g_assert_not_reached ();
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
		g_signal_emit (G_OBJECT (border->pos),
			       border_pos_signals[EDGE_CHANGE_SIGNAL],
			       0, edge);
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
