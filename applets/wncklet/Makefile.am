libexec_PROGRAMS = wnck-applet

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_builddir)/applets/wncklet \
	-I$(top_srcdir)/libpanel-applet \
	-I$(top_builddir)/libpanel-applet \
	$(WNCKLET_CFLAGS) \
	$(DISABLE_DEPRECATED_CFLAGS) \
	$(WARN_CFLAGS) \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPAGER_GLADEDIR=\""$(gladedir)"\" \
	-DTASKLIST_GLADEDIR=\""$(gladedir)"\" \
	-DPREFIX=\"$(prefix)\" \
        -DSYSCONFDIR=\"$(sysconfdir)\" \
        -DDATADIR=\"$(datadir)\" \
        -DLIBDIR=\"$(libdir)\"

wnck_applet_SOURCES = \
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

wnck_applet_LDADD = \
	../../libpanel-applet/libpanel-applet-2.la \
	$(WNCKLET_LIBS)

EXTRA_DIST = \
	GNOME_Wncklet_Factory.server.in.in \
	$(ui_DATA) \
	$(schemas_in_files) \
	$(glade_DATA)

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_Wncklet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files).in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

CLEANFILES = $(server_in_files) $(server_DATA) $(BUILT_SOURCES) $(schemas_DATA)

uidir    = $(datadir)/gnome-2.0/ui
ui_DATA  = \
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

gladedir   = $(datadir)/gnome/panel/glade
glade_DATA = workspace-switcher.glade window-list.glade

