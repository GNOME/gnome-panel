#include "panel-widget.h"
#include "panel-types.h"

#ifndef PANEL_CONFIG_GLOBAL_H
#define PANEL_CONFIG_GLOBAL_H

typedef struct _GlobalConfig GlobalConfig;
struct _GlobalConfig {
	int auto_hide_step_size;
	int explicit_hide_step_size;
	int drawer_step_size;
	int minimized_size;
	int minimize_delay;
	gboolean tooltips_enabled;
	/*gboolean show_small_icons;*/
	gboolean show_dot_buttons;
	gboolean hungry_menus;
	gboolean off_panel_popups;
	PanelMovementType movement_type;
	gboolean disable_animations;
	int applet_padding;
	gboolean autoraise;
	gboolean keep_bottom;
	gboolean drawer_auto_close;
	gboolean simple_movement;
	gboolean tiles_enabled[LAST_TILE];
	char *tile_up[LAST_TILE];
	char *tile_down[LAST_TILE];
	int tile_border[LAST_TILE];
	int tile_depth[LAST_TILE];
	gboolean hide_panel_frame;
	gboolean tile_when_over;
	gboolean saturate_when_over;
	gboolean confirm_panel_remove;
	gboolean keys_enabled;
	char *menu_key;
	  guint menu_keysym;  /* these are not really properties but */
	  guint menu_state;   /* from the above */
	char *run_key;
	  guint run_keysym;   /* these are not really properties but */
	  guint run_state;    /* from the above */
	int menu_flags;
	gboolean use_large_icons;
	gboolean merge_menus;
	gboolean fast_button_scaling;
};

#endif /* PANEL_CONFIG_GLOBAL_H */
