/* Gnome panel: drawer widget
 * (C) 1999 the Free Software Foundation
 * 
 * Authors:  Jacob Berkman
 *           George Lebl
 */

#ifndef __DRAWER_WIDGET_H__
#define __DRAWER_WIDGET_H__

#include "basep-widget.h"
#include "drawer.h"

BEGIN_GNOME_DECLS

#define DRAWER_POS_TYPE            (drawer_pos_get_type ())
#define DRAWER_POS(o)              (GTK_CHECK_CAST ((o), DRAWER_POS_TYPE, DrawerPos))
#define DRAWER_POS_CLASS(k)        (GTK_CHECK_CLASS_CAST ((k), DRAWER_POS_TYPE, DrawerPosClass))
#define IS_DRAWER_POS(o)          (GTK_CHECK_TYPE ((o), DRAWER_POS_TYPE))
#define IS_DRAWER_POS_CLASS(k)    (GTK_CHECK_CLASS_TYPE ((k), DRAWER_POS_TYPE))

#define DRAWER_WIDGET_TYPE         (BASEP_WIDGET_TYPE)
#define DRAWER_WIDGET(o)           (BASEP_WIDGET(o))
#define DRAWER_WIDGET_CLASS(k)     (BASEP_WIDGET_CLASS(k))
#define IS_DRAWER_WIDGET(o)        (IS_BASEP_WIDGET(o) && IS_DRAWER_POS(BASEP_WIDGET(o)->pos))
/* this is not reliable */
#define IS_DRAWER_WIDGET_CLASS(k) (IS_BASEP_WIDGET_CLASS(k))

typedef BasePWidget            DrawerWidget;
typedef BasePWidgetClass       DrawerWidgetClass;

typedef struct _DrawerPos      DrawerPos;
typedef struct _DrawerPosClass DrawerPosClass;

struct _DrawerPos {
	BasePPos pos;

	BasePState temp_hidden;
	PanelOrientType orient;
};

struct _DrawerPosClass {
	BasePPosClass parent_class;

#if 0
	/* signal */
	void (*orient_change) (DrawerPos *pos,
			       PanelOrientType orient);
#endif
};

GtkType drawer_pos_get_type (void);
GtkWidget *drawer_widget_new (PanelOrientType orient,
			      BasePMode mode,
			      BasePState state,
			      int sz,
			      int hidebuttons_enabled,
			      int hidebutton_pixmap_enabled,
			      PanelBackType back_type,
			      char *back_pixmap,
			      int fit_pixmap_bg,
			      GdkColor *back_color);

void drawer_widget_change_params (DrawerWidget *drawer,
				  PanelOrientType orient,
				  BasePMode mode,
				  BasePState state,
				  int sz,
				  int hidebuttons_enabled,
				  int hidebutton_pixmap_enabled,
				  PanelBackType back_type,
				  char *back_pixmap,
				  int fit_pixmap_bg,
				  GdkColor *back_color);

void drawer_widget_change_orient (DrawerWidget *drawer,
				  PanelOrientType orient);

void drawer_widget_restore_state (DrawerWidget *drawer);

/*open and close drawers*/
void		drawer_widget_open_drawer	(DrawerWidget *panel,
						 BasePWidget *parentp);
void		drawer_widget_close_drawer	(DrawerWidget *panel,
						 BasePWidget *parentp);

void            drawer_widget_restore_state     (DrawerWidget *drawer);

END_GNOME_DECLS

#endif
