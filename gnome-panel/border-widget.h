/* Gnome panel: a border widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors: Jacob Berkman
 *          George Lebl
 */

#ifndef BORDER_WIDGET_H
#define BORDER_WIDGET_H

#include "basep-widget.h"

BEGIN_GNOME_DECLS

#define TYPE_BORDER_POS        (border_pos_get_type ())
#define BORDER_POS(o)          (GTK_CHECK_CAST ((o), TYPE_BORDER_POS, BorderPos))
#define BORDER_POS_CLASS(k)    (GTK_CHECK_CLASS_CAST ((k), TYPE_BORDER_POS, BorderPosClass))
#define IS_BORDER_POS(o)       (GTK_CHECK_TYPE ((o), TYPE_BORDER_POS))
#define IS_BORDER_POS_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), TYPE_BORDER_POS))

#define TYPE_BORDER_WIDGET        (TYPE_BASEP_WIDGET) 
#define BORDER_WIDGET(o)          (BASEP_WIDGET(o))
#define BORDER_WIDGET_CLASS(k)    (BASEP_WIDGET_CLASS(o))
#define IS_BORDER_WIDGET(o)       (IS_BASEP_WIDGET(o) && IS_BORDER_POS( BASEP_WIDGET(o)->pos ))
/* this is not reliable... */
#define IS_BORDER_WIDGET_CLASS(k) (IS_BASEP_WIDGET_CLASS (k))

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
	
GtkType border_pos_get_type (void) G_GNUC_CONST;
GtkWidget *border_widget_construct (BorderWidget *border,
				    int screen,
				    BorderEdge edge,
				    gboolean packed,
				    gboolean reverse_arrows,
				    int sz,
				    BasePMode mode,
				    BasePState state,
				    BasePLevel level,
				    gboolean avoid_on_maximize,
				    gboolean hidebuttons_enabled,
				    gboolean hidebutton_pixmaps_enabled,
				    PanelBackType back_type,
				    char *back_pixmap,
				    gboolean fit_pixmap_bg,
				    gboolean strech_pixmap_bg,
				    gboolean rotate_pixmap_bg,
				    GdkColor *back_color);

void border_widget_change_params (BorderWidget *border,
				  int screen,
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


void border_widget_change_edge (BorderWidget *border, BorderEdge edge);

END_GNOME_DECLS
#endif
