## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.3

NULL =

SUBDIRS =

EXTRA_SUBDIRS = config help

INCLUDES = \
	-I.			\
	-I$(srcdir)		\
	-I$(top_srcdir)/gnome-desktop	\
	-I$(top_srcdir)/intl	\
	-I$(top_builddir)/intl	\
	-I$(top_builddir)/panel	\
	-DGNOME_CORE_INTERNAL	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DGNOME_DATADIR=\""$(datadir)"\" \
	-DGLADEDIR=\""$(datadir)/gnome/panel"\" \
	-DGLOBAL_CONFDIR=\""$(sysconfdir)/gnome/panel-config"\" \
	-I$(includedir)		\
	$(PANEL_CFLAGS) 

bin_PROGRAMS = \
	panel-2

gwmh_dir = $(top_srcdir)/applets/desk-guide

# libpanel_appletincludedir = $(includedir)
# libpanel_statusincludedir = $(includedir)
# 
# lib_LTLIBRARIES = libpanel_applet.la libpanel_status.la

CORBA_SRCLIST = \
	gnome-panel-stubs.c	\
	gnome-panel-skels.c	\
	gnome-panel-common.c	\
	gnome-panel.h

$(CORBA_SRCLIST): $(top_srcdir)/idl/gnome-panel.idl $(ORBIT_IDL)
	$(ORBIT_IDL) -I $(BONOBO_IDLDIR) $(top_srcdir)/idl/gnome-panel.idl

panel-marshal.h: panel-marshal.list
	cd $(srcdir) \
	&& glib-genmarshal --prefix=panel_marshal panel-marshal.list --header >> xgen-gmh \
	&& (cmp -s xgen-gmh panel-marshal.h || cp xgen-gmh panel-marshal.h)

panel-marshal.c: panel-marshal.h panel-marshal.list
	cd $(srcdir) \
	&& glib-genmarshal --prefix=panel_marshal panel-marshal.list --body >> xgen-gmc \
	&& cp xgen-gmc panel-marshal.c \
	&& rm -f xgen-gmc xgen-gmc~

# We need explicit dependencies here for people building out
# of the tarballs.

extern.c: gnome-panel.h

# libpanel_applet_la_SOURCES = \
# 	applet-widget.c \
# 	$(CORBA_SRCLIST)
# 
# libpanel_appletinclude_HEADERS = \
# 	applet-widget.h		\
# 	gnome-panel.h
# 
# libpanel_applet_la_LDFLAGS = -version-info 0:1:0 -rpath $(libdir)
# libpanel_applet_la_LIBADD = $(PANEL_LIBS) -lm
# 
# libpanel_status_la_SOURCES = \
# 	status-docklet.c \
# 	$(CORBA_SRCLIST)
# 
# libpanel_statusinclude_HEADERS = \
# 	status-docklet.h
# 
# libpanel_status_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)
# libpanel_status_la_LIBADD = $(PANEL_LIBS) -lm

GWMH_SRCLIST = \
	gwmh.c		\
	gwmh.h		\
	gstc.c		\
	gstc.h

$(GWMH_SRCLIST):
	for file in $(GWMH_SRCLIST); do \
		echo $$file ; \
		ln -sf $(gwmh_dir)/$$file . ; \
	done

BUILT_SOURCES = \
	$(CORBA_SRCLIST)	\
	$(GWMH_SRCLIST)		\
	panel-typebuiltins.c	\
	panel-typebuiltins.h	\
	panel-marshal.c		\
	panel-marshal.h

panel_sources = \
	panel-typebuiltins.c	\
	panel-typebuiltins.h	\
	panel-marshal-main.c	\
	main.c			\
	panel-widget.c  	\
	basep-widget.c  	\
	button-widget.c  	\
	multiscreen-stuff.c	\
	xstuff.c		\
	session.c		\
	conditional.c		\
	foobar-widget.c		\
	tasklist_icon.c		\
	border-widget.c		\
	edge-widget.c		\
	aligned-widget.c	\
	drawer-widget.c		\
	sliding-widget.c	\
	floating-widget.c	\
	rgb-stuff.c		\
	tearoffitem.c		\
	title-item.c		\
	panel.c			\
	applet.c		\
	logout.c		\
	drawer.c  		\
	panel-util.c		\
	global-keys.c		\
	panel_config.c  	\
	status.c  		\
	distribution.c		\
	gnome-run.c  		\
	menu.c  		\
	menu-util.c		\
	menu-rh.c  		\
	menu-fentry.c  		\
	swallow.c  		\
	menu-properties.c	\
	launcher.c		\
	extern.c  		\
	$(NULL)

