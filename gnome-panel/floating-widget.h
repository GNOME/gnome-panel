/* Gnome panel: floating (as in movement, not speech)  widget
 * (C) 1999 the Free Software Foundation
 * 
 * Authors:  Jacob Berkman
 */

#ifndef FLOATING_WIDGET_H
#define FLOATING_WIDGET_H

#include "basep-widget.h"

G_BEGIN_DECLS

#define FLOATING_TYPE_POS            	(floating_pos_get_type ())
#define FLOATING_POS(object)            (G_TYPE_CHECK_INSTANCE_CAST ((object), FLOATING_TYPE_POS, FloatingPos))
#define FLOATING_POS_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), FLOATING_TYPE_POS, FloatingPosClass))
#define FLOATING_IS_POS(object)         (G_TYPE_CHECK_INSTANCE_TYPE ((object), FLOATING_TYPE_POS))
#define FLOATING_IS_POS_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOATING_TYPE_POS))

#define FLOATING_TYPE_WIDGET         	(BASEP_TYPE_WIDGET)
#define FLOATING_WIDGET(object)        	(BASEP_WIDGET(object))
#define FLOATING_WIDGET_CLASS(klass)   	(BASEP_WIDGET_CLASS(klass))
#define FLOATING_IS_WIDGET(object)     	(BASEP_IS_WIDGET(object) && FLOATING_IS_POS(BASEP_WIDGET(object)->pos))
/* this is not reliable */
#define FLOATING_IS_WIDGET_CLASS(klass)	(BASEP_IS_WIDGET_CLASS(klass))

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

GType floating_pos_get_type (void) G_GNUC_CONST;

GtkWidget *floating_widget_new (gchar *panel_id,
				int screen,
				gint16 x,
				gint16 y,
				GtkOrientation orient,
				BasePMode mode,
				BasePState state,
				int sz,
				gboolean hidebuttons_enabled,
				gboolean hidebutton_pixmap_enabled,
				PanelBackType back_type,
				char *back_pixmap,
				gboolean fit_pixmap_bg,
				gboolean stretch_pixmap_bg,
				gboolean rotate_pixmap_bg,
				GdkColor *back_color);

void floating_widget_change_params (FloatingWidget *floating,
				    int screen,
				    gint16 x,
				    gint16 y,
				    GtkOrientation orient,
				    BasePMode mode,
				    BasePState state,
				    int sz,
				    gboolean hidebuttons_enabled,
				    gboolean hidebutton_pixmap_enabled,
				    PanelBackType back_type,
				    char *back_pixmap,
				    gboolean fit_pixmap_bg,
				    gboolean stretch_pixmap_bg,
				    gboolean rotate_pixmap_bg,
				    GdkColor *back_color);

void floating_widget_change_coords (FloatingWidget *floating,
				    gint16 x, gint16 y);

void floating_widget_change_orient (FloatingWidget *floating,
				    GtkOrientation orient);

G_END_DECLS

#endif
