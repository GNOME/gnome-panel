NULL =

libfish_applet_la_CFLAGS = \
	-DGNOMELOCALEDIR=\""$(localedir)"\" \
	-DFISH_ICONDIR=\""$(fishbitmapsdir)"\" \
	-I$(srcdir) \
	-I$(srcdir)/../../libpanel-applet \
	-I$(top_builddir)/libpanel-applet \
	$(LIBPANEL_APPLET_CFLAGS) \
	$(FISH_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libfish_applet_la_SOURCES = \
	fish.c		\
	$(BUILT_SOURCES)

libfish_applet_la_LIBADD = \
	../../libpanel-applet/libpanel-applet.la	\
	$(FISH_LIBS)					\
	$(LIBPANEL_APPLET_LIBS)

libfish_applet_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

APPLET_LOCATION   = $(pkglibdir)/$(LIBPANEL_APPLET_API_VERSION)/libfish-applet.so

fish_applet_libdir = $(pkglibdir)/$(LIBPANEL_APPLET_API_VERSION)
fish_applet_lib_LTLIBRARIES = libfish-applet.la

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
