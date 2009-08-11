INCLUDES = \
	-I$(top_builddir) \
	-I$(top_builddir)/applets/wncklet \
	-I$(top_srcdir)/libpanel-applet \
	-I$(top_builddir)/libpanel-applet \
	$(WNCKLET_CFLAGS) \
	$(LIBPANEL_APPLET_CFLAGS) \
	$(DISABLE_DEPRECATED_CFLAGS) \
	$(WARN_CFLAGS) \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPAGER_BUILDERDIR=\""$(uidir)"\" \
	-DTASKLIST_BUILDERDIR=\""$(uidir)"\" \
	-DPREFIX=\"$(prefix)\" \
        -DSYSCONFDIR=\"$(sysconfdir)\" \
        -DDATADIR=\"$(datadir)\" \
        -DLIBDIR=\"$(libdir)\"

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

$(wnck_applet_OBJECTS): $(BUILT_SOURCES)

WNCKLET_LDADD =						\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(WNCKLET_LIBS)					\
	$(LIBPANEL_APPLET_LIBS)

if WNCKLET_INPROCESS
APPLET_TYPE     = shlib
APPLET_LOCATION = $(pkglibdir)/libwnck-applet.so

wnck_appletlibdir = $(pkglibdir)
wnck_appletlib_LTLIBRARIES = libwnck-applet.la
libwnck_applet_la_SOURCES = $(WNCKLET_SOURCES)
libwnck_applet_la_LIBADD = $(WNCKLET_LDADD)
libwnck_applet_la_LDFLAGS = -module -avoid-version
libwnck_applet_la_CFLAGS =
else
APPLET_TYPE     = exe
APPLET_LOCATION = $(libexecdir)/wnck-applet

libexec_PROGRAMS = wnck-applet
wnck_applet_SOURCES = $(WNCKLET_SOURCES)
wnck_applet_LDADD = $(WNCKLET_LDADD)
wnck_applet_CFLAGS =
endif

EXTRA_DIST = \
	GNOME_Wncklet_Factory.server.in.in \
	$(xmlui_DATA) \
	$(schemas_in_files) \
	$(ui_DATA)

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_Wncklet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files).in Makefile
	$(AM_V_GEN)sed \
	    -e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
	    -e "s|\@TYPE\@|$(APPLET_TYPE)|" \
	    -e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
	    $< > $@

@INTLTOOL_SERVER_RULE@

CLEANFILES = $(server_in_files) $(server_DATA) $(BUILT_SOURCES) $(schemas_DATA)

xmluidir    = $(datadir)/gnome-2.0/ui
xmlui_DATA  = \
	GNOME_WindowMenuApplet.xml \
	GNOME_WorkspaceSwitcherApplet.xml \
	GNOME_ShowDesktopApplet.xml \
	GNOME_WindowListApplet.xml

schemasdir       = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = workspace-switcher.schemas.in window-list.schemas.in 
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/applets/wncklet/$$p ; \
		done \
	fi
else
install-data-local:
endif

uidir   = $(datadir)/gnome-panel/ui
ui_DATA = workspace-switcher.ui window-list.ui


-include $(top_srcdir)/git.mk
