#ifndef __PANEL_SCHEMAS_H__
#define __PANEL_SCHEMAS_H__

#define PANEL_GENERAL_SCHEMA                    "org.gnome.gnome-panel.general"
#define PANEL_GENERAL_CONFIRM_PANEL_REMOVAL_KEY "confirm-panel-removal"
#define PANEL_GENERAL_ENABLE_TOOLTIPS_KEY       "enable-tooltips"
#define PANEL_GENERAL_THEME_VARIANT_KEY         "theme-variant"

#define PANEL_LOCKDOWN_SCHEMA                 "org.gnome.gnome-panel.lockdown"
#define PANEL_LOCKDOWN_COMPLETE_LOCKDOWN_KEY  "locked-down"
#define PANEL_LOCKDOWN_DISABLE_FORCE_QUIT_KEY "disable-force-quit"
#define PANEL_LOCKDOWN_DISABLED_APPLETS_KEY   "disabled-applets"

#define PANEL_DESKTOP_LOCKDOWN_SCHEMA          "org.gnome.desktop.lockdown"
#define PANEL_DESKTOP_DISABLE_COMMAND_LINE_KEY "disable-command-line"
#define PANEL_DESKTOP_DISABLE_LOCK_SCREEN_KEY  "disable-lock-screen"
#define PANEL_DESKTOP_DISABLE_LOG_OUT_KEY      "disable-log-out"
#define PANEL_DESKTOP_DISABLE_SWITCH_USER_KEY  "disable-user-switching"

#define PANEL_LAYOUT_SCHEMA               "org.gnome.gnome-panel.layout"
#define PANEL_LAYOUT_TOPLEVEL_ID_LIST_KEY "toplevel-id-list"
#define PANEL_LAYOUT_OBJECT_ID_LIST_KEY   "object-id-list"

#define PANEL_LAYOUT_TOPLEVEL_PATH           "/org/gnome/gnome-panel/layout/toplevels/"
#define PANEL_LAYOUT_OBJECT_PATH             "/org/gnome/gnome-panel/layout/objects/"
#define PANEL_LAYOUT_OBJECT_CONFIG_SUFFIX    "instance-config/"

#define PANEL_TOPLEVEL_SCHEMA               "org.gnome.gnome-panel.toplevel"
#define PANEL_TOPLEVEL_NAME_KEY             "name"
#define PANEL_TOPLEVEL_MONITOR_KEY          "monitor"
#define PANEL_TOPLEVEL_EXPAND_KEY           "expand"
#define PANEL_TOPLEVEL_ORIENTATION_KEY      "orientation"
#define PANEL_TOPLEVEL_ALIGNMENT_KEY        "alignment"
#define PANEL_TOPLEVEL_SIZE_KEY             "size"
#define PANEL_TOPLEVEL_AUTO_HIDE_KEY        "auto-hide"
#define PANEL_TOPLEVEL_ENABLE_BUTTONS_KEY   "enable-buttons"
#define PANEL_TOPLEVEL_ENABLE_ARROWS_KEY    "enable-arrows"
#define PANEL_TOPLEVEL_HIDE_DELAY_KEY       "hide-delay"
#define PANEL_TOPLEVEL_UNHIDE_DELAY_KEY     "unhide-delay"
#define PANEL_TOPLEVEL_AUTO_HIDE_SIZE_KEY   "auto-hide-size"
#define PANEL_TOPLEVEL_ANIMATION_SPEED_KEY  "animation-speed"

#define PANEL_OBJECT_SCHEMA             "org.gnome.gnome-panel.object"
#define PANEL_OBJECT_IID_KEY            "object-iid"
#define PANEL_OBJECT_MODULE_ID_KEY      "module-id"
#define PANEL_OBJECT_APPLET_ID_KEY      "applet-id"
#define PANEL_OBJECT_TOPLEVEL_ID_KEY    "toplevel-id"
#define PANEL_OBJECT_PACK_TYPE_KEY      "pack-type"
#define PANEL_OBJECT_PACK_INDEX_KEY     "pack-index"

#define PANEL_MENU_BUTTON_SCHEMA          "org.gnome.gnome-panel.menu-button"
#define PANEL_MENU_BUTTON_TOOLTIP_KEY     "tooltip"
#define PANEL_MENU_BUTTON_CUSTOM_ICON_KEY "custom-icon"
#define PANEL_MENU_BUTTON_MENU_PATH_KEY   "menu-path"

#define GNOME_DESKTOP_WM_KEYBINDINGS_SCHEMA                    "org.gnome.desktop.wm.keybindings"
#define GNOME_DESKTOP_WM_KEYBINDINGS_ACTIVATE_WINDOW_MENU_KEY  "activate-window-menu"
#define GNOME_DESKTOP_WM_KEYBINDINGS_TOGGLE_MAXIMIZED_KEY      "toggle-maximized"
#define GNOME_DESKTOP_WM_KEYBINDINGS_MAXIMIZE_KEY              "maximize"
#define GNOME_DESKTOP_WM_KEYBINDINGS_UNMAXIMIZE_KEY            "unmaximize"
#define GNOME_DESKTOP_WM_KEYBINDINGS_BEGIN_MOVE_KEY            "begin-move"
#define GNOME_DESKTOP_WM_KEYBINDINGS_BEGIN_RESIZE_KEY          "begin-resize"

#define GNOME_DESKTOP_WM_PREFERENCES_SCHEMA                    "org.gnome.desktop.wm.preferences"
#define GNOME_DESKTOP_WM_PREFERENCES_MOUSE_BUTTON_MODIFIER_KEY "mouse-button-modifier"

#endif /* __PANEL_SCHEMAS_H__ */
