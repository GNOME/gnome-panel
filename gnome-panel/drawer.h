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

gboolean load_drawer_applet (int mypanel, const char *pixmap,
			     const char *tooltip,
			     PanelWidget *panel, int pos, gboolean exactpos);

void set_drawer_applet_orient(Drawer *drawer, PanelOrientType orient);

void add_drawer_properties_page(PerPanelConfig *ppc, GtkNotebook *prop_nbook, Drawer *drawer);

END_GNOME_DECLS

#endif
