/* Gnome panel: aligned (corner) widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#ifndef __ALIGNED_WIDGET_H__
#define __ALIGNED_WIDGET_H__

#include "border-widget.h"

BEGIN_GNOME_DECLS

#define TYPE_ALIGNED_POS           (aligned_pos_get_type ())
#define ALIGNED_POS(o)             (GTK_CHECK_CAST ((o), TYPE_ALIGNED_POS, AlignedPos))
#define ALIGNED_POS_CLASS(k)       (GTK_CHECK_CLASS_CAST ((k), TYPE_ALIGNED_POS, AlignedPosClass))
#define IS_ALIGNED_POS(o)          (GTK_CHECK_TYPE ((o), TYPE_ALIGNED_POS))
#define IS_ALIGNED_POS_CLASS(k)    (GTK_CHECK_CLASS_TYPE ((k), TYPE_ALIGNED_POS))

#define TYPE_ALIGNED_WIDGET        (TYPE_BORDER_WIDGET)
#define ALIGNED_WIDGET(o)          (BORDER_WIDGET(o))
#define ALIGNED_WIDGET_CLASS(k)    (BORDER_WIDGET_CLASS(k))
#define IS_ALIGNED_WIDGET(o)       (IS_BORDER_WIDGET(o) && IS_ALIGNED_POS(BASEP_WIDGET(o)->pos))
/* this is not reliable */
#define IS_ALIGNED_WIDGET_CLASS(k) (IS_BORDER_WIDGET_CLASS(k))

typedef BorderWidget            AlignedWidget;
typedef BorderWidgetClass       AlignedWidgetClass;

typedef struct _AlignedPos      AlignedPos;
typedef struct _AlignedPosClass AlignedPosClass;

typedef enum {
	ALIGNED_LEFT,
	ALIGNED_CENTER,
	ALIGNED_RIGHT
} AlignedAlignment;

struct _AlignedPos {
	BorderPos pos;

	AlignedAlignment align;
};

struct _AlignedPosClass {
	BorderPosClass parent_class;

	/* signal */
	void (*align_change) (AlignedPos *aligned,
			      AlignedAlignment align);
};

GtkType aligned_pos_get_type (void) G_GNUC_CONST;
GtkWidget *aligned_widget_new (int screen,
			       AlignedAlignment aligned,
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

void aligned_widget_change_params (AlignedWidget *alignedw,
				   int screen,
				   AlignedAlignment align,
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

void aligned_widget_change_align (AlignedWidget *aligned,
				  AlignedAlignment align);

void aligned_widget_change_align_edge (AlignedWidget *aligned,
				       AlignedAlignment align,
				       BorderEdge edge);
END_GNOME_DECLS

#endif
