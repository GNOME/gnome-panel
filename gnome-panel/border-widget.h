/* Gnome panel: a border widget
 * (C) 1999 the Free Software Foundation
 *
 * Authors: Jacob Berkman
 *          George Lebl
 */

#ifndef __BORDER_WIDGET_H__
#define __BORDER_WIDGET_H__

#include "basep-widget.h"

BEGIN_GNOME_DECLS

#define BORDER_POS_TYPE        (border_pos_get_type ())
#define BORDER_POS(o)          (GTK_CHECK_CAST ((o), BORDER_POS_TYPE, BorderPos))
#define BORDER_POS_CLASS(k)    (GTK_CHECK_CLASS_CAST ((k), BORDER_POS_TYPE, BorderPosClass))
#define IS_BORDER_POS(o)       (GTK_CHECK_TYPE ((o), BORDER_POS_TYPE))
#define IS_BORDER_POS_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), BORDER_POS_TYPE))

#define BORDER_WIDGET_TYPE        (BASEP_WIDGET_TYPE) 
#define BORDER_WIDGET(o)          (BASEP_WIDGET(o))
#define BORDER_WIDGET_CLASS(k)    (BASEP_WIDGET_CLASS(o))
#define IS_BORDER_WIDGET(o)       (IS_BASEP_WIDGET(o) && IS_BORDER_POS( BASEP_WIDGET(o)->pos ))
/* this is not reliable... */
#define IS_BORDER_WIDGET_CLASS(k) (IS_BASEP_WIDGET_CLASS (k))

typedef BasePWidget          BorderWidget;
typedef BasePWidgetClass     BorderWidgetClass;

typedef struct _BorderPos    BorderPos;
typedef struct _BorderPosClass BorderPosClass;

typedef enum {
	BORDER_TOP,
	BORDER_RIGHT,
	BORDER_BOTTOM,
	BORDER_LEFT
} BorderEdge;

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
	
GtkType border_pos_get_type (void);
GtkWidget *border_widget_construct (BorderWidget *border,
				    BorderEdge edge,
				    int packed,
				    int reverse_arrows,
				    int sz,
				    BasePMode mode,
				    BasePState state,
				    int hidebuttons_enabled,
				    int hidebutton_pixmaps_enabled,
				    PanelBackType back_type,
				    char *back_pixmap,
				    gboolean fit_pixmap_bg,
				    gboolean strech_pixmap_bg,
				    gboolean rotate_pixmap_bg,
				    GdkColor *back_color);

void border_widget_change_params (BorderWidget *border,
				  BorderEdge edge,
				  int sz,
				  BasePMode mode,
				  BasePState state,
				  int hidebuttons_enabled,
				  int hidebutton_pixmaps_enabled,
				  PanelBackType back_type,
				  char *pixmap_name,
				  gboolean fit_pixmap_bg,
				  gboolean strech_pixmap_bg,
				  gboolean rotate_pixmap_bg,
				  GdkColor *back_color);


void border_widget_change_edge (BorderWidget *border, BorderEdge edge);

END_GNOME_DECLS
#endif
