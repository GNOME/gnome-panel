/* Gnome panel: edge (snapped) widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#include "edge-widget.h"
#include "panel_config_global.h"
#include "foobar-widget.h"

extern GlobalConfig global_config;
extern int pw_minimized_size;

static void edge_pos_class_init (EdgePosClass *klass);
static void edge_pos_init (EdgePos *pos);

static void edge_pos_set_pos (BasePWidget *basep,
			      int x, int y,
			      int w, int h);
static void edge_pos_get_pos (BasePWidget *basep,
			      int *x, int *y,
			      int w, int h);

static void edge_pos_get_size (BasePWidget *basep,
			       int *w, int *h);

static void edge_pos_pre_convert_hook (BasePWidget *basep);

static BorderPosClass *parent_class;

GtkType
edge_pos_get_type ()
{
	static GtkType edge_pos_type = 0;

	if (edge_pos_type == 0) {
		GtkTypeInfo edge_pos_info = {
			"EdgePos",
			sizeof (EdgePos),
			sizeof (EdgePosClass),
			(GtkClassInitFunc) edge_pos_class_init,
			(GtkObjectInitFunc) edge_pos_init,
			NULL,
			NULL,
			NULL
		};

		edge_pos_type = gtk_type_unique (TYPE_BORDER_POS,
						 &edge_pos_info);
	}
			       
	return edge_pos_type;
}

static void
edge_pos_class_init (EdgePosClass *klass)
{
	BasePPosClass *pos_class = BASEP_POS_CLASS(klass);

	parent_class = gtk_type_class(TYPE_BORDER_POS);

	pos_class->set_pos = edge_pos_set_pos;
	pos_class->get_pos = edge_pos_get_pos;
	pos_class->get_size = edge_pos_get_size;
	pos_class->pre_convert_hook = edge_pos_pre_convert_hook;
}

static void
edge_pos_init (EdgePos *pos) { }

static void
edge_pos_set_pos (BasePWidget *basep,
		  int x, int y,
		  int w, int h)
{
	BorderEdge newloc;
	int minx, miny, maxx, maxy;

	gdk_window_get_geometry (GTK_WIDGET(basep)->window,
				 &minx, &miny, &maxx, &maxy, NULL);
	gdk_window_get_origin(GTK_WIDGET(basep)->window,
			      &minx, &miny);

	newloc = BORDER_POS(basep->pos)->edge;

	maxx += minx;
	maxy += miny;

	if (x >= minx && 
	    x <= maxx &&
	    y >= miny &&
	    y <= maxy)
		return;

	if ( x>(gdk_screen_width()/3) &&
	     x<(2*gdk_screen_width()/3) &&
	     y>(gdk_screen_height()/3) &&
	     y<(2*gdk_screen_height()/3))
		return;

	if ((x) * gdk_screen_height() > y * gdk_screen_width() ) {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() )
			newloc = BORDER_TOP;
		else
			newloc = BORDER_RIGHT;
	} else {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() )
			newloc = BORDER_LEFT;
		else
			newloc = BORDER_BOTTOM;
	}
	if(newloc != BORDER_POS(basep->pos)->edge)
		border_widget_change_edge(BORDER_WIDGET(basep), 
					  newloc);
}

static void
edge_pos_get_pos (BasePWidget *basep, int *x, int *y,
		  int w, int h)
{
	BorderEdge edge;

	*x = *y = 0;

	edge = BORDER_POS(basep->pos)->edge;

	switch (edge) {
	case BORDER_RIGHT:
		basep_border_get (BORDER_TOP, NULL, NULL, y);
		*y += foobar_widget_get_height ();
		*x = gdk_screen_width() - w;
		break;
	case BORDER_LEFT:
		basep_border_get (BORDER_TOP, y, NULL, NULL);
		*y += foobar_widget_get_height ();
		break;
	case BORDER_TOP:
		*y = foobar_widget_get_height ();
		break;
	case BORDER_BOTTOM:
		*y = gdk_screen_height() - h;
		break;
	}

	basep_border_queue_recalc ();
}

static void
edge_pos_get_size (BasePWidget *basep, int *w, int *h)
{
	int a, b;

	BorderEdge edge = BORDER_POS(basep->pos)->edge;

	switch (edge) {
	case BORDER_RIGHT:
		basep_border_get (BORDER_TOP, NULL, NULL, &a);
		basep_border_get (BORDER_BOTTOM, NULL, NULL, &b);
		*h = gdk_screen_height() - foobar_widget_get_height () - a - b;
		break;
	case BORDER_LEFT:
		basep_border_get (BORDER_TOP, &a, NULL, NULL);
		basep_border_get (BORDER_BOTTOM, &b, NULL, NULL);
		*h = gdk_screen_height() - foobar_widget_get_height () - a - b;
		break;
	case BORDER_TOP:
	case BORDER_BOTTOM:
		*w = gdk_screen_width();
		break;
	}
}

static void
edge_pos_pre_convert_hook (BasePWidget *basep)
{
	basep->keep_in_screen = TRUE;
	PANEL_WIDGET (basep->panel)->packed = FALSE;
}

GtkWidget *
edge_widget_new (BorderEdge edge,
		 BasePMode mode,
		 BasePState state,
		 int sz,
		 int hidebuttons_enabled,
		 int hidebutton_pixmaps_enabled,
		 PanelBackType back_type,
		 char *back_pixmap,
		 gboolean fit_pixmap_bg,
		 gboolean strech_pixmap_bg,
		 gboolean rotate_pixmap_bg,
		 GdkColor *back_color)
{
	EdgeWidget *edgew = gtk_type_new (TYPE_EDGE_WIDGET);
	BasePWidget *basep = BASEP_WIDGET (edgew);

	basep->pos = gtk_type_new (TYPE_EDGE_POS);

	border_widget_construct (BORDER_WIDGET (basep), 
				 edge, 
				 TRUE, FALSE,
				 sz, mode, state,
				 hidebuttons_enabled,
				 hidebutton_pixmaps_enabled,
				 back_type, back_pixmap,
				 fit_pixmap_bg, strech_pixmap_bg,
				 rotate_pixmap_bg,
				 back_color);

	PANEL_WIDGET (basep->panel)->packed = FALSE;
	
	return GTK_WIDGET (edgew);
}
