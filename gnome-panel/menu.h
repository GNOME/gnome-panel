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

void init_main_menu(void);

Menu * create_menu_applet(char *arguments,
			  MenuOrient orient);

void set_menu_applet_orient(Menu *menu, MenuOrient orient);

/*used in foreach to set unset visibility of small_icons*/
void set_show_small_icons(gpointer data, gpointer user_data);

END_GNOME_DECLS

#endif
