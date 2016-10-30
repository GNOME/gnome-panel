NULL =

SUBDIRS = pixmaps

noinst_LTLIBRARIES = libsystem-timezone.la
noinst_PROGRAMS = test-system-timezone

libsystem_timezone_la_CFLAGS = \
	$(LIBPANEL_APPLET_CFLAGS) \
	$(TZ_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libsystem_timezone_la_SOURCES = \
	system-timezone.c	\
	system-timezone.h
libsystem_timezone_la_LIBADD = $(TZ_LIBS)

if ENABLE_EDS
CALENDAR_SOURCES =		\
	calendar-client.c	\
	calendar-client.h	\
	calendar-sources.c	\
	calendar-sources.h	\
	calendar-debug.h
endif

libclock_applet_la_SOURCES = \
	calendar-window.c	\
	calendar-window.h	\
	clock-applet.c		\
	clock-applet.h		\
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
	set-timezone.c		\
	set-timezone.h		\
	$(BUILT_SOURCES)	\
	$(CALENDAR_SOURCES)

libclock_applet_la_CFLAGS = \
	-DGNOMELOCALEDIR=\""$(localedir)"\"	\
	-DCLOCK_EDS_ICONDIR="\"$(CLOCK_EDS_ICONDIR)\""		\
	-DGWEATHER_I_KNOW_THIS_IS_UNSTABLE \
	-I$(srcdir)/../../libpanel-applet \
	-I$(top_builddir)/libpanel-applet \
	-I$(srcdir)/../../gnome-panel \
	-I$(top_builddir)/gnome-panel \
	$(LIBPANEL_APPLET_CFLAGS) \
	$(TZ_CFLAGS) \
	$(CLOCK_CFLAGS) \
	$(CLOCK_EDS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libclock_applet_la_LIBADD = \
	../../libpanel-applet/libpanel-applet.la	\
	../../gnome-panel/libpanel-util/libpanel-util.la	\
	libsystem-timezone.la \
	$(CLOCK_LIBS)					\
	$(CLOCK_EDS_LIBS)					\
	$(LIBPANEL_APPLET_LIBS)				\
	$(LIBM) \
	$(NULL)

libclock_applet_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

test_system_timezone_SOURCES = 	\
	test-system-timezone.c
test_system_timezone_LDADD = libsystem-timezone.la

test_system_timezone_CFLAGS = \
	$(LIBPANEL_APPLET_CFLAGS) \
	$(TZ_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

test_system_timezone_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

APPLET_LOCATION   = $(pkglibdir)/libclock-applet.so

clock_appletlibdir = $(pkglibdir)
clock_appletlib_LTLIBRARIES = libclock-applet.la
$(libclock_applet_la_OBJECTS): $(BUILT_SOURCES)

clock_enum_headers =					\
	$(top_srcdir)/applets/clock/clock-utils.h

clock-typebuiltins.c: $(clock_enum_headers)
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

clock-typebuiltins.h: $(clock_enum_headers)
	$(AM_V_GEN)glib-mkenums \
			--fhead "#ifndef __CLOCK_TYPEBUILTINS_H__\n" \
			--fhead "#define __CLOCK_TYPEBUILTINS_H__ 1\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __CLOCK_TYPEBUILTINS_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define CLOCK_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(clock_enum_headers) > $@

BUILT_SOURCES =					\
	clock-resources.c			\
	clock-resources.h			\
	clock-typebuiltins.c			\
	clock-typebuiltins.h

appletdir       = $(appletsdir)
applet_in_files = org.gnome.panel.ClockApplet.panel-applet.in
applet_DATA     = $(applet_in_files:.panel-applet.in=.panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		-e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
		$< > $@

@PANEL_INTLTOOL_PANEL_APPLET_RULE@

ui_FILES =		\
	clock.ui	\
	clock-menu.ui

clock-resources.c: clock.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/clock.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name clock $<

clock-resources.h: clock.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/clock.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name clock $<

EXTRA_DIST =						\
	org.gnome.panel.ClockApplet.panel-applet.in.in	\
	$(ui_FILES)					\
	clock.gresource.xml				\
	$(NULL)

CLEANFILES =			\
	$(BUILT_SOURCES)	\
	$(applet_DATA) 		\
	$(applet_DATA).in

-include $(top_srcdir)/git.mk
