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

void load_drawer_applet(char *params, char *pixmap, char *tooltip,
			int pos, PanelWidget *panel);

void set_drawer_applet_orient(Drawer *drawer, PanelOrientType orient);

void reposition_drawer(Drawer *drawer);
void drawer_properties(Drawer *drawer);

END_GNOME_DECLS

#endif
