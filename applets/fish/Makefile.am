INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../panel				\
	-I$(top_builddir)/panel				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	@PIXBUF_CFLAGS@

SUBDIRS = help

#if DO_EXE
#bin_PROGRAMS = fish_applet
#else
lib_LTLIBRARIES=libfish_applet.la
#endif

libfish_applet_la_SOURCES = fish.c

#fish_applet_SOURCES = fish.c

#fish_applet_LDADD = ../../panel/libpanel_applet.la \
#		 	$(GNOME_LIBDIR) \
#		     	$(GNOMEGNORBA_LIBS) $(INTLLIBS) \
#			@PIXBUF_LIBS@

#libfish_applet_la_LIBADD = .e./../panel/libpanel_applet.la \
#		 	$(GNOME_LIBDIR) \
#		     	$(GNOMEGNORBA_LIBS) $(INTLLIBS) 

FISH_BITMAPS = fishanim.png oldwanda.png footguy.png monkey.png

EXTRA_DIST = \
	fish_applet.desktop.in \
	$(FISH_BITMAPS)	\
	gnome-fish.png \
	fish_applet.gnorba

fishbitmapsdir = $(datadir)/pixmaps/fish
fishbitmaps_DATA = $(FISH_BITMAPS)

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = gnome-fish.png

sysdir = $(datadir)/applets/Amusements
sys_in_files = fish_applet.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

gnorbadir  = $(sysconfdir)/CORBA/servers
gnorba_DATA = fish_applet.gnorba
