## Process this file with automake to produce Makefile.in.
# CFLAGS += -Wall  \
# 	            -Wpointer-arith  	\
# 	-Wmissing-prototypes -Wmissing-declarations 

## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.3

NULL =

SUBDIRS =

EXTRA_SUBDIRS = config help

INCLUDES = \
	-I.			\
	-I$(srcdir)		\
	-I$(top_srcdir)/intl	\
	-I$(top_builddir)/intl	\
	-I$(top_builddir)/panel	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DGNOME_DATADIR=\""$(datadir)"\" \
	-DGLADEDIR=\""$(datadir)/gnome/panel"\" \
	-DGLOBAL_CONFDIR=\""$(sysconfdir)/gnome/panel-config"\" \
	-I$(includedir)		\
	$(PANEL_CFLAGS) -Werror

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

# try to make these get cleaned properly
CLEANFILES += $(CORBA_SRCLIST)

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
	panel-marshal.c		\
	panel-marshal.h

panel_2_SOURCES=\
	$(CORBA_SRCLIST)	\
	gwmh.h			\
	panel-include.h		\
	panel-types.h		\
	panel-marshal-main.c	\
	main.c			\
	main.h			\
	panel-widget.c  	\
	panel-widget.h		\
	basep-widget.c  	\
	basep-widget.h		\
	button-widget.c  	\
	button-widget.h		\
	multiscreen-stuff.c	\
	multiscreen-stuff.h	\
	xstuff.c		\
	xstuff.h		\
	session.c		\
	session.h		\
	conditional.c		\
	conditional.h		\
	foobar-widget.h		\
	foobar-widget.c		\
	tasklist_icon.c		\
	tasklist_icon.h		\
	border-widget.c		\
	border-widget.h		\
	edge-widget.c		\
	edge-widget.h		\
	aligned-widget.c	\
	aligned-widget.h	\
	drawer-widget.c		\
	drawer-widget.h		\
	sliding-widget.c	\
	sliding-widget.h	\
	floating-widget.c	\
	floating-widget.h	\
	rgb-stuff.h		\
	rgb-stuff.c		\
	tearoffitem.c		\
	tearoffitem.h		\
	title-item.h		\
	title-item.c		\
	title-item-private.h	\
	panel.c			\
	panel.h         	\
	applet.c		\
	applet.h         	\
	logout.c		\
	logout.h		\
	drawer.c  		\
	drawer.h  		\
	panel-util.c		\
	panel-util.h		\
	global-keys.c		\
	global-keys.h		\
	panel_config.c  	\
	panel_config.h		\
	panel_config_global.h  	\
	status.c  		\
	status.h  		\
	distribution.c		\
	distribution.h		\
	menu.c  		\
	menu.h  		\
	menu-util.c		\
	menu-util.h		\
	menu-rh.c  		\
	menu-rh.h  		\
	menu-fentry.c  		\
	menu-fentry.h  		\
	swallow.c  		\
	swallow.h  		\
	menu-properties.c	\
	menu-properties.h	\
	launcher.c		\
	launcher.h		\
	extern.c  		\
	extern.h  		\
	$(NULL)

panel_2_LDADD=\
	$(PANEL_LIBS)		\
	$(top_builddir)/gnome-desktop/libgnome-desktop-2.la

panel_2_LDFLAGS=-export-dynamic

MAINTAINERCLEANFILES = $(CORBA_SRCLIST)

EXTRA_DIST = \
	APPLET_WRITING			\
	panel.server.in			\
	panel.hints			\
	gnome-panel-properties.desktop	\
	nothing.cP			\
	title-item.gob			\
	gnome-panel-screenshot.glade	\
	gnome-panel-screenshot.glade.h

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
