always_built_SUBDIRS = applet-dirs gen_util batmon asclock \
	 fish netload dialer webcontrol icewm-pager \
	 drivemount clockmail fifteen battery

if HAVE_LIBGTOP
libgtop_SUBDIRS = cpuload cpumemusage diskusage multiload
else
libgtop_SUBDIRS =
endif

if APPLET_CDPLAYER
cdplayer = cdplayer
endif

if APPLET_MIXER
mixer = mixer
endif

if APPLET_MODEMLIGHTS
modemlights = modemlights
endif

if FALSE
# not yet ready for primetime?
winlist = winlist
endif

SUBDIRS = $(always_built_SUBDIRS) $(libgtop_SUBDIRS) \
	 $(cdplayer) $(mixer) $(modemlights) $(winlist)

#sometimes_built_SUBDIRS = cdplayer mixer modemlights winlist
#DIST_SUBDIRS = $(always_built_SUBDIRS) $(sometimes_built_SUBDIRS)
