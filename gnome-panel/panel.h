#ifndef PANEL_H
#define PANEL_H

#include "panel-widget.h"

BEGIN_GNOME_DECLS

#define MENU_ID "Menu"
#define DRAWER_ID "Drawer"
#define LOGOUT_ID "Logout"
#define SWALLOW_ID "Swallow"
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
	APPLET_SWALLOW,
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
	GtkWidget *widget; /*an event box*/
	GtkWidget *applet_widget; /*the actual applet widget*/
	GtkWidget *assoc; /*associated widget, e.g. a drawer or a menu*/
	GtkWidget *menu; /*the applet menu*/
	gpointer data;
	gchar *id_str; /*used for IOR or string Id*/
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

gint register_toy(GtkWidget *applet,
		  GtkWidget *assoc,
		  gpointer data,
		  char *id_str,
		  char *params,
		  int pos,
		  int panel,
		  char *cfgpath,
		  AppletType type);

void panel_quit(void);

void apply_global_config(void);

void panel_clean_applet(AppletInfo *info);

/*stuff for corba*/
int applet_request_id (const char *path, char **cfgpath,
		       char **globcfgpath, guint32 *winid);
void applet_register (const char * ior, int applet_id);
void applet_request_glob_cfg (char **globcfgpath);
guint32 reserve_applet_spot (const char *id_str, const char *path,
			     int panel, int pos, char *cfgpath,
			     AppletType type);
void applet_abort_id(gint applet_id);
int applet_get_panel(gint applet_id);
int applet_get_pos(gint applet_id);
void applet_show_menu(gint applet_id);
void applet_drag_start(gint applet_id);
void applet_drag_stop(gint applet_id);
void applet_add_callback(gint applet_id,
			 char *callback_name,
			 char *menuitem_text);
void applet_set_tooltip(gint applet_id, const char *tooltip);
void applet_remove_from_panel(gint applet_id);


/*this is in main.c*/
void load_applet(char *id, char *params, int pos, int panel, char *cfgpath);
void orientation_change(AppletInfo *info, PanelWidget *panel);

END_GNOME_DECLS

#endif
