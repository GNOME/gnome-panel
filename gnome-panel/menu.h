#ifndef MENU_H
#define MENU_H

#include <panel-widget.h>

BEGIN_GNOME_DECLS

typedef enum {
	MAIN_MENU_BOTH,
	MAIN_MENU_USER,
	MAIN_MENU_SYSTEM
} MainMenuType;
typedef struct _Menu Menu;
struct _Menu {
	GtkWidget *button;
	GtkWidget *menu;
	char *path;
	MainMenuType main_menu_type;
	PanelOrientType orient;
};

void load_menu_applet(char *params, int main_menu_type,
		      int pos, PanelWidget *panel);

void set_menu_applet_orient(Menu *menu, PanelOrientType orient);

/*used to set unset visibility of small_icons*/
void set_show_small_icons(void);

void setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title);
void make_panel_submenu (GtkWidget *menu, int fake_submenus);

GtkWidget * create_panel_root_menu(GtkWidget *panel);

END_GNOME_DECLS

#endif
