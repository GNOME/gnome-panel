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

# "top_panel" toplevel
SCHEMADIR=/schemas$BASE/toplevels
BASEDIR=$BASE/default_setup/toplevels/top_panel
gconf_set $SCHEMADIR $BASEDIR name string "Top Panel"
gconf_set $SCHEMADIR $BASEDIR expand bool true
gconf_set $SCHEMADIR $BASEDIR orientation string top
gconf_set $SCHEMADIR $BASEDIR size int 24

# "top_panel" toplevel
SCHEMADIR=/schemas$BASE/toplevels
BASEDIR=$BASE/default_setup/toplevels/bottom_panel
gconf_set $SCHEMADIR $BASEDIR name string "Bottom Panel"
gconf_set $SCHEMADIR $BASEDIR expand bool true
gconf_set $SCHEMADIR $BASEDIR orientation string bottom
gconf_set $SCHEMADIR $BASEDIR size int 24
