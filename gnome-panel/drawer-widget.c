/* Gnome panel: drawer widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 *
 */
#include "config.h"
#include <stdio.h>
#include "drawer-widget.h"
#include "border-widget.h"
#include "floating-widget.h"
#include "panel-config-global.h"

#include "multiscreen-stuff.h"

extern GlobalConfig global_config;
extern int pw_minimized_size;

#undef DRAWER_WIDGET_DEBUG

static void drawer_pos_class_init (DrawerPosClass *klass);
static void drawer_pos_instance_init (DrawerPos *pos);

static void drawer_pos_set_hidebuttons (BasePWidget *basep);
static PanelOrient drawer_pos_get_applet_orient (BasePWidget *basep);

static PanelOrient drawer_pos_get_hide_orient (BasePWidget *basep);
static void drawer_pos_get_hide_pos (BasePWidget *basep,
				     PanelOrient hide_orient,
				     int *x, int *y,
				     int w, int h);
static void drawer_pos_get_hide_size (BasePWidget *basep, 
				      PanelOrient hide_orient,
				      int *w, int *h);

static void drawer_pos_get_pos(BasePWidget *basep,
			       int *x, int *y,
			       int width, int height);

static void drawer_pos_hidebutton_click (BasePWidget *basep);

static void drawer_pos_pre_convert_hook (BasePWidget *basep);

static BasePPosClass *drawer_pos_parent_class;

GType
drawer_pos_get_type (void)
{
	static GType object_type = 0;
	if (object_type == 0) {
		static const GTypeInfo object_info = {
			sizeof (DrawerPosClass),
 	                (GBaseInitFunc)         NULL,
       		        (GBaseFinalizeFunc)     NULL,
       		        (GClassInitFunc)        drawer_pos_class_init,
                    	NULL,                   /* class_finalize */
                    	NULL,                   /* class_data */
                    	sizeof (DrawerPos),
                    	0,                      /* n_preallocs */
                    	(GInstanceInitFunc)    drawer_pos_instance_init 
		};
		object_type = g_type_register_static (BASEP_TYPE_POS, "DrawerPos", &object_info, 0);
		drawer_pos_parent_class = g_type_class_ref (BASEP_TYPE_POS);
	}

	return object_type;
}

static void
drawer_pos_class_init (DrawerPosClass *klass)
{
	/*GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);*/
	BasePPosClass *pos_class = BASEP_POS_CLASS(klass);

	/* fill out the virtual funcs */
	pos_class->set_hidebuttons = drawer_pos_set_hidebuttons;
	pos_class->get_applet_orient = drawer_pos_get_applet_orient;
	pos_class->get_size = NULL; /* the default is ok */
	pos_class->get_hide_orient = drawer_pos_get_hide_orient;
	pos_class->get_hide_pos = drawer_pos_get_hide_pos;
	pos_class->get_hide_size = drawer_pos_get_hide_size;
	pos_class->get_pos = drawer_pos_get_pos;

	pos_class->north_clicked = 
		pos_class->west_clicked = 
		pos_class->south_clicked = 
		pos_class->east_clicked =
		drawer_pos_hidebutton_click;
	pos_class->pre_convert_hook = drawer_pos_pre_convert_hook;
}

static void
drawer_pos_instance_init (DrawerPos *pos) { }

static void
drawer_pos_set_hidebuttons (BasePWidget *basep)
{
	switch(DRAWER_POS(basep->pos)->orient) {
	case PANEL_ORIENT_UP:
		gtk_widget_show(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);
		break;
	case PANEL_ORIENT_DOWN:
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_show(basep->hidebutton_s);
		break;
	case PANEL_ORIENT_LEFT:
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_hide(basep->hidebutton_e);
		gtk_widget_show(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);
		break;
	case PANEL_ORIENT_RIGHT:
		gtk_widget_hide(basep->hidebutton_n);
		gtk_widget_show(basep->hidebutton_e);
		gtk_widget_hide(basep->hidebutton_w);
		gtk_widget_hide(basep->hidebutton_s);
		break;
	}
}

static PanelOrient
drawer_pos_get_applet_orient (BasePWidget *basep)
{
	PanelWidget *panel = PANEL_WIDGET (basep->panel);
	GtkOrientation porient = panel->orient;
	int x,y;

	x = GTK_WIDGET(basep)->allocation.x;
	y = GTK_WIDGET(basep)->allocation.y;

	if(porient == GTK_ORIENTATION_VERTICAL) {
		if (x > (multiscreen_width (basep->screen, basep->monitor)/2 +
			 multiscreen_x (basep->screen, basep->monitor)))
			return PANEL_ORIENT_LEFT;
		else
			return PANEL_ORIENT_RIGHT;
	} else {
		if (y > (multiscreen_height (basep->screen, basep->monitor)/2 +
			 multiscreen_y (basep->screen, basep->monitor)))
			return PANEL_ORIENT_UP;
		else
			return PANEL_ORIENT_DOWN;
	}
}

