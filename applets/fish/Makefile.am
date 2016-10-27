NULL =

fish_libdir = $(pkglibdir)
fish_lib_LTLIBRARIES = fish.la

fish_la_SOURCES = \
	fish-applet.c \
	fish-applet.h \
	$(BUILT_SOURCES) \
	$(NULL)

fish_la_CPPFLAGS = \
	-DFISH_ICONDIR=\""$(fishbitmapsdir)"\" \
	-DGNOMELOCALEDIR=\""$(localedir)"\" \
	-I$(srcdir)/../../libpanel-applet \
	-I$(top_builddir)/libpanel-applet \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

fish_la_CFLAGS = \
	$(LIBPANEL_APPLET_CFLAGS) \
	$(FISH_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

fish_la_LIBADD = \
	$(top_builddir)/libpanel-applet/libpanel-applet.la \
	$(LIBPANEL_APPLET_LIBS) \
	$(FISH_LIBS) \
	$(NULL)

fish_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

APPLET_LOCATION   = $(pkglibdir)/fish.so

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
fishbitmaps_DATA = \
	wanda.fish \
	wanda.png \
	fishanim.fish \
	fishanim.png \
	oldwanda.fish \
	oldwanda.png \
	footguy.fish \
	footguy.png \
	monkey.fish \
	monkey.png \
	$(NULL)

ui_FILES = \
	fish.ui \
	fish-menu.ui \
	$(NULL)

fish-resources.c: fish.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/fish.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name fish $<

fish-resources.h: fish.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/fish.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name fish $<

BUILT_SOURCES = \
	fish-resources.c \
	fish-resources.h \
	$(NULL)

EXTRA_DIST = \
	wanda.svg \
	org.gnome.panel.FishApplet.panel-applet.in.in \
	fish.gresource.xml \
	$(fishbitmaps_DATA) \
	$(ui_FILES) \
	$(NULL)

CLEANFILES = \
	$(applet_DATA) \
	$(applet_DATA).in \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
