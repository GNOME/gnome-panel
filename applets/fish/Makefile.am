INCLUDES =						\
	-I$(srcdir)					\
	-I$(srcdir)/../../libpanel-applet		\
	-I$(top_builddir)/libpanel-applet		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(GNOME_INCLUDEDIR)				\
	$(WARN_CFLAGS)					\
	$(PANEL_CFLAGS)

EXTRA_SUBDIRS = help

bin_PROGRAMS = fish-applet-2

fish_applet_2_SOURCES = fish.c

fish_applet_2_LDADD =					\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(PANEL_LIBS)

FISH_BITMAPS =		\
	fishanim.png	\
	oldwanda.png	\
	footguy.png	\
	monkey.png

EXTRA_DIST =					\
	$(FISH_BITMAPS)				\
	GNOME_FishApplet_Factory.server.in	\
	gnome-fish.png

fishbitmapsdir = $(datadir)/pixmaps/fish
fishbitmaps_DATA = $(FISH_BITMAPS)

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = gnome-fish.png

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_FishApplet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

