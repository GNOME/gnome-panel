/* Gnome panel: aligned (corner) widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#ifndef __ALIGNED_WIDGET_H__
#define __ALIGNED_WIDGET_H__

#include "border-widget.h"

G_BEGIN_DECLS

#define ALIGNED_TYPE_POS           	(aligned_pos_get_type ())
#define ALIGNED_POS(object)             (G_TYPE_CHECK_INSTANCE_CAST ((object), ALIGNED_TYPE_POS, AlignedPos))
#define ALIGNED_POS_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), ALIGNED_TYPE_POS, AlignedPosClass))
#define ALIGNED_IS_POS(object)          (G_TYPE_CHECK_INSTANCE_TYPE ((object), ALIGNED_TYPE_POS))
#define ALIGNED_IS_POS_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), ALIGNED_TYPE_POS))

#define ALIGNED_TYPE_WIDGET        	(BORDER_TYPE_WIDGET)
#define ALIGNED_WIDGET(object)          (BORDER_WIDGET(object))
#define ALIGNED_WIDGET_CLASS(klass)     (BORDER_WIDGET_CLASS(klass))
#define ALIGNED_IS_WIDGET(object)       (BORDER_IS_WIDGET(object) && ALIGNED_IS_POS(BASEP_WIDGET(object)->pos))
/* this is not reliable */
#define ALIGNED_IS_WIDGET_CLASS(klass)  (BORDER_IS_WIDGET_CLASS(klass))

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

GType aligned_pos_get_type (void) G_GNUC_CONST;

GtkWidget *aligned_widget_new (gchar *panel_id,
			       int screen,
			       AlignedAlignment aligned,
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

void aligned_widget_change_params (AlignedWidget *alignedw,
				   int screen,
				   AlignedAlignment align,
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

void aligned_widget_change_align (AlignedWidget *aligned,
				  AlignedAlignment align);

void aligned_widget_change_align_edge (AlignedWidget *aligned,
				       AlignedAlignment align,
				       BorderEdge edge);
G_END_DECLS

#endif
