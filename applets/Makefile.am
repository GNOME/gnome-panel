always_built_SUBDIRS = applet-dirs gen_util batmon cpuload cpumemusage	\
	 fish bussign netload dialer webcontrol diskusage icewm-pager	\
	 drivemount clockmail fifteen battery

sometimes_built_SUBDIRS = cdplayer mixer modemlights

SUBDIRS = $(always_built_SUBDIRS)				\
	 @PROGRAMS_PANEL_CDPLAYER@ @PROGRAMS_PANEL_MIXER@	\
	 @PROGRAMS_PANEL_MODEMLIGHTS@

DIST_SUBDIRS = $(always_built_SUBDIRS) $(sometimes_built_SUBDIRS)
