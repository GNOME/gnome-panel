#ifndef PANEL_H
#define PANEL_H

#include <gtk/gtk.h>
#include "panel-widget.h"
#include "applet.h"

BEGIN_GNOME_DECLS

typedef struct _PanelData PanelData;
struct _PanelData {
	PanelType type;
	GtkWidget *panel;
	GtkWidget *menu;
	int menu_age;
};

void freeze_changes (AppletInfo *info);
void thaw_changes (AppletInfo *info);

void orientation_change(AppletInfo *info, PanelWidget *panel);
void size_change(AppletInfo *info, PanelWidget *panel);
void back_change(AppletInfo *info, PanelWidget *panel);

PanelOrientType get_applet_orient(PanelWidget *panel);

void panel_setup(GtkWidget *panel);
void basep_pos_connect_signals (BasePWidget *basep);

/*send state change to all the panels*/
void send_state_change(void);

void panel_freeze_changes(PanelWidget *panel);

void panel_thaw_changes(PanelWidget *panel);

GtkWidget * make_popup_panel_menu (PanelWidget *panel);

PanelData * panel_data_by_id (int id);
void panel_set_id (GtkWidget *widget, int id);

void status_unparent (GtkWidget *widget);



#define get_panel_parent(appletw) \
	 (PANEL_WIDGET(GTK_WIDGET(appletw)->parent)->panel_parent)


END_GNOME_DECLS

#endif
