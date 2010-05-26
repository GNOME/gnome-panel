INCLUDES =							\
	-I$(srcdir)						\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DFISH_ICONDIR=\""$(fishbitmapsdir)"\"			\
	-DFISH_BUILDERDIR=\""$(uidir)"\"			\
	-DFISH_MENU_UI_DIR=\""$(xmluidir)"\" 			\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(FISH_CFLAGS)						\
	$(LIBPANEL_APPLET_CFLAGS)

FISH_SOURCES = fish.c

FISH_LDADD =						\
	../../libpanel-applet/libpanel-applet-3.la	\
	$(FISH_LIBS)					\
	$(LIBPANEL_APPLET_LIBS)

if FISH_INPROCESS
APPLET_IN_PROCESS = true
APPLET_LOCATION   = $(pkglibdir)/libfish-applet.so

fish_applet_libdir = $(pkglibdir)
fish_applet_lib_LTLIBRARIES = libfish-applet.la
libfish_applet_la_SOURCES = $(FISH_SOURCES)
libfish_applet_la_LIBADD = $(FISH_LDADD)
libfish_applet_la_LDFLAGS = -module -avoid-version
libfish_applet_la_CFLAGS =
else
APPLET_IN_PROCESS = false
APPLET_LOCATION = $(libexecdir)/fish-applet

libexec_PROGRAMS = fish-applet
fish_applet_SOURCES = $(FISH_SOURCES)
fish_applet_LDADD = $(FISH_LDADD)
fish_applet_CFLAGS =
endif

FISH_BITMAPS =		\
	wanda.png	\
	fishanim.png	\
	oldwanda.png	\
	footguy.png	\
	monkey.png

EXTRA_DIST =						\
	wanda.svg					\
	$(FISH_BITMAPS)					\
	org.gnome.panel.FishApplet.panel-applet.in.in	\
	$(service_in_files)				\
	fish.schemas.in					\
	$(ui_DATA)					\
	$(xmlui_DATA)

uidir		= $(datadir)/gnome-panel/ui
ui_DATA		= fish.ui

fishbitmapsdir = $(datadir)/gnome-panel/pixmaps
fishbitmaps_DATA = $(FISH_BITMAPS)

xmluidir 	= $(datadir)/gnome-panel/ui
xmlui_DATA	= fish-menu.xml

CLEANFILES = $(schemas_DATA) $(applet_DATA) $(applet_DATA).in $(service_DATA)

appletdir       = $(datadir)/gnome-panel/applets
applet_in_files = org.gnome.panel.FishApplet.panel-applet.in
applet_DATA     = $(applet_in_files:.panel-applet.in=.panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed \
	    -e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
	    -e "s|\@IN_PROCESS\@|$(APPLET_IN_PROCESS)|" \
	    -e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
	    $< > $@

@PANEL_INTLTOOL_PANEL_APPLET_RULE@

if !FISH_INPROCESS
servicedir       = $(datadir)/dbus-1/services
service_in_files = org.gnome.panel.applet.FishAppletFactory.service.in
service_DATA     = $(service_in_files:.service.in=.service)

org.gnome.panel.applet.FishAppletFactory.service: $(service_in_files)
	$(AM_V_GEN)sed \
	    -e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
	    $< > $@
endif

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
