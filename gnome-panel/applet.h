#ifndef APPLET_H
#define APPLET_H

#include <glib.h>
#include <gdk/gdktypes.h>
#include "panel-widget.h"

G_BEGIN_DECLS

#define EMPTY_ID    "Empty"
#define MENU_ID     "Menu"
#define DRAWER_ID   "Drawer"
#define LOGOUT_ID   "Logout"
#define SWALLOW_ID  "Swallow"
#define EXTERN_ID   "Extern"
#define LAUNCHER_ID "Launcher"
#define LOCK_ID     "Lock"
#define STATUS_ID   "Status"
#define RUN_ID      "Run"

typedef enum {
	APPLET_EXTERN,
	APPLET_EXTERN_RESERVED,
	APPLET_EXTERN_PENDING,
	APPLET_DRAWER,
	APPLET_MENU,
	APPLET_LOGOUT,
	APPLET_SWALLOW,
	APPLET_LAUNCHER,
	APPLET_EMPTY,
	APPLET_LOCK,
	APPLET_STATUS,
	APPLET_RUN,
	APPLET_BONOBO
} AppletType;

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)
typedef struct {
	AppletType      type;
	int             applet_id;
	GtkWidget      *widget;

	GtkWidget      *menu;
	int             menu_age;
	GList          *user_menu;

	gpointer        data;
	GDestroyNotify  data_destroy;
} AppletInfo;

typedef struct {
	gchar        *name;
	gchar        *stock_item;
	gchar        *text;

	gint          sensitive;
	AppletInfo   *info;

	GtkWidget    *menuitem;
	GtkWidget    *submenu;
} AppletUserMenu;

AppletInfo *panel_applet_register    (GtkWidget      *applet,
				      gpointer        data,
				      GDestroyNotify  data_destroy,
				      PanelWidget    *panel,
				      gint            pos,
				      gboolean        exactpos,
				      AppletType      type);

void        panel_applet_clean       (AppletInfo *info);

void        panel_applet_create_menu (AppletInfo *info,
				      gboolean    is_basep);


void            panel_applet_add_callback    (AppletInfo  *info,
					      const gchar *callback_name,
					      const gchar *stock_item,
					      const gchar *menuitem_text);

void            panel_applet_remove_callback (AppletInfo *info,
					      const char *callback_name);

AppletUserMenu *panel_applet_get_callback    (GList       *user_menu,
					      const gchar *name);


void        panel_applet_callback_set_sensitive (AppletInfo *info,
						 const char *callback_name,
						 gint        sensitive);

void remove_unused_launchers (void);

G_END_DECLS

#endif
