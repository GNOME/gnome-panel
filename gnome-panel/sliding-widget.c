/* Gnome panel: sliding widget
 * (C) 1999 the Free Software Foundation
 * 
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#include "config.h"

#include "sliding-widget.h"
#include "panel-config-global.h"
#include "foobar-widget.h"
#include "panel-marshal.h"
#include "panel-util.h"
#include "multiscreen-stuff.h"
#include "panel-typebuiltins.h"

static void sliding_pos_class_init (SlidingPosClass *klass);
static void sliding_pos_instance_init (SlidingPos *pos);

static void sliding_pos_set_pos (BasePWidget *basep,
				 int x, int y,
				 int w, int h,
				 gboolean force);
static void sliding_pos_get_pos (BasePWidget *basep,
				 int *x, int *y,
				 int w, int h);

static BorderPosClass *sliding_pos_parent_class;

GType
sliding_pos_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) {
		static const GTypeInfo object_info = {
                    sizeof (SlidingPosClass),
                    (GBaseInitFunc)         NULL,
                    (GBaseFinalizeFunc)     NULL,
                    (GClassInitFunc)        sliding_pos_class_init,
                    NULL,                   /* class_finalize */
                    NULL,                   /* class_data */
                    sizeof (SlidingPos),
                    0,                      /* n_preallocs */
                    (GInstanceInitFunc)     sliding_pos_instance_init 
		};

		object_type = g_type_register_static (BORDER_TYPE_POS, "SlidingPos", &object_info, 0);
		sliding_pos_parent_class = g_type_class_ref (BORDER_TYPE_POS);
	}
			       
	return object_type;
}

enum {
	ANCHOR_CHANGE_SIGNAL,
	OFFSET_CHANGE_SIGNAL,
	LAST_SIGNAL
};

static guint sliding_pos_signals[LAST_SIGNAL] = { 0, 0 };

