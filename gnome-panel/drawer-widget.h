/* Gnome panel: drawer widget
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
#ifndef __DRAWER_WIDGET_H__
#define __DRAWER_WIDGET_H__


#include <gtk/gtk.h>
#include "panel-widget.h"

BEGIN_GNOME_DECLS

#define DRAWER_WIDGET(obj)          GTK_CHECK_CAST (obj, drawer_widget_get_type (), DrawerWidget)
#define DRAWER_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, drawer_widget_get_type (), DrawerWidgetClass)
#define IS_DRAWER_WIDGET(obj)       GTK_CHECK_TYPE (obj, drawer_widget_get_type ())

typedef struct _DrawerWidget		DrawerWidget;
typedef struct _DrawerWidgetClass	DrawerWidgetClass;

typedef enum {
	DRAWER_SHOWN,
	DRAWER_MOVING,
	DRAWER_HIDDEN
} DrawerState;
typedef enum {
	DROP_ZONE_LEFT,
	DROP_ZONE_RIGHT
} DrawerDropZonePos;

struct _DrawerWidget
{
	GtkWindow		window;

	GtkWidget		*panel;
	GtkWidget		*table;
	GtkWidget		*handle_n;
	GtkWidget		*handle_e;
	GtkWidget		*handle_w;
	GtkWidget		*handle_s;

	DrawerState		state;

	DrawerDropZonePos	drop_zone_pos;
	int			x;
	int			y;
};

struct _DrawerWidgetClass
{
	GtkWindowClass parent_class;

	void (* state_change) (DrawerWidget *panel,
			       DrawerState state);
};

guint		drawer_widget_get_type		(void);
GtkWidget*	drawer_widget_new		(PanelOrientation orient,
						 DrawerState state,
						 DrawerDropZonePos
						 	drop_zone_pos,
						 PanelBackType back_type,
						 char *back_pixmap,
						 int fit_pixmap_bg,
						 GdkColor *back_color);

/*open and close drawers*/
void		drawer_widget_open_drawer	(DrawerWidget *panel);
void		drawer_widget_close_drawer	(DrawerWidget *panel);

/* changing parameters */
void		drawer_widget_change_params	(DrawerWidget *drawer,
						 PanelOrientation orient,
						 DrawerState state,
						 DrawerDropZonePos
						 	drop_zone_pos,
						 PanelBackType back_type,
						 char *pixmap_name,
						 int fit_pixmap_bg,
						 GdkColor *back_color);

/* changing parameters (orient only) */
void		drawer_widget_change_orient	(DrawerWidget *drawer,
						 PanelOrientation orient);
/*changing parameters (dropzonepos only)*/
void		drawer_widget_change_drop_zone_pos(DrawerWidget *drawer,
						   DrawerDropZonePos
						 	drop_zone_pos);

void		drawer_widget_restore_state	(DrawerWidget *drawer);

void		drawer_widget_set_pos		(DrawerWidget *panel,
						 int x,
						 int y);

END_GNOME_DECLS

#endif /* __DRAWER_WIDGET_H__ */
