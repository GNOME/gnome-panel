## Process this file with automake to produce Makefile.in.
# CFLAGS += -Wall  \
# 	            -Wpointer-arith  	\
# 	-Wmissing-prototypes -Wmissing-declarations 

## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.3

SUBDIRS = config doc help

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
	$(GNOME_INCLUDEDIR)	\
	$(ORB_CFLAGS)		\
	$(GNOME_PRINT_CFLAGS)	\
	$(GLADE_CFLAGS)		\
	$(PIXBUF_CFLAGS)

gwmh_dir = $(top_srcdir)/applets/desk-guide

bin_PROGRAMS = \
	panel				\
	gnome-panel-properties-capplet	\
	gnome-panel-add-launcher	\
	gnome-panel-screenshot

noinst_PROGRAMS = bad-applet

libpanel_appletincludedir = $(includedir)
libpanel_statusincludedir = $(includedir)

lib_LTLIBRARIES = libpanel_applet.la libpanel_status.la

CORBA_SRCLIST = \
	gnome-panel-stubs.c	\
	gnome-panel-skels.c	\
	gnome-panel-common.c	\
	gnome-panel.h

# try to make these get cleaned properly
CLEANFILES += $(CORBA_SRCLIST) my_gnome_panel_idl

$(CORBA_SRCLIST): my_gnome_panel_idl

my_gnome_panel_idl: $(top_srcdir)/idl/gnome-panel.idl $(ORBIT_IDL)
	if $(ORBIT_IDL) `@GNOME_CONFIG@ --cflags idl` $< ; then touch my_gnome_panel_idl; else /bin/false; fi

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
libpanel_applet_la_LIBADD = $(ORB_LIBS) -lm $(DL_LIB)

libpanel_status_la_SOURCES = \
	status-docklet.c \
	$(CORBA_SRCLIST)

libpanel_statusinclude_HEADERS = \
	status-docklet.h

libpanel_status_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)
libpanel_status_la_LIBADD = $(ORB_LIBS) -lm $(DL_LIB)

bad_applet_SOURCES = bad-applet.c

bad_applet_LDADD = \
	libpanel_applet.la	\
	$(GNOME_LIBDIR) 	\
	$(GNORBA_LIBS)		\
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

BUILT_SOURCES = $(CORBA_SRCLIST) $(GWMH_SRCLIST)

panel_SOURCES=\
	$(GWMH_SRCLIST)		\
	panel-include.h		\
	panel-types.h		\
	main.c			\
	main.h			\
	applet.c		\
	applet.h         	\
	panel.c			\
	panel.h         	\
	session.c		\
	session.h		\
	panel_config.c  	\
	panel_config.h		\
	panel_config_global.h  	\
	distribution.c		\
	distribution.h		\
	menu.c  		\
	menu.h  		\
	menu-fentry.c  		\
	menu-fentry.h  		\
	menu-rh.c  		\
	menu-rh.h  		\
	menu-properties.c	\
	menu-properties.h	\
	menu-util.c		\
	menu-util.h		\
	launcher.c		\
	launcher.h		\
	logout.c		\
	logout.h		\
	extern.c  		\
	extern.h  		\
	status.c  		\
	status.h  		\
	swallow.c  		\
	swallow.h  		\
	drawer.c  		\
	drawer.h  		\
	panel-widget.c  	\
	panel-widget.h		\
	basep-widget.c  	\
	basep-widget.h		\
	border-widget.c		\
	border-widget.h		\
	edge-widget.c		\
	edge-widget.h		\
	aligned-widget.c	\
	aligned-widget.h	\
	sliding-widget.c	\
	sliding-widget.h	\
	floating-widget.c	\
	floating-widget.h	\
	drawer-widget.c		\
	drawer-widget.h		\
	button-widget.c  	\
	button-widget.h		\
	panel-util.c		\
	panel-util.h		\
	tearoffitem.c		\
	tearoffitem.h		\
	rgb-stuff.h		\
	rgb-stuff.c		\
	global-keys.c		\
	global-keys.h		\
	gnome-run.c		\
	gnome-run.h		\
	xstuff.c		\
	xstuff.h		\
	foobar-widget.h		\
	foobar-widget.c		\
	tasklist_icon.c		\
	tasklist_icon.h		\
	title-item.h		\
	title-item.c		\
	title-item-private.h	\
	scroll-menu.h		\
	scroll-menu.c		\
	icon-entry-hack.c	\
	icon-entry-hack.h	\
	multiscreen-stuff.c	\
	multiscreen-stuff.h	\
	conditional.c		\
	conditional.h

man_MANS = panel.1			\
	gnome-panel-add-launcher.1	\
	gnome-panel-properties-capplet.1
	
