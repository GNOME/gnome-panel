always_built_SUBDIRS = applet-dirs gen_util batmon asclock \
	 fish dialer webcontrol icewm-pager \
	 drivemount clockmail fifteen battery charpick \
	 gnome-pager mini-commander

if HAVE_LIBESD
esdmanager = esd-manager
else
esdmanager = 
endif

if HAVE_LIBGTOP
libgtop_SUBDIRS = cpumemusage diskusage multiload netload
else
libgtop_SUBDIRS =
endif

if HAVE_LIBGHTTP
bussign = bussign
gticker = gticker
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

if APPLET_FVWM_PAGER
fvwm_pager = fvwm-pager
endif

if FALSE
# not yet ready for primetime?
winlist = winlist
endif

SUBDIRS = $(always_built_SUBDIRS) $(libgtop_SUBDIRS) \
	 $(cdplayer) $(mixer) $(modemlights) $(winlist) \
         $(slashapp) $(fvwm_pager) $(esdmanager) $(gticker) $(bussign)

