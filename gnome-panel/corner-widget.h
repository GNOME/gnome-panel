/* Gnome panel: corner widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#ifndef __CORNER_WIDGET_H__
#define __CORNER_WIDGET_H__

#include <gtk/gtk.h>
#include "basep-widget.h"

BEGIN_GNOME_DECLS

#define CORNER_WIDGET(obj)          GTK_CHECK_CAST (obj, corner_widget_get_type (), CornerWidget)
#define CORNER_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, corner_widget_get_type (), CornerWidgetClass)
#define IS_CORNER_WIDGET(obj)       GTK_CHECK_TYPE (obj, corner_widget_get_type ())

typedef struct _CornerWidget		CornerWidget;
typedef struct _CornerWidgetClass	CornerWidgetClass;

typedef enum {
	CORNER_NE=0,
	CORNER_SE,
	CORNER_SW,
	CORNER_NW
} CornerPos;
typedef enum {
	CORNER_EXPLICIT_HIDE,
	CORNER_AUTO_HIDE
} CornerMode;
typedef enum {
	CORNER_SHOWN,
	CORNER_MOVING,
	CORNER_HIDDEN,
	CORNER_AUTO_HIDDEN
} CornerState;

struct _CornerWidget
{
	BasePWidget		basep;
	
        CornerPos		pos;
        CornerMode              mode;
	CornerState		state;

	int			leave_notify_timer_tag;

	int			autohide_inhibit;
	int			drawers_open; /* a count which can be used
						 to block the autohide, until
						 it is 0 .. it's set by the
						 app not the widget*/
};

struct _CornerWidgetClass
{
	BasePWidgetClass parent_class;

	void (* pos_change) (CornerWidget *panel,
			     CornerPos pos);
	void (* state_change) (CornerWidget *panel,
			       CornerState state);
};

guint		corner_widget_get_type		(void);
GtkWidget*	corner_widget_new		(CornerPos pos,
						 PanelOrientation orient,
						 CornerMode mode,
						 CornerState state,
						 PanelSizeType sz,
						 int hidebuttons_enabled,
						 int hidebutton_pixmaps_enabled,
						 PanelBackType back_type,
						 char *back_pixmap,
						 int fit_pixmap_bg,
						 GdkColor *back_color);

/* changing parameters */
void		corner_widget_change_params	(CornerWidget *corner,
						 PanelOrientation orient,
						 CornerPos pos,
						 CornerMode mode,
						 CornerState state,
						 PanelSizeType sz,
						 int hidebuttons_enabled,
						 int hidebutton_pixmaps_enabled,
						 PanelBackType back_type,
						 char *pixmap_name,
						 int fit_pixmap_bg,
						 GdkColor *back_color);

/* changing parameters (pos/orient only) */
void		corner_widget_change_pos_orient	(CornerWidget *corner,
						 CornerPos pos,
						 PanelOrientation orient);
/*popup the widget if it's popped down (autohide)*/
void		corner_widget_pop_up		(CornerWidget *corner);

/*queue a pop_down in autohide mode*/
void		corner_widget_queue_pop_down	(CornerWidget *corner);

END_GNOME_DECLS

#endif /* __CORNER_WIDGET_H__ */
