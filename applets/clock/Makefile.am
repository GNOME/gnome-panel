SUBDIRS = help

INCLUDES = 					\
	-I$(srcdir)/../../libpanel-applet 	\
	-I$(top_builddir)/libpanel-applet 	\
	$(DISABLE_DEPRECATED_CFLAGS)		\
	$(GNOME_INCLUDEDIR) 			\
	$(WARN_CFLAGS) 				\
	$(WNCKLET_CFLAGS) 			\
	-DPAGER_GLADEDIR=\""$(gladedir)"\"	\
	-DTASKLIST_GLADEDIR=\""$(gladedir)"\"	\
	-DGNOME_ICONDIR=\""$(datadir)/pixmaps\""\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 

DISABLE_DEPRECATED_CFLAGS =			\
	-DG_DISABLE_DEPRECATED			\
	-DGDK_DISABLE_DEPRECATED		\
	-DGTK_DISABLE_DEPRECATED		\
	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
	-DGNOME_DISABLE_DEPRECATED

gladedir   = $(datadir)/gen_util
glade_DATA = pager.glade tasklist.glade

libgen_util_applet_2_la_SOURCES = \
	main.c \
	clock.c \
	clock.h	\
	pager.c	\
	pager.h	\
	tasklist.c \
	tasklist.h \
	mailcheck.c \
	mailcheck.h \
	popcheck.c \
	popcheck.h \
	remote-helper.c \
	remote-helper.h

libgen_util_applet_2_la_LDFLAGS = -module -avoid-version
libgen_util_applet_2_la_LIBADD =			\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(WNCKLET_LIBS)

appletdir = $(libdir)
applet_LTLIBRARIES = libgen_util_applet-2.la

GNOME_GenUtilApplet_Factory.server.in: GNOME_GenUtilApplet_Factory.server.in.in
	sed -e "s|\@APPLET_LIBDIR\@|$(appletdir)|" $< > $@

PRINTER_BITMAPS = print.xpm
MAILCHECK_BITMAPS = email.png email-e.png tux-anim.png email-eclouds.png \
                    email-tiny.png eclouds.png

mailcheckbitmapsdir = $(datadir)/pixmaps/mailcheck
mailcheckbitmaps_DATA = $(MAILCHECK_BITMAPS)

soundlistdir = $(sysconfdir)/sound/events
soundlist_in_files = mailcheck.soundlist.in
soundlist_DATA = $(soundlist_in_files:.soundlist.in=.soundlist)

@INTLTOOL_SOUNDLIST_RULE@

schemadir   = $(sysconfdir)/gconf/schemas
schema_DATA = mailcheck.schemas pager.schemas tasklist.schemas clock.schemas

uidir 		= $(datadir)/gnome-2.0/ui
ui_DATA		= \
	GNOME_ClockApplet.xml		\
	GNOME_MailCheckApplet.xml	\
	GNOME_PagerApplet.xml		\
	GNOME_TasklistApplet.xml	
	
serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_GenUtilApplet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
		        GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p ; \
		done \
	fi

EXTRA_DIST = GNOME_GenUtilApplet_Factory.server.in.in	\
	$(PRINTER_BITMAPS)				\
	$(MAILCHECK_BITMAPS)				\
	$(soundlist_in_files)				\
	$(schema_DATA)					\
	$(glade_DATA)					\
	$(ui_DATA)
