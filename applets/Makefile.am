always_built_SUBDIRS = applet-dirs gen_util asclock \
	 fish webcontrol \
	 drivemount clockmail fifteen life battery charpick \
	 gnome-pager mini-commander gkb jbc another_clock

if FALSE
if HAVE_LIBESD
esdmanager = esd-manager
else
esdmanager = 
endif
endif

if FALSE
if HAVE_FVWM
fvwm = fvwm-pager
endif
endif

if HAVE_LIBGTOP
if FALSE
libgtop_SUBDIRS = cpumemusage diskusage multiload
else
libgtop_SUBDIRS = cpumemusage diskusage multiload
endif
else
libgtop_SUBDIRS =
endif

if HAVE_LIBGHTTP
slashapp = slashapp
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
winlist = winlist
endif

SUBDIRS = $(always_built_SUBDIRS) $(libgtop_SUBDIRS) \
	 $(cdplayer) $(mixer) $(modemlights) $(winlist) \
         $(slashapp) $(esdmanager) $(fvwm)

DIST_SUBDIRS = $(always_built_SUBDIRS) esd-manager cpumemusage \
	diskusage multiload netload slashapp cdplayer \
	 mixer modemlights winlist fvwm-pager

