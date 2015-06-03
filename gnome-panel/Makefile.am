SUBDIRS = libpanel-applet-private libpanel-util

bin_PROGRAMS = \
	gnome-panel 			\
	gnome-desktop-item-edit		\
	panel-test-applets

AM_CPPFLAGS =							\
	$(PANEL_CFLAGS)						\
	-I.							\
	-I$(srcdir)						\
	-I$(top_builddir)/gnome-panel				\
	-I$(top_builddir)/gnome-panel/libpanel-util		\
	-DPANELDATADIR=\""$(datadir)/gnome-panel"\"		\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

panel_sources =			\
	$(gnome_panel_BUILT_SOURCES) \
	main.c			\
	panel-widget.c		\
	button-widget.c		\
	xstuff.c		\
	free-the-fish.c		\
	panel-session.c		\
	panel-compatibility.c	\
	panel.c			\
	applet.c		\
	panel-util.c		\
	panel-properties-dialog.c	\
	panel-run-dialog.c	\
	menu.c			\
	panel-context-menu.c	\
	launcher.c		\
	panel-applet-frame.c	\
	panel-applets-manager.c	\
	panel-background.c	\
	panel-background-monitor.c	\
	panel-stock-icons.c	\
	panel-action-button.c	\
	panel-image-menu-item.c \
	panel-menu-bar.c	\
	panel-menu-bar-object.c	\
	panel-menu-button.c	\
	panel-menu-items.c	\
	panel-separator.c	\
	panel-recent.c		\
	panel-action-protocol.c	\
	panel-toplevel.c	\
	panel-struts.c		\
	panel-frame.c		\
	panel-xutils.c		\
	panel-multiscreen.c	\
	panel-a11y.c		\
	panel-bindings.c	\
	panel-force-quit.c	\
	panel-lockdown.c	\
	panel-addto.c		\
	panel-ditem-editor.c	\
	panel-layout.c		\
	panel-modules.c		\
	panel-object-loader.c	\
	panel-user-menu.c	\
	panel-applet-info.c

panel_headers =			\
	panel-types.h		\
	panel-widget.h		\
	panel-globals.h		\
	button-widget.h		\
	xstuff.h		\
	free-the-fish.h		\
	panel-session.h		\
	panel-compatibility.h	\
	panel.h			\
	applet.h		\
	panel-util.h		\
	panel-properties-dialog.h	\
	panel-run-dialog.h	\
	menu.h			\
	panel-context-menu.h	\
	launcher.h		\
	panel-applet-frame.h	\
	panel-applets-manager.h	\
	panel-background.h	\
	panel-background-monitor.h	\
	panel-stock-icons.h	\
	panel-action-button.h	\
	panel-image-menu-item.h \
	panel-menu-bar.h	\
	panel-menu-bar-object.h	\
	panel-menu-button.h	\
	panel-menu-items.h	\
	panel-separator.h	\
	panel-recent.h		\
	panel-action-protocol.h	\
	panel-toplevel.h	\
	panel-struts.h		\
	panel-frame.h		\
	panel-xutils.h		\
	panel-multiscreen.h	\
	panel-a11y.h		\
	panel-bindings.h	\
	panel-enums.h		\
	panel-enums-gsettings.h	\
	panel-force-quit.h	\
	panel-lockdown.h	\
	panel-addto.h		\
	panel-ditem-editor.h	\
	panel-icon-names.h	\
	panel-layout.h		\
	panel-modules.h		\
	panel-object-loader.h	\
	panel-schemas.h		\
	panel-user-menu.h	\
	panel-applet-info.h

gnome_panel_SOURCES =			\
	$(panel_sources)		\
	$(panel_headers)

gnome_panel_CPPFLAGS = 				\
	$(AM_CPPFLAGS)				\
	$(TELEPATHY_GLIB_CFLAGS)		\
	$(XRANDR_CFLAGS)			\
	-DPANEL_MODULES_DIR=\"$(modulesdir)\"	\
	-DGMENU_I_KNOW_THIS_IS_UNSTABLE		\
	-DGNOME_DESKTOP_USE_UNSTABLE_API

