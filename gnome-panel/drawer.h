#ifndef DRAWER_H
#define DRAWER_H

#include "panel.h"
#include "panel-config.h"

G_BEGIN_DECLS

#define DRAWER_PANEL_KEY "drawer_panel"

typedef struct {
	char *pixmap;
	char *tooltip;
	GtkWidget *button;
	GtkWidget *drawer;
	GtkWidget *properties;
} Drawer;

Drawer *load_drawer_applet (gchar       *mypanel,
			    const char  *pixmap,
			    const char  *tooltip,
			    PanelWidget *panel,
			    int          pos,
			    gboolean     exactpos);

void set_drawer_applet_orient(Drawer *drawer, PanelOrient orient);

void add_drawer_properties_page(PerPanelConfig *ppc, GtkNotebook *prop_nbook, Drawer *drawer);

G_END_DECLS

#endif
