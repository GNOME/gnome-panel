/* Gnome panel: aligned (corner) widget
 * (C) 1999 the Free Software Foundation
 * 
 * Authores:  Jacob Berkman
 *            George Lebl
 */

#include "aligned-widget.h"
#include "panel_config_global.h"
#include "foobar-widget.h"

extern GlobalConfig global_config;
extern int pw_minimized_size;

static void aligned_pos_class_init (AlignedPosClass *klass);
static void aligned_pos_init (AlignedPos *pos);

static void aligned_pos_set_pos (BasePWidget *basep,
				 int x, int y,
				 int w, int h);
static void aligned_pos_get_pos (BasePWidget *basep,
				 int *x, int *y,
				 int w, int h);

static void aligned_pos_show_hide_left (BasePWidget *basep);
static void aligned_pos_show_hide_right (BasePWidget *basep);
static BorderPosClass *parent_class;

GtkType
aligned_pos_get_type (void)
{
	static GtkType aligned_pos_type = 0;

	if (aligned_pos_type == 0) {
		GtkTypeInfo aligned_pos_info = {
			"AlignedPos",
			sizeof (AlignedPos),
			sizeof (AlignedPosClass),
			(GtkClassInitFunc) aligned_pos_class_init,
			(GtkObjectInitFunc) aligned_pos_init,
			NULL,
			NULL,
			NULL
		};

		aligned_pos_type = gtk_type_unique (TYPE_BORDER_POS,
						    &aligned_pos_info);
	}
			       
	return aligned_pos_type;
}

enum {
	ALIGN_CHANGE_SIGNAL,
	LAST_SIGNAL
};

static guint aligned_pos_signals[LAST_SIGNAL] = { 0 };

static void
aligned_pos_class_init (AlignedPosClass *klass)
{
	BasePPosClass *pos_class = BASEP_POS_CLASS(klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class(TYPE_BORDER_POS);
	
	aligned_pos_signals[ALIGN_CHANGE_SIGNAL] =
		gtk_signal_new ("align_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (AlignedPosClass,
						   align_change),
				gtk_marshal_NONE__ENUM,
				GTK_TYPE_NONE,
				1, GTK_TYPE_ENUM);

	gtk_object_class_add_signals (object_class,
				      aligned_pos_signals,
				      LAST_SIGNAL);

	pos_class->set_pos = aligned_pos_set_pos;
	pos_class->get_pos = aligned_pos_get_pos;
	pos_class->north_clicked = pos_class->west_clicked = 
		aligned_pos_show_hide_left;
	pos_class->south_clicked = pos_class->east_clicked =
		aligned_pos_show_hide_right;
}

static void
aligned_pos_init (AlignedPos *pos) { }

static void
aligned_pos_set_pos (BasePWidget *basep,
		     int x, int y,
		     int w, int h)
{
	int minx, miny, maxx, maxy;

	BorderEdge newpos = BORDER_POS(basep->pos)->edge;
	AlignedAlignment newalign = ALIGNED_POS(basep->pos)->align;

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

	/*if in the inner 1/3rd, don't change to avoid fast flickery
	  movement*/
	if ( x>(gdk_screen_width()/3) &&
	     x<(2*gdk_screen_width()/3) &&
	     y>(gdk_screen_height()/3) &&
	     y<(2*gdk_screen_height()/3))
		return;

	if ((x) * gdk_screen_height() > y * gdk_screen_width() ) {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() ) {
			newpos = BORDER_TOP;
			if(x<gdk_screen_width()/3)
				newalign = ALIGNED_LEFT;
			else if (x<2*gdk_screen_width()/3)
				newalign = ALIGNED_CENTER;
			else
				newalign = ALIGNED_RIGHT;
		} else {
			newpos = BORDER_RIGHT;
			if(y<gdk_screen_height()/3)
				newalign = ALIGNED_LEFT;
			else if (y<2*gdk_screen_height()/3)
				newalign = ALIGNED_CENTER;
			else
				newalign = ALIGNED_RIGHT;
		}
	} else {
		if(gdk_screen_height() * (gdk_screen_width()-(x)) >
		   y * gdk_screen_width() ) {
			newpos = BORDER_LEFT;
			if(y<gdk_screen_height()/3)
				newalign = ALIGNED_LEFT;
			else if(y<2*gdk_screen_height()/3)
				newalign = ALIGNED_CENTER;
			else
				newalign = ALIGNED_RIGHT;
		} else {
			newpos = BORDER_BOTTOM;
			if(x<gdk_screen_width()/3)
				newalign = ALIGNED_LEFT;
			else if(x<2*gdk_screen_width()/3)
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
	*x = *y = 0;
	switch (BORDER_POS(basep->pos)->edge) {
	case BORDER_BOTTOM:
		*y = gdk_screen_height() - h - foobar_widget_get_height ();
                /* fall through */
	case BORDER_TOP:
		/* if we wanted to be more hackish we could just do: 
		 *x = (ALIGNED_POS(basep->pos)->align)/2.0) * (gdk_screen_width() - w) 
		 */
		(*y) += foobar_widget_get_height ();
		switch (ALIGNED_POS(basep->pos)->align) {
		case ALIGNED_LEFT:
			break;
		case ALIGNED_CENTER:
			*x = (gdk_screen_width() - w) / 2;
			break;
		case ALIGNED_RIGHT:
			*x = gdk_screen_width() - w;
			break;
		}
		break;
	case BORDER_RIGHT:
		*x = gdk_screen_width() - w;
		/* fall through */
	case BORDER_LEFT:
		/* could do same as above, with height */
		switch (ALIGNED_POS(basep->pos)->align) {
		case ALIGNED_LEFT:
			*y = foobar_widget_get_height ();
			break;
		case ALIGNED_CENTER:
			*y = (gdk_screen_height() - h) / 2;
			break;
		case ALIGNED_RIGHT:
			*y = gdk_screen_height() - h;
			break;
		}
		break;
	}
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

void aligned_widget_change_params (AlignedWidget *aligned,
				   AlignedAlignment align,
				   BorderEdge edge,
				   int sz,
				   BasePMode mode,
				   BasePState state,
				   int hidebuttons_enabled,
				   int hidebutton_pixmaps_enabled,
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
		gtk_signal_emit (GTK_OBJECT (pos),
				 aligned_pos_signals[ALIGN_CHANGE_SIGNAL],
				 align);
	}

	border_widget_change_params (BORDER_WIDGET (aligned),
				     edge, sz, mode, state,
				     hidebuttons_enabled,
				     hidebutton_pixmaps_enabled,
				     back_type, pixmap_name,
				     fit_pixmap_bg, strech_pixmap_bg,
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

	aligned_widget_change_params (aligned, align,
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

	aligned_widget_change_params (aligned, align, edge,
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
				 
GtkWidget *
aligned_widget_new (AlignedAlignment align,
		    BorderEdge edge,
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
	AlignedWidget *aligned = gtk_type_new (TYPE_ALIGNED_WIDGET);
	AlignedPos *pos = gtk_type_new (TYPE_ALIGNED_POS);
	pos->align = align;

	BASEP_WIDGET (aligned)->pos = BASEP_POS (pos);

	border_widget_construct (BORDER_WIDGET (aligned),
				 edge, 
				 TRUE, FALSE,
				 sz, mode, state,
				 hidebuttons_enabled,
				 hidebutton_pixmaps_enabled,
				 back_type, back_pixmap,
				 fit_pixmap_bg, strech_pixmap_bg,
				 rotate_pixmap_bg,
				 back_color);

	return GTK_WIDGET (aligned);
}
