/* Gnome panel: floating (as in movement, not speech)  widget
 * (C) 1999 the Free Software Foundation
 * 
 * Authors:  Jacob Berkman
 */

#ifndef FLOATING_WIDGET_H
#define FLOATING_WIDGET_H

#include "basep-widget.h"

BEGIN_GNOME_DECLS

#define TYPE_FLOATING_POS            (floating_pos_get_type ())
#define FLOATING_POS(o)              (GTK_CHECK_CAST ((o), TYPE_FLOATING_POS, FloatingPos))
#define FLOATING_POS_CLASS(k)        (GTK_CHECK_CLASS_CAST ((k), TYPE_FLOATING_POS, FloatingPosClass))
#define IS_FLOATING_POS(o)           (GTK_CHECK_TYPE ((o), TYPE_FLOATING_POS))
#define IS_FLOATING_POS_CLASS(k)     (GTK_CHECK_CLASS_TYPE ((k), TYPE_FLOATING_POS))

#define TYPE_FLOATING_WIDGET         (TYPE_BASEP_WIDGET)
#define FLOATING_WIDGET(o)           (BASEP_WIDGET(o))
#define FLOATING_WIDGET_CLASS(k)     (BASEP_WIDGET_CLASS(k))
#define IS_FLOATING_WIDGET(o)        (IS_BASEP_WIDGET(o) && IS_FLOATING_POS(BASEP_WIDGET(o)->pos))
/* this is not reliable */
#define IS_FLOATING_WIDGET_CLASS(k)  (IS_BASEP_WIDGET_CLASS(k))

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

GtkType floating_pos_get_type (void) G_GNUC_CONST;
GtkWidget *floating_widget_new (int screen,
				gint16 x,
				gint16 y,
				PanelOrientation orient,
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
				GdkColor *back_color);

void floating_widget_change_params (FloatingWidget *floating,
				    int screen,
				    gint16 x,
				    gint16 y,
				    PanelOrientation orient,
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
				    GdkColor *back_color);

void floating_widget_change_coords (FloatingWidget *floating,
				    gint16 x, gint16 y);

void floating_widget_change_orient (FloatingWidget *floating,
				    PanelOrientation orient);

END_GNOME_DECLS

#endif
