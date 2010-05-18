INCLUDES =							\
	-I$(top_builddir)/libpanel-applet			\
	-DPANEL_APPLET_DATADIR=\""$(datadir)"\"			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	$(LIBPANEL_APPLET_CFLAGS)				\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(WARN_CFLAGS)

lib_LTLIBRARIES = libpanel-applet-2.la

libpanel_applet_2_la_SOURCES =		\
	$(BUILT_SOURCES)		\
	panel-applet.h			\
	panel-applet.c			\
	panel-applet-factory.h		\
	panel-applet-factory.c		\
	panel-applet-gconf.c		\
	panel-applet-gconf.h

libpanel_applet_2_la_LDFLAGS = \
	-version-info $(LIB_PANEL_APPLET_LT_VERSION) 	\
	-no-undefined

libpanel_applet_2_la_LIBADD  = \
	$(LIBPANEL_APPLET_LIBS) \
	$(X_LIBS)

libpanel_appletincludedir = $(includedir)/panel-2.0

libpanel_appletinclude_HEADERS =	\
	panel-applet.h			\
	panel-applet-gconf.h		\
	panel-applet-enums.h

noinst_PROGRAMS = test-dbus-applet

test_dbus_applet_LDADD =               \
       $(LIBPANEL_APPLET_LIBS)         \
       libpanel-applet-2.la

BUILT_SOURCES = \
	panel-applet-enums.c			\
	panel-applet-enums.h			\
	panel-applet-marshal.c			\
	panel-applet-marshal.h

$(libpanel_applet_2_la_OBJECTS) $(test_dbus_applet_OBJECTS): $(BUILT_SOURCES)

EXTRA_DIST =					\
	GNOME_Panel_TestApplet.panel-applet.in	\
	panel-applet-marshal.list		\
	libpanelapplet-2.0.pc.in		\
	libpanelapplet-2.0-uninstalled.pc.in

panel-applet-marshal.h: panel-applet-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --header --prefix=panel_applet_marshal > $@

panel-applet-marshal.c: panel-applet-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)echo "#include \"panel-applet-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=panel_applet_marshal >> $@

panel_applet_enum_headers = $(top_srcdir)/libpanel-applet/panel-applet.h

panel-applet-enums.c: @REBUILD@ $(panel_applet_enum_headers)
	$(AM_V_GEN)glib-mkenums \
		     --fhead "#include <glib-object.h>\n" \
		     --fhead "#include \"panel-applet-enums.h\"\n\n" \
		     --fprod "\n/* enumerations from \"@filename@\" */" \
		     --fprod "\n#include \"@filename@\"\n" \
		     --vhead "static const G@Type@Value _@enum_name@_values[] = {" \
		     --vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		     --vtail "  { 0, NULL, NULL }\n};\n\n" \
		     --vtail "GType\n@enum_name@_get_type (void)\n{\n" \
		     --vtail "  static GType type = 0;\n\n" \
		     --vtail "  if (!type)\n" \
		     --vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
		     --vtail "  return type;\n}\n\n" \
		$(panel_applet_enum_headers) > $@

panel-applet-enums.h: @REBUILD@ $(panel_applet_enum_headers)
	$(AM_V_GEN)glib-mkenums \
		     --fhead "#ifndef __PANEL_APPLET_ENUMS_H__\n" \
		     --fhead "#define __PANEL_APPLET_ENUMS_H__\n\n" \
		     --fhead "G_BEGIN_DECLS\n\n" \
		     --ftail "G_END_DECLS\n\n" \
		     --ftail "#endif /* __PANEL_APPLET_ENUMS_H__ */\n" \
		     --fprod "\n/* --- @filename@ --- */" \
		     --eprod "#define PANEL_TYPE_@ENUMNAME@ @enum_name@_get_type()\n" \
		     --eprod "GType @enum_name@_get_type (void);\n" \
		$(panel_applet_enum_headers) > $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libpanelapplet-2.0.pc

CLEANFILES = $(BUILT_SOURCES) $(noinst_DATA)

appletdir       = $(datadir)/gnome-panel/applets
applet_in_files = GNOME_Panel_TestApplet.panel-applet.in
noinst_DATA     = $(applet_in_files:.panel-applet.in=.panel-applet)

@PANEL_INTLTOOL_PANEL_APPLET_RULE@

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
