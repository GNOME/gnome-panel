/* Gnome panel: edge (snapped) widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#ifndef EDGE_WIDGET_H
#define EDGE_WIDGET_H

#include "border-widget.h"

BEGIN_GNOME_DECLS

/* even though edge_pos is currently structurally
   the same as border_pos, make it its own type 
   since we do need a seperate GtkType
*/

#define TYPE_EDGE_POS          (edge_pos_get_type ())
#define EDGE_POS(o)            (GTK_CHECK_CAST ((o), TYPE_EDGE_POS, EdgePos))
#define EDGE_POS_CLASS(k)      (GTK_CHECK_CLASS_CAST ((k), TYPE_EDGE_POS, EdgePosClass))
#define IS_EDGE_POS(o)         (GTK_CHECK_TYPE ((o), TYPE_EDGE_POS))
#define IS_EDGE_POS_CLASS(k)   (GTK_CHECK_CLASS_TYPE ((k), TYPE_EDGE_POS))

#define TYPE_EDGE_WIDGET       (TYPE_BORDER_WIDGET)
#define EDGE_WIDGET(o)         (BORDER_WIDGET(o))
#define EDGE_WIDGET_CLASS(k)   (BORDER_WIDGET_CLASS(k))
#define IS_EDGE_WIDGET(o)      (IS_BORDER_WIDGET(o) && IS_EDGE_POS(BASEP_WIDGET(o)->pos))
/* this is not reliable */
#define IS_EDGE_WIDGET_CLASS(k) (IS_BORDER_WIDGET_CLASS(k))

typedef BorderWidget            EdgeWidget;
typedef BorderWidgetClass       EdgeWidgetClass;

typedef struct _EdgePos         EdgePos;
typedef struct _EdgePosClass    EdgePosClass;

struct _EdgePos {
	BorderPos pos;
};

struct _EdgePosClass {
	BorderPosClass parent_class;
};

GtkType edge_pos_get_type (void) G_GNUC_CONST;
GtkWidget *edge_widget_new (int screen,
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

END_GNOME_DECLS

#endif
