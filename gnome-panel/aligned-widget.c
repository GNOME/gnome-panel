/* Gnome panel: aligned (corner) widget
 * (C) 1999 the Free Software Foundation
 * 
 * Authores:  Jacob Berkman
 *            George Lebl
 */

#include "config.h"
#include "panel-marshal.h"
#include "aligned-widget.h"
#include "panel-config-global.h"
#include "foobar-widget.h"
#include "multiscreen-stuff.h"
#include "panel-typebuiltins.h"

static void aligned_pos_class_init (AlignedPosClass *klass);
static void aligned_pos_instance_init (AlignedPos *pos);

static void aligned_pos_set_pos (BasePWidget *basep,
				 int x, int y,
				 int w, int h,
				 gboolean force);
static void aligned_pos_get_pos (BasePWidget *basep,
				 int *x, int *y,
				 int w, int h);

static void aligned_pos_show_hide_left (BasePWidget *basep);
static void aligned_pos_show_hide_right (BasePWidget *basep);
static BorderPosClass *aligned_pos_parent_class;

GType
aligned_pos_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) {
		static const GTypeInfo object_info = {
                    sizeof (AlignedPosClass),
                    (GBaseInitFunc)         NULL,
                    (GBaseFinalizeFunc)     NULL,
                    (GClassInitFunc)        aligned_pos_class_init,
                    NULL,                   /* class_finalize */
                    NULL,                   /* class_data */
                    sizeof (AlignedPos),
                    0,                      /* n_preallocs */
                    (GInstanceInitFunc)     aligned_pos_instance_init
		};

		object_type = g_type_register_static (BORDER_TYPE_POS, "AlignedPos", &object_info, 0);
		aligned_pos_parent_class = g_type_class_ref (BORDER_TYPE_POS);
	}
			       
	return object_type;
}

enum {
	ALIGN_CHANGE_SIGNAL,
	LAST_SIGNAL
};

static guint aligned_pos_signals[LAST_SIGNAL] = { 0 };

static void
aligned_pos_class_init (AlignedPosClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BasePPosClass *pos_class = BASEP_POS_CLASS (klass);

	
	aligned_pos_signals[ALIGN_CHANGE_SIGNAL] =
		g_signal_new ("align_change",
			      G_SIGNAL_RUN_LAST,
			      G_TYPE_FROM_CLASS (object_class),
			      G_STRUCT_OFFSET (AlignedPosClass, align_change),
			      NULL,
			      NULL,
			      panel_marshal_VOID__ENUM,
			      G_TYPE_NONE,
			      1, 
			      PANEL_TYPE_ALIGNED_ALIGNMENT);

	pos_class->set_pos = aligned_pos_set_pos;
	pos_class->get_pos = aligned_pos_get_pos;
	pos_class->north_clicked = pos_class->west_clicked = 
		aligned_pos_show_hide_left;
	pos_class->south_clicked = pos_class->east_clicked =
		aligned_pos_show_hide_right;
}

static void
aligned_pos_instance_init (AlignedPos *pos) {
	/* Does nothing */
}

