#include "panel-widget.h"

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
	int prompt_for_logout;
	PanelMovementType movement_type;
	int disable_animations;
	int applet_padding;
	int tiles_enabled;
};


void panel_config_global(void);

#endif /* PANEL_CONFIG_H */
