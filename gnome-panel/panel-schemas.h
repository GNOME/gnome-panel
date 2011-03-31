#ifndef __PANEL_SCHEMAS_H__
#define __PANEL_SCHEMAS_H__

#define PANEL_GENERAL_SCHEMA                    "org.gnome.gnome-panel.general"
#define PANEL_GENERAL_CONFIRM_PANEL_REMOVAL_KEY "confirm-panel-removal"
#define PANEL_GENERAL_ENABLE_TOOLTIPS_KEY       "enable-tooltips"

#define PANEL_LOCKDOWN_SCHEMA                 "org.gnome.gnome-panel.lockdown"
#define PANEL_LOCKDOWN_COMPLETE_LOCKDOWN_KEY  "locked-down"
#define PANEL_LOCKDOWN_DISABLE_FORCE_QUIT_KEY "disable-force-quit"
#define PANEL_LOCKDOWN_DISABLED_APPLETS_KEY   "disabled-applets"

#define PANEL_DESKTOP_LOCKDOWN_SCHEMA          "org.gnome.desktop.lockdown"
#define PANEL_DESKTOP_DISABLE_COMMAND_LINE_KEY "disable-command-line"
#define PANEL_DESKTOP_DISABLE_LOCK_SCREEN_KEY  "disable-lock-screen"
#define PANEL_DESKTOP_DISABLE_LOG_OUT_KEY      "disable-log-out"
#define PANEL_DESKTOP_DISABLE_SWITCH_USER_KEY  "disable-user-switching"

#define PANEL_RUN_SCHEMA                 "org.gnome.gnome-panel.run-dialog"
#define PANEL_RUN_HISTORY_KEY            "history"
#define PANEL_RUN_ENABLE_COMPLETION_KEY  "enable-autocompletion"
#define PANEL_RUN_ENABLE_LIST_KEY        "enable-program-list"
#define PANEL_RUN_SHOW_LIST_KEY          "show-program-list"

#define PANEL_LAYOUT_SCHEMA               "org.gnome.gnome-panel.layout"
#define PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY "toplevel-id-list"
#define PANEL_LAYOUT_OBJECT_ID_LIST_KEY   "object-id-list"

#define PANEL_LAYOUT_TOPLEVEL_PATH           "/org/gnome/gnome-panel/layout/toplevels/"
#define PANEL_LAYOUT_TOPLEVEL_DEFAULT_PREFIX "toplevel"
#define PANEL_LAYOUT_OBJECT_PATH             "/org/gnome/gnome-panel/layout/objects/"
#define PANEL_LAYOUT_OBJECT_DEFAULT_PREFIX   "object"
#define PANEL_LAYOUT_OBJECT_CONFIG_SUFFIX    "instance-config/"

#define PANEL_TOPLEVEL_SCHEMA               "org.gnome.gnome-panel.toplevel"
#define PANEL_TOPLEVEL_NAME_KEY             "name"
#define PANEL_TOPLEVEL_SCREEN_KEY           "screen"
#define PANEL_TOPLEVEL_MONITOR_KEY          "monitor"
#define PANEL_TOPLEVEL_EXPAND_KEY           "expand"
#define PANEL_TOPLEVEL_ORIENTATION_KEY      "orientation"
#define PANEL_TOPLEVEL_SIZE_KEY             "size"
#define PANEL_TOPLEVEL_X_KEY                "x"
#define PANEL_TOPLEVEL_Y_KEY                "y"
#define PANEL_TOPLEVEL_X_RIGHT_KEY          "x-right"
#define PANEL_TOPLEVEL_Y_BOTTOM_KEY         "y-bottom"
#define PANEL_TOPLEVEL_X_CENTERED_KEY       "x-centered"
#define PANEL_TOPLEVEL_Y_CENTERED_KEY       "y-centered"
#define PANEL_TOPLEVEL_AUTO_HIDE_KEY        "auto-hide"
#define PANEL_TOPLEVEL_ENABLE_BUTTONS_KEY   "enable-buttons"
#define PANEL_TOPLEVEL_ENABLE_ARROWS_KEY    "enable-arrows"
#define PANEL_TOPLEVEL_HIDE_DELAY_KEY       "hide-delay"
#define PANEL_TOPLEVEL_UNHIDE_DELAY_KEY     "unhide-delay"
#define PANEL_TOPLEVEL_AUTO_HIDE_SIZE_KEY   "auto-hide-size"
#define PANEL_TOPLEVEL_ANIMATION_SPEED_KEY  "animation-speed"

#define PANEL_BACKGROUND_SCHEMA_CHILD     "background"
#define PANEL_BACKGROUND_TYPE_KEY         "type"
#define PANEL_BACKGROUND_COLOR_KEY        "color"
#define PANEL_BACKGROUND_IMAGE_URI_KEY    "image-uri"
#define PANEL_BACKGROUND_IMAGE_STYLE_KEY  "image-style"
#define PANEL_BACKGROUND_IMAGE_ROTATE_KEY "image-rotate"
#define PANEL_BACKGROUND_COLOR_DEFAULT    "rgba(255,255,255,.2)"

#define PANEL_OBJECT_SCHEMA             "org.gnome.gnome-panel.object"
#define PANEL_OBJECT_IID_KEY            "object-iid"
#define PANEL_OBJECT_TOPLEVEL_ID_KEY    "toplevel-id"
#define PANEL_OBJECT_PACK_TYPE_KEY      "pack-type"
#define PANEL_OBJECT_PACK_INDEX_KEY     "pack-index"

#define PANEL_LAUNCHER_SCHEMA      "org.gnome.gnome-panel.launcher"
#define PANEL_LOCATION_KEY         "location"

#define PANEL_MENU_BUTTON_SCHEMA          "org.gnome.gnome-panel.menu-button"
#define PANEL_MENU_BUTTON_TOOLTIP_KEY     "tooltip"
#define PANEL_MENU_BUTTON_CUSTOM_ICON_KEY "custom-icon"
#define PANEL_MENU_BUTTON_MENU_PATH_KEY   "menu-path"

#endif /* __PANEL_SCHEMAS_H__ */