static void
aligned_pos_set_pos (BasePWidget *basep,
		     int x, int y,
		     int w, int h,
		     gboolean force)
{
	int innerx, innery;
	int screen_width, screen_height;

	BorderEdge newpos = BORDER_POS(basep->pos)->edge;
	AlignedAlignment newalign = ALIGNED_POS(basep->pos)->align;

	if ( ! force) {
		int minx, miny, maxx, maxy;
		gdk_window_get_geometry (GTK_WIDGET(basep)->window,
					 &minx, &miny, &maxx, &maxy, NULL);
		gdk_window_get_origin (GTK_WIDGET(basep)->window, &minx, &miny);
		maxx += minx;
		maxy += miny;
		if (x >= minx &&
		    x <= maxx &&
		    y >= miny &&
		    y <= maxy)
			return;
	}
	
	innerx = x - multiscreen_x (basep->screen);
	innery = y - multiscreen_y (basep->screen);
	screen_width = multiscreen_width (basep->screen);
	screen_height = multiscreen_height (basep->screen);

	/*if in the inner 1/3rd, don't change to avoid fast flickery
	  movement*/
	if ( innerx > (screen_width / 3) &&
	     innerx < (2*screen_width / 3) &&
	     innery > (screen_height / 3) &&
	     innery < (2*screen_height / 3))
		return;

	if (innerx * screen_height > innery * screen_width ) {
		if (screen_height * (screen_width-innerx) >
		    innery * screen_width ) {
			newpos = BORDER_TOP;
			if (innerx < screen_width/3)
				newalign = ALIGNED_LEFT;
			else if (innerx < 2*screen_width/3)
				newalign = ALIGNED_CENTER;
			else
				newalign = ALIGNED_RIGHT;
		} else {
			newpos = BORDER_RIGHT;
			if (innery < screen_height/3)
				newalign = ALIGNED_LEFT;
			else if (innery < 2*screen_height/3)
				newalign = ALIGNED_CENTER;
			else
				newalign = ALIGNED_RIGHT;
		}
	} else {
		if (screen_height * (screen_width-innerx) >
		    innery * screen_width ) {
			newpos = BORDER_LEFT;
			if (innery < screen_height/3)
				newalign = ALIGNED_LEFT;
			else if (innery < 2*screen_height/3)
				newalign = ALIGNED_CENTER;
			else
				newalign = ALIGNED_RIGHT;
		} else {
			newpos = BORDER_BOTTOM;
			if (innerx < screen_width/3)
				newalign = ALIGNED_LEFT;
			else if (innerx < 2*screen_width/3)
				newalign = ALIGNED_CENTER;
			else
				newalign = ALIGNED_RIGHT;
		}
	}

	if(newalign != ALIGNED_POS(basep->pos)->align)
		aligned_widget_change_align (ALIGNED_WIDGET(basep), newalign);
	
	if(newpos != BORDER_POS(basep->pos)->edge)
		border_widget_change_edge (BORDER_WIDGET(basep), newpos);

}

static void
aligned_pos_get_pos (BasePWidget *basep, int *x, int *y,
		     int w, int h)
{
	int a, b;
	BorderEdge edge = BORDER_POS(basep->pos)->edge;

	*x = *y = 0;
	switch (edge) {
	case BORDER_BOTTOM:
		*y = multiscreen_height(basep->screen) - h - foobar_widget_get_height (basep->screen);
		/* fall thru */
	case BORDER_TOP:
		*y += foobar_widget_get_height (basep->screen);
		switch (ALIGNED_POS(basep->pos)->align) {
		case ALIGNED_LEFT:
			break;
		case ALIGNED_CENTER:
			*x = (multiscreen_width(basep->screen) - w) / 2;
			break;
		case ALIGNED_RIGHT:
			*x = multiscreen_width(basep->screen) - w;
			break;
		}
		break;
	case BORDER_RIGHT:
		*x = multiscreen_width(basep->screen) - w;
		basep_border_get (basep->screen, BORDER_TOP, NULL, NULL, &a);
		basep_border_get (basep->screen, BORDER_BOTTOM, NULL, NULL, &b);
		switch (ALIGNED_POS(basep->pos)->align) {
		case ALIGNED_LEFT:
			*y = foobar_widget_get_height (basep->screen) + a;
			break;
		case ALIGNED_CENTER:
			*y = (multiscreen_height(basep->screen) - h) / 2;
			break;
		case ALIGNED_RIGHT:
			*y = multiscreen_height(basep->screen) - h - b;
			break;
		}
		break;
	case BORDER_LEFT:
		basep_border_get (basep->screen, BORDER_TOP, &a, NULL, NULL);
		basep_border_get (basep->screen, BORDER_BOTTOM, &b, NULL, NULL);
		switch (ALIGNED_POS(basep->pos)->align) {
		case ALIGNED_LEFT:
			*y = foobar_widget_get_height (basep->screen) + a;
			break;
		case ALIGNED_CENTER:
			*y = (multiscreen_height(basep->screen) - h) / 2;
			break;
		case ALIGNED_RIGHT:
			*y = multiscreen_height(basep->screen) - h - b;
			break;
		}
		break;
	}

	*x += multiscreen_x (basep->screen);
	*y += multiscreen_y (basep->screen);

	basep_border_queue_recalc (basep->screen);
}

