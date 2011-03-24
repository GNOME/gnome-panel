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

#define PANEL_RUN_SCHEMA                 "org.gnome.gnome-panel.run-dialog"
#define PANEL_RUN_HISTORY_KEY            "history"
#define PANEL_RUN_ENABLE_COMPLETION_KEY  "enable-autocompletion"
#define PANEL_RUN_ENABLE_LIST_KEY        "enable-program-list"
#define PANEL_RUN_SHOW_LIST_KEY          "show-program-list"

#define PANEL_LAYOUT_SCHEMA               "org.gnome.gnome-panel.layout"
#define PANEL_LAYOUT_TOPLEVEL_ID_LIST     "toplevel-id-list"
#define PANEL_LAYOUT_OBJECT_ID_LIST       "object-id-list"

#define PANEL_LAYOUT_TOPLEVEL_PATH           "/org/gnome/gnome-panel/layout/toplevels"
#define PANEL_LAYOUT_TOPLEVEL_DEFAULT_PREFIX "toplevel"
#define PANEL_LAYOUT_OBJECT_PATH             "/org/gnome/gnome-panel/layout/objects"
#define PANEL_LAYOUT_OBJECT_DEFAULT_PREFIX   "object"
#define PANEL_LAYOUT_OBJECT_CONFIG_SUFFIX    "instance-config"

#define PANEL_TOPLEVEL_SCHEMA           "org.gnome.gnome-panel.toplevel"
#define PANEL_TOPLEVEL_NAME             "name"
#define PANEL_TOPLEVEL_SCREEN           "screen"
#define PANEL_TOPLEVEL_MONITOR          "monitor"
#define PANEL_TOPLEVEL_EXPAND           "expand"
#define PANEL_TOPLEVEL_ORIENTATION      "orientation"
#define PANEL_TOPLEVEL_SIZE             "size"
#define PANEL_TOPLEVEL_X                "x"
#define PANEL_TOPLEVEL_Y                "y"
#define PANEL_TOPLEVEL_X_RIGHT          "x-right"
#define PANEL_TOPLEVEL_Y_BOTTOM         "y-bottom"
#define PANEL_TOPLEVEL_X_CENTERED       "x-centered"
#define PANEL_TOPLEVEL_Y_CENTERED       "y-centered"
#define PANEL_TOPLEVEL_AUTO_HIDE        "auto-hide"
#define PANEL_TOPLEVEL_ENABLE_BUTTONS   "enable-buttons"
#define PANEL_TOPLEVEL_ENABLE_ARROWS    "enable-arrows"
#define PANEL_TOPLEVEL_HIDE_DELAY       "hide-delay"
#define PANEL_TOPLEVEL_UNHIDE_DELAY     "unhide-delay"
#define PANEL_TOPLEVEL_AUTO_HIDE_SIZE   "auto-hide-size"
#define PANEL_TOPLEVEL_ANIMATION_SPEED  "animation-speed"

#define PANEL_OBJECT_SCHEMA             "org.gnome.gnome-panel.object"
#define PANEL_OBJECT_IID_KEY            "object-iid"
#define PANEL_OBJECT_TOPLEVEL_ID_KEY    "toplevel-id"
#define PANEL_OBJECT_POSITION_KEY       "position"
#define PANEL_OBJECT_PACK_END_KEY       "pack-end"

#endif /* __PANEL_SCHEMAS_H__ */
