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

gwmh_dir = $(top_srcdir)/applets/desk-guide

bin_PROGRAMS = \
	panel				\
	gnome-panel-properties-capplet	\
	gnome-panel-add-launcher	\
	gnome-panel-screenshot

noinst_PROGRAMS = panel-temp bad-applet

libpanel_appletincludedir = $(includedir)
libpanel_statusincludedir = $(includedir)

lib_LTLIBRARIES = libpanel_applet.la libpanel_status.la

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

libpanel_applet_la_SOURCES = \
	applet-widget.c \
	$(CORBA_SRCLIST)

libpanel_appletinclude_HEADERS = \
	applet-widget.h		\
	gnome-panel.h

libpanel_applet_la_LDFLAGS = -version-info 0:1:0 -rpath $(libdir)
libpanel_applet_la_LIBADD = $(PANEL_LIBS) -lm

libpanel_status_la_SOURCES = \
	status-docklet.c \
	$(CORBA_SRCLIST)

libpanel_statusinclude_HEADERS = \
	status-docklet.h

libpanel_status_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)
libpanel_status_la_LIBADD = $(PANEL_LIBS) -lm

bad_applet_SOURCES = bad-applet.c

bad_applet_LDADD = \
	libpanel_applet.la	\
	$(PANEL_LIBS)		\
	$(INTLLIBS)

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

panel_temp_SOURCES=\
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
	$(NULL)

panel_temp_LDADD=\
	$(PANEL_LIBS)		\
	$(top_builddir)/gnome-desktop/libgnome-desktop-2.la

panel_SOURCES=\
	$(GWMH_SRCLIST)		\
	things-will-not-compile-below-this-line		\
	menu-properties.c	\
	menu-properties.h	\
	launcher.c		\
	launcher.h		\
	extern.c  		\
	extern.h  		\
	swallow.c  		\
	swallow.h  		\
	gnome-run.c		\
	gnome-run.h		\
	scroll-menu.h		\
	scroll-menu.c		\
	icon-entry-hack.c	\
	icon-entry-hack.h

panel_LDADD = \
	libpanel_applet.la	\
	$(PANEL_LIBS)		\
	$(INTLLIBS)		\
	$(XINERAMA_LIBS)

panel_LDFLAGS=-export-dynamic

gnome_panel_properties_capplet_SOURCES = \
	gnome-panel-properties.c	\
	panel-util.c			\
	icon-entry-hack.c		\
	conditional.c			\
	$(CORBA_SRCLIST)

gnome_panel_properties_capplet_LDADD = \
	$(PANEL_LIBS)		\
	$(INTLLIBS)

gnome_panel_add_launcher_SOURCES = \
	gnome-panel-add-launcher.c	\
	$(CORBA_SRCLIST)

gnome_panel_screenshot_SOURCES = \
	gnome-panel-screenshot.c

gnome_panel_add_launcher_LDADD = \
	$(PANEL_LIBS)		\
	$(INTLLIBS)

gnome_panel_screenshot_LDADD = \
	$(PANEL_LIBS)		\
	$(INTLLIBS)

MAINTAINERCLEANFILES = $(CORBA_SRCLIST)

EXTRA_DIST = \
	appletsConf.sh.in		\
	dockletsConf.sh.in		\
	panel.gnorba			\
	panel.soundlist			\
	APPLET_WRITING			\
	panel.hints			\
	gnome-panel-properties.desktop	\
	nothing.cP			\
	title-item.gob			\
	gnome-panel-screenshot.glade	\
	gnome-panel-screenshot.glade.h


panelgnorbadir  = $(sysconfdir)/CORBA/servers
panelgnorba_DATA = panel.gnorba

soundlistdir = $(sysconfdir)/sound/events
soundlist_DATA = panel.soundlist

gladedir = $(datadir)/gnome/panel
glade_DATA =gnome-panel-screenshot.glade

hintsdir = $(datadir)/gnome/hints
hints_DATA = panel.hints

sysdir = $(datadir)/control-center/Desktop/
sys_DATA = gnome-panel-properties.desktop

deskdir = $(datadir)/gnome/apps/Settings/Desktop/
desk_DATA = gnome-panel-properties.desktop

$(srcdir)/panel-util.c: $(CORBA_SRCLIST)