static void
sliding_pos_class_init (SlidingPosClass *klass)
{
	BasePPosClass *pos_class = BASEP_POS_CLASS(klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	sliding_pos_signals[ANCHOR_CHANGE_SIGNAL] =
                g_signal_new ("anchor_change",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (SlidingPosClass, anchor_change),
                              NULL,
                              NULL, 
                              panel_marshal_VOID__ENUM,
                              G_TYPE_NONE,
                              1,
                              PANEL_TYPE_SLIDING_ANCHOR);

	sliding_pos_signals[OFFSET_CHANGE_SIGNAL] =
                g_signal_new ("offset_change",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (SlidingPosClass, offset_change),
                              NULL,
                              NULL, 
                              panel_marshal_VOID__ENUM,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_INT, G_TYPE_INT); 

	pos_class->set_pos = sliding_pos_set_pos;
	pos_class->get_pos = sliding_pos_get_pos;
}

static void
sliding_pos_instance_init (SlidingPos *pos) { }

static void
sliding_pos_set_pos (BasePWidget *basep,
		     int x, int y,
		     int w, int h,
		     gboolean force)
{
	int minx, miny, maxx, maxy, offset_x, offset_y;
	SlidingPos *pos = SLIDING_POS(basep->pos);
	BorderEdge newedge = BORDER_POS(basep->pos)->edge;
	SlidingAnchor newanchor = pos->anchor;
	gint16 newoffset = pos->offset;
	gboolean check_pos = TRUE;
	int innerx, innery;
	int screen_width, screen_height;

	gdk_window_get_geometry (GTK_WIDGET(basep)->window, &minx, &miny,
				 &maxx, &maxy, NULL);
	gdk_window_get_origin (GTK_WIDGET(basep)->window, &minx, &miny);
	maxx += minx;
	maxy += miny;

	/* FIXME: how does screenchanging interact with depending on
	 * the above positions of the actual X windows stuff */
	
	innerx = x - multiscreen_x (basep->screen);
	innery = y - multiscreen_y (basep->screen);
	screen_width = multiscreen_width (basep->screen);
	screen_height = multiscreen_height (basep->screen);

	/*if in the inner 1/3rd, don't change to avoid fast flickery
	  movement*/
	if (innerx > (screen_width / 3) &&
	    innerx < (2*screen_width / 3) &&
	    innery > (screen_height / 3) &&
	    innery < (2*screen_height / 3))
		return;

	/* don't switch the position if we are along the edge.
	   do this so that it won't flip-flop orientations in
	   the corners */
	switch (BORDER_POS (pos)->edge) {
	case BORDER_TOP:
		check_pos = (y > maxy);
		break;
	case BORDER_BOTTOM:
		check_pos = (y < miny);
		break;
	case BORDER_LEFT:
		check_pos = (x > maxx);
		break;
	case BORDER_RIGHT:
		check_pos = (x < minx);
		break;
	}

	if (check_pos) {
		if (innerx * screen_height > innery * screen_width ) {
			if(screen_height * (screen_width - innerx) >
			   innery * screen_width)
				newedge = BORDER_TOP;
			else
				newedge = BORDER_RIGHT;
		} else {
			if(screen_height * (screen_width - innerx) >
			   innery * screen_width)
				newedge = BORDER_LEFT;
			else
				newedge = BORDER_BOTTOM;
		}

		/* we need to do this since the sizes might have changed 
		   (orientation changes and what not) */
		if(newedge != BORDER_POS(basep->pos)->edge) {
			GtkOrientation old_orient;
			old_orient = PANEL_WIDGET (basep->panel)->orient;

			border_widget_change_edge (BORDER_WIDGET(basep),
						   newedge);
			basep_widget_get_size (basep, &w, &h);

			/* if change of orient, swap offsets */
			if (old_orient != PANEL_WIDGET (basep->panel)->orient) {
				int tmp = basep->offset_x;
				basep->offset_x = basep->offset_y;
				basep->offset_y = tmp;
			}
		}
	}

	offset_x = basep->offset_x;
	if (offset_x > w)
		offset_x = 0.99 * w; /* not completely on the edge */
	offset_y = basep->offset_y;
	if (offset_y > h)
		offset_y = 0.99 * h; /* not completely on the edge */

	g_assert (newedge == BORDER_POS (pos)->edge);
	g_assert (newanchor == pos->anchor);

	switch (PANEL_WIDGET (basep->panel)->orient) {
	case GTK_ORIENTATION_HORIZONTAL:
		newanchor =  (innerx < 0.1 * screen_width) 
			? SLIDING_ANCHOR_LEFT
			: ( (innerx > 0.9 * screen_width)
			    ? SLIDING_ANCHOR_RIGHT
			    : newanchor);
		if (newanchor == SLIDING_ANCHOR_LEFT) {
			newoffset = innerx - offset_x;
			if (basep->state == BASEP_HIDDEN_RIGHT)
				newoffset -= w - get_requisition_width (basep->hidebutton_e);
		} else {
			newoffset = screen_width -
				(innerx - offset_x) - w;
			if (basep->state == BASEP_HIDDEN_LEFT)
				newoffset -= w - get_requisition_width (basep->hidebutton_w);
		}
		newoffset = CLAMP (newoffset, 0, screen_width - w);
		break;
	case GTK_ORIENTATION_VERTICAL:
		newanchor =  (innery < 0.1 * screen_height) 
			? SLIDING_ANCHOR_LEFT
			: ( (innery > 0.9 * screen_height)
			    ? SLIDING_ANCHOR_RIGHT
			    : newanchor);
		if (newanchor == SLIDING_ANCHOR_LEFT) {
			newoffset = innery - offset_y;
			if (basep->state == BASEP_HIDDEN_RIGHT)
				newoffset -= h - get_requisition_height (basep->hidebutton_s);
		} else {
			newoffset = screen_height -
				(innery - offset_y) - h;
			if (basep->state == BASEP_HIDDEN_LEFT)
				newoffset -= h - get_requisition_height (basep->hidebutton_n);
		}
		newoffset = CLAMP (newoffset, 0, screen_height - h);
		break;
	}

	if (newanchor != pos->anchor) 
		sliding_widget_change_anchor (SLIDING_WIDGET (basep), 
					      newanchor);

	if (newoffset != pos->offset)
		sliding_widget_change_offset (SLIDING_WIDGET (basep),
					      newoffset);
}

static void
sliding_pos_get_pos (BasePWidget *basep, int *x, int *y,
		     int w, int h)
{
	SlidingPos *pos = SLIDING_POS (basep->pos);
	*x = *y = 0;

	switch (BORDER_POS (basep->pos)->edge) {
	case BORDER_BOTTOM:
		*y = multiscreen_height (basep->screen) - h - foobar_widget_get_height (basep->screen);
		/* fall through */
	case BORDER_TOP:
		(*y) += foobar_widget_get_height (basep->screen);
		*x = (pos->anchor == SLIDING_ANCHOR_LEFT)
			? pos->offset
			: multiscreen_width (basep->screen) - pos->offset - w;
		break;
	case BORDER_RIGHT:
		*x = multiscreen_width (basep->screen) - w;
                /* fall through */
	case BORDER_LEFT:
		*y = (pos->anchor == SLIDING_ANCHOR_LEFT)
			? pos->offset
			: multiscreen_height (basep->screen) - pos->offset - h;
		*y = MAX (*y, foobar_widget_get_height (basep->screen));
		break;
	}

	*x += multiscreen_x (basep->screen);
	*y += multiscreen_y (basep->screen);
}

GtkWidget *
sliding_widget_new (gchar *panel_id,
		    int screen,
		    SlidingAnchor anchor,
		    gint16 offset,
		    BorderEdge edge,
		    BasePMode mode,
		    BasePState state,
		    int sz,
		    gboolean hidebuttons_enabled,
		    gboolean hidebutton_pixmaps_enabled,
		    PanelBackType back_type,
		    char *back_pixmap,
		    gboolean fit_pixmap_bg,
		    gboolean stretch_pixmap_bg,
		    gboolean rotate_pixmap_bg,
		    GdkColor *back_color)
{
	SlidingWidget *sliding = gtk_type_new (SLIDING_TYPE_WIDGET);
	SlidingPos *pos = gtk_type_new (SLIDING_TYPE_POS);

	pos->anchor = anchor;
	pos->offset = offset;
	
	BASEP_WIDGET (sliding)->pos = BASEP_POS (pos);

	border_widget_construct (panel_id,
				 BORDER_WIDGET (sliding),
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
				 fit_pixmap_bg, stretch_pixmap_bg,
				 rotate_pixmap_bg,
				 back_color);

	return GTK_WIDGET (sliding);
}

void 
sliding_widget_change_params (SlidingWidget *sliding,
			      int screen,
			      SlidingAnchor anchor,
			      gint16 offset,
			      BorderEdge edge,
			      int sz,
			      BasePMode mode,
			      BasePState state,
			      gboolean hidebuttons_enabled,
			      gboolean hidebutton_pixmaps_enabled,
			      PanelBackType back_type,
			      char *pixmap_name,
			      gboolean fit_pixmap_bg,
			      gboolean stretch_pixmap_bg,
			      gboolean rotate_pixmap_bg,
			      GdkColor *back_color)
{
	SlidingPos *pos = SLIDING_POS (BASEP_WIDGET (sliding)->pos);

	if (anchor != pos->anchor) {
		pos->anchor = anchor;
		g_signal_emit (G_OBJECT (pos),
			       sliding_pos_signals[ANCHOR_CHANGE_SIGNAL],
			       0, anchor);
		
	}

	if (offset != pos->offset) {
		pos->offset = offset;
		g_signal_emit (G_OBJECT (pos),
			       sliding_pos_signals[OFFSET_CHANGE_SIGNAL],
			       0, offset);
	}

	border_widget_change_params (BORDER_WIDGET (sliding),
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
				     stretch_pixmap_bg,
				     rotate_pixmap_bg,
				     back_color);
}

void
sliding_widget_change_offset (SlidingWidget *sliding, gint16 offset)
{
	BasePWidget *basep = BASEP_WIDGET (sliding);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);
	SlidingPos *pos = SLIDING_POS (basep->pos);

	if (offset == pos->offset)
		return;

	sliding_widget_change_params (sliding,
				      basep->screen,
				      pos->anchor,
				      offset,
				      BORDER_POS (pos)->edge,
				      panel->sz,
				      basep->mode,
				      basep->state,
				      basep->hidebuttons_enabled,
				      basep->hidebutton_pixmaps_enabled,
				      panel->back_type,
				      panel->back_pixmap,
				      panel->fit_pixmap_bg,
				      panel->stretch_pixmap_bg,
				      panel->rotate_pixmap_bg,
				      &panel->back_color);
}

