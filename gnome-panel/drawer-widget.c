/* Gnome panel: drawer widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 *
 */

#include "config.h"
#include "drawer-widget.h"
#include "border-widget.h"
#include "floating-widget.h"
#include "panel_config_global.h"

#include "multiscreen-stuff.h"

extern GlobalConfig global_config;
extern int pw_minimized_size;

static void drawer_pos_class_init (DrawerPosClass *klass);
static void drawer_pos_init (DrawerPos *pos);

static void drawer_pos_set_hidebuttons (BasePWidget *basep);
static PanelOrientType drawer_pos_get_applet_orient (BasePWidget *basep);

static PanelOrientType drawer_pos_get_hide_orient (BasePWidget *basep);
static void drawer_pos_get_hide_pos (BasePWidget *basep,
				     PanelOrientType hide_orient,
				     int *x, int *y,
				     int w, int h);
static void drawer_pos_get_hide_size (BasePWidget *basep, 
				      PanelOrientType hide_orient,
				      int *w, int *h);

static void drawer_pos_get_pos(BasePWidget *basep,
			       int *x, int *y,
			       int width, int height);

static void drawer_pos_get_menu_pos (BasePWidget *basep,
				     GtkWidget *widget,
				     GtkRequisition *mreq,
				     int *x, int *y,
				     int wx, int wy,
				     int ww, int wh);

static void drawer_pos_hidebutton_click (BasePWidget *basep);

static void drawer_pos_pre_convert_hook (BasePWidget *basep);

static BasePPosClass *parent_class;

GtkType
drawer_pos_get_type (void)
{
	static GtkType drawer_pos_type = 0;

	if (drawer_pos_type == 0) {
		GtkTypeInfo drawer_pos_info = {
			"DrawerPos",
			sizeof (DrawerPos),
			sizeof (DrawerPosClass),
			(GtkClassInitFunc) drawer_pos_class_init,
			(GtkObjectInitFunc) drawer_pos_init,
			NULL,
			NULL,
			NULL
		};

		drawer_pos_type = gtk_type_unique (TYPE_BASEP_POS,
						   &drawer_pos_info);
	}

	return drawer_pos_type;
}

static void
drawer_pos_class_init (DrawerPosClass *klass)
{
	/*GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);*/
	BasePPosClass *pos_class = BASEP_POS_CLASS(klass);

	parent_class = gtk_type_class(TYPE_BASEP_POS);

	/* fill out the virtual funcs */
	pos_class->set_hidebuttons = drawer_pos_set_hidebuttons;
	pos_class->get_applet_orient = drawer_pos_get_applet_orient;
	pos_class->get_size = NULL; /* the default is ok */
	pos_class->get_hide_orient = drawer_pos_get_hide_orient;
	pos_class->get_hide_pos = drawer_pos_get_hide_pos;
	pos_class->get_hide_size = drawer_pos_get_hide_size;
	pos_class->get_pos = drawer_pos_get_pos;
	pos_class->get_menu_pos = drawer_pos_get_menu_pos;

	pos_class->north_clicked = 
		pos_class->west_clicked = 
		pos_class->south_clicked = 
		pos_class->east_clicked =
		drawer_pos_hidebutton_click;
	pos_class->pre_convert_hook = drawer_pos_pre_convert_hook;
}

static void
drawer_pos_init (DrawerPos *pos) { }

