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
	int tooltips_enabled;
	int show_small_icons;
	int show_dot_buttons;
	int hungry_menus;
	int off_panel_popups;
	PanelMovementType movement_type;
	int disable_animations;
	int applet_padding;
	int autoraise;
	int keep_bottom;
	int drawer_auto_close;
	int simple_movement;
	int tiles_enabled[LAST_TILE];
	char *tile_up[LAST_TILE];
	char *tile_down[LAST_TILE];
	int tile_border[LAST_TILE];
	int tile_depth[LAST_TILE];
	int hide_panel_frame;
	int tile_when_over;
};

#endif /* PANEL_CONFIG_GLOBAL_H */
