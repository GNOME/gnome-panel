#ifndef DRAWER_H
#define DRAWER_H

#include "panel.h"

BEGIN_GNOME_DECLS

#define DRAWER_PANEL "drawer_panel"

typedef struct {
	GtkWidget *button;
	GtkWidget *drawer;
	PanelOrientType orient;
} Drawer;

Drawer * create_drawer_applet(GtkWidget * drawer_panel,
			      PanelOrientType orient);

void set_drawer_applet_orient(Drawer *drawer, PanelOrientType orient);

Drawer * create_empty_drawer_applet(PanelOrientType orient);

void reposition_drawer(Drawer *drawer);


END_GNOME_DECLS

#endif
