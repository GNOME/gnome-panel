/* Gnome panel: a border widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors: Jacob Berkman
 *          George Lebl
 */

#ifndef BORDER_WIDGET_H
#define BORDER_WIDGET_H

#include "basep-widget.h"

G_BEGIN_DECLS

#define BORDER_TYPE_POS        		(border_pos_get_type ())
#define BORDER_POS(object)     		(G_TYPE_CHECK_INSTANCE_CAST ((object), BORDER_TYPE_POS, BorderPos))
#define BORDER_POS_CLASS(klass)    	(G_TYPE_CHECK_CLASS_CAST ((klass), BORDER_TYPE_POS, BorderPosClass))
#define BORDER_IS_POS(object)       	(G_TYPE_CHECK_INSTANCE_TYPE ((object), BORDER_TYPE_POS))
#define BORDER_IS_POS_CLASS(klass) 	(G_TYPE_CHECK_CLASS_TYPE ((klass), BORDER_TYPE_POS))

#define BORDER_TYPE_WIDGET        	(BASEP_TYPE_WIDGET) 
#define BORDER_WIDGET(object)     	(BASEP_WIDGET(object))
#define BORDER_WIDGET_CLASS(klass)	(BASEP_WIDGET_CLASS(klass))
#define BORDER_IS_WIDGET(object)  	(BASEP_IS_WIDGET(object) && BORDER_IS_POS( BASEP_WIDGET(object)->pos ))
/* this is not reliable... */
#define BORDER_IS_WIDGET_CLASS(klass) 	(BASEP_IS_WIDGET_CLASS (klass))

typedef BasePWidget          BorderWidget;
typedef BasePWidgetClass     BorderWidgetClass;

typedef struct _BorderPos    BorderPos;
typedef struct _BorderPosClass BorderPosClass;

struct _BorderPos {
	BasePPos pos;
	BorderEdge edge;
};

struct _BorderPosClass {
	BasePPosClass parent_class;

	/* signals */
	void (*edge_change)  (BorderPos *border,
			      BorderEdge edge);

	/* virtual funcs */
#if 0
	void (*set_initial_pos) (BorderWidget *border);

	void (*save_settings) (BorderWidget *border);
#endif
};
	
GType border_pos_get_type (void) G_GNUC_CONST;

GtkWidget *border_widget_construct (gchar *panel_id,
				    BorderWidget *border,
				    int screen,
				    BorderEdge edge,
				    gboolean packed,
				    gboolean reverse_arrows,
				    int sz,
				    BasePMode mode,
				    BasePState state,
				    gboolean hidebuttons_enabled,
				    gboolean hidebutton_pixmaps_enabled,
				    PanelBackType back_type,
				    char *back_pixmap,
				    gboolean fit_pixmap_bg,
				    gboolean stretch_pixmap_bg,
				    gboolean rotate_pixmap_bg,
				    GdkColor *back_color);

void border_widget_change_params (BorderWidget *border,
				  int screen,
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


void border_widget_change_edge (BorderWidget *border, BorderEdge edge);

G_END_DECLS
#endif
