AM_CPPFLAGS =							\
	$(LIBPANEL_APPLET_CFLAGS)				\
	$(WNCKLET_CFLAGS)					\
	-I$(top_builddir)					\
	-I$(top_builddir)/applets/wncklet			\
	-I$(top_srcdir)/libpanel-applet				\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DPAGER_BUILDERDIR=\""$(uidir)"\"			\
	-DTASKLIST_BUILDERDIR=\""$(uidir)"\"			\
	-DWNCK_MENU_UI_DIR=\""$(xmluidir)"\"			\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

WNCKLET_SOURCES = \
	wncklet.c \
	wncklet.h \
	window-menu.c \
	window-menu.h \
	window-list.c \
	window-list.h \
	workspace-switcher.c \
	workspace-switcher.h \
	showdesktop.c \
	showdesktop.h

WNCKLET_LDADD =						\
	../../libpanel-applet/libpanel-applet-3.la	\
	$(WNCKLET_LIBS)					\
	$(LIBPANEL_APPLET_LIBS)

if WNCKLET_INPROCESS
APPLET_IN_PROCESS = true
APPLET_LOCATION   = $(pkglibdir)/libwnck-applet.so

wnck_appletlibdir = $(pkglibdir)
wnck_appletlib_LTLIBRARIES = libwnck-applet.la
libwnck_applet_la_SOURCES = $(WNCKLET_SOURCES)
libwnck_applet_la_LIBADD = $(WNCKLET_LDADD)
libwnck_applet_la_LDFLAGS = -module -avoid-version
libwnck_applet_la_CFLAGS = $(AM_CFLAGS)
else
APPLET_IN_PROCESS = false
APPLET_LOCATION   = $(libexecdir)/wnck-applet

libexec_PROGRAMS = wnck-applet
wnck_applet_SOURCES = $(WNCKLET_SOURCES)
wnck_applet_LDADD = $(WNCKLET_LDADD)
wnck_applet_CFLAGS = $(AM_CFLAGS)
endif

appletdir       = $(datadir)/gnome-panel/applets
applet_in_files = org.gnome.panel.Wncklet.panel-applet.in
applet_DATA     = $(applet_in_files:.panel-applet.in=.panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		-e "s|\@IN_PROCESS\@|$(APPLET_IN_PROCESS)|" \
		-e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
		$< > $@

@PANEL_INTLTOOL_PANEL_APPLET_RULE@

if !WNCKLET_INPROCESS
servicedir       = $(datadir)/dbus-1/services
service_in_files = org.gnome.panel.applet.WnckletFactory.service.in
service_DATA     = $(service_in_files:.service.in=.service)

org.gnome.panel.applet.WnckletFactory.service: $(service_in_files)
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		$< > $@
endif

uidir   = $(datadir)/gnome-panel/ui
ui_DATA = workspace-switcher.ui window-list.ui

xmluidir    = $(datadir)/gnome-panel/ui
xmlui_DATA  = \
	window-menu-menu.xml \
	workspace-switcher-menu.xml \
	showdesktop-menu.xml \
	window-list-menu.xml

schemasdir       = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = workspace-switcher.schemas.in window-list.schemas.in 
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/applets/wncklet/$$p ; \
		done \
	fi
uninstall-local:
	for p in $(schema_DATA) ; do \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-uninstall-rule $(top_builddir)/applets/wncklet/$$p ; \
	done
endif

EXTRA_DIST = \
	org.gnome.panel.Wncklet.panel-applet.in.in \
	$(service_in_files) \
	$(xmlui_DATA) \
	$(schemas_in_files) \
	$(ui_DATA)

CLEANFILES = $(schemas_DATA) $(applet_DATA) $(applet_DATA).in $(service_DATA)

-include $(top_srcdir)/git.mk