void 
sliding_widget_change_anchor (SlidingWidget *sliding, SlidingAnchor anchor) 
{
	BasePWidget *basep = BASEP_WIDGET (sliding);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);
	SlidingPos *pos = SLIDING_POS (basep->pos);

	if (anchor == pos->anchor)
		return;

	sliding_widget_change_params (sliding,
				      basep->screen,
				      anchor,
				      pos->offset,
				      BORDER_POS (pos)->edge,
				      panel->sz,
				      basep->mode,
				      basep->state,
				      basep->hidebuttons_enabled,
				      basep->hidebutton_pixmaps_enabled,
				      panel->back_type,
				      panel->back_pixmap,
				      panel->fit_pixmap_bg,
				      panel->stretch_pixmap_bg,
				      panel->rotate_pixmap_bg,
				      &panel->back_color);
}

void 
sliding_widget_change_anchor_offset_edge (SlidingWidget *sliding, 
					  SlidingAnchor anchor, 
					  gint16 offset,
					  BorderEdge edge) 
{
	BasePWidget *basep = BASEP_WIDGET (sliding);
	PanelWidget *panel = PANEL_WIDGET (basep->panel);

	sliding_widget_change_params (sliding,
				      basep->screen,
				      anchor,
				      offset,
				      edge,
				      panel->sz,
				      basep->mode,
				      basep->state,
				      basep->hidebuttons_enabled,
				      basep->hidebutton_pixmaps_enabled,
				      panel->back_type,
				      panel->back_pixmap,
				      panel->fit_pixmap_bg,
				      panel->stretch_pixmap_bg,
				      panel->rotate_pixmap_bg,
				      &panel->back_color);
}
