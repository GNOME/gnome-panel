#include <config.h>
#include <string.h>

#include <gconf/gconf.h>

#include "panel-util.h"

#undef PANEL_GLOBAL_CONFIG_DEBUG

extern GlobalConfig global_config;

static GConfEnumStringPair panel_speed_map [] = {
        { PANEL_SPEED_MEDIUM, "panel-speed-medium" },
        { PANEL_SPEED_SLOW,   "panel-speed-slow" },
        { PANEL_SPEED_FAST,   "panel-speed-fast" },
};

static GConfEnumStringPair panel_layer_map [] = {
        { LAYER_NORMAL, "panel-normal-layer" },
        { LAYER_BELOW, "panel-below-layer" },
        { LAYER_ABOVE, "panel-above-layer" },
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

	value = gconf_entry_get_value (entry);

	key = g_path_get_basename (gconf_entry_get_key (entry));

#ifdef PANEL_GLOBAL_CONFIG_DEBUG
	fprintf (stderr, "global config: handling %s\n", key);
#endif

	if (!strcmp (key, "tooltips-enabled"))
		global_config.tooltips_enabled =
				gconf_value_get_bool (value);

	else if (!strcmp (key, "keep-menus-in-memory"))
		global_config.keep_menus_in_memory =
				gconf_value_get_bool (value);

	else if (!strcmp (key, "enable-animations"))
		global_config.enable_animations =
				gconf_value_get_bool (value);

	else if (!strcmp (key, "panel-minimized-size"))
		global_config.minimized_size =
				gconf_value_get_int (value);

	else if (!strcmp (key, "panel-show-delay"))
		global_config.show_delay =
				gconf_value_get_int (value);

	else if (!strcmp (key, "panel-animation-speed"))
		gconf_string_to_enum (
			panel_speed_map,
			gconf_value_get_string (value),
			(gint *) &global_config.animation_speed);

	else if (!strcmp (key, "panel-hide-delay"))
		global_config.hide_delay =
				gconf_value_get_int (value);

	else if (!strcmp (key, "enable-key-bindings"))
		global_config.keys_enabled =
				gconf_value_get_bool (value);

	else if (!strcmp (key, "menu-key")) { 
		if (global_config.menu_key.str)
			g_free (global_config.menu_key.str);

		global_config.menu_key.str =
			g_strdup (gconf_value_get_string (value));

		panel_parse_accelerator (&global_config.menu_key);

	} else if (!strcmp (key, "run-key")) {
		if (global_config.run_key.str)
			g_free (global_config.run_key.str);

		global_config.run_key.str =
			g_strdup (gconf_value_get_string (value));

		panel_parse_accelerator (&global_config.run_key);

	} else if (!strcmp (key, "screenshot-key")) {
		if (global_config.screenshot_key.str)
			g_free (global_config.screenshot_key.str);

		global_config.screenshot_key.str =
			 g_strdup (gconf_value_get_string (value));

		panel_parse_accelerator (&global_config.screenshot_key);

	} else if (!strcmp (key, "window-screenshot-key")) {
		if (global_config.window_screenshot_key.str)
			g_free (global_config.window_screenshot_key.str);

		global_config.window_screenshot_key.str =
			 g_strdup (gconf_value_get_string (value));

		panel_parse_accelerator (&global_config.window_screenshot_key);

	} else if (!strcmp (key, "autoraise-panel"))
		global_config.autoraise =
			gconf_value_get_bool (value);

	else if (!strcmp (key, "panel-window-layer"))
		gconf_string_to_enum (
			panel_layer_map,
  			gconf_value_get_string (value),
			(gint *) &global_config.layer);

	else if (!strcmp (key, "drawer-autoclose"))
		global_config.drawer_auto_close =
			gconf_value_get_bool (value);

	else if (!strcmp (key, "confirm-panel-remove"))
		global_config.confirm_panel_remove =
			gconf_value_get_bool (value);

	else if (!strcmp (key, "highlight-launchers-on-mouseover"))
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
