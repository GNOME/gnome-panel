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
	int insertion_pos;
	guint deactivate_idle;
};

void orientation_change(AppletInfo *info, PanelWidget *panel);
void size_change(AppletInfo *info, PanelWidget *panel);
void back_change(AppletInfo *info, PanelWidget *panel);

void basep_pos_connect_signals (BasePWidget *basep);

/*send state change to all the panels*/
void send_state_change(void);

PanelData *panel_setup(GtkWidget *panel);
PanelData *panel_data_by_id (const char *id);

GtkWidget *make_popup_panel_menu (PanelWidget *panel);

#define get_panel_parent(appletw) \
	 (PANEL_WIDGET(GTK_WIDGET(appletw)->parent)->panel_parent)

void panel_load_global_config  (void);
void panel_save_global_config  (void);
void panel_apply_global_config (void);

void panel_load_panels_from_gconf (void);

void panel_remove_from_gconf (PanelWidget *panel);
void panel_save_to_gconf     (PanelData *pd);

void panel_register_window_icon (void);

GdkScreen *panel_screen_from_toplevel (GtkWidget *panel);
int        panel_monitor_from_toplevel (GtkWidget *panel);
int        panel_screen_from_panel_widget  (PanelWidget *panel);
int        panel_monitor_from_panel_widget (PanelWidget *panel);

gboolean panel_is_applet_right_stick (GtkWidget *applet);


gboolean panel_check_dnd_target_data (GtkWidget      *widget,
				      GdkDragContext *context,
				      guint          *ret_info,
				      GdkAtom        *ret_atom);

void panel_receive_dnd_data (PanelWidget      *panel,
			     guint             info,
			     int               pos,
			     GtkSelectionData *selection_data,
			     GdkDragContext   *context,
			     guint             time_,
			     gboolean          is_foobar);

gboolean panel_check_drop_forbidden (PanelWidget    *panel,
				     GdkDragContext *context,
				     guint           info,
				     guint           time_);

G_END_DECLS

#endif
