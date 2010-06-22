SUBDIRS = pixmaps

INCLUDES =							\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(GNOME_INCLUDEDIR)					\
	$(WARN_CFLAGS)						\
	$(CLOCK_CFLAGS)						\
	$(POLKIT_CFLAGS)					\
	$(LIBPANEL_APPLET_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\"				\
	-DBUILDERDIR=\""$(uidir)"\"				\
	-DICONDIR=\""$(datadir)/gnome-panel/pixmaps"\"		\
	-DLIBDIR=\""$(libdir)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DPREFIX=\""$(prefix)"\"				\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DCLOCK_EDS_ICONDIR="\"$(CLOCK_EDS_ICONDIR)\""		\
	-DGWEATHER_I_KNOW_THIS_IS_UNSTABLE

if HAVE_LIBECAL
CALENDAR_SOURCES =		\
	calendar-client.c	\
	calendar-client.h	\
	calendar-sources.c	\
	calendar-sources.h	\
	calendar-debug.h
endif

if HAVE_POLKIT
SETTIME_SOURCES =		\
	set-timezone.c		\
	set-timezone.h
else
SETTIME_SOURCES =		\
	set-timezone-dummy.c	\
	set-timezone.h
endif

CLOCK_SOURCES = 		\
	calendar-window.c	\
	calendar-window.h	\
	clock.c			\
	clock.h			\
	clock-face.c		\
	clock-face.h		\
	clock-location.c	\
	clock-location.h	\
	clock-location-tile.c	\
	clock-location-tile.h	\
	clock-map.c		\
	clock-map.h		\
	clock-sunpos.c		\
	clock-sunpos.h		\
	clock-utils.c		\
	clock-utils.h		\
	system-timezone.c	\
	system-timezone.h	\
	$(BUILT_SOURCES)	\
	$(CALENDAR_SOURCES)	\
	$(SETTIME_SOURCES)

CLOCK_LDADD =						\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(CLOCK_LIBS)					\
	$(LIBPANEL_APPLET_LIBS)				\
	$(POLKIT_LIBS)				\
	-lgweather -lm

BUILT_SOURCES =					\
	clock-marshallers.c			\
	clock-marshallers.h			\
	clock-typebuiltins.c			\
	clock-typebuiltins.h

clock-marshallers.c: clock-marshallers.list
	$(AM_V_GEN)glib-genmarshal --prefix _clock_marshal --header --body --internal $< > $@

clock-marshallers.h: clock-marshallers.list
	$(AM_V_GEN)glib-genmarshal --prefix _clock_marshal --header --internal $< > $@

clock_enum_headers =					\
	$(top_srcdir)/applets/clock/clock-utils.h

clock-typebuiltins.c: @REBUILD@ $(clock_enum_headers)
	$(AM_V_GEN)glib-mkenums \
			--fhead "#include <glib-object.h>\n" \
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
	$(AM_V_GEN)glib-mkenums \
			--fhead "#ifndef __CLOCK_TYPEBUILTINS_H__\n" \
			--fhead "#define __CLOCK_TYPEBUILTINS_H__ 1\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __CLOCK_TYPEBUILTINS_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define CLOCK_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(clock_enum_headers) >  $@

gnome-clock-applet-mechanism-glue.h: $(srcdir)/gnome-clock-applet-mechanism.xml
	$(AM_V_GEN)dbus-binding-tool \
			--prefix=gnome_clock_applet_mechanism --mode=glib-server	\
			--output=gnome-clock-applet-mechanism-glue.h			\
			$(srcdir)/gnome-clock-applet-mechanism.xml

gnome_clock_applet_mechanism_SOURCES =		\
	gnome-clock-applet-mechanism.c		\
	gnome-clock-applet-mechanism.h		\
	gnome-clock-applet-mechanism-main.c	\
	system-timezone.c			\
	system-timezone.h

if HAVE_POLKIT
BUILT_SOURCES += gnome-clock-applet-mechanism-glue.h
endif

gnome_clock_applet_mechanism_INCLUDES =	\
	$(CLOCK_MECHANISM_CFLAGS) 	\
	$(POLKIT_CFLAGS)

gnome_clock_applet_mechanism_LDADD =	\
	$(CLOCK_MECHANISM_LIBS)		\
	$(POLKIT_LIBS)

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

libexec_PROGRAMS = 
else
APPLET_TYPE     = exe
APPLET_LOCATION = $(libexecdir)/clock-applet

libexec_PROGRAMS = clock-applet 
clock_applet_SOURCES = $(CLOCK_SOURCES)
clock_applet_LDADD = $(CLOCK_LDADD)
clock_applet_CFLAGS =
$(clock_applet_OBJECTS): $(BUILT_SOURCES)
endif

if HAVE_POLKIT
libexec_PROGRAMS += gnome-clock-applet-mechanism
endif

uidir		= $(datadir)/gnome-panel/ui
ui_DATA		= clock.ui

xmluidir 	= $(datadir)/gnome-2.0/ui
xmlui_DATA	= GNOME_ClockApplet.xml

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_ClockApplet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files).in Makefile
	$(AM_V_GEN)sed \
	    -e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
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

dbus_servicesdir = $(datadir)/dbus-1/system-services
dbus_confdir = $(sysconfdir)/dbus-1/system.d
polkitdir = $(datadir)/polkit-1/actions

dbus_services_in_files = org.gnome.ClockApplet.Mechanism.service.in
polkit_in_files = org.gnome.clockapplet.mechanism.policy.in

if HAVE_POLKIT
dbus_services_DATA = $(dbus_services_in_files:.service.in=.service)

$(dbus_services_DATA): $(dbus_services_in_files)
	$(AM_V_GEN)sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

dbus_conf_DATA = org.gnome.ClockApplet.Mechanism.conf

@INTLTOOL_POLICY_RULE@
polkit_DATA = $(polkit_in_files:.policy.in=.policy)

else
dbus_services_DATA =
dbus_conf_DATA =
polkit_DATA =
endif

noinst_PROGRAMS = test-system-timezone

test_system_timezone_SOURCES = 	\
	system-timezone.c	\
	system-timezone.h	\
	test-system-timezone.c	\
	$(NULL)

test_system_timezone_LDADD =	\
	$(CLOCK_LDADD)		\
	$(NULL)

EXTRA_DIST =						\
	GNOME_ClockApplet_Factory.server.in.in		\
	$(schemas_in_files)				\
	$(xmlui_DATA)					\
	$(ui_DATA)					\
	clock-marshallers.list				\
	$(dbus_services_in_files)			\
	org.gnome.ClockApplet.Mechanism.conf		\
	$(polkit_in_files)				\
	gnome-clock-applet-mechanism.xml

CLEANFILES =			\
	$(BUILT_SOURCES)	\
	$(server_in_files)	\
	$(server_DATA)		\
	$(schemas_DATA)		\
	$(polkit_DATA)		\
	$(dbus_services_DATA)

-include $(top_srcdir)/git.mk
