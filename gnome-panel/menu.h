#ifndef MENU_H
#define MENU_H

BEGIN_GNOME_DECLS

typedef struct _Menu Menu;
struct _Menu {
	GtkWidget *button;
	GtkWidget *menu;
	char *path;
	PanelOrientType orient;
};

void init_main_menu(void);

Menu * create_menu_applet(char *arguments,
			  PanelOrientType orient);

void set_menu_applet_orient(Menu *menu, PanelOrientType orient);

/*used in foreach to set unset visibility of small_icons*/
void set_show_small_icons(gpointer data, gpointer user_data);

void setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title);

END_GNOME_DECLS

#endif
