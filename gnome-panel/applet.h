#ifndef APPLET_H
#define APPLET_H

#include <glib.h>
#include "panel-widget.h"

BEGIN_GNOME_DECLS

#define EMPTY_ID "Empty" /*this applet should not be loaded*/
#define MENU_ID "Menu"
#define DRAWER_ID "Drawer"
#define LOGOUT_ID "Logout"
#define SWALLOW_ID "Swallow"
#define EXTERN_ID "Extern"
#define LAUNCHER_ID "Launcher"

typedef enum {
	APPLET_EXTERN,
	APPLET_EXTERN_RESERVED,
	APPLET_EXTERN_PENDING,
	APPLET_DRAWER,
	APPLET_MENU,
	APPLET_LOGOUT,
	APPLET_SWALLOW,
	APPLET_LAUNCHER,
	APPLET_EMPTY
} AppletType;

typedef enum {
	ORIENT_UP,
	ORIENT_DOWN,
	ORIENT_LEFT,
	ORIENT_RIGHT
} PanelOrientType;

typedef struct _AppletUserMenu AppletUserMenu;
typedef struct _AppletInfo AppletInfo;

struct _AppletUserMenu {
	char *name;
	char *stock_item;
	char *text;
	int applet_id;
	GtkWidget *menuitem;
	GtkWidget *submenu;
};

struct _AppletInfo {
	AppletType type;
	int applet_id;
	GtkWidget *widget; /*an event box*/
	GtkWidget *applet_widget; /*the actual applet widget*/
	GtkWidget *menu; /*the applet menu*/
	GList *user_menu; /*list of AppletUserMenu items for callbacks*/
	gpointer data; /*the per applet structure, if it exists*/
};

int register_toy(GtkWidget *applet,
		 gpointer data,
		 int pos,
		 PanelWidget *panel,
		 AppletType type);

void panel_clean_applet(int applet_id);

/*applet menu stuff*/
void create_applet_menu(AppletInfo *info);
void applet_add_callback(int applet_id,
			 char *callback_name,
			 char *stock_item,
			 char *menuitem_text);
void applet_remove_callback(int applet_id,
			    char *callback_name);
void applet_menu_position (GtkMenu *menu, int *x, int *y,
			   gpointer data);

#define get_applet_info(applet_id) \
	((applet_id>=0 && applet_id<applet_count) ? \
	 (&g_array_index(applets,AppletInfo,applet_id)):NULL)

END_GNOME_DECLS

#endif
