NULL =

SUBDIRS = \
	libpanel-util \
	$(NULL)

bin_PROGRAMS = \
	gnome-panel \
	$(NULL)

panel_sources =			\
	gp-add-applet-window.c \
	gp-add-applet-window.h \
	gp-applet-list-row.c \
	gp-applet-list-row.h \
	gp-applet-manager.c \
	gp-applet-manager.h \
	gp-applet-row.c \
	gp-applet-row.h \
	gp-application.c \
	gp-application.h \
	gp-arrow-button.c \
	gp-arrow-button.h \
	gp-handle.c \
	gp-handle.h \
	gp-main.c \
	gp-module-manager.c \
	gp-module-manager.h \
	gp-properties-dialog.c \
	gp-properties-dialog.h \
	gp-session.c \
	gp-session.h \
	gp-theme.c \
	gp-theme.h \
	panel-widget.c		\
	panel.c			\
	applet.c		\
	panel-util.c		\
	panel-run-dialog.c	\
	panel-context-menu.c	\
	panel-applet-frame.c	\
	panel-applets-manager.c	\
	panel-action-protocol.c	\
	panel-toplevel.c	\
	panel-struts.c		\
	panel-xutils.c		\
	panel-multiscreen.c	\
	panel-a11y.c		\
	panel-bindings.c	\
	panel-lockdown.c	\
	panel-layout.c		\
	panel-object-loader.c	\
	$(NULL)

panel_headers =			\
	panel-types.h		\
	panel-widget.h		\
	panel.h			\
	applet.h		\
	panel-util.h		\
	panel-run-dialog.h	\
	panel-context-menu.h	\
	panel-applet-frame.h	\
	panel-applets-manager.h	\
	panel-action-protocol.h	\
	panel-toplevel.h	\
	panel-struts.h		\
	panel-xutils.h		\
	panel-multiscreen.h	\
	panel-a11y.h		\
	panel-bindings.h	\
	panel-enums-gsettings.h	\
	panel-lockdown.h	\
	panel-icon-names.h	\
	panel-layout.h		\
	panel-object-loader.h	\
	panel-schemas.h		\
	$(NULL)

gnome_panel_SOURCES =			\
	$(panel_sources)		\
	$(panel_headers) \
	$(BUILT_SOURCES) \
	$(NULL)

gnome_panel_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gnome-panel\" \
	$(NULL)

gnome_panel_CFLAGS = \
	-DGMENU_I_KNOW_THIS_IS_UNSTABLE \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	-DLAYOUTSDIR=\""$(datadir)/gnome-panel/layouts"\" \
	-DMODULESDIR=\""$(libdir)/gnome-panel/modules"\" \
	-DGNOMELOCALEDIR=\""$(localedir)"\" \
	-I$(srcdir) \
	-I$(top_builddir)/gnome-panel \
	-I$(top_builddir)/gnome-panel/libpanel-util \
	-I$(top_srcdir) \
	$(XRANDR_CFLAGS) \
	$(PANEL_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

gnome_panel_LDADD =		\
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(top_builddir)/gnome-panel/libpanel-util/libpanel-util.la			\
	$(PANEL_LIBS)									\
	$(NULL)

gnome_panel_LDFLAGS = \
	-export-dynamic \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

panel_enum_headers =						\
	$(top_srcdir)/gnome-panel/panel-enums-gsettings.h	\
	$(top_srcdir)/gnome-panel/panel-types.h

panel-typebuiltins.c: $(panel_enum_headers) Makefile.am
	$(AM_V_GEN)glib-mkenums \
			--fhead "#include \"panel-typebuiltins.h\"\n\n" \
			--fprod "\n/* enumerations from \"@basename@\" */" \
			--fprod "\n#include \"@basename@\"\n" \
			--vhead "static const GEnumValue _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_enum_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(panel_enum_headers) > $@

panel-typebuiltins.h: $(panel_enum_headers) Makefile.am
	$(AM_V_GEN)glib-mkenums \
			--fhead "#ifndef __PANEL_TYPEBUILTINS_H__\n" \
			--fhead "#define __PANEL_TYPEBUILTINS_H__ 1\n\n" \
			--fhead "#include <glib-object.h>\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __PANEL_TYPEBUILTINS_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define PANEL_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(panel_enum_headers) > $@

BUILT_SOURCES = \
	panel-typebuiltins.c \
	panel-typebuiltins.h \
	panel-resources.c \
	panel-resources.h \
	$(NULL)

ui_FILES =					\
	gp-properties-dialog.ui \
	panel-run-dialog.ui

panel-resources.c: panel.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/panel.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name panel $<

panel-resources.h: panel.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/panel.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name panel $<

desktopdir = $(datadir)/applications
desktop_in_files = gnome-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t \
		-d $(top_srcdir)/po && mv $@-t $@

EXTRA_DIST =					\
	$(ui_FILES)				\
	panel.gresource.xml			\
	$(desktop_in_files) \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(sys_DATA) \
	$(desktop_DATA) \
	$(NULL)

-include $(top_srcdir)/git.mk
