/* Gnome panel: sliding widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#ifndef SLIDING_WIDGET_H
#define SLIDING_WIDGET_H

#include "border-widget.h"

G_BEGIN_DECLS

#define SLIDING_TYPE_POS           	(sliding_pos_get_type ())
#define SLIDING_POS(object)             (G_TYPE_CHECK_INSTANCE_CAST ((object), SLIDING_TYPE_POS, SlidingPos))
#define SLIDING_POS_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), SLIDING_TYPE_POS, SlidingPosClass))
#define SLIDING_IS_POS(object)          (G_TYPE_CHECK_INSTANCE_TYPE ((object), SLIDING_TYPE_POS))
#define SLIDING_IS_POS_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), SLIDING_TYPE_POS))

#define SLIDING_TYPE_WIDGET        	(BORDER_TYPE_WIDGET)
#define SLIDING_WIDGET(object)          (BORDER_WIDGET(object))
#define SLIDING_WIDGET_CLASS(klass)     (BORDER_WIDGET_CLASS(klass))
#define SLIDING_IS_WIDGET(object)       (BORDER_IS_WIDGET(object) && SLIDING_IS_POS(BASEP_WIDGET(object)->pos))
/* this is not reliable */
#define SLIDING_IS_WIDGET_CLASS(klass) (BORDER_IS_WIDGET_CLASS(klass))

typedef BorderWidget            SlidingWidget;
typedef BorderWidgetClass       SlidingWidgetClass;

typedef struct _SlidingPos      SlidingPos;
typedef struct _SlidingPosClass SlidingPosClass;

typedef enum {
	SLIDING_ANCHOR_LEFT,
	SLIDING_ANCHOR_RIGHT
} SlidingAnchor;

struct _SlidingPos {
	BorderPos pos;

	SlidingAnchor anchor;
	gint16 offset;
};

struct _SlidingPosClass {
	BorderPosClass parent_class;

	/* signal */
	void (*anchor_change) (SlidingPos *sliding,
			       SlidingAnchor anchor);

	void (*offset_change) (SlidingPos *sliding,
			       gint16 offset);
};

GType sliding_pos_get_type (void) G_GNUC_CONST;

GtkWidget *sliding_widget_new (gchar *panel_id,
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
			       GdkColor *back_color);

void sliding_widget_change_params (SlidingWidget *slidingw,
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
				   GdkColor *back_color);

void sliding_widget_change_anchor (SlidingWidget *sliding,
				   SlidingAnchor anchor);

void sliding_widget_change_offset (SlidingWidget *sliding,
				   gint16 offset);

void sliding_widget_change_anchor_offset_edge (SlidingWidget *sliding, 
					       SlidingAnchor anchor, 
					       gint16 offset,
					       BorderEdge edge);
G_END_DECLS

#endif
