#ifndef PANEL_H
#define PANEL_H

#include "panel-widget.h"

BEGIN_GNOME_DECLS

#define MENU_ID "Menu"
/*FIXME: maybe add a temporary launcher but these will be provided by the
  filemanager*/
/*#define LAUNCHER_ID "Launcher"*/
#define DRAWER_ID "Drawer"
#define LOGOUT_ID "Logout"
#define EXTERN_ID "Extern"

#define DEFAULT_AUTO_HIDE_STEP_SIZE 10
#define DEFAULT_EXPLICIT_HIDE_STEP_SIZE 50

/* amount of time in ms. to wait before lowering panel */
#define DEFAULT_MINIMIZE_DELAY 300

/* number of pixels it'll stick up from the bottom when using
   PANEL_AUTO_HIDE */
#define DEFAULT_MINIMIZED_SIZE 6

#define DEFAULT_PANEL_NUM 0

#define PANEL_UNKNOWN_APPLET_POSITION -1
#define PANEL_UNKNOWN_STEP_SIZE -1


typedef struct _PanelConfig PanelConfig;
struct _PanelConfig {
	PanelOrientation orient;
	PanelSnapped snapped;
	PanelMode mode;
};

typedef enum {
	APPLET_EXTERN,
	APPLET_EXTERN_RESERVED,
	APPLET_EXTERN_PENDING,
	APPLET_DRAWER,
	APPLET_MENU,
	APPLET_LOGOUT,
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
	gchar *name;
	gchar *text;
	AppletInfo *info;
};

struct _AppletInfo {
	AppletType type;
	int applet_id;
	GtkWidget *widget;
	GtkWidget *assoc; /*associated widget, e.g. a drawer or a menu*/
	GtkWidget *menu; /*the applet menu*/
	gpointer data;
	gchar *id; /*used for IOR or string Id*/
	gchar *cfg; /*used for passing around the per applet config path*/
	gchar *params; /*used for parameters to internal applets and for path
			 for external applets*/
	GList *user_menu; /*list of AppletUserMenu items for callbacks*/
};


gint panel_session_save (GnomeClient *client,
			 gint phase,
			 GnomeSaveStyle save_style,
			 gint shutdown,
			 GnomeInteractStyle interact_style,
			 gint fast,
			 gpointer client_data);

GtkWidget * create_panel_root_menu(PanelWidget *panel);

void register_toy(GtkWidget *applet,
		  GtkWidget *assoc,
		  gpointer data,
		  char *id,
		  char *params,
		  int pos,
		  int panel,
		  char *cfgpath,
		  AppletType type);

void panel_quit(void);

void apply_global_config(void);

void reparent_window_id (unsigned long winid, int id);
int applet_request_id (const char * ior, const char *path, char **cfgpath,
		       char **globcfgpath);
void applet_request_glob_cfg (char **globcfgpath);
void reserve_applet_spot (const char *id, const char *path, int panel,
			  int pos, char *cfgpath, AppletType type);

/*stuff for corba*/
int applet_get_panel(int id);
int applet_get_pos(int id);
void applet_show_menu(int id);
void applet_drag_start(int id);
void applet_drag_stop(int id);
void applet_add_callback(short id, char *callback_name, char *menuitem_text);

/*this is in main.c*/
void load_applet(char *id, char *params, int pos, int panel, char *cfgpath);
void orientation_change(AppletInfo *info, PanelWidget *panel);

END_GNOME_DECLS

#endif
