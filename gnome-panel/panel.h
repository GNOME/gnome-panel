#ifndef PANEL_H
#define PANEL_H

#include "panel_cmds.h"
#include "libgnomeui/gnome-session.h"

BEGIN_GNOME_DECLS

#define DEFAULT_APPLET_WIDTH 50
#define DEFAULT_APPLET_HEIGHT 50


typedef enum {
	PANEL_POS_TOP,
	PANEL_POS_BOTTOM,
	PANEL_POS_LEFT,
	PANEL_POS_RIGHT
} PanelPos;

typedef enum {
	PANEL_HIDDEN,
	PANEL_MOVING,
	PANEL_SHOWN
} PanelState;

typedef enum {
	PANEL_STAYS_PUT,
	PANEL_GETS_HIDDEN
} PanelMode;

typedef struct {
	GtkWidget  *window;
	GtkWidget  *table;
	GtkWidget  *hidebutton_l_h;
	GtkWidget  *hidebutton_r_h;
	GtkWidget  *hidebutton_l_v;
	GtkWidget  *hidebutton_r_v;
	GtkWidget  *fixed;
	PanelPos    pos;
	PanelState  state;
	PanelMode   mode;
	gint        step_size;
	gint        delay;
	gint        tooltips_enabled;
	gint        enter_notify_id;
	gint        leave_notify_id;
	gint        visibility_notify_id;
	gint        leave_notify_timer_tag;
	gint        minimize_delay;
	gint        minimized_size;
	GtkWidget  *applet_being_dragged;
	gint        applet_drag_click_x;
	gint        applet_drag_click_y;
	gint        applet_drag_orig_x;
	gint        applet_drag_orig_y;
} Panel;


extern Panel *the_panel;


void panel_init(void);
void panel_init_applet_modules(void);

gpointer panel_command(PanelCommand *cmd);

int panel_session_save (gpointer client_data,
			GnomeSaveStyle save_style,
			int is_shutdown,
			GnomeInteractStyle interact_style,
			int is_fast);

void panel_reconfigure(Panel *newconfig);


END_GNOME_DECLS

#endif
