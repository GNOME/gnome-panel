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

CLOCK_SOURCES = clock.c $(CALENDAR_SOURCES)

CLOCK_LDADD =						\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(CLOCK_LIBS)

if CLOCK_INPROCESS
APPLET_TYPE     = shlib
APPLET_LOCATION = $(pkglibdir)/libclock-applet.so

clock_appletlibdir = $(pkglibdir)
clock_appletlib_LTLIBRARIES = libclock-applet.la
libclock_applet_la_SOURCES = $(CLOCK_SOURCES)
libclock_applet_la_LIBADD = $(CLOCK_LDADD)
libclock_applet_la_LDFLAGS = -module -avoid-version
libclock_applet_la_CFLAGS =
else
APPLET_TYPE     = exe
APPLET_LOCATION = $(libexecdir)/clock-applet

libexec_PROGRAMS = clock-applet
clock_applet_SOURCES = $(CLOCK_SOURCES)
clock_applet_LDADD = $(CLOCK_LDADD)
clock_applet_CFLAGS =
endif

uidir 	= $(datadir)/gnome-2.0/ui
ui_DATA	= GNOME_ClockApplet.xml	

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_ClockApplet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files).in Makefile
	sed -e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
	    -e "s|\@TYPE\@|$(APPLET_TYPE)|" \
	    -e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
	    $< > $@

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
