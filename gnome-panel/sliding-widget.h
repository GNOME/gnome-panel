/* Gnome panel: sliding widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#ifndef __SLIDING_WIDGET_H__
#define __SLIDING_WIDGET_H__

#include "border-widget.h"

BEGIN_GNOME_DECLS

#define SLIDING_POS_TYPE           (sliding_pos_get_type ())
#define SLIDING_POS(o)             (GTK_CHECK_CAST ((o), SLIDING_POS_TYPE, SlidingPos))
#define SLIDING_POS_CLASS(k)       (GTK_CHECK_CLASS_CAST ((k), SLIDING_POS_TYPE, SlidingPosClass))
#define IS_SLIDING_POS(o)          (GTK_CHECK_TYPE ((o), SLIDING_POS_TYPE))
#define IS_SLIDING_POS_CLASS(k)    (GTK_CHECK_CLASS_TYPE ((k), SLIDING_POS_TYPE))

#define SLIDING_WIDGET_TYPE        (BORDER_WIDGET_TYPE)
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

GtkType sliding_pos_get_type (void);
GtkWidget *sliding_widget_new (SlidingAnchor anchor,
			       gint16 offset,
			       BorderEdge edge,
			       BasePMode mode,
			       BasePState state,
			       int sz,
			       int hidebuttons_enabled,
			       int hidebutton_pixmaps_enabled,
			       PanelBackType back_type,
			       char *back_pixmap,
			       int fit_pixmap_bg,
			       GdkColor *back_color);

void sliding_widget_change_params (SlidingWidget *slidingw,
				   SlidingAnchor anchor,
				   gint16 offset,
				   BorderEdge edge,
				   int sz,
				   BasePMode mode,
				   BasePState state,
				   int hidebuttons_enabled,
				   int hidebutton_pixmaps_enabled,
				   PanelBackType back_type,
				   char *pixmap_name,
				   int fit_pixmap_bg,
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
