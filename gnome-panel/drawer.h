#ifndef DRAWER_H
#define DRAWER_H

BEGIN_GNOME_DECLS

#define DRAWER_PANEL "drawer_panel"

typedef enum {
	DRAWER_UP,
	DRAWER_LEFT,
	DRAWER_RIGHT,
	DRAWER_DOWN
} DrawerOrient;

typedef struct {
	GtkWidget *button;
	GtkWidget *drawer;
	DrawerOrient orient;
} Drawer;

Drawer * create_drawer_applet(GtkWidget * drawer_panel,
			      DrawerOrient orient);

void set_drawer_applet_orient(Drawer *drawer, DrawerOrient orient);

Drawer * create_empty_drawer_applet(DrawerOrient orient);

void reposition_drawer(Drawer *drawer);


END_GNOME_DECLS

#endif
