#ifndef PANEL_CONFIG_GLOBAL_H
#define PANEL_CONFIG_GLOBAL_H

#include <gconf/gconf-client.h>

#include "panel-types.h"

G_BEGIN_DECLS

typedef struct {
	char  *str;
	guint  keysym;
	guint  state;
} GlobalConfigKey;

/*
 * Corresponding keys in the global config schema:
 *
 * tooltips-enabled
 * keep-menus-in-memory
 * enable-animations
 *
 * panel-minimized-size
 * panel-show-delay
 * panel-animation-speed
 * panel-hide-delay
 *
 * enable-key-bindings
 * menu-key
 * menu-key
 * run-key
 * window-screenshot-key
 *
 * autoraise-panel
 * panel-window-layer
 *
 * drawer-autoclose
 * confirm-panel-remove
 * highlight-launchers-on-mouseover
 */

typedef struct _GlobalConfig GlobalConfig;
struct _GlobalConfig {
	gboolean           tooltips_enabled;
	gboolean           keep_menus_in_memory;
	gboolean           enable_animations;

	int                minimized_size;
	int                show_delay;
	PanelSpeed         animation_speed;
	int                hide_delay;

	gboolean           keys_enabled;
	GlobalConfigKey    menu_key;
	GlobalConfigKey    run_key;
	GlobalConfigKey    screenshot_key;
	GlobalConfigKey    window_screenshot_key;

	gboolean           autoraise;
	PanelLayer         layer;

	gboolean           drawer_auto_close;
	gboolean           confirm_panel_remove;
	gboolean           highlight_when_over;

	int                menu_flags;
	gboolean           menu_check;
};

void panel_global_config_set_entry (GConfEntry  *entry);

void panel_global_config_notify    (GConfClient *client,
				    guint        cnxn_id,
				    GConfEntry  *entry,
				    gpointer     user_data);

G_END_DECLS

#endif /* PANEL_CONFIG_GLOBAL_H */
