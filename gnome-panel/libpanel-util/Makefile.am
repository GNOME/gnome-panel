noinst_LTLIBRARIES = libpanel-util.la

AM_CPPFLAGS =							\
	$(PANEL_CFLAGS)						\
	-I.							\
	-I$(srcdir)						\
	-I$(top_builddir)/gnome-panel/libpanel-util		\
	-DDATADIR=\""$(datadir)"\"				\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

panel_util_enum_headers =	\
	panel-session-manager.h

BUILT_SOURCES =			\
	panel-util-types.c	\
	panel-util-types.h

panel-util-types.c: $(panel_util_enum_headers)
	$(AM_V_GEN)glib-mkenums \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"panel-util-types.h\"\n\n" \
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
		$(addprefix $(srcdir)/,$(panel_util_enum_headers)) > $@.tmp && mv $@.tmp $@

panel-util-types.h: $(panel_util_enum_headers)
	$(AM_V_GEN)glib-mkenums \
			--fhead "#ifndef __PANEL_UTIL_TYPES_H__\n" \
			--fhead "#define __PANEL_UTIL_TYPES_H__ 1\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __PANEL_UTIL_TYPES_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define PANEL_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(addprefix $(srcdir)/,$(panel_util_enum_headers)) > $@.tmp && mv $@.tmp $@

libpanel_util_la_SOURCES =		\
	$(BUILT_SOURCES)		\
	panel-cleanup.c			\
	panel-cleanup.h			\
	panel-dconf.c			\
	panel-dconf.h			\
	panel-error.c			\
	panel-error.h			\
	panel-glib.c			\
	panel-glib.h			\
	panel-gsettings.c		\
	panel-gsettings.h		\
	panel-gtk.c			\
	panel-gtk.h			\
	panel-icon-chooser.c		\
	panel-icon-chooser.h		\
	panel-keyfile.c			\
	panel-keyfile.h			\
	panel-launch.c			\
	panel-launch.h			\
	panel-list.c			\
	panel-list.h			\
	panel-screensaver.c		\
	panel-screensaver.h		\
	panel-session-manager.c		\
	panel-session-manager.h		\
	panel-show.c			\
	panel-show.h			\
	panel-xdg.c			\
	panel-xdg.h

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
