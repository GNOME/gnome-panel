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
 *   (+) indicates its in the capplet
 *
 * tooltips_enabled
 * keep_menus_in_memory
 * enable_animations (+)
 *
 * panel_minimized_size
 * panel_show_delay
 * panel_animation_speed (+)
 * panel_hide_delay
 *
 * enable_key_bindings
 * menu_key
 * menu_key
 * run_key
 * window_screenshot_key
 *
 * drawer_autoclose (+)
 * confirm_panel_remove
 * highlight_launchers_on_mouseover
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

	gboolean           drawer_auto_close;
	gboolean           confirm_panel_remove;
	gboolean           highlight_when_over;
};

void panel_global_config_set_entry (GConfEntry  *entry);

void panel_global_config_notify    (GConfClient *client,
				    guint        cnxn_id,
				    GConfEntry  *entry,
				    gpointer     user_data);

G_END_DECLS

#endif /* PANEL_CONFIG_GLOBAL_H */
