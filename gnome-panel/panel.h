#ifndef PANEL_H
#define PANEL_H

#include <gtk/gtk.h>
#include "basep-widget.h"
#include "panel-widget.h"
#include "applet.h"

G_BEGIN_DECLS

typedef struct _PanelData PanelData;
struct _PanelData {
	PanelType type;
	GtkWidget *panel;
	GtkWidget *menu;
	int menu_age;
};

void orientation_change(AppletInfo *info, PanelWidget *panel);
void size_change(AppletInfo *info, PanelWidget *panel);
void back_change(AppletInfo *info, PanelWidget *panel);

PanelOrient get_applet_orient(PanelWidget *panel);

void panel_setup(GtkWidget *panel);
void basep_pos_connect_signals (BasePWidget *basep);

/*send state change to all the panels*/
void send_state_change(void);

GtkWidget * make_popup_panel_menu (PanelWidget *panel);

PanelData * panel_data_by_id (const char *id);
void panel_set_id (GtkWidget *widget, const char *id);

void status_unparent (GtkWidget *widget);



#define get_panel_parent(appletw) \
	 (PANEL_WIDGET(GTK_WIDGET(appletw)->parent)->panel_parent)


void panel_session_init_global_config (void);
void panel_session_save_global_config (void);
void panel_session_apply_global_config(void);

void panel_session_init_panels (void);
void panel_session_save_panel (PanelData *pd);
void panel_session_remove_panel_from_config (PanelWidget *panel);

G_END_DECLS

#endif
