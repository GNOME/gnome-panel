#ifndef PANEL_H
#define PANEL_H

#include "panel_cmds.h"
#include "panel-widget.h"

BEGIN_GNOME_DECLS

#define MENU_ID "Menu"
/*FIXME: maybe add a temporary launcher but these will be provided by the
  filemanager*/
/*#define LAUNCHER_ID "Launcher"*/
#define DRAWER_ID "Drawer"

#define DEFAULT_AUTO_HIDE_STEP_SIZE 10
#define DEFAULT_EXPLICIT_HIDE_STEP_SIZE 50

/* amount of time in ms. to wait before lowering panel */
#define DEFAULT_MINIMIZE_DELAY 300

/* number of pixels it'll stick up from the bottom when using
   PANEL_AUTO_HIDE */
#define DEFAULT_MINIMIZED_SIZE 6

#define DEFAULT_PANEL_NUM 0

typedef struct _PanelConfig PanelConfig;
struct _PanelConfig {
	PanelOrientation orient;
	PanelSnapped snapped;
	PanelMode mode;
};

typedef enum {
	APPLET_HAS_PROPERTIES = 1L << 0
} AppletFlags;

typedef enum {
	APPLET_EXTERN,
	APPLET_DRAWER,
	APPLET_MENU,
	APPLET_LAUNCHER
} AppletType;

typedef struct _AppletInfo AppletInfo;
struct _AppletInfo {
	GtkWidget *widget;
	GtkWidget *assoc; /*associated widget, e.g. a drawer or a menu*/
	gpointer data;
	AppletType type;
	AppletFlags flags;
	gchar *id;
	gchar *params;
};


gpointer panel_command(PanelCommand *cmd);

gint panel_session_save (GnomeClient *client,
			 gint phase,
			 GnomeSaveStyle save_style,
			 gint shutdown,
			 GnomeInteractStyle interact_style,
			 gint fast,
			 gpointer client_data);

GtkWidget * create_panel_root_menu(PanelWidget *panel);

void create_applet_menu(void);

void register_toy(GtkWidget *applet,
		  GtkWidget *assoc,
		  char *id,
		  char *params,
		  int pos,
		  int panel,
		  long flags,
		  AppletType type);

void panel_quit(void);

void apply_global_config(void);

int reparent_window_id (unsigned long id, int panel, int pos);

int applet_get_panel(int id);
int applet_get_pos(int id);


/*this is in main.c*/
void load_applet(char *id, char *params, int pos, int panel);

END_GNOME_DECLS

#endif
