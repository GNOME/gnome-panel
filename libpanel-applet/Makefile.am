# Before making a release, the LTVERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LTVERSION = 0:0:0

INCLUDES =							\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOME_DATADIR=\""$(datadir)"\"			\
	$(PANEL_CFLAGS)						\
	$(WARN_CFLAGS)

lib_LTLIBRARIES = libpanel-applet-2.la

libpanel_applet_2_la_SOURCES =		\
	panel-applet.h			\
	panel-applet.c			\
	panel-applet-gconf.c		\
	panel-applet-gconf.h		\
	panel-applet-shell.c		\
	panel-applet-shell.h		\
	panel-applet-private.h		\
	panel-applet-marshal.c		\
	panel-applet-marshal.h		\
	panel-applet-enums.c		\
	panel-applet-enums.h		\
	$(CORBA_SRCLIST)

libpanel_applet_2_la_LDFLAGS = -version-info $(LTVERSION)
libpanel_applet_2_la_LIBADD  = $(PANEL_LIBS)

libpanel_appletincludedir = $(includedir)/panel-2.0

libpanel_appletinclude_HEADERS =	\
	panel-applet-gconf.h		\
	panel-applet.h			\
	GNOME_Panel.h

CORBA_SRCLIST =			\
	GNOME_Panel-stubs.c	\
	GNOME_Panel-skels.c	\
	GNOME_Panel-common.c	\
	GNOME_Panel.h

$(CORBA_SRCLIST): $(top_srcdir)/idl/GNOME_Panel.idl $(ORBIT_IDL)
	$(ORBIT_IDL) -I $(BONOBO_IDLDIR) -I $(BONOBO_ACT_IDLDIR) $(top_srcdir)/idl/GNOME_Panel.idl



bin_PROGRAMS = test-bonobo-applet

test_bonobo_applet_SOURCES = test-bonobo-applet.c

test_bonobo_applet_LDADD =	\
	$(PANEL_LIBS)		\
	libpanel-applet-2.la



BUILT_SOURCES = \
	$(CORBA_SRCLIST)	\
	panel-applet-enums.c	\
	panel-applet-enums.h	\
	panel-applet-marshal.c	\
	panel-applet-marshal.h

EXTRA_DIST =					\
	GNOME_Panel_TestApplet.server.in	\
	panel-applet-marshal.list		\
	libpanelapplet-2.0.pc.in

panel-applet-marshal.h: panel-applet-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=panel_applet_marshal > $@

panel-applet-marshal.c: panel-applet-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"panel-applet-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=panel_applet_marshal >> $@

panel_applet_enum_headers = $(top_srcdir)/libpanel-applet/panel-applet.h

panel-applet-enums.c: @REBUILD@ $(panel_applet_enum_headers)
	glib-mkenums --fhead "#include <glib-object.h>\n" \
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
	glib-mkenums --fhead "#ifndef __PANEL_APPLET_ENUMS_H__\n" \
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

CLEANFILES = $(BUILT_SOURCES)

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_Panel_TestApplet.server.in
server_DATA     = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)
