/* Gnome panel: edge (snapped) widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#ifndef EDGE_WIDGET_H
#define EDGE_WIDGET_H

#include "border-widget.h"

G_BEGIN_DECLS

/* even though edge_pos is currently structurally
   the same as border_pos, make it its own type 
   since we do need a seperate GType
*/

#define EDGE_TYPE_POS          		(edge_pos_get_type ())
#define EDGE_POS(object)            	(G_TYPE_CHECK_INSTANCE_CAST ((object), EDGE_TYPE_POS, EdgePos))
#define EDGE_POS_CLASS(klass)      	(G_TYPE_CHECK_CLASS_CAST ((klass), EDGE_TYPE_POS, EdgePosClass))
#define EDGE_IS_POS(object)         	(G_TYPE_CHECK_INSTANCE_TYPE ((object), EDGE_TYPE_POS))
#define EDGE_IS_POS_CLASS(klass)   	(G_TYPE_CHECK_CLASS_TYPE ((klass), EDGE_TYPE_POS))

#define EDGE_TYPE_WIDGET       		(BORDER_TYPE_WIDGET)
#define EDGE_WIDGET(object)         	(BORDER_WIDGET(object))
#define EDGE_WIDGET_CLASS(klass)   	(BORDER_WIDGET_CLASS(klass))
#define EDGE_IS_WIDGET(object)      	(BORDER_IS_WIDGET(object) && EDGE_IS_POS(BASEP_WIDGET(object)->pos))
/* this is not reliable */
#define EDGE_IS_WIDGET_CLASS(klass) 	(BORDER_IS_WIDGET_CLASS(klass))

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

GType edge_pos_get_type (void) G_GNUC_CONST;

GtkWidget *edge_widget_new (int screen,
			    BorderEdge edge,
			    BasePMode mode,
			    BasePState state,
			    int sz,
			    gboolean hidebuttons_enabled,
			    gboolean hidebutton_pixmaps_enabled,
			    PanelBackType back_type,
			    char *back_pixmap,
			    gboolean fit_pixmap_bg,
			    gboolean strech_pixmap_bg,
			    gboolean rotate_pixmap_bg,
			    GdkColor *back_color);

G_END_DECLS

#endif
