#ifndef DRAWER_H
#define DRAWER_H

#include "panel.h"

BEGIN_GNOME_DECLS

#define DRAWER_PANEL_KEY "drawer_panel"

typedef struct {
	char *pixmap;
	char *tooltip;
	GtkWidget *button;
	GtkWidget *drawer;
	PanelOrientType orient;
} Drawer;

Drawer * create_drawer_applet(GtkWidget * drawer_panel,
			      char *tooltip, char *pixmap,
			      PanelOrientType orient);

void set_drawer_applet_orient(Drawer *drawer, PanelOrientType orient);

Drawer * create_empty_drawer_applet(char *tooltip, char *pixmap,
				    PanelOrientType orient);

void reposition_drawer(Drawer *drawer);

void drawer_properties(Drawer *drawer);

END_GNOME_DECLS

#endif