static void
drawer_pos_set_hidebuttons (BasePWidget *basep)
{
	switch(DRAWER_POS(basep->pos)->orient) {
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

static PanelOrientType
drawer_pos_get_applet_orient (BasePWidget *basep)
{
	PanelWidget *panel = PANEL_WIDGET (basep->panel);
	PanelOrientation porient = panel->orient;
	int x,y;

	x = GTK_WIDGET(basep)->allocation.x;
	y = GTK_WIDGET(basep)->allocation.y;

	if(porient == PANEL_VERTICAL) {
		if (x > (multiscreen_width (basep->screen)/2 +
			 multiscreen_x (basep->screen)))
			return ORIENT_LEFT;
		else
			return ORIENT_RIGHT;
	} else {
		if (y > (multiscreen_height (basep->screen)/2 +
			 multiscreen_y (basep->screen)))
			return ORIENT_UP;
		else
			return ORIENT_DOWN;
	}
}

static PanelOrientType
drawer_pos_get_hide_orient (BasePWidget *basep)
{
	DrawerPos *pos = DRAWER_POS (basep->pos);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);

	switch (basep->state) {
	case BASEP_AUTO_HIDDEN:
		switch (pos->orient) {
		case ORIENT_UP: return ORIENT_DOWN;
		case ORIENT_RIGHT: return ORIENT_LEFT;
		case ORIENT_DOWN: return ORIENT_UP;
		case ORIENT_LEFT: return ORIENT_RIGHT;
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
	return -1;
}
	
void
drawer_widget_open_drawer (DrawerWidget *drawer, GtkWidget *parentp)
{
	if (IS_BASEP_WIDGET (parentp))
		BASEP_WIDGET (parentp)->drawers_open++;
	basep_widget_explicit_show (BASEP_WIDGET (drawer));
}

void
drawer_widget_close_drawer (DrawerWidget *drawer, GtkWidget *parentp)
{
	BasePWidget *basep = BASEP_WIDGET (drawer);

	if(GTK_WIDGET(parentp)->window)
		gdk_window_raise(GTK_WIDGET(parentp)->window);

	switch (DRAWER_POS (basep->pos)->orient) {
	case ORIENT_UP:
	case ORIENT_LEFT:
		basep_widget_explicit_hide (basep, BASEP_HIDDEN_RIGHT);
		break;
	case ORIENT_RIGHT:
	case ORIENT_DOWN:
		basep_widget_explicit_hide (basep, BASEP_HIDDEN_LEFT);
		break;
	}

	if (IS_BASEP_WIDGET (parentp))
		BASEP_WIDGET (parentp)->drawers_open--;
}

static void
drawer_pos_hidebutton_click (BasePWidget *basep)
{
	Drawer *drawer = gtk_object_get_data (GTK_OBJECT (basep),
					      DRAWER_PANEL_KEY);
	PanelWidget *panel = PANEL_WIDGET (drawer->button->parent);
	GtkWidget *parent = panel->panel_parent;

	drawer_widget_close_drawer (DRAWER_WIDGET (basep), parent);
}

static void
drawer_pos_get_menu_pos (BasePWidget *basep,
			 GtkWidget *widget,
			 GtkRequisition *mreq,
			 int *x, int *y,
			 int wx, int wy,
			 int ww, int wh)
{	
	PanelWidget *panel =
		PANEL_WIDGET(basep->panel);

	if(panel->orient==PANEL_VERTICAL) {
		*x = wx + ww;
		*y += wy;
	} else {
		*x += wx;
		*y = wy - mreq->height;
	}
}

static void
drawer_pos_get_pos(BasePWidget *basep,
		   int *x, int *y,
		   int width, int height)
{
	PanelWidget *panel = PANEL_WIDGET(basep->panel);
	DrawerPos *pos = DRAWER_POS (basep->pos);

	/* we are shown but we are hidden, life is full of ironies */
	if (pos->temp_hidden) {
		*x = -width - 1;
		*y = -height - 1;
		return;
	}

	if (panel->master_widget &&
	    GTK_WIDGET_REALIZED (panel->master_widget) &&
	    /*"allocated" data will be set on each allocation, until then,
	      don't show the actual panel*/
	    gtk_object_get_data(GTK_OBJECT(panel->master_widget),"allocated")) {
		int bx, by, bw, bh;
		int px, py, pw, ph;
		GtkWidget *ppanel; /*parent panel*/
		
		/*get the parent of the applet*/
		/*note we know these are not NO_WINDOW widgets, so
		  we don't need to check*/
		ppanel = panel->master_widget->parent;
		bx = panel->master_widget->allocation.x +
			ppanel->allocation.x;
		by = panel->master_widget->allocation.y +
			ppanel->allocation.y;
		/*go the the toplevel panel widget*/
		while(ppanel->parent) {
			ppanel = ppanel->parent;
			if(!GTK_WIDGET_NO_WINDOW(ppanel)) {
				bx += ppanel->allocation.x;
				by += ppanel->allocation.y;
			}
		}

		bw = panel->master_widget->allocation.width;
		bh = panel->master_widget->allocation.height;
		px = ppanel->allocation.x;
		py = ppanel->allocation.y;
		pw = ppanel->allocation.width;
		ph = ppanel->allocation.height;

		switch(pos->orient) {
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
	}
}

static void
drawer_pos_get_hide_pos (BasePWidget *basep,
			 PanelOrientType hide_orient,
			 int *x, int *y,
			 int w, int h)
{
	if (basep->state != BASEP_SHOWN ||
	    DRAWER_POS (basep->pos)->temp_hidden) {
		*x = -ABS(*x) - 1;
		*y = -ABS(*y) - 1;
	}
}

static void
drawer_pos_get_hide_size (BasePWidget *basep, 
			  PanelOrientType hide_orient,
			  int *w, int *h)
{
	switch (hide_orient) {
	case ORIENT_UP:
	case ORIENT_DOWN:
		*h = 1;
		break;
	case ORIENT_RIGHT:
	case ORIENT_LEFT:
		*w = 1;
		break;
	}
}


static void
drawer_pos_pre_convert_hook (BasePWidget *basep)
{
	basep->keep_in_screen = FALSE;
	PANEL_WIDGET (basep->panel)->packed = TRUE;
}

void
drawer_widget_change_params (DrawerWidget *drawer,
			     PanelOrientType orient,
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
	PanelOrientation porient;
	DrawerPos *pos = DRAWER_POS (BASEP_WIDGET (drawer)->pos);

	switch (orient) {
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

	if (PANEL_WIDGET (BASEP_WIDGET (drawer)->panel)->orient != porient)
		BASEP_WIDGET (drawer)->request_cube = TRUE;

	if (state != BASEP_WIDGET (drawer)->state ||
	    pos->orient != orient) {
		if (state == BASEP_HIDDEN_LEFT &&
		    (orient == ORIENT_LEFT ||
		     orient == ORIENT_UP))
			state = BASEP_HIDDEN_RIGHT;
		else if (state == BASEP_HIDDEN_RIGHT &&
			 (orient == ORIENT_RIGHT ||
			  orient == ORIENT_DOWN))
			 state = BASEP_HIDDEN_LEFT;
	}

	if (pos->orient != orient) {
		pos->orient = orient;
#if 0
		gtk_signal_emit (GTK_OBJECT (drawer),
				 drawer_pos_signals[ORIENT_CHANGE_SIGNAL],
				 orient);
#endif
	}

	basep_widget_change_params (BASEP_WIDGET (drawer),
				    0 /*FIXME */,
				    porient,
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
drawer_widget_change_orient (DrawerWidget *drawer,
			     PanelOrientType orient)
{
	DrawerPos *pos = DRAWER_POS (drawer->pos);
	if (pos->orient != orient) {
		BasePWidget *basep = BASEP_WIDGET (drawer);
		PanelWidget *panel = PANEL_WIDGET (basep->panel);
		drawer_widget_change_params (drawer,
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

GtkWidget *
drawer_widget_new (PanelOrientType orient,
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
	DrawerWidget *drawer;
	DrawerPos *pos;
	PanelOrientation porient;

	drawer = gtk_type_new (TYPE_DRAWER_WIDGET);
	drawer->pos = gtk_type_new (TYPE_DRAWER_POS);
	pos = DRAWER_POS (drawer->pos);
	pos->orient = orient;

	switch (orient) {
	case ORIENT_UP:
	case ORIENT_DOWN:
		porient = PANEL_VERTICAL;
		break;
	default:
		porient = PANEL_HORIZONTAL;
		break;
	}

	basep_widget_construct (BASEP_WIDGET (drawer),
				TRUE, TRUE,
				0 /*FIXME */,
				porient,
				sz, mode, state,
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

	return GTK_WIDGET (drawer);
}

#if 0
void
drawer_widget_restore_state(DrawerWidget *drawer)
{
	DRAWER_POS (BASEP_WIDGET (drawer)->pos)->temp_hidden = FALSE;
	gtk_widget_queue_resize(GTK_WIDGET(drawer));
	gtk_widget_show(GTK_WIDGET(drawer));
}
#endif
