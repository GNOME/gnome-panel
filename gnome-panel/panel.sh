#!/bin/sh
#this is a temporary script to launch the launcher and the panel

#this will kill the launcher from before if it is still running
#it's a little dirty, but it works
kill `ps x|awk '/[l]auncher_applet/ {print $1}'`

#now started in panel binary
#launcher_applet &

panel
