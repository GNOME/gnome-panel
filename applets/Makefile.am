always_built_SUBDIRS = applet-dirs gen_util batmon cpuload cpumemusage	\
	 fish bussign netload dialer webcontrol diskusage icewm-pager	\
	 drivemount clockmail fifteen battery

if APPLET_CDPLAYER
cdplayer = cdplayer
endif

if APPLET_MIXER
mixer = mixer
endif

if APPLET_MODEMLIGHTS
modemlights = modemlights
endif

SUBDIRS = $(always_built_SUBDIRS) $(cdplayer) $(mixer) $(modemlights)

#sometimes_built_SUBDIRS = cdplayer mixer modemlights
#DIST_SUBDIRS = $(always_built_SUBDIRS) $(sometimes_built_SUBDIRS)
