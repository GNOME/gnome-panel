INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../panel				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR)

bin_PROGRAMS = fish_applet

fish_applet_SOURCES = fish.c

fish_applet_LDADD = ../../panel/libpanel_applet.la \
		     $(GNOME_LIBDIR) $(ORB_LIBS) \
		     $(GNOMEUI_LIBS) $(INTLLIBS) 

FISH_BITMAPS = fishanim.png

EXTRA_DIST = \
	fish_applet.desktop \
	$(FISH_BITMAPS)

fishbitmapsdir = $(datadir)/pixmaps/fish
fishbitmaps_DATA = $(FISH_BITMAPS)

sysdir = $(datadir)/applets/Amusements
sys_DATA = fish_applet.desktop