static void
aligned_pos_show_hide_left (BasePWidget *basep)
{
	switch (basep->state) {
	case BASEP_SHOWN:
		if (ALIGNED_POS (basep->pos)->align == ALIGNED_RIGHT)
			aligned_widget_change_align (ALIGNED_WIDGET (basep), 
						     ALIGNED_LEFT);
		else 
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
aligned_pos_show_hide_right (BasePWidget *basep)
{
	switch (basep->state) {
	case BASEP_SHOWN:
		if (ALIGNED_POS (basep->pos)->align == ALIGNED_LEFT)
			aligned_widget_change_align (ALIGNED_WIDGET (basep), 
						     ALIGNED_RIGHT);
		else
			basep_widget_explicit_hide (basep, BASEP_HIDDEN_RIGHT);
		break;
	case BASEP_HIDDEN_LEFT:
		basep_widget_explicit_show (basep);
		break;
	default:
		break;
	}
}

void
aligned_widget_change_params (AlignedWidget *aligned,
			      int screen,
			      AlignedAlignment align,
			      BorderEdge edge,
			      int sz,
			      BasePMode mode,
			      BasePState state,
			      gboolean hidebuttons_enabled,
			      gboolean hidebutton_pixmaps_enabled,
			      PanelBackType back_type,
			      char *pixmap_name,
			      gboolean fit_pixmap_bg,
			      gboolean strech_pixmap_bg,
			      gboolean rotate_pixmap_bg,
			      GdkColor *back_color)
{
	AlignedPos *pos = ALIGNED_POS (BASEP_WIDGET (aligned)->pos);

	if (pos->align != align) {
		pos->align = align;
		g_signal_emit (G_OBJECT (pos),
			       aligned_pos_signals[ALIGN_CHANGE_SIGNAL],
			       0, align);
	}

	border_widget_change_params (BORDER_WIDGET (aligned),
				     screen,
				     edge,
				     sz,
				     mode,
				     state,
				     hidebuttons_enabled,
				     hidebutton_pixmaps_enabled,
				     back_type,
				     pixmap_name,
				     fit_pixmap_bg,
				     strech_pixmap_bg,
				     rotate_pixmap_bg,
				     back_color);
}


void 
aligned_widget_change_align (AlignedWidget *aligned,
			     AlignedAlignment align)
{
	BasePWidget *basep = BASEP_WIDGET (aligned);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);
	AlignedPos *pos = ALIGNED_POS (basep->pos);

	if (pos->align == align)
		return;

	aligned_widget_change_params (aligned,
				      basep->screen,
				      align,
				      BORDER_POS (pos)->edge,
				      panel->sz, basep->mode,
				      basep->state,
				      basep->hidebuttons_enabled,
				      basep->hidebutton_pixmaps_enabled,
				      panel->back_type,
				      panel->back_pixmap,
				      panel->fit_pixmap_bg,
				      panel->strech_pixmap_bg,
				      panel->rotate_pixmap_bg,
				      &panel->back_color);
}

void 
aligned_widget_change_align_edge (AlignedWidget *aligned,
				  AlignedAlignment align,
				  BorderEdge edge)
{
	BasePWidget *basep = BASEP_WIDGET (aligned);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);

	aligned_widget_change_params (aligned,
				      basep->screen,
				      align,
				      edge,
				      panel->sz,
				      basep->mode,
				      basep->state,
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
aligned_widget_new (int screen,
		    AlignedAlignment align,
		    BorderEdge edge,
		    BasePMode mode,
		    BasePState state,
		    int sz,
		    gboolean hidebuttons_enabled,
		    gboolean hidebutton_pixmaps_enabled,
		    PanelBackType back_type,
		    char *back_pixmap,
		    gboolean fit_pixmap_bg,
		    gboolean strech_pixmap_bg,
		    gboolean rotate_pixmap_bg,
		    GdkColor *back_color)
{
	AlignedWidget *aligned = gtk_type_new (ALIGNED_TYPE_WIDGET);
	AlignedPos *pos = gtk_type_new (ALIGNED_TYPE_POS);
	pos->align = align;

	BASEP_WIDGET (aligned)->pos = BASEP_POS (pos);

	border_widget_construct (BORDER_WIDGET (aligned),
				 screen,
				 edge, 
				 TRUE,
				 FALSE,
				 sz,
				 mode,
				 state,
				 hidebuttons_enabled,
				 hidebutton_pixmaps_enabled,
				 back_type,
				 back_pixmap,
				 fit_pixmap_bg,
				 strech_pixmap_bg,
				 rotate_pixmap_bg,
				 back_color);

	return GTK_WIDGET (aligned);
}
