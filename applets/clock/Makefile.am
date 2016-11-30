NULL =

SUBDIRS = pixmaps

clock_libdir = $(libdir)/gnome-panel/applets
clock_lib_LTLIBRARIES = clock.la

if ENABLE_EDS
CALENDAR_SOURCES = \
	calendar-client.c \
	calendar-client.h \
	calendar-sources.c \
	calendar-sources.h \
	calendar-debug.h \
	$(NULL)
endif

clock_la_SOURCES = \
	calendar-window.c \
	calendar-window.h \
	clock-applet.c \
	clock-applet.h \
	clock-face.c \
	clock-face.h \
	clock-location.c \
	clock-location.h \
	clock-location-tile.c \
	clock-location-tile.h \
	clock-map.c \
	clock-map.h \
	clock-module.c \
	clock-sunpos.c \
	clock-sunpos.h \
	clock-utils.c \
	clock-utils.h \
	set-timezone.c \
	set-timezone.h \
	system-timezone.c \
	system-timezone.h \
	$(BUILT_SOURCES) \
	$(CALENDAR_SOURCES) \
	$(NULL)

clock_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DCLOCK_EDS_ICONDIR="\"$(CLOCK_EDS_ICONDIR)\"" \
	-DGWEATHER_I_KNOW_THIS_IS_UNSTABLE \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

clock_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(CLOCK_CFLAGS) \
	$(CLOCK_EDS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

clock_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(CLOCK_LIBS) \
	$(CLOCK_EDS_LIBS) \
	$(LIBGNOME_PANEL_LIBS) \
	$(LIBM) \
	$(NULL)

clock_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

clock_enum_headers = \
	$(top_srcdir)/applets/clock/clock-utils.h \
	$(NULL)

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

BUILT_SOURCES = \
	clock-resources.c \
	clock-resources.h \
	clock-typebuiltins.c \
	clock-typebuiltins.h \
	$(NULL)

ui_FILES = \
	clock.ui \
	clock-menu.ui \
	$(NULL)

clock-resources.c: clock.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/clock.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name clock $<

clock-resources.h: clock.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/clock.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name clock $<

EXTRA_DIST = \
	clock.gresource.xml \
	$(ui_FILES) \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
