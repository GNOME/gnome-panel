#ifndef PANEL_H
#define PANEL_H

#include "panel_cmds.h"
#include "applet_cmds.h"
#include "panel-widget.h"
#include "libgnomeui/gnome-session.h"

BEGIN_GNOME_DECLS

#define MENU_ID "Menu"
#define LAUNCHER_ID "Launcher"
#define DRAWER_ID "Drawer"

typedef struct _PanelConfig PanelConfig;
struct _PanelConfig {
	PanelOrientation orient;
	PanelSnapped snapped;
	PanelMode mode;
	PanelState state;
	gint step_size;
	gint minimized_size;
	gint minimize_delay;
};

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
};


void panel_init(void);
void panel_init_applet_modules(void);

gpointer panel_command(PanelCommand *cmd);

int panel_session_save (gpointer client_data,
			GnomeSaveStyle save_style,
			int is_shutdown,
			GnomeInteractStyle interact_style,
			int is_fast);

GtkWidget * create_panel_root_menu(PanelWidget *panel);

void create_applet_menu(void);

END_GNOME_DECLS

#endif
