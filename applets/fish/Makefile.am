INCLUDES =							\
	-I$(srcdir)						\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGNOME_ICONDIR=\""$(datadir)/pixmaps\""		\
	-DFISH_GLADEDIR=\""$(gladedir)"\"			\
	$(WARN_CFLAGS)						\
	$(FISH_CFLAGS)

libexec_PROGRAMS = fish-applet-2

EGGFILES =                      \
	egg-screen-exec.h       \
	egg-screen-exec.c       \
	egg-screen-help.h       \
	egg-screen-help.c       \
	egg-screen-url.h        \
	egg-screen-url.c

fish_applet_2_SOURCES = fish.c $(EGGFILES)

fish_applet_2_LDADD =					\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(FISH_LIBS)

FISH_BITMAPS =		\
	fishanim.png	\
	oldwanda.png	\
	footguy.png	\
	monkey.png

EXTRA_DIST =					\
	$(FISH_BITMAPS)				\
	GNOME_FishApplet_Factory.server.in.in	\
	gnome-fish.png				\
	fish.schemas.in				\
	$(ui_DATA)				\
	$(glade_DATA)

gladedir   = $(datadir)/fish
glade_DATA = fish.glade

fishbitmapsdir = $(datadir)/pixmaps/fish
fishbitmaps_DATA = $(FISH_BITMAPS)

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = gnome-fish.png

uidir		= $(datadir)/gnome-2.0/ui
ui_DATA		= GNOME_FishApplet.xml

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_FishApplet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files).in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

schemasdir       = $(sysconfdir)/gconf/schemas
schemas_in_files = fish.schemas.in
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/applets/fish/$$p ; \
	done \
	fi

EGGDIR = $(srcdir)/../../../libegg/libegg/screen-exec

regenerate-built-sources:
	EGGFILES="$(EGGFILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh
