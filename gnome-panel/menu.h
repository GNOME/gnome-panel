#ifndef MENU_H
#define MENU_H

#include <panel-widget.h>

BEGIN_GNOME_DECLS

/*compatibility only*/
typedef enum {
	X_MAIN_MENU_BOTH,
	X_MAIN_MENU_USER,
	X_MAIN_MENU_SYSTEM
} X_MainMenuType;

enum {
	MAIN_MENU_USER = 1<<0,
	MAIN_MENU_USER_SUB = 1<<1,
	MAIN_MENU_SYSTEM = 1<<2,
	MAIN_MENU_SYSTEM_SUB = 1<<3,
	MAIN_MENU_REDHAT = 1<<4,
	MAIN_MENU_REDHAT_SUB = 1<<5,
	MAIN_MENU_KDE = 1<<6,
	MAIN_MENU_KDE_SUB = 1<<7,
	MAIN_MENU_DEBIAN = 1<<8,
	MAIN_MENU_DEBIAN_SUB = 1<<9
};

typedef struct _Menu Menu;
struct _Menu {
	GtkWidget *button;
	GtkWidget *menu;
	char *path;
	int main_menu_flags;
	int age;
};

void load_menu_applet(char *params, int main_menu_flags,
		      PanelWidget *panel, int pos);

void set_menu_applet_orient(Menu *menu, PanelOrientType orient);

void setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title);
void make_panel_submenu (GtkWidget *menu, int fake_submenus);

/*do this before showing the panel menu*/
void show_x_on_panels(GtkWidget *menu);
GtkWidget * create_panel_root_menu(GtkWidget *panel);

void menu_properties(Menu *menu);

/*menu related utility functions .. also used elswhere*/
void applet_menu_position (GtkMenu *menu, int *x, int *y, gpointer data);
void panel_menu_position (GtkMenu *menu, int *x, int *y, gpointer data);


/*to be called on startup to load in some of the directories*/
void init_menus(void);

END_GNOME_DECLS

#endif
