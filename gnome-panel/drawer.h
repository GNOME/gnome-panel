#ifndef DRAWER_H
#define DRAWER_H

#include "panel.h"
#include "panel_config.h"

BEGIN_GNOME_DECLS

#define DRAWER_PANEL_KEY "drawer_panel"

typedef struct {
	char *pixmap;
	char *tooltip;
	GtkWidget *button;
	GtkWidget *drawer;
	GtkWidget *properties;
} Drawer;

void load_drawer_applet(int mypanel, char *pixmap, char *tooltip,
			PanelWidget *panel, int pos, gboolean exactpos);

void set_drawer_applet_orient(Drawer *drawer, PanelOrientType orient);

void add_drawer_properties_page(PerPanelConfig *ppc, Drawer *drawer);

END_GNOME_DECLS

#endif
