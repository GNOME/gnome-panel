INCLUDES =							\
	-I$(srcdir)						\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DFISH_ICONDIR=\""$(fishbitmapsdir)"\"			\
	-DFISH_GLADEDIR=\""$(gladedir)"\"			\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(FISH_CFLAGS)

libexec_PROGRAMS = fish-applet-2

fish_applet_2_SOURCES = fish.c

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

gladedir   = $(datadir)/gnome/panel/glade
glade_DATA = fish.glade

fishbitmapsdir = $(datadir)/gnome/panel/pixmaps
fishbitmaps_DATA = $(FISH_BITMAPS)

pixmapdir = $(datadir)/icons/hicolor/48x48/apps
pixmap_DATA = gnome-fish.png

uidir		= $(datadir)/gnome-2.0/ui
ui_DATA		= GNOME_FishApplet.xml

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_FishApplet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files).in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

CLEANFILES = $(server_DATA) $(server_DATA).in $(schemas_DATA)

@INTLTOOL_SERVER_RULE@

schemasdir       = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = fish.schemas.in
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/applets/fish/$$p ; \
	done \
	fi
else
install-data-local:
endif