panel_headers = \
	panel-types.h		\
	main.h			\
	panel-widget.h		\
	basep-widget.h		\
	button-widget.h		\
	multiscreen-stuff.h	\
	xstuff.h		\
	session.h		\
	conditional.h		\
	foobar-widget.h		\
	tasklist_icon.h		\
	border-widget.h		\
	edge-widget.h		\
	aligned-widget.h	\
	drawer-widget.h		\
	sliding-widget.h	\
	floating-widget.h	\
	rgb-stuff.h		\
	tearoffitem.h		\
	title-item.h		\
	title-item-private.h	\
	panel.h         	\
	applet.h         	\
	logout.h		\
	drawer.h  		\
	panel-util.h		\
	global-keys.h		\
	panel_config.h		\
	panel_config_global.h  	\
	status.h  		\
	distribution.h		\
	gnome-run.h  		\
	menu.h  		\
	menu-util.h		\
	menu-rh.h  		\
	menu-fentry.h  		\
	swallow.h  		\
	menu-properties.h	\
	launcher.h		\
	extern.h  		\
	$(NULL)

panel_2_SOURCES = \
	$(CORBA_SRCLIST)	\
	gwmh.h			\
	$(panel_sources)	\
	$(panel_headers)	\
	$(NULL)

panel_2_LDADD=\
	$(PANEL_LIBS)		\
	$(XINERAMA_LIBS)	\
	$(top_builddir)/gnome-desktop/libgnome-desktop-2.la

panel_2_LDFLAGS=-export-dynamic

EXTRA_DIST = \
	APPLET_WRITING			\
	panel.server.in			\
	panel.hints			\
	gnome-panel-properties.desktop	\
	nothing.cP			\
	title-item.gob			\
	gnome-panel-screenshot.glade	\
	gnome-panel-screenshot.glade.h	\
	libpanelapplet-2.0.pc.in

SERVER_FILES = 						\
	panel.server

serverdir = $(datadir)/bonobo-activation
server_DATA = $(SERVER_FILES)

panel.server : $(srcdir)/panel.server.in $(top_builddir)/config.status
	sed -e "s|\@bindir\@|$(bindir)|" \
	$(srcdir)/panel.server.in > panel.server 


gladedir = $(datadir)/gnome/panel
glade_DATA =gnome-panel-screenshot.glade

hintsdir = $(datadir)/gnome/hints
hints_DATA = panel.hints

sysdir = $(datadir)/control-center/Desktop/
sys_DATA = gnome-panel-properties.desktop

deskdir = $(datadir)/gnome/apps/Settings/Desktop/
desk_DATA = gnome-panel-properties.desktop

$(srcdir)/panel-util.c: $(CORBA_SRCLIST)

panel_enum_headers =      \
	panel-types.h     \
	panel-widget.h    \
	basep-widget.h    \
	aligned-widget.h  \
	sliding-widget.h

$(srcdir)/panel-typebuiltins.c: @REBUILD@ Makefile.am $(panel_enum_headers)
	cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#include <gobject/genums.h>\n" \
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
		$(panel_enum_headers) > xgen-gtbec \
	&& cp xgen-gtbec $(@F) \
	&& rm -f xgen-gtbec

$(srcdir)/panel-typebuiltins.h: @REBUILD@ Makefile.am $(panel_enum_headers)
	cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#ifndef __PANEL_TYPEBUILTINS_H__\n" \
			--fhead "#define __PANEL_TYPEBUILTINS_H__ 1\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __PANEL_TYPEBUILTINS_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define PANEL_TYPE_@ENUMNAME@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(panel_enum_headers) > xgen-gth \
	&& cp xgen-gth $(@F) \
	&& rm -f xgen-gth

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libpanelapplet-2.0.pc

CLEANFILES    = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)
