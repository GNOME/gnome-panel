#ifndef MENU_H
#define MENU_H

#include <panel-widget.h>
#include "applet.h"

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
	MAIN_MENU_DEBIAN_SUB = 1<<9,
	MAIN_MENU_APPLETS = 1<<10,
	MAIN_MENU_APPLETS_SUB = 1<<11,
	MAIN_MENU_PANEL = 1<<12,
	MAIN_MENU_PANEL_SUB = 1<<13,
	MAIN_MENU_DESKTOP = 1<<14,
	MAIN_MENU_DESKTOP_SUB = 1<<15
};

typedef struct _Menu Menu;
struct _Menu {
	GtkWidget *button;
	GtkWidget *menu;
	char *path;
	int main_menu_flags;
	int age;
	AppletInfo *info;
};

void load_menu_applet(char *params, int main_menu_flags,
		      PanelWidget *panel, int pos);
void add_menu_widget (Menu *menu, GSList *menudirl, int main_menu,
		      int fake_subs);

void set_menu_applet_orient(Menu *menu, PanelOrientType orient);

void setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, char *title);
void make_panel_submenu (GtkWidget *menu, int fake_submenus);

/*do this before showing the panel menu*/
/*static void show_x_on_panels(GtkWidget *menu, gpointer data);*/
GtkWidget * create_panel_root_menu(GtkWidget *panel, int tearoff);

void menu_properties(Menu *menu);

void panel_lock (GtkWidget *widget, void *data);

/*to be called on startup to load in some of the directories*/
void init_menus(void);

#define MENU_PATH "menu_path"

#define MENU_PROPERTIES "menu_properties"

#define DEBIAN_MENUDIR "/var/lib/gnome/Debian/."

#define MENU_TYPES "types_menu"
#define MENU_TYPE_EDGE "Edge panel"
#define MENU_TYPE_ALIGNED "Aligned panel"
#define MENU_TYPE_SLIDING "Sliding panel"
#define MENU_TYPE_FLOATING "Floating panel"

#define MENU_MODES "modes_menu"
#define MENU_MODE_EXPLICIT_HIDE "Explicit hide"
#define MENU_MODE_AUTO_HIDE "Auto hide"

#define MENU_HIDEBUTTONS "hidebuttons_menu"
#define MENU_HIDEBUTTONS_PIXMAP "With pixmap arrow"
#define MENU_HIDEBUTTONS_PLAIN "Without pixmap"
#define MENU_HIDEBUTTONS_NONE "None"

/* perhaps into basep-widget.h? */
enum {
	HIDEBUTTONS_PIXMAP,
	HIDEBUTTONS_PLAIN,
	HIDEBUTTONS_NONE
};

#define MENU_SIZES "sizes_menu"
#define MENU_SIZE_TINY "Tiny (24 pixels)"
#define MENU_SIZE_SMALL "Small (36 pixels)"
#define MENU_SIZE_STANDARD "Standard (48 pixels)"
#define MENU_SIZE_LARGE "Large (64 pixels)"
#define MENU_SIZE_HUGE "Huge (80 pixels"

#define MENU_BACKS "background_menu"
#define MENU_BACK_NONE "Standard"
#define MENU_BACK_PIXMAP "Pixmap"
#define MENU_BACK_COLOR "Color"

#define MENU_ORIENTS "orients_menu"
#define MENU_ORIENT_HORIZONTAL "Horizontal"
#define MENU_ORIENT_VERTICAL "Vertical"

END_GNOME_DECLS

#endif
