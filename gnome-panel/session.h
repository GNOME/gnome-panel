#ifndef SESSION_H
#define SESSION_H

#include <glib.h>
#include <libgnomeui/gnome-client.h>
#include "panel-widget.h"

G_BEGIN_DECLS

#define DEFAULT_HIDE_SPEED 50

/* amount of time in ms. to wait before lowering panel */
#define DEFAULT_HIDE_DELAY 300
#define DEFAULT_SHOW_DELAY 0

/* number of pixels it'll stick up from the bottom when using
   PANEL_AUTO_HIDE */
#define DEFAULT_MINIMIZED_SIZE 6

#define DEFAULT_PANEL_NUM 0

#define PANEL_CONFIG_PATH "panel2.d/default/"

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
void session_load_global_config (void);

/* only to be used on beginning, we don't really want to write globals much */
void write_global_config (void);

void load_session (void);

void apply_global_config(void);

/* just temporary so that we can bridge the gap between old and new config */
void convert_old_config(void);

/* prefix thingie */
char * get_correct_prefix (char const **sep);
void push_correct_global_prefix (void);

void session_add_dead_launcher (const gchar *location);

G_END_DECLS

#endif
