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

BEGIN_GNOME_DECLS

#define TYPE_DRAWER_POS            (drawer_pos_get_type ())
#define DRAWER_POS(o)              (GTK_CHECK_CAST ((o), TYPE_DRAWER_POS, DrawerPos))
#define DRAWER_POS_CLASS(k)        (GTK_CHECK_CLASS_CAST ((k), DRAWER_POS_TYPE, DrawerPosClass))
#define IS_DRAWER_POS(o)           (GTK_CHECK_TYPE ((o), TYPE_DRAWER_POS))
#define IS_DRAWER_POS_CLASS(k)     (GTK_CHECK_CLASS_TYPE ((k), TYPE_DRAWER_POS))

#define TYPE_DRAWER_WIDGET         (TYPE_BASEP_WIDGET)
#define DRAWER_WIDGET(o)           (BASEP_WIDGET(o))
#define DRAWER_WIDGET_CLASS(k)     (BASEP_WIDGET_CLASS(k))
#define IS_DRAWER_WIDGET(o)        (IS_BASEP_WIDGET(o) && IS_DRAWER_POS(BASEP_WIDGET(o)->pos))
/* this is not reliable */
#define IS_DRAWER_WIDGET_CLASS(k)  (IS_BASEP_WIDGET_CLASS(k))

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

GtkType drawer_pos_get_type (void) G_GNUC_CONST;
GtkWidget *drawer_widget_new (PanelOrientType orient,
			      BasePMode mode,
			      BasePState state,
			      BasePLevel level,
			      gboolean avoid_on_maximize,
			      int sz,
			      gboolean hidebuttons_enabled,
			      gboolean hidebutton_pixmap_enabled,
			      PanelBackType back_type,
			      char *back_pixmap,
			      gboolean fit_pixmap_bg,
			      gboolean strech_pixmap_bg,
			      gboolean rotate_pixmap_bg,
			      GdkColor *back_color);

void drawer_widget_change_params (DrawerWidget *drawer,
				  PanelOrientType orient,
				  BasePMode mode,
				  BasePState state,
				  BasePLevel level,
				  gboolean avoid_on_maximize,
				  int sz,
				  gboolean hidebuttons_enabled,
				  gboolean hidebutton_pixmap_enabled,
				  PanelBackType back_type,
				  char *back_pixmap,
				  gboolean fit_pixmap_bg,
				  gboolean strech_pixmap_bg,
				  gboolean rotate_pixmap_bg,
				  GdkColor *back_color);

void drawer_widget_change_orient (DrawerWidget *drawer,
				  PanelOrientType orient);

void drawer_widget_restore_state (DrawerWidget *drawer);

/*open and close drawers*/
void		drawer_widget_open_drawer	(DrawerWidget *panel,
						 GtkWidget *parentp);
void		drawer_widget_close_drawer	(DrawerWidget *panel,
						 GtkWidget *parentp);

void            drawer_widget_restore_state     (DrawerWidget *drawer);

END_GNOME_DECLS

#endif
