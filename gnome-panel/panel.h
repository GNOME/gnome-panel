#ifndef PANEL_H
#define PANEL_H

#include "panel_cmds.h"
#include "panel-widget.h"
#include "libgnomeui/gnome-session.h"

BEGIN_GNOME_DECLS

extern PanelWidget *the_panel;


void panel_init(void);
void panel_init_applet_modules(void);

gpointer panel_command(PanelCommand *cmd);

int panel_session_save (gpointer client_data,
			GnomeSaveStyle save_style,
			int is_shutdown,
			GnomeInteractStyle interact_style,
			int is_fast);

END_GNOME_DECLS

#endif
