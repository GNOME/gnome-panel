#ifndef PANEL_H
#define PANEL_H

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

#define DEFAULT_AUTO_HIDE_STEP_SIZE 10
#define DEFAULT_EXPLICIT_HIDE_STEP_SIZE 50
#define DEFAULT_DRAWER_STEP_SIZE 30

/* amount of time in ms. to wait before lowering panel */
#define DEFAULT_MINIMIZE_DELAY 300

/* number of pixels it'll stick up from the bottom when using
   PANEL_AUTO_HIDE */
#define DEFAULT_MINIMIZED_SIZE 6

#define DEFAULT_PANEL_NUM 0

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
	GtkWidget *remove_item; /*remove item in the applet_menu*/
	GList *user_menu; /*list of AppletUserMenu items for callbacks*/
	gpointer data; /*the per applet structure, if it exists*/
};

/*FIXME: split into extern.[ch] files*/
typedef struct _Extern Extern;
struct _Extern {
	char *ior;
	char *path;
	char *params;
	char *cfg;
};
void extern_clean(Extern *ext);




int panel_session_save (GnomeClient *client,
			int phase,
			GnomeSaveStyle save_style,
			int shutdown,
			GnomeInteractStyle interact_style,
			int fast,
			gpointer client_data);

int panel_session_die (GnomeClient *client,
			gpointer client_data);

GtkWidget * create_panel_root_menu(GtkWidget *panel);

int register_toy(GtkWidget *applet,
		 gpointer data,
		 int pos,
		 PanelWidget *panel,
		 AppletType type);

void panel_quit(void);

void panel_config_sync(void);

void apply_global_config(void);

void panel_clean_applet(int applet_id);

/*stuff for corba*/
int applet_request_id (const char *path, const char *param,
		       int dorestart, char **cfgpath,
		       char **globcfgpath, guint32 *winid);
void applet_register (const char * ior, int applet_id);
guint32 reserve_applet_spot (Extern *ext, PanelWidget *panel, int pos,
			     AppletType type);
void applet_abort_id(int applet_id);
int applet_get_panel(int applet_id);
int applet_get_pos(int applet_id);
PanelOrientType applet_get_panel_orient(int applet_id);
void applet_show_menu(int applet_id);
void applet_drag_start(int applet_id);
void applet_drag_stop(int applet_id);
void applet_add_callback(int applet_id,
			 char *callback_name,
			 char *stock_item,
			 char *menuitem_text);
void applet_remove_callback(int applet_id,
			    char *callback_name);
void applet_set_tooltip(int applet_id, const char *tooltip);

extern char *cookie;

#define get_applet_info(applet_id) \
	((applet_id>=0 && applet_id<applet_count) ? \
	 (&g_array_index(applets,AppletInfo,applet_id)):NULL)

/* Gross backward compatibility hack.  */
#ifndef GPOINTER_TO_INT
# if SIZEOF_INT == SIZEOF_VOID_P
#  define GPOINTER_TO_INT(p)	((gint)(p))
#  define GINT_TO_POINTER(i)    ((gpointer)(i))
# elif SIZEOF_LONG == SIZEOF_VOID_P
#  define GPOINTER_TO_INT(p)	((gint)(glong)(p))
#  define GINT_TO_POINTER(i)	((gpointer)(glong)(i))
# endif /* SIZEOF_INT */
#endif /* GPOINTER_TO_INT */

END_GNOME_DECLS

#endif
