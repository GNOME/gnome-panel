AM_CPPFLAGS =							\
	$(LIBPANEL_APPLET_CFLAGS)				\
	$(FISH_CFLAGS)						\
	-I$(srcdir)						\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DFISH_ICONDIR=\""$(fishbitmapsdir)"\"			\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

FISH_SOURCES = 		\
	fish.c		\
	$(BUILT_SOURCES)

FISH_LDADD =						\
	../../libpanel-applet/libpanel-applet.la	\
	$(FISH_LIBS)					\
	$(LIBPANEL_APPLET_LIBS)

APPLET_LOCATION   = $(pkglibdir)/$(LIBPANEL_APPLET_API_VERSION)/libfish-applet.so

fish_applet_libdir = $(pkglibdir)/$(LIBPANEL_APPLET_API_VERSION)
fish_applet_lib_LTLIBRARIES = libfish-applet.la
libfish_applet_la_SOURCES = $(FISH_SOURCES)
libfish_applet_la_LIBADD = $(FISH_LDADD)
libfish_applet_la_LDFLAGS = -module -avoid-version
libfish_applet_la_CFLAGS = $(AM_CFLAGS)

appletdir       = $(appletsdir)
applet_in_files = org.gnome.panel.FishApplet.panel-applet.in
applet_DATA     = $(applet_in_files:.panel-applet.in=.panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		-e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
		$< > $@

@PANEL_INTLTOOL_PANEL_APPLET_RULE@

fishbitmapsdir = $(datadir)/gnome-panel/fish
fishbitmaps_DATA = 	\
	wanda.fish	\
	wanda.png	\
	fishanim.fish	\
	fishanim.png	\
	oldwanda.fish	\
	oldwanda.png	\
	footguy.fish	\
	footguy.png	\
	monkey.fish	\
	monkey.png

ui_FILES = 		\
	fish.ui		\
	fish-menu.xml

fish-resources.c: fish.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/fish.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name fish $<

fish-resources.h: fish.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/fish.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name fish $<

BUILT_SOURCES = 		\
	fish-resources.c	\
	fish-resources.h

EXTRA_DIST =						\
	wanda.svg					\
	$(fishbitmaps_DATA)				\
	org.gnome.panel.FishApplet.panel-applet.in.in	\
	$(ui_FILES)					\
	fish.gresource.xml

CLEANFILES = $(BUILT_SOURCES) $(applet_DATA) $(applet_DATA).in

-include $(top_srcdir)/git.mk
