/* Gnome panel: corner widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#ifndef __CORNER_WIDGET_H__
#define __CORNER_WIDGET_H__

#include <gtk/gtk.h>
#include "corner-widget.h"

BEGIN_GNOME_DECLS

#define CORNER_WIDGET(obj)          GTK_CHECK_CAST (obj, corner_widget_get_type (), CornerWidget)
#define CORNER_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, corner_widget_get_type (), CornerWidgetClass)
#define IS_CORNER_WIDGET(obj)       GTK_CHECK_TYPE (obj, corner_widget_get_type ())

typedef struct _CornerWidget		CornerWidget;
typedef struct _CornerWidgetClass	CornerWidgetClass;

typedef enum {
	CORNER_NE,
	CORNER_SE,
	CORNER_SW,
	CORNER_NW
} CornerPos;
typedef enum {
	CORNER_SHOWN,
	CORNER_MOVING,
	CORNER_HIDDEN
} CornerState;

struct _CornerWidget
{
	GtkWindow		window;
	
	GtkWidget		*panel;

	GtkWidget		*table;
	GtkWidget		*hidebutton_n;
	GtkWidget		*hidebutton_e;
	GtkWidget		*hidebutton_w;
	GtkWidget		*hidebutton_s;

	CornerPos		pos;
	CornerState		state;
};

struct _CornerWidgetClass
{
	GtkWindowClass parent_class;

	void (* pos_change) (CornerWidget *panel,
			     CornerPos pos);
	void (* state_change) (CornerWidget *panel,
			       CornerState state);
};

guint		corner_widget_get_type		(void);
GtkWidget*	corner_widget_new		(CornerPos pos,
						 PanelOrientation orient,
						 CornerState state,
						 PanelBackType back_type,
						 char *back_pixmap,
						 int fit_pixmap_bg,
						 GdkColor *back_color);

/* changing parameters */
void		corner_widget_change_params	(CornerWidget *corner,
						 PanelOrientation orient,
						 CornerPos pos,
						 CornerState state,
						 PanelBackType back_type,
						 char *pixmap_name,
						 int fit_pixmap_bg,
						 GdkColor *back_color);

/* changing parameters (pos/orient only) */
void		corner_widget_change_pos_orient	(CornerWidget *corner,
						 CornerPos pos,
						 PanelOrientation orient);

void		corner_widget_enable_buttons	(CornerWidget *corner);
void		corner_widget_disable_buttons	(CornerWidget *corner);

END_GNOME_DECLS

#endif /* __CORNER_WIDGET_H__ */
