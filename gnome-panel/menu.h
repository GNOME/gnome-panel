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

Menu * create_menu_applet(GtkWidget *window ,char *arguments,
			  MenuOrient orient);


END_GNOME_DECLS

#endif
