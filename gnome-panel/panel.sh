#!/bin/sh
#this is a temporary script to launch the launcher and the panel

#this will kill the launcher from before if it is still running
#it shouldn't have to be done, instead we should only launch one
#launcher_applet and that should be persistent though more then
#one panel sessions. that works but I've had it not work a few
#times so this works 100% even though it's not very "clean"

#now started in panel binary
#kill `ps x|awk '/[l]auncher_applet/ {print $1}'`
#launcher_applet &

panel
