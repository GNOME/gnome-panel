#!/bin/sh

if [ -z $GCONFTOOL ]; then
  echo "You must set \$GCONFTOOL before running this script"
  exit 1
fi

if [ -z $GCONF_CONFIG_SOURCE ]; then
  echo "You must set \$GCONF_CONFIG_SOURCE before running this script"
  exit 1
fi

GCONFTOOL="$GCONFTOOL --direct --config-source $GCONF_CONFIG_SOURCE"

gconf_set()
{
  schemadir=$1
  basedir=$2
  key=$3
  type=$4
  value=$5

  echo "Setting default for $basedir/$key to $value"

  $GCONFTOOL --apply-schema $schemadir/$key $basedir/$key
  $GCONFTOOL -s $basedir/$key -t $type "$value"
}

gconf_set_list()
{
  schemadir=$1
  basedir=$2
  key=$3
  type=$4
  value=$5

  echo "Setting default for $basedir/$key to $value"

  $GCONFTOOL --apply-schema $schemadir/$key $basedir/$key
  $GCONFTOOL -s $basedir/$key -t list --list-type $type "$value"
}

BASE=/apps/new_panel

SCHEMADIR=/schemas$BASE
BASEDIR=$BASE/default_setup
gconf_set_list $SCHEMADIR $BASEDIR general/toplevel_id_list string "[top_panel,bottom_panel]"

# Top Panel
SCHEMADIR=/schemas$BASE/toplevels
BASEDIR=$BASE/default_setup/toplevels/top_panel
gconf_set $SCHEMADIR $BASEDIR name string "Top Panel"
gconf_set $SCHEMADIR $BASEDIR expand bool true
gconf_set $SCHEMADIR $BASEDIR orientation string top
gconf_set $SCHEMADIR $BASEDIR size int 24

# Bottom Panel
BASEDIR=$BASE/default_setup/toplevels/bottom_panel
gconf_set $SCHEMADIR $BASEDIR name string "Bottom Panel"
gconf_set $SCHEMADIR $BASEDIR expand bool true
gconf_set $SCHEMADIR $BASEDIR orientation string bottom
gconf_set $SCHEMADIR $BASEDIR size int 24


SCHEMADIR=/schemas$BASE
BASEDIR=$BASE/default_setup
gconf_set_list $SCHEMADIR $BASEDIR general/object_id_list string \
               "[menu_bar,nautilus_launcher,terminal_launcher]"
gconf_set_list $SCHEMADIR $BASEDIR general/applet_id_list string \
               "[window_menu,mixer,clock,show_desktop_button,window_list,workspace_switcher]"

# Menu Bar on the Top Panel
SCHEMADIR=/schemas$BASE/objects
BASEDIR=$BASE/default_setup/objects/menu_bar
gconf_set $SCHEMADIR $BASEDIR object_type string "menu-bar"
gconf_set $SCHEMADIR $BASEDIR toplevel_id string "top_panel"
gconf_set $SCHEMADIR $BASEDIR position int 0
gconf_set $SCHEMADIR $BASEDIR panel_right_stick bool false

# Nautilus launcher on the Top Panel
BASEDIR=$BASE/default_setup/objects/nautilus_launcher
gconf_set $SCHEMADIR $BASEDIR object_type string "launcher-object"
gconf_set $SCHEMADIR $BASEDIR toplevel_id string "top_panel"
gconf_set $SCHEMADIR $BASEDIR position int 1
gconf_set $SCHEMADIR $BASEDIR panel_right_stick bool false
gconf_set $SCHEMADIR $BASEDIR launcher_location string "applications:///nautilus.desktop"

# Nautilus launcher on the Top Panel
BASEDIR=$BASE/default_setup/objects/terminal_launcher
gconf_set $SCHEMADIR $BASEDIR object_type string "launcher-object"
gconf_set $SCHEMADIR $BASEDIR toplevel_id string "top_panel"
gconf_set $SCHEMADIR $BASEDIR position int 2
gconf_set $SCHEMADIR $BASEDIR panel_right_stick bool false
gconf_set $SCHEMADIR $BASEDIR launcher_location string "applications:///System/gnome-terminal.desktop"

# Window Menu Applet on the Top Panel
SCHEMADIR=/schemas$BASE/objects
BASEDIR=$BASE/default_setup/applets/window_menu
gconf_set $SCHEMADIR $BASEDIR object_type string "bonobo-applet"
gconf_set $SCHEMADIR $BASEDIR toplevel_id string "top_panel"
gconf_set $SCHEMADIR $BASEDIR position int 0
gconf_set $SCHEMADIR $BASEDIR panel_right_stick bool true
gconf_set $SCHEMADIR $BASEDIR bonobo_iid string "OAFIID:GNOME_WindowMenuApplet"

# Mixer Applet on the Top Panel
BASEDIR=$BASE/default_setup/applets/mixer
gconf_set $SCHEMADIR $BASEDIR object_type string "bonobo-applet"
gconf_set $SCHEMADIR $BASEDIR toplevel_id string "top_panel"
gconf_set $SCHEMADIR $BASEDIR position int 1
gconf_set $SCHEMADIR $BASEDIR panel_right_stick bool true
gconf_set $SCHEMADIR $BASEDIR bonobo_iid string "OAFIID:GNOME_MixerApplet"

# Clock Applet on the Top Panel
BASEDIR=$BASE/default_setup/applets/clock
gconf_set $SCHEMADIR $BASEDIR object_type string "bonobo-applet"
gconf_set $SCHEMADIR $BASEDIR toplevel_id string "top_panel"
gconf_set $SCHEMADIR $BASEDIR position int 2
gconf_set $SCHEMADIR $BASEDIR panel_right_stick bool true
gconf_set $SCHEMADIR $BASEDIR bonobo_iid string "OAFIID:GNOME_ClockApplet"

# Show Desktop Button on the Bottom Panel
BASEDIR=$BASE/default_setup/applets/show_desktop_button
gconf_set $SCHEMADIR $BASEDIR object_type string "bonobo-applet"
gconf_set $SCHEMADIR $BASEDIR toplevel_id string "bottom_panel"
gconf_set $SCHEMADIR $BASEDIR position int 0
gconf_set $SCHEMADIR $BASEDIR panel_right_stick bool false
gconf_set $SCHEMADIR $BASEDIR bonobo_iid string "OAFIID:GNOME_ShowDesktopApplet"

# Window List on the Bottom Panel
BASEDIR=$BASE/default_setup/applets/window_list
gconf_set $SCHEMADIR $BASEDIR object_type string "bonobo-applet"
gconf_set $SCHEMADIR $BASEDIR toplevel_id string "bottom_panel"
gconf_set $SCHEMADIR $BASEDIR position int 1
gconf_set $SCHEMADIR $BASEDIR panel_right_stick bool false
gconf_set $SCHEMADIR $BASEDIR bonobo_iid string "OAFIID:GNOME_WindowListApplet"

# Workspace Switcher on the Bottom Panel
BASEDIR=$BASE/default_setup/applets/workspace_switcher
gconf_set $SCHEMADIR $BASEDIR object_type string "bonobo-applet"
gconf_set $SCHEMADIR $BASEDIR toplevel_id string "bottom_panel"
gconf_set $SCHEMADIR $BASEDIR position int 0
gconf_set $SCHEMADIR $BASEDIR panel_right_stick bool true
gconf_set $SCHEMADIR $BASEDIR bonobo_iid string "OAFIID:GNOME_WorkspaceSwitcherApplet"
