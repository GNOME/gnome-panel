/* Gnome panel: floating (as in movement, not speech)  widget
 * (C) 1999 the Free Software Foundation
 * 
 * Authors:  Jacob Berkman
 */

#ifndef __FLOATING_WIDGET_H__
#define __FLOATING_WIDGET_H__

#include "basep-widget.h"

BEGIN_GNOME_DECLS

#define FLOATING_POS_TYPE            (floating_pos_get_type ())
#define FLOATING_POS(o)              (GTK_CHECK_CAST ((o), FLOATING_POS_TYPE, FloatingPos))
#define FLOATING_POS_CLASS(k)        (GTK_CHECK_CLASS_CAST ((k), FLOATING_POS_TYPE, FloatingPosClass))
#define IS_FLOATING_POS(o)          (GTK_CHECK_TYPE ((o), FLOATING_POS_TYPE))
#define IS_FLOATING_POS_CLASS(k)    (GTK_CHECK_CLASS_TYPE ((k), FLOATING_POS_TYPE))

#define FLOATING_WIDGET_TYPE         (BASEP_WIDGET_TYPE)
#define FLOATING_WIDGET(o)           (BASEP_WIDGET(o))
#define FLOATING_WIDGET_CLASS(k)     (BASEP_WIDGET_CLASS(k))
#define IS_FLOATING_WIDGET(o)        (IS_BASEP_WIDGET(o) && IS_FLOATING_POS(BASEP_WIDGET(o)->pos))
/* this is not reliable */
#define IS_FLOATING_WIDGET_CLASS(k) (IS_BASEP_WIDGET_CLASS(k))

typedef BasePWidget            FloatingWidget;
typedef BasePWidgetClass       FloatingWidgetClass;

typedef struct _FloatingPos      FloatingPos;
typedef struct _FloatingPosClass FloatingPosClass;

struct _FloatingPos {
	BasePPos pos;

	gint16 x, y;
};

struct _FloatingPosClass {
	BasePPosClass parent_class;

	/* signal */
	void (*coords_change) (FloatingPos *pos,
			       gint x, gint y);
};

GtkType floating_pos_get_type (void);
GtkWidget *floating_widget_new (gint16 x, gint16 y,
				PanelOrientType orient,
				BasePMode mode,
				BasePState state,
				PanelSizeType sz,
				int hidebuttons_enabled,
				int hidebutton_pixmap_enabled,
				PanelBackType back_type,
				char *back_pixmap,
				int fit_pixmap_bg,
				GdkColor *back_color);

void floating_widget_change_params (FloatingWidget *floating,
				    gint16 x, gint16 y,
				    PanelOrientType orient,
				    BasePMode mode,
				    BasePState state,
				    PanelSizeType sz,
				    int hidebuttons_enabled,
				    int hidebutton_pixmap_enabled,
				    PanelBackType back_type,
				    char *back_pixmap,
				    int fit_pixmap_bg,
				    GdkColor *back_color);

void floating_widget_change_coords (FloatingWidget *floating,
				    gint16 x, gint16 y);

void floating_widget_change_orient (FloatingWidget *floating,
				    PanelOrientType orient);

END_GNOME_DECLS

#endif