panel_LDADD = \
	libpanel_applet.la	\
	$(GNOME_LIBDIR)		\
	$(GNORBA_LIBS)		\
	$(ORB_LIBS)		\
	$(GNOMEUI_LIBS)		\
	$(INTLLIBS)		\
	$(PIXBUF_LIBS)		\
	$(XINERAMA_LIBS)

panel_LDFLAGS=-export-dynamic

gnome_panel_properties_capplet_SOURCES = \
	gnome-panel-properties.c	\
	panel-util.c			\
	icon-entry-hack.c		\
	conditional.c			\
	$(CORBA_SRCLIST)

gnome_panel_properties_capplet_LDADD = \
	$(GNOME_LIBDIR)		\
	$(GNOME_CAPPLET_LIBS)	\
	$(INTLLIBS)		\
	$(PIXBUF_LIBS)

gnome_panel_add_launcher_SOURCES = \
	gnome-panel-add-launcher.c	\
	$(CORBA_SRCLIST)

gnome_panel_screenshot_SOURCES = \
	gnome-panel-screenshot.c

gnome_panel_add_launcher_LDADD = \
	$(GNOME_LIBDIR)		\
	$(GNOMEUI_LIBS)		\
	$(GNORBA_LIBS)		\
	$(ORB_LIBS)		\
	$(INTLLIBS)		\
	$(PIXBUF_LIBS)

gnome_panel_screenshot_LDADD = \
	$(GNOME_LIBDIR)		\
	$(GNOMEUI_LIBS)		\
	$(GLADE_LIBS)		\
	$(GNOME_PRINT_LIBS)	\
	$(INTLLIBS)		\
	$(PNG_LIBS)		\
	$(PIXBUF_LIBS)

MAINTAINERCLEANFILES = $(CORBA_SRCLIST)

EXTRA_DIST = \
	appletsConf.sh.in		\
	dockletsConf.sh.in		\
	panel.gnorba			\
	panel.soundlist.in		\
	APPLET_WRITING			\
	panel.hints			\
	gnome-panel-properties.desktop.in	\
	nothing.cP			\
	title-item.gob			\
	gnome-panel-screenshot.glade	\
	gnome-panel-screenshot.glade.h	\
	$(man_MANS)

## Put `exec' in the name because this should be installed by
## `install-exec', not `install-data'.
confexecdir = $(libdir)
confexec_DATA = appletsConf.sh dockletsConf.sh

#
# Create the appletsConf.sh file from the .sh.in file
#
appletsConf.sh: appletsConf.sh.in Makefile
	sed -e 's?\@GNOME_LIBDIR\@?$(GNOME_LIBDIR)?g' \
	    -e 's?\@GNOME_INCLUDEDIR\@?$(GNOME_INCLUDEDIR)?g' \
	    -e 's?\@GNOMEGNORBA_LIBS\@?$(GNOMEGNORBA_LIBS)?g' \
	    -e 's?\@VERSION\@?$(VERSION)?g' \
	    < $(srcdir)/appletsConf.sh.in > appletsConf.tmp \
	  && mv appletsConf.tmp appletsConf.sh

#
# Create the dockletsConf.sh file from the .sh.in file
#
dockletsConf.sh: dockletsConf.sh.in Makefile
	sed -e 's?\@GNOME_LIBDIR\@?$(GNOME_LIBDIR)?g' \
	    -e 's?\@GNOME_INCLUDEDIR\@?$(GNOME_INCLUDEDIR)?g' \
	    -e 's?\@GNOMEGNORBA_LIBS\@?$(GNOMEGNORBA_LIBS)?g' \
	    -e 's?\@VERSION\@?$(VERSION)?g' \
	    < $(srcdir)/dockletsConf.sh.in > dockletsConf.tmp \
	  && mv dockletsConf.tmp dockletsConf.sh

panelgnorbadir  = $(sysconfdir)/CORBA/servers
panelgnorba_DATA = panel.gnorba

soundlistdir = $(sysconfdir)/sound/events
soundlist_in_files = panel.soundlist.in
soundlist_DATA = $(soundlist_in_files:.soundlist.in=.soundlist)
@XML_I18N_MERGE_SOUNDLIST_RULE@

gladedir = $(datadir)/gnome/panel
glade_DATA =gnome-panel-screenshot.glade

hintsdir = $(datadir)/gnome/hints
hints_DATA = panel.hints

sysdir = $(datadir)/control-center/Desktop/
sys_in_files = gnome-panel-properties.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

deskdir = $(datadir)/gnome/apps/Settings/Desktop/
desk_in_files = gnome-panel-properties.desktop.in
desk_DATA = $(desk_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

$(srcdir)/panel-util.c: $(CORBA_SRCLIST)