static PanelOrient
drawer_pos_get_hide_orient (BasePWidget *basep)
{
	DrawerPos *pos = DRAWER_POS (basep->pos);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);

	switch (basep->state) {
	case BASEP_AUTO_HIDDEN:
		switch (pos->orient) {
		case PANEL_ORIENT_UP: return PANEL_ORIENT_DOWN;
		case PANEL_ORIENT_RIGHT: return PANEL_ORIENT_LEFT;
		case PANEL_ORIENT_DOWN: return PANEL_ORIENT_UP;
		case PANEL_ORIENT_LEFT: return PANEL_ORIENT_RIGHT;
		}
		g_assert_not_reached ();
		break;
	case BASEP_HIDDEN_LEFT:
		return (panel->orient == GTK_ORIENTATION_HORIZONTAL)
			? PANEL_ORIENT_LEFT : PANEL_ORIENT_UP;
	case BASEP_HIDDEN_RIGHT:
		return (panel->orient == GTK_ORIENTATION_HORIZONTAL)
			? PANEL_ORIENT_RIGHT : PANEL_ORIENT_DOWN;
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
	if (BASEP_IS_WIDGET (parentp))
		BASEP_WIDGET (parentp)->drawers_open++;
	basep_widget_explicit_show (BASEP_WIDGET (drawer));
}

void
drawer_widget_close_drawer (DrawerWidget *drawer, GtkWidget *parentp)
{
	BasePWidget *basep = BASEP_WIDGET (drawer);

	if (GTK_WIDGET (parentp)->window)
		gdk_window_raise (GTK_WIDGET (parentp)->window);

	switch (DRAWER_POS (basep->pos)->orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_LEFT:
		basep_widget_explicit_hide (basep, BASEP_HIDDEN_RIGHT);
		break;
	case PANEL_ORIENT_RIGHT:
	case PANEL_ORIENT_DOWN:
		basep_widget_explicit_hide (basep, BASEP_HIDDEN_LEFT);
		break;
	}

	if (BASEP_IS_WIDGET (parentp))
		BASEP_WIDGET (parentp)->drawers_open--;
}

static void
drawer_pos_hidebutton_click (BasePWidget *basep)
{
	Drawer *drawer = drawer_widget_get_drawer (DRAWER_WIDGET (basep));
	PanelWidget *panel = PANEL_WIDGET (drawer->button->parent);
	GtkWidget *parent = panel->panel_parent;

	drawer_widget_close_drawer (DRAWER_WIDGET (basep), parent);
	drawer->moving_focus = TRUE;
	gtk_window_present (GTK_WINDOW (parent));
	gtk_widget_grab_focus (drawer->button);
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
		*x = -ABS (width) - 1;
		*y = -ABS (height) - 1;
		return;
	}

	if (panel->master_widget &&
	    GTK_WIDGET_REALIZED (panel->master_widget) &&
	    /*"allocated" data will be set on each allocation, until then,
	      don't show the actual panel*/
	    g_object_get_data (G_OBJECT (panel->master_widget), "allocated")) {
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
		case PANEL_ORIENT_UP:
			*x = bx+(bw-width)/2;
			*y = py - height;
			break;
		case PANEL_ORIENT_DOWN:
			*x = bx+(bw-width)/2;
			*y = py + ph;
			break;
		case PANEL_ORIENT_LEFT:
			*x = px - width;
			*y = by+(bh-height)/2;
			break;
		case PANEL_ORIENT_RIGHT:
			*x = px + pw;
			*y = by+(bh-height)/2;
			break;
		}
	}
}

