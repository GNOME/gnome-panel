SUBDIRS = cut-n-paste

INCLUDES = 						\
	-I$(srcdir)/../../libpanel-applet 		\
	-I$(top_builddir)/libpanel-applet 		\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(GNOME_INCLUDEDIR) 				\
	$(WARN_CFLAGS) 					\
	$(CLOCK_CFLAGS) 				\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DPREFIX=\""$(prefix)"\"				\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 

if HAVE_LIBECAL
CALENDAR_SOURCES =		\
	calendar-client.c	\
	calendar-client.h	\
	calendar-sources.c	\
	calendar-sources.h	\
	calendar-debug.h
endif

clock_applet_SOURCES = clock.c $(CALENDAR_SOURCES)

clock_applet_LDADD =					\
	../../libpanel-applet/libpanel-applet-2.la	\
	cut-n-paste/libclockcnp.la 				\
	$(CLOCK_LIBS)

libexec_PROGRAMS = clock-applet

GNOME_ClockApplet_Factory.server.in: GNOME_ClockApplet_Factory.server.in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

uidir 	= $(datadir)/gnome-2.0/ui
ui_DATA	= GNOME_ClockApplet.xml	

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_ClockApplet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

schemasdir       = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = clock.schemas.in
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
		        GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/applets/clock/$$p ; \
		done \
	fi
else
install-data-local:
endif

EXTRA_DIST =						\
	GNOME_ClockApplet_Factory.server.in.in		\
	$(schemas_in_files)				\
	$(ui_DATA)

CLEANFILES = $(server_in_files) $(server_DATA) $(schemas_DATA)