gnome_panel_LDADD =		\
	$(top_builddir)/gnome-panel/libpanel-applet-private/libpanel-applet-private.la	\
	$(top_builddir)/gnome-panel/libpanel-util/libpanel-util.la			\
	$(PANEL_LIBS)									\
	$(TELEPATHY_GLIB_LIBS)								\
	$(XRANDR_LIBS)									\
	$(X_LIBS)

gnome_panel_LDFLAGS = -export-dynamic

gnome_desktop_item_edit_SOURCES =	\
	gnome-desktop-item-edit.c	\
	panel-ditem-editor.c		\
	panel-marshal.c			\
	panel-util.c			\
	xstuff.c

gnome_desktop_item_edit_LDADD = 	\
	$(top_builddir)/gnome-panel/libpanel-util/libpanel-util.la	\
	$(PANEL_LIBS)							\
	$(X_LIBS)

panel_test_applets_SOURCES =			\
	$(panel_test_applets_BUILT_SOURCES)	\
	panel-modules.c				\
	panel-applet-info.c			\
	panel-applets-manager.c			\
	panel-test-applets.c

panel_test_applets_CPPFLAGS =			\
	$(AM_CPPFLAGS)				\
	-DPANEL_MODULES_DIR=\"$(modulesdir)\"	\
	-DPANEL_APPLETS_DIR=\"$(appletsdir)\"

panel_test_applets_LDADD = 		\
	$(top_builddir)/gnome-panel/libpanel-applet-private/libpanel-applet-private-mini.la	\
	$(top_builddir)/gnome-panel/libpanel-util/libpanel-util.la				\
	$(PANEL_LIBS)

panel_test_applets_LDFLAGS = -export-dynamic

panel_enum_headers =						\
	$(top_srcdir)/gnome-panel/panel-enums.h			\
	$(top_srcdir)/gnome-panel/panel-enums-gsettings.h	\
	$(top_srcdir)/gnome-panel/panel-types.h

panel-marshal.h: panel-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --header --prefix=panel_marshal > $@

panel-marshal.c: panel-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)echo "#include \"panel-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=panel_marshal >> $@

panel-typebuiltins.c: $(panel_enum_headers)
	$(AM_V_GEN)glib-mkenums \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"panel-typebuiltins.h\"\n\n" \
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
		$(panel_enum_headers) > $@

panel-typebuiltins.h: $(panel_enum_headers)
	$(AM_V_GEN)glib-mkenums \
			--fhead "#ifndef __PANEL_TYPEBUILTINS_H__\n" \
			--fhead "#define __PANEL_TYPEBUILTINS_H__ 1\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __PANEL_TYPEBUILTINS_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define PANEL_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(panel_enum_headers) > $@

gnome_panel_BUILT_SOURCES = \
	panel-typebuiltins.c		\
	panel-typebuiltins.h		\
	panel-resources.c		\
	panel-resources.h		\
	panel-marshal.c			\
	panel-marshal.h

panel_test_applets_BUILT_SOURCES = 	\
	panel-test-applets-resources.c	\
	panel-test-applets-resources.h	\
	panel-marshal.c			\
	panel-marshal.h

BUILT_SOURCES = 				\
	$(gnome_panel_BUILT_SOURCES)		\
	$(panel_test_applets_BUILT_SOURCES)

ui_FILES =					\
	panel-properties-dialog.ui		\
	panel-run-dialog.ui

panel-resources.c: panel.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/panel.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name panel $<

panel-resources.h: panel.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/panel.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name panel $<

uitest_FILES = panel-test-applets.ui

panel-test-applets-resources.c: panel-test-applets.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/panel-test-applets.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name panel $<

panel-test-applets-resources.h: panel-test-applets.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/panel-test-applets.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name panel $<

desktopdir = $(datadir)/applications
desktop_in_files = gnome-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =					\
	$(ui_FILES)				\
	panel.gresource.xml			\
	Adwaita.css \
	HighContrast.css \
	$(uitest_FILES)				\
	panel-test-applets.gresource.xml	\
	panel-marshal.list			\
	wanda_no_sea.png			\
	$(desktop_in_files)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(sys_DATA) \
	$(desktop_DATA)

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
