#ifndef PANEL_H
#define PANEL_H

#include <gtk/gtk.h>
#include "panel-widget.h"
#include "applet.h"

BEGIN_GNOME_DECLS

typedef enum {
	SNAPPED_PANEL,
	DRAWER_PANEL,
	CORNER_PANEL,
	FREE_PANEL, /*not yet implemented*/
	TABBED_PANEL /*not yet implemented*/
} PanelType;

typedef struct _PanelData PanelData;
struct _PanelData {
	PanelType type;
	GtkWidget *panel;
	GtkWidget *menu;
	int menu_age;
};

/*get the default panel widget if the panel has more then one or
  just get the that one*/
PanelWidget * get_def_panel_widget(GtkWidget *panel);

void orientation_change(AppletInfo *info, PanelWidget *panel);
void back_change(AppletInfo *info, PanelWidget *panel);

PanelOrientType get_applet_orient(PanelWidget *panel);

void panel_setup(GtkWidget *panel);

/*send state change to all the panels*/
void send_state_change(void);

/* Destroy all panels - called upon exit */
void destroy_all_panels(void);

#define get_panel_parent(appletw) \
	(gtk_object_get_data(GTK_OBJECT(appletw->parent), PANEL_PARENT))


END_GNOME_DECLS

#endif
