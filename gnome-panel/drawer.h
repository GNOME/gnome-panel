#ifndef DRAWER_H
#define DRAWER_H

BEGIN_GNOME_DECLS

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

Drawer * create_drawer_applet(GtkWidget *window, GtkWidget * drawer_panel,
			      DrawerOrient orient);

void set_drawer_applet_orient(Drawer *drawer, DrawerOrient orient);

Drawer * create_empty_drawer_applet(GtkWidget *window, DrawerOrient orient);

void reposition_drawer(Drawer *drawer);


END_GNOME_DECLS

#endif