static void
drawer_pos_get_hide_pos (BasePWidget *basep,
			 PanelOrient hide_orient,
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
			  PanelOrient hide_orient,
			  int *w, int *h)
{
	switch (hide_orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_DOWN:
		*h = 1;
		break;
	case PANEL_ORIENT_RIGHT:
	case PANEL_ORIENT_LEFT:
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
drawer_widget_change_params (DrawerWidget        *drawer,
			     int                  screen,
			     int                  monitor,
			     PanelOrient          orient,
			     BasePMode            mode,
			     BasePState           state,
			     int                  sz,
			     gboolean             hidebuttons_enabled,
			     gboolean             hidebutton_pixmap_enabled,
			     PanelBackgroundType  back_type,
			     const char          *back_pixmap,
			     gboolean             fit_pixmap_bg,
			     gboolean             stretch_pixmap_bg,
			     gboolean             rotate_pixmap_bg,
			     PanelColor          *back_color)
{
	GtkOrientation porient;
	DrawerPos *pos = DRAWER_POS (BASEP_WIDGET (drawer)->pos);

	switch (orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_DOWN:
		porient = GTK_ORIENTATION_VERTICAL;
		break;
	case PANEL_ORIENT_LEFT:
	case PANEL_ORIENT_RIGHT:
	default:
		porient = GTK_ORIENTATION_HORIZONTAL;
		break;
	}

	if (PANEL_WIDGET (BASEP_WIDGET (drawer)->panel)->orient != porient)
		BASEP_WIDGET (drawer)->request_cube = TRUE;

	if (state != BASEP_WIDGET (drawer)->state ||
	    pos->orient != orient) {
		if (state == BASEP_HIDDEN_LEFT &&
		    (orient == PANEL_ORIENT_LEFT ||
		     orient == PANEL_ORIENT_UP))
			state = BASEP_HIDDEN_RIGHT;
		else if (state == BASEP_HIDDEN_RIGHT &&
			 (orient == PANEL_ORIENT_RIGHT ||
			  orient == PANEL_ORIENT_DOWN))
			 state = BASEP_HIDDEN_LEFT;
	}

	if (pos->orient != orient) {
		pos->orient = orient;
#if 0
		g_signal_emit (G_OBJECT (drawer),
			       drawer_pos_signals[ORIENT_CHANGE_SIGNAL],
			       0, orient);
#endif
	}

	basep_widget_change_params (BASEP_WIDGET (drawer),
				    screen,
				    monitor,
				    porient,
				    sz,
				    mode,
				    state,
				    hidebuttons_enabled,
				    hidebutton_pixmap_enabled,
				    back_type,
				    back_pixmap,
				    fit_pixmap_bg,
				    stretch_pixmap_bg,
				    rotate_pixmap_bg,
				    back_color);
				    
}

void
drawer_widget_change_orient (DrawerWidget *drawer,
			     PanelOrient orient)
{
	DrawerPos *pos = DRAWER_POS (drawer->pos);
	if (pos->orient != orient) {
		BasePWidget *basep = BASEP_WIDGET (drawer);
		PanelWidget *panel = PANEL_WIDGET (basep->panel);
		drawer_widget_change_params (drawer,
					     basep->screen,
					     basep->monitor,
					     orient,
					     basep->mode,
					     basep->state,
					     panel->sz,
					     basep->hidebuttons_enabled,
					     basep->hidebutton_pixmaps_enabled,
					     panel->background.type,
					     panel->background.image,
					     panel->background.fit_image,
					     panel->background.stretch_image,
					     panel->background.rotate_image,
					     &panel->background.color);
	}
}

GtkWidget *
drawer_widget_new (const char          *panel_id,
		   int                  screen,
		   int                  monitor,
		   PanelOrient          orient,
		   BasePMode            mode,
		   BasePState           state,
		   int                  sz,
		   gboolean             hidebuttons_enabled,
		   gboolean             hidebutton_pixmap_enabled,
		   PanelBackgroundType  back_type,
		   const char          *back_pixmap,
		   gboolean             fit_pixmap_bg,
		   gboolean             stretch_pixmap_bg,
		   gboolean             rotate_pixmap_bg,
		   PanelColor          *back_color)
{
	DrawerWidget *drawer;
	DrawerPos *pos;
	GtkOrientation porient;

	drawer = g_object_new (DRAWER_TYPE_WIDGET, NULL);
	drawer->pos = g_object_new (DRAWER_TYPE_POS, NULL);
	pos = DRAWER_POS (drawer->pos);
	pos->orient = orient;

	switch (orient) {
	case PANEL_ORIENT_UP:
	case PANEL_ORIENT_DOWN:
		porient = GTK_ORIENTATION_VERTICAL;
		break;
	default:
		porient = GTK_ORIENTATION_HORIZONTAL;
		break;
	}

	basep_widget_construct (panel_id, 
				BASEP_WIDGET (drawer),
				TRUE, TRUE,
				screen,
				monitor,
				porient,
				sz, mode, state,
				hidebuttons_enabled,
				hidebutton_pixmap_enabled,
				back_type,
				back_pixmap,
				fit_pixmap_bg,
				stretch_pixmap_bg,
				rotate_pixmap_bg,
				back_color);
	return GTK_WIDGET (drawer);
}

void
drawer_widget_set_drawer (DrawerWidget *widget,
			  Drawer       *drawer)
{
	g_object_set_data (G_OBJECT (widget), "drawer-panel", drawer);
}

Drawer *
drawer_widget_get_drawer (DrawerWidget *widget)
{
	return g_object_get_data (G_OBJECT (widget), "drawer-panel");
}
