INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../panel				\
	-I$(top_builddir)/panel \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR)

#if DO_EXE
#bin_PROGRAMS = fish_applet
#else
lib_LTLIBRARIES=libfish_applet.la
#endif

libfish_applet_la_SOURCES = fish.c

#libfish_applet_la_LIBADD = ../../panel/libpanel_applet.la \
#		 	$(GNOME_LIBDIR) \
#		     	$(GNOMEGNORBA_LIBS) $(INTLLIBS) 

FISH_BITMAPS = fishanim.png oldwanda.png

EXTRA_DIST = \
	fish_applet.desktop \
	$(FISH_BITMAPS)	\
	fish_applet.gnorba

fishbitmapsdir = $(datadir)/pixmaps/fish
fishbitmaps_DATA = $(FISH_BITMAPS)

sysdir = $(datadir)/applets/Amusements
sys_DATA = fish_applet.desktop

gnorbadir  = $(sysconfdir)/CORBA/servers
gnorba_DATA = fish_applet.gnorba
