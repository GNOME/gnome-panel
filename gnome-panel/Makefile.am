## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.3

NULL =

INCLUDES =							\
	-I.							\
	-I$(srcdir)						\
	-I$(top_builddir)/gnome-panel				\
	-DPANEL_DEBUG						\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGLADEDIR=\""$(datadir)/gnome/panel"\"			\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(STANDARD_PROPERTIES_CFLAGS)				\
	$(ALLOW_CRACKPOT_MENU_CODE)				\
	$(PANEL_CFLAGS)						\
	$(PANELCONFIG_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(NULL)

# FIXME:
# this define is to allow menu.c to continue
# using deprecated gtk+ internals
ALLOW_CRACKPOT_MENU_CODE=-DGTK_MENU_INTERNALS

# Flags to make sure we're not using any deprecated functions
DISABLE_DEPRECATED_CFLAGS = 					\
	-DG_DISABLE_DEPRECATED					\
	-DGDK_DISABLE_DEPRECATED				\
	-DGTK_DISABLE_DEPRECATED				\
	-DGDK_PIXBUF_DISABLE_DEPRECATED				\
	-DGNOME_DISABLE_DEPRECATED				\
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
	gnome-panel-preferences		\
	gnome-panel-screenshot		\
	gnome-desktop-item-edit

$(gnome_panel_OBJECTS) $(gnome_panel_properties_OBJECTS) $(gnome_panel_screenshot_OBJECTS): $(BUILT_SOURCES)

CORBA_SRCLIST =			\
	GNOME_Panel-stubs.c	\
	GNOME_Panel-skels.c	\
	GNOME_Panel-common.c	\
	GNOME_Panel.h		\
	$(NULL)

$(CORBA_SRCLIST): $(top_srcdir)/idl/GNOME_Panel.idl $(ORBIT_IDL)
	$(ORBIT_IDL) -I $(BONOBO_IDLDIR) -I $(BONOBO_ACT_IDLDIR) $(top_srcdir)/idl/GNOME_Panel.idl

EGG_RECENT_FILES_C =		\
	egg-recent-model.c	\
	egg-recent-item.c	\
	egg-recent-view.c	\
	egg-recent-view-gtk.c	\
	egg-recent-util.c

EGG_RECENT_FILES_H =		\
	egg-recent-model.h	\
	egg-recent-item.h	\
	egg-recent-view.h	\
	egg-recent-view-gtk.h	\
	egg-recent-util.h

EGG_SCREEN_EXEC_FILES =         \
	egg-screen-exec.h       \
	egg-screen-exec.c       \
	egg-screen-help.h       \
	egg-screen-help.c       \
	egg-screen-url.h        \
	egg-screen-url.c        \
	$(NULL)

EGG_ACCELERATORS_FILES =        \
	eggaccelerators.c       \
	eggaccelerators.h       \
	$(NULL)

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
	basep-widget.c  	\
	button-widget.c  	\
	multiscreen-stuff.c	\
	xstuff.c		\
	session.c		\
	foobar-widget.c		\
	border-widget.c		\
	edge-widget.c		\
	aligned-widget.c	\
	drawer-widget.c		\
	sliding-widget.c	\
	floating-widget.c	\
	disclosure-widget.c	\
	panel.c			\
	applet.c		\
	drawer.c  		\
	panel-config-global.c  	\
	panel-util.c		\
	panel-gconf.c		\
	global-keys.c		\
	panel-config.c  	\
	distribution.c		\
	gnome-run.c  		\
	menu.c  		\
	menu-util.c		\
	menu-ditem.c		\
	menu-fentry.c  		\
	launcher.c		\
	panel-applet-frame.c	\
	panel-shell.c		\
	quick-desktop-reader.c	\
	panel-access.c		\
	panel-background.c	\
	panel-background-monitor.c \
	panel-stock-icons.c	\
	panel-action-button.c	\
	panel-recent.c		\
	panel-gdk-pixbuf-extensions.c \
	$(EGG_RECENT_FILES_C)	\
	$(NULL)

panel_headers =			\
	panel-types.h		\
	panel-widget.h		\
	panel-main.h		\
	basep-widget.h		\
	button-widget.h		\
	multiscreen-stuff.h	\
	xstuff.h		\
	session.h		\
	foobar-widget.h		\
	border-widget.h		\
	edge-widget.h		\
	aligned-widget.h	\
	drawer-widget.h		\
	sliding-widget.h	\
	floating-widget.h	\
	disclosure-widget.h	\
	panel.h         	\
	applet.h         	\
	drawer.h  		\
	panel-util.h		\
	global-keys.h		\
	panel-config.h		\
	panel-config-global.h  	\
	panel-gconf.h		\
	distribution.h		\
	gnome-run.h  		\
	menu.h  		\
	menu-util.h		\
	menu-ditem.h		\
	menu-fentry.h  		\
	launcher.h		\
	panel-applet-frame.h	\
	quick-desktop-reader.h	\
	panel-shell.h		\
	panel-background.h	\
	panel-background-monitor.h \
	panel-stock-icons.h	\
	panel-action-button.h	\
	panel-recent.h		\
	panel-gdk-pixbuf-extensions.h \
	$(EGG_RECENT_FILES_H)	\
	$(NULL)

