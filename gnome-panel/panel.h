#ifndef PANEL_H
#define PANEL_H

#include "panel_cmds.h"
#include "panel-widget.h"
#include "libgnomeui/gnome-session.h"

BEGIN_GNOME_DECLS

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


void panel_init(void);
void panel_init_applet_modules(void);

gpointer panel_command(PanelCommand *cmd);

int panel_session_save (gpointer client_data,
			GnomeSaveStyle save_style,
			int is_shutdown,
			GnomeInteractStyle interact_style,
			int is_fast);

GtkWidget * create_panel_root_menu(PanelWidget *panel);

END_GNOME_DECLS

#endif
