NULL =

fish_libdir = $(libdir)/gnome-panel/applets
fish_lib_LTLIBRARIES = fish.la

fish_la_SOURCES = \
	fish-applet.c \
	fish-applet.h \
	fish-module.c \
	$(BUILT_SOURCES) \
	$(NULL)

fish_la_CPPFLAGS = \
	-DFISH_ICONDIR=\""$(fishbitmapsdir)"\" \
	-DLOCALEDIR=\""$(localedir)"\" \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

fish_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(FISH_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

fish_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(FISH_LIBS) \
	$(NULL)

fish_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

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
	fish.gresource.xml \
	$(fishbitmaps_DATA) \
	$(ui_FILES) \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
