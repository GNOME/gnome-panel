/* Gnome panel: drawer widget
 * (C) 1999 the Free Software Foundation
 * 
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#ifndef DRAWER_WIDGET_H
#define DRAWER_WIDGET_H

#include "basep-widget.h"
#include "drawer.h"

G_BEGIN_DECLS

#define DRAWER_TYPE_POS			(drawer_pos_get_type ())
#define DRAWER_POS(object)              (G_TYPE_CHECK_INSTANCE_CAST ((object), DRAWER_TYPE_POS, DrawerPos))
#define DRAWER_POS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), DRAWER_TYPE_POS, DrawerPosClass))
#define DRAWER_IS_POS(object)           (G_TYPE_CHECK_INSTANCE_TYPE ((object), DRAWER_TYPE_POS))
#define DRAWER_IS_POS_CLASS(k)     	(G_TYPE_CHECK_CLASS_TYPE ((klass), DRAWER_TYPE_POS))

#define DRAWER_TYPE_WIDGET         	(BASEP_TYPE_WIDGET)
#define DRAWER_WIDGET(object)          	(BASEP_WIDGET(object))
#define DRAWER_WIDGET_CLASS(klass)      (BASEP_WIDGET_CLASS(klass))
#define DRAWER_IS_WIDGET(object)        (BASEP_IS_WIDGET(object) && DRAWER_IS_POS(BASEP_WIDGET(object)->pos))
/* this is not reliable */
#define DRAWER_IS_WIDGET_CLASS(klass)	(BASEP_IS_WIDGET_CLASS(klass))

typedef BasePWidget            DrawerWidget;
typedef BasePWidgetClass       DrawerWidgetClass;

typedef struct _DrawerPos      DrawerPos;
typedef struct _DrawerPosClass DrawerPosClass;

struct _DrawerPos {
	BasePPos pos;

	BasePState temp_hidden;
	PanelOrient orient;
};

struct _DrawerPosClass {
	BasePPosClass parent_class;

#if 0
	/* signal */
	void (*orient_change) (DrawerPos *pos,
			       PanelOrient orient);
#endif
};

GType drawer_pos_get_type (void) G_GNUC_CONST;

GtkWidget *drawer_widget_new (PanelOrient orient,
			      BasePMode mode,
			      BasePState state,
			      int sz,
			      gboolean hidebuttons_enabled,
			      gboolean hidebutton_pixmap_enabled,
			      PanelBackType back_type,
			      char *back_pixmap,
			      gboolean fit_pixmap_bg,
			      gboolean stretch_pixmap_bg,
			      gboolean rotate_pixmap_bg,
			      GdkColor *back_color);

void drawer_widget_change_params (DrawerWidget *drawer,
				  PanelOrient orient,
				  BasePMode mode,
				  BasePState state,
				  int sz,
				  gboolean hidebuttons_enabled,
				  gboolean hidebutton_pixmap_enabled,
				  PanelBackType back_type,
				  char *back_pixmap,
				  gboolean fit_pixmap_bg,
				  gboolean stretch_pixmap_bg,
				  gboolean rotate_pixmap_bg,
				  GdkColor *back_color);

void drawer_widget_change_orient (DrawerWidget *drawer,
				  PanelOrient orient);

void drawer_widget_restore_state (DrawerWidget *drawer);

/*open and close drawers*/
void		drawer_widget_open_drawer	(DrawerWidget *panel,
						 GtkWidget *parentp);
void		drawer_widget_close_drawer	(DrawerWidget *panel,
						 GtkWidget *parentp);

void            drawer_widget_restore_state     (DrawerWidget *drawer);

G_END_DECLS

#endif
