always_built_SUBDIRS = applet-dirs gen_util asclock \
	 fish dialer webcontrol \
	 drivemount clockmail fifteen battery charpick \
	 gnome-pager mini-commander gkb jbc

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
libgtop_SUBDIRS = cpumemusage diskusage multiload netload
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

