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
	-DPREFIX=\""$(prefix)"\"			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"  \
	-DCLOCK_EDS_ICONDIR="\"$(CLOCK_EDS_ICONDIR)\""	\
	-DCLOCK_TIME_UTILITY="\"$(CLOCK_TIME_UTILITY)\""

if HAVE_LIBECAL
CALENDAR_SOURCES =		\
	calendar-client.c	\
	calendar-client.h	\
	calendar-sources.c	\
	calendar-sources.h	\
	calendar-debug.h
endif

CLOCK_SOURCES = 		\
	calendar-window.c	\
	calendar-window.h	\
	clock.c			\
	clock.h			\
	$(BUILT_SOURCES)	\
	$(CALENDAR_SOURCES)

CLOCK_LDADD =						\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(CLOCK_LIBS)

BUILT_SOURCES = \
	clock-typebuiltins.c	\
	clock-typebuiltins.h

clock_enum_headers =					\
	$(top_srcdir)/applets/clock/clock.h

clock-typebuiltins.c: @REBUILD@ $(clock_enum_headers)
	glib-mkenums    --fhead "#include <glib-object.h>\n" \
			--fhead "#include \"clock-typebuiltins.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--fprod "\n#include \"@filename@\"\n" \
			--vhead "static const GEnumValue _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_enum_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(clock_enum_headers) > $@

clock-typebuiltins.h: @REBUILD@ $(clock_enum_headers)
	glib-mkenums    --fhead "#ifndef __CLOCK_TYPEBUILTINS_H__\n" \
			--fhead "#define __CLOCK_TYPEBUILTINS_H__ 1\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __CLOCK_TYPEBUILTINS_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define CLOCK_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(clock_enum_headers) >  $@

if CLOCK_INPROCESS
APPLET_TYPE     = shlib
APPLET_LOCATION = $(pkglibdir)/libclock-applet.so

clock_appletlibdir = $(pkglibdir)
clock_appletlib_LTLIBRARIES = libclock-applet.la
libclock_applet_la_SOURCES = $(CLOCK_SOURCES)
libclock_applet_la_LIBADD = $(CLOCK_LDADD)
libclock_applet_la_LDFLAGS = -module -avoid-version
libclock_applet_la_CFLAGS =
$(libclock_applet_la_OBJECTS): $(BUILT_SOURCES)
else
APPLET_TYPE     = exe
APPLET_LOCATION = $(libexecdir)/clock-applet

libexec_PROGRAMS = clock-applet
clock_applet_SOURCES = $(CLOCK_SOURCES)
clock_applet_LDADD = $(CLOCK_LDADD)
clock_applet_CFLAGS =
$(clock_applet_OBJECTS): $(BUILT_SOURCES)
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

CLEANFILES =			\
	$(BUILT_SOURCES)	\
	$(server_in_files)	\
	$(server_DATA)		\
	$(schemas_DATA)
