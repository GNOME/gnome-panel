/* Gnome panel: sliding widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#ifndef SLIDING_WIDGET_H
#define SLIDING_WIDGET_H

#include "border-widget.h"

BEGIN_GNOME_DECLS

#define TYPE_SLIDING_POS           (sliding_pos_get_type ())
#define SLIDING_POS(o)             (GTK_CHECK_CAST ((o), TYPE_SLIDING_POS, SlidingPos))
#define SLIDING_POS_CLASS(k)       (GTK_CHECK_CLASS_CAST ((k), TYPE_SLIDING_POS, SlidingPosClass))
#define IS_SLIDING_POS(o)          (GTK_CHECK_TYPE ((o), TYPE_SLIDING_POS))
#define IS_SLIDING_POS_CLASS(k)    (GTK_CHECK_CLASS_TYPE ((k), TYPE_SLIDING_POS))

#define TYPE_SLIDING_WIDGET        (TYPE_BORDER_WIDGET)
#define SLIDING_WIDGET(o)          (BORDER_WIDGET(o))
#define SLIDING_WIDGET_CLASS(k)    (BORDER_WIDGET_CLASS(k))
#define IS_SLIDING_WIDGET(o)       (IS_BORDER_WIDGET(o) && IS_SLIDING_POS(BASEP_WIDGET(o)->pos))
/* this is not reliable */
#define IS_SLIDING_WIDGET_CLASS(k) (IS_BORDER_WIDGET_CLASS(k))

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

GtkType sliding_pos_get_type (void) G_GNUC_CONST;
GtkWidget *sliding_widget_new (int screen,
			       SlidingAnchor anchor,
			       gint16 offset,
			       BorderEdge edge,
			       BasePMode mode,
			       BasePState state,
			       BasePLevel level,
			       gboolean avoid_on_maximize,
			       int sz,
			       gboolean hidebuttons_enabled,
			       gboolean hidebutton_pixmaps_enabled,
			       PanelBackType back_type,
			       char *back_pixmap,
			       gboolean fit_pixmap_bg,
			       gboolean strech_pixmap_bg,
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
				   BasePLevel level,
				   gboolean avoid_on_maximize,
				   gboolean hidebuttons_enabled,
				   gboolean hidebutton_pixmaps_enabled,
				   PanelBackType back_type,
				   char *pixmap_name,
				   gboolean fit_pixmap_bg,
				   gboolean strech_pixmap_bg,
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
END_GNOME_DECLS

#endif