gnome_panel_SOURCES =			\
	$(CORBA_SRCLIST)		\
	$(EGG_SCREEN_EXEC_FILES)	\
	$(EGG_ACCELERATORS_FILES)	\
	$(panel_sources)		\
	$(panel_headers)		\
	$(NULL)

gnome_panel_LDADD =		\
	$(X_LIBS)		\
	$(PANEL_LIBS)		\
	$(XINERAMA_LIBS)	\
	-lpopt

gnome_panel_LDFLAGS = -export-dynamic

gnome_panel_preferences_SOURCES =	\
	gnome-panel-preferences.c	\
	panel-gconf.c			\
	panel-access.c

gnome_panel_preferences_LDADD =		\
	$(X_LIBS)			\
	$(PANELCONFIG_LIBS)

gnome_panel_preferences_LDFLAGS =	\
	-export-dynamic

gnome_desktop_item_edit_SOURCES = \
	gnome-desktop-item-edit.c \
	menu-ditem.c		  \
	panel-util.c		  \
	panel-access.c		  \
	egg-screen-exec.c	  \
	egg-screen-help.c	  \
	egg-screen-url.c

gnome_desktop_item_edit_LDFLAGS = -export-dynamic

gnome_desktop_item_edit_LDADD = 	\
	$(X_LIBS)			\
	$(PANEL_LIBS)			\
	-lpopt

gnome_panel_screenshot_SOURCES = \
	gnome-panel-screenshot.c

gnome_panel_screenshot_LDFLAGS = -export-dynamic

gnome_panel_screenshot_LDADD = 	\
	$(XSHAPE_LIBS)		\
	$(X_LIBS)		\
	$(PANELCONFIG_LIBS)	\
	$(PNG_LIBS)

gladedir = $(datadir)/gnome/panel
glade_DATA =					\
	gnome-panel-preferences.glade		\
	gnome-panel-screenshot.glade

EXTRA_DIST =					\
	GNOME_Panel.server.in			\
	gnome-panel-preferences.desktop.in	\
	$(glade_DATA)				\
	nothing.cP				\
	nothing.h				\
	$(schemas_in_files)			\
	$(ui_DATA)				\
	panel-marshal.list			\
	update-from-egg.sh			\
	gnome-panelrc

sysdir = $(datadir)/control-center-2.0/capplets
sys_in_files = gnome-panel-preferences.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

panel_enum_headers =					\
	$(top_srcdir)/gnome-panel/panel-types.h		\
	$(top_srcdir)/gnome-panel/panel-widget.h	\
	$(top_srcdir)/gnome-panel/basep-widget.h	\
	$(top_srcdir)/gnome-panel/aligned-widget.h	\
	$(top_srcdir)/gnome-panel/sliding-widget.h	\
	$(top_srcdir)/gnome-panel/panel-action-button.h	\
	$(top_builddir)/gnome-panel/GNOME_Panel.h

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
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
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

schemasdir       = $(sysconfdir)/gconf/schemas
schemas_in_files =				\
	panel-global-config.schemas.in		\
	panel-per-panel-config.schemas.in	\
	gnome-panel-screenshot.schemas.in	\
	$(NULL)
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/gnome-panel/$$p ; \
		done \
	fi

CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)

EGG_SCREEN_EXEC_DIR = $(srcdir)/../../libegg/libegg/screen-exec
EGG_ACCELERATORS_DIR = $(srcdir)/../../libegg/libegg/treeviewutils
EGG_RECENT_DIR = $(srcdir)/../../libegg/libegg/recent-files

regenerate-built-sources:
	EGGFILES="$(EGG_SCREEN_EXEC_FILES)"  EGGDIR="$(EGG_SCREEN_EXEC_DIR)"  $(srcdir)/update-from-egg.sh
	EGGFILES="$(EGG_ACCELERATORS_FILES)" EGGDIR="$(EGG_ACCELERATORS_DIR)" $(srcdir)/update-from-egg.sh
	EGGFILES="$(EGG_RECENT_FILES_C)" EGGDIR="$(EGG_RECENT_DIR)" $(srcdir)/update-from-egg.sh
	EGGFILES="$(EGG_RECENT_FILES_H)" EGGDIR="$(EGG_RECENT_DIR)" $(srcdir)/update-from-egg.sh
