## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.3

NULL =

INCLUDES =							\
	-I.							\
	-I$(srcdir)						\
	-I$(top_builddir)/gnome-panel				\
	-DPANEL_DEBUG						\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGLADEDIR=\""$(gladedir)"\"				\
	-DICONDIR=\""$(datadir)/gnome/panel/pixmaps"\"		\
	-DGMENU_I_KNOW_THIS_IS_UNSTABLE				\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(STANDARD_PROPERTIES_CFLAGS)				\
	$(PANEL_CFLAGS)						\
	$(PANELCONFIG_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(NULL)

# Used by the GNOME_PROGRAM_STANDARD_PROPERTIES macros
STANDARD_PROPERTIES_CFLAGS = 					\
	-DPREFIX=\""$(prefix)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DLIBDIR=\""$(libdir)"\"				\
	-DDATADIR=\""$(datadir)"\"				\
	$(NULL)

bin_PROGRAMS = \
	gnome-panel 			\
	gnome-desktop-item-edit

$(gnome_panel_OBJECTS): $(BUILT_SOURCES)

CORBA_SRCLIST =			\
	GNOME_Panel-stubs.c	\
	GNOME_Panel-skels.c	\
	GNOME_Panel-common.c	\
	GNOME_Panel.h		\
	$(NULL)

$(CORBA_SRCLIST): $(top_srcdir)/idl/GNOME_Panel.idl $(ORBIT_IDL)
	$(ORBIT_IDL) -I $(BONOBO_IDLDIR) -I $(BONOBO_ACT_IDLDIR) $(top_srcdir)/idl/GNOME_Panel.idl

BUILT_SOURCES = \
	$(CORBA_SRCLIST)	\
	panel-typebuiltins.c	\
	panel-typebuiltins.h	\
	panel-marshal.c		\
	panel-marshal.h

panel_sources =			\
	panel-typebuiltins.c	\
	panel-typebuiltins.h	\
	panel-marshal.c		\
	panel-marshal.h		\
	main.c			\
	panel-widget.c  	\
	button-widget.c  	\
	xstuff.c		\
	panel-session.c		\
	panel-compatibility.c   \
	panel.c			\
	applet.c		\
	drawer.c  		\
	panel-config-global.c  	\
	panel-util.c		\
	panel-gconf.c		\
	panel-properties-dialog.c \
	panel-run-dialog.c	\
	menu.c  		\
	panel-context-menu.c 	\
	launcher.c		\
	panel-applet-frame.c	\
	panel-shell.c		\
	panel-background.c	\
	panel-background-monitor.c \
	panel-stock-icons.c	\
	panel-action-button.c	\
	panel-menu-bar.c        \
	panel-menu-button.c     \
	panel-menu-items.c      \
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
	panel-profile.c         \
	panel-force-quit.c	\
	panel-lockdown.c	\
	panel-addto.c		\
	panel-logout.c		\
	panel-gdm.c		\
	panel-power-manager.c   \
	panel-ditem-editor.c	\
	$(NULL)

panel_headers =			\
	panel-types.h		\
	panel-widget.h		\
	panel-globals.h		\
	button-widget.h		\
	xstuff.h		\
	panel-session.h		\
	panel-compatibility.h   \
	panel.h         	\
	applet.h         	\
	drawer.h  		\
	panel-util.h		\
	panel-properties-dialog.h \
	panel-config-global.h  	\
	panel-gconf.h		\
	panel-run-dialog.h	\
	menu.h  		\
	panel-context-menu.h 	\
	launcher.h		\
	panel-applet-frame.h	\
	panel-shell.h		\
	panel-background.h	\
	panel-background-monitor.h \
	panel-stock-icons.h	\
	panel-action-button.h	\
	panel-menu-bar.h        \
	panel-menu-button.h     \
	panel-menu-items.h      \
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
	panel-profile.h         \
	panel-enums.h		\
	panel-force-quit.h	\
	panel-lockdown.h	\
	panel-addto.h		\
	panel-logout.h		\
	panel-gdm.h		\
	panel-power-manager.h	\
	panel-ditem-editor.h	\
	$(NULL)

gnome_panel_SOURCES =			\
	$(CORBA_SRCLIST)		\
	$(panel_sources)		\
	$(panel_headers)		\
	$(NULL)

gnome_panel_LDADD =		\
	$(PANEL_LIBS)		\
	$(X_LIBS)

gnome_panel_LDFLAGS = -export-dynamic

gnome_desktop_item_edit_SOURCES = \
	gnome-desktop-item-edit.c \
	panel-ditem-editor.c	  \
	panel-marshal.c		  \
	panel-util.c		  \
	xstuff.c

gnome_desktop_item_edit_LDFLAGS = -export-dynamic

gnome_desktop_item_edit_LDADD = 	\
	$(PANEL_LIBS)			\
	$(X_LIBS)

gladedir = $(datadir)/gnome/panel/glade
glade_DATA =					\
	panel-properties-dialog.glade		\
	panel-run-dialog.glade

EXTRA_DIST =					\
	GNOME_Panel.server.in			\
	$(glade_DATA)				\
	nothing.cP				\
	nothing.h				\
	$(schemas_in_files)			\
	panel-compatibility.schemas		\
	$(ui_DATA)				\
	panel-marshal.list			\
	gnome-panelrc				\
	$(entries_DATA)

@INTLTOOL_DESKTOP_RULE@

panel_enum_headers =					\
	$(top_srcdir)/gnome-panel/panel-enums.h		\
	$(top_srcdir)/gnome-panel/panel-types.h		\
	GNOME_Panel.h

panel-marshal.h: panel-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=panel_marshal > $@

panel-marshal.c: panel-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"panel-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=panel_marshal >> $@

panel-typebuiltins.c: @REBUILD@ $(panel_enum_headers)
	glib-mkenums    --fhead "#include <glib-object.h>\n" \
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

panel-typebuiltins.h: @REBUILD@ $(panel_enum_headers)
	glib-mkenums    --fhead "#ifndef __PANEL_TYPEBUILTINS_H__\n" \
			--fhead "#define __PANEL_TYPEBUILTINS_H__ 1\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __PANEL_TYPEBUILTINS_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define PANEL_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(panel_enum_headers) >  $@

uidir   = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_Panel_Popup.xml

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_Panel.server.in
server_DATA     = $(server_in_files:.server.in=.server)

rcdir = $(datadir)
rc_DATA = gnome-panelrc

@INTLTOOL_SERVER_RULE@

entriesdir       = $(GCONF_SCHEMA_FILE_DIR)
entries_DATA     = panel-default-setup.entries

schemasdir       = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files =				\
	panel-global.schemas.in 		\
	panel-general.schemas.in		\
	panel-toplevel.schemas.in		\
	panel-object.schemas.in			\
	$(NULL)
schemas_DATA     = panel-compatibility.schemas $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/gnome-panel/$$p ; \
		done ; \
		$(GCONFTOOL) --direct --config-source=$(GCONF_SCHEMA_CONFIG_SOURCE) --load $(srcdir)/panel-default-setup.entries ; \
		$(GCONFTOOL) --direct --config-source=$(GCONF_SCHEMA_CONFIG_SOURCE) --load $(srcdir)/panel-default-setup.entries /apps/panel ; \
	fi
else
install-data-local:
endif

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(schemas_in_files:.schemas.in=.schemas) \
	$(server_DATA) \
	$(sys_DATA)

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)
