#ifndef SESSION_H
#define SESSION_H

#include <glib.h>
#include "panel-widget.h"

BEGIN_GNOME_DECLS

#define DEFAULT_AUTO_HIDE_STEP_SIZE 10
#define DEFAULT_EXPLICIT_HIDE_STEP_SIZE 80
#define DEFAULT_DRAWER_STEP_SIZE 80

/* amount of time in ms. to wait before lowering panel */
#define DEFAULT_MINIMIZE_DELAY 300
#define DEFAULT_MAXIMIZE_DELAY 0

/* number of pixels it'll stick up from the bottom when using
   PANEL_AUTO_HIDE */
#define DEFAULT_MINIMIZED_SIZE 6

#define DEFAULT_PANEL_NUM 0

#define PANEL_CONFIG_PATH "/panel.d/default/"

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
void load_up_globals (void);
/* only to be used on beginning, we don't really want to write globals much */
void write_global_config (void);

void init_user_applets(void);
void init_user_panels(void);

void apply_global_config(void);

/* just temporary so that we can bridge the gap between old and new config */
void convert_old_config(void);

/* prefix thingie */
char * get_correct_prefix (char const **sep);
void push_correct_global_prefix (void);

END_GNOME_DECLS

#endif
