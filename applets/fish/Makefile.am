INCLUDES =							\
	-I$(srcdir)						\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DFISH_ICONDIR=\""$(fishbitmapsdir)"\"			\
	-DFISH_BUILDERDIR=\""$(uidir)"\"			\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(FISH_CFLAGS)						\
	$(LIBPANEL_APPLET_CFLAGS)

FISH_SOURCES = fish.c

FISH_LDADD =						\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(FISH_LIBS)					\
	$(LIBPANEL_APPLET_LIBS)

if FISH_INPROCESS
APPLET_TYPE     = shlib
APPLET_LOCATION = $(pkglibdir)/libfish-applet-2.so

fish_applet_2libdir = $(pkglibdir)
fish_applet_2lib_LTLIBRARIES = libfish-applet-2.la
libfish_applet_2_la_SOURCES = $(FISH_SOURCES)
libfish_applet_2_la_LIBADD = $(FISH_LDADD)
libfish_applet_2_la_LDFLAGS = -module -avoid-version
libfish_applet_2_la_CFLAGS =
else
APPLET_TYPE     = exe
APPLET_LOCATION = $(libexecdir)/fish-applet-2

libexec_PROGRAMS = fish-applet-2
fish_applet_2_SOURCES = $(FISH_SOURCES)
fish_applet_2_LDADD = $(FISH_LDADD)
fish_applet_2_CFLAGS =
endif

FISH_BITMAPS =		\
	wanda.png	\
	fishanim.png	\
	oldwanda.png	\
	footguy.png	\
	monkey.png

EXTRA_DIST =					\
	wanda.svg				\
	$(FISH_BITMAPS)				\
	GNOME_FishApplet_Factory.server.in.in	\
	fish.schemas.in				\
	$(ui_DATA)				\
	$(xmlui_DATA)

uidir		= $(datadir)/gnome-panel/ui
ui_DATA		= fish.ui

fishbitmapsdir = $(datadir)/gnome-panel/pixmaps
fishbitmaps_DATA = $(FISH_BITMAPS)

xmluidir	= $(datadir)/gnome-2.0/ui
xmlui_DATA	= GNOME_FishApplet.xml

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_FishApplet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files).in Makefile
	$(AM_V_GEN)sed \
	    -e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
	    -e "s|\@TYPE\@|$(APPLET_TYPE)|" \
	    -e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
	    $< > $@

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

-include $(top_srcdir)/git.mk
