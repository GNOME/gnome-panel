#include "panel-widget.h"

#ifndef PANEL_CONFIG_GLOBAL_H
#define PANEL_CONFIG_GLOBAL_H

typedef struct _GlobalConfig GlobalConfig;
struct _GlobalConfig {
	gint auto_hide_step_size;
	gint explicit_hide_step_size;
	gint minimized_size;
	gint minimize_delay;
	gint tooltips_enabled;
	gint show_small_icons;
	PanelMovementType movement_type;
};


void panel_config_global(void);

#endif /* PANEL_CONFIG_H */
