#include <config.h>
#include <string.h>

#include <gconf/gconf.h>

#include "panel.h"
#include "panel-util.h"
#include "panel-gconf.h"

#undef PANEL_GLOBAL_CONFIG_DEBUG

extern GlobalConfig global_config;

static GConfEnumStringPair panel_speed_map [] = {
        { PANEL_SPEED_SLOW,   "panel-speed-slow" },
        { PANEL_SPEED_MEDIUM, "panel-speed-medium" },
        { PANEL_SPEED_FAST,   "panel-speed-fast" },
};

/*
 * Map between a gconf entry and a member of the
 * GlobalConfig structure.
 *
 * Note: keep in sync with panel-config-global.h and
 * panel-global-config.schemas
 */
void
panel_global_config_set_entry (GConfEntry *entry)
{
	GConfValue *value;
	gchar      *key;

	g_return_if_fail (entry != NULL);

	value = gconf_entry_get_value (entry);
	key   = g_path_get_basename (gconf_entry_get_key (entry));

	if (!value || !key)
		return;

	if (strcmp (key, "tooltips_enabled") == 0)
		global_config.tooltips_enabled =
				gconf_value_get_bool (value);

	else if (strcmp (key, "keep_menus_in_memory") == 0)
		global_config.keep_menus_in_memory =
				gconf_value_get_bool (value);

	else if (strcmp (key, "enable_animations") == 0)
		global_config.enable_animations =
				gconf_value_get_bool (value);

	else if (strcmp (key, "panel_minimized_size") == 0)
		global_config.minimized_size =
				gconf_value_get_int (value);

	else if (strcmp (key, "panel_show_delay") == 0)
		global_config.show_delay =
				gconf_value_get_int (value);

	else if (strcmp (key, "panel_animation_speed") == 0) {
		PanelSpeed speed = PANEL_SPEED_SLOW;

		if (gconf_string_to_enum (
			panel_speed_map,
			gconf_value_get_string (value),
			(int *) &speed))
			global_config.animation_speed = speed;

	} else if (strcmp (key, "panel_hide_delay") == 0)
		global_config.hide_delay =
				gconf_value_get_int (value);

	else if (strcmp (key, "enable_key_bindings") == 0)
		global_config.keys_enabled =
				gconf_value_get_bool (value);

	else if (strcmp (key, "menu_key") == 0) { 
		if (global_config.menu_key.str)
			g_free (global_config.menu_key.str);

		global_config.menu_key.str =
			g_strdup (gconf_value_get_string (value));

		panel_parse_accelerator (&global_config.menu_key);

	} else if (strcmp (key, "run_key") == 0) {
		if (global_config.run_key.str)
			g_free (global_config.run_key.str);

		global_config.run_key.str =
			g_strdup (gconf_value_get_string (value));

		panel_parse_accelerator (&global_config.run_key);

	} else if (strcmp (key, "screenshot_key") == 0) {
		if (global_config.screenshot_key.str)
			g_free (global_config.screenshot_key.str);

		global_config.screenshot_key.str =
			 g_strdup (gconf_value_get_string (value));

		panel_parse_accelerator (&global_config.screenshot_key);

	} else if (strcmp (key, "window_screenshot_key") == 0) {
		if (global_config.window_screenshot_key.str)
			g_free (global_config.window_screenshot_key.str);

		global_config.window_screenshot_key.str =
			 g_strdup (gconf_value_get_string (value));

		panel_parse_accelerator (&global_config.window_screenshot_key);

	} else if (strcmp (key, "drawer_autoclose") == 0)
		global_config.drawer_auto_close =
			gconf_value_get_bool (value);

	else if (strcmp (key, "confirm_panel_remove") == 0)
		global_config.confirm_panel_remove =
			gconf_value_get_bool (value);

	else if (strcmp (key, "highlight_launchers_on_mouseover") == 0)
		global_config.highlight_when_over =
			gconf_value_get_bool (value);

	else
		g_warning ("%s not handled", key);

	g_free (key);
}

void
panel_global_config_notify (GConfClient *client,
			    guint        cnxn_id,
			    GConfEntry  *entry,
			    gpointer     user_data)
{
        panel_global_config_set_entry (entry);

	/*
	 * FIXME: we should handle config changes
	 *        per config item.
	 */
	panel_apply_global_config ();
}
