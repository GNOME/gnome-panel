#ifndef MENU_H
#define MENU_H

BEGIN_GNOME_DECLS

typedef enum {
	MENU_UP,
	MENU_LEFT,
	MENU_RIGHT,
	MENU_DOWN
} MenuOrient;

typedef struct {
	GtkWidget *button;
	GtkWidget *menu;
	char *path;
	MenuOrient orient;
} Menu;

END_GNOME_DECLS

#endif
