NULL =

wnck_appletlibdir = $(pkglibdir)
wnck_appletlib_LTLIBRARIES = libwnck-applet.la

libwnck_applet_la_CPPFLAGS = \
	-DGNOMELOCALEDIR=\""$(localedir)"\" \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	-I$(top_builddir) \
	-I$(top_builddir)/applets/wncklet \
	-I$(top_srcdir)/libpanel-applet \
	-I$(top_builddir)/libpanel-applet \
	$(AM_CPPFLAGS) \
	$(NULL)

libwnck_applet_la_CFLAGS = \
	$(LIBPANEL_APPLET_CFLAGS) \
	$(WNCKLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libwnck_applet_la_SOURCES = \
	wncklet.c \
	wncklet.h \
	window-menu.c \
	window-menu.h \
	window-list.c \
	window-list.h \
	workspace-switcher.c \
	workspace-switcher.h \
	showdesktop.c \
	showdesktop.h \
	$(BUILT_SOURCES) \
	$(NULL)

libwnck_applet_la_LIBADD = \
	$(top_builddir)/libpanel-applet/libpanel-applet.la \
	$(LIBPANEL_APPLET_LIBS) \
	$(WNCKLET_LIBS) \
	$(NULL)

libwnck_applet_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

APPLET_LOCATION = $(pkglibdir)/libwnck-applet.so

appletdir       = $(appletsdir)
applet_in_files = org.gnome.panel.Wncklet.panel-applet.in
applet_DATA     = $(applet_in_files:.panel-applet.in=.panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		-e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
		$< > $@

@PANEL_INTLTOOL_PANEL_APPLET_RULE@

ui_FILES = \
	window-list-menu.ui \
	window-list.ui \
	workspace-switcher-menu.ui \
	workspace-switcher.ui \
	$(NULL)

wncklet-resources.c: wncklet.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/wncklet.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name wncklet $<

wncklet-resources.h: wncklet.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/wncklet.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name wncklet $<

BUILT_SOURCES = \
	wncklet-resources.c \
	wncklet-resources.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.panel.Wncklet.panel-applet.in.in \
	wncklet.gresource.xml \
	$(ui_FILES) \
	$(NULL)

CLEANFILES = \
	$(applet_DATA) \
	$(applet_DATA).in \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
