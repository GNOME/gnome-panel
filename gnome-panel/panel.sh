#!/bin/sh
#this is a temporary script to launch the launcher and the panel

#this will kill the launcher from before if it is still running
kill `ps x|awk '/[l]auncher_applet/ {print $1}'`

launcher_applet &
panel
