#ifndef SESSION_H
#define SESSION_H

#include <glib.h>
#include <libgnomeui/gnome-client.h>
#include "panel-widget.h"

G_BEGIN_DECLS

int panel_session_save (GnomeClient *client,
			int phase,
			GnomeSaveStyle save_style,
			int shutdown,
			GnomeInteractStyle interact_style,
			int fast,
			gpointer client_data);

void save_next_applet(void);


int panel_session_die (GnomeClient *client,
			gpointer client_data);

void panel_quit(void);

void panel_config_sync(void);
void panel_config_sync_schedule (void);

void load_system_wide (void);

void session_load (void);

void session_add_dead_launcher (const gchar *location);

void panel_session_setup_config_sync   (void);

void panel_session_set_restart_command (GnomeClient *client,
					char        *exec);

G_END_DECLS

#endif
