# trick Automake into defining $(CXXLINK)
if FALSE
BUILT_SOURCES = dummy.cc

dummy.cc:
	@touch dummy.cc
endif

INCLUDES = -I. -I$(srcdir) 				\
	-I$(srcdir)/../../panel				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR)

bin_PROGRAMS = fish_applet

fish_applet_SOURCES = fish.c

fish_applet_LDADD = ../../panel/libpanel_applet.la \
		     $(GNOME_LIBDIR) $(MICO_LIBS) \
		     $(GNOMEUI_LIBS) $(INTLLIBS) 

fish_applet_LINK = $(CXXLINK)

EXTRA_DIST = \
	fish_applet.desktop	\
	fish1.xpm		\
	fish2.xpm		\
	fish3.xpm

sysdir = $(datadir)/applets/Amusements
sys_DATA = fish_applet.desktop

