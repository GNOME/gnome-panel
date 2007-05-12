#ifndef DRAWER_H
#define DRAWER_H

#include "panel.h"

G_BEGIN_DECLS

#define PANEL_DRAWER_N_LISTENERS 3

typedef struct {
	char          *tooltip;

	PanelToplevel *toplevel;
	GtkWidget     *button;

	gboolean       opened_for_drag;
	guint          close_timeout_id;

	AppletInfo    *info;

	guint          listeners [PANEL_DRAWER_N_LISTENERS];
} Drawer;

void  panel_drawer_create          (PanelToplevel *toplevel,
				    int            position,
				    const char    *custom_icon,
				    gboolean       use_custom_icon,
				    const char    *tooltip);

char *panel_drawer_create_with_id  (const char    *toplevel_id,
				    int            position,
				    const char    *custom_icon,
				    gboolean       use_custom_icon,
				    const char    *tooltip);

void  panel_drawer_set_dnd_enabled (Drawer        *drawer,
				    gboolean       dnd_enabled);

void  drawer_load_from_gconf       (PanelWidget   *panel_widget,
				    gboolean       locked,
				    gint           position,
				    const char    *id);

void  drawer_query_deletion        (Drawer *drawer);

G_END_DECLS

#endif
