SUBDIRS = help

INCLUDES = -I. -I$(srcdir) 			\
	-I$(srcdir)/../../libpanel-applet 	\
	-I$(top_builddir)/libpanel-applet 	\
	-I$(includedir) 			\
	$(GNOME_INCLUDEDIR) 			\
	$(WARN_CFLAGS) 				\
	$(WNCKLET_CFLAGS) 			\
	-DPAGER_GLADEDIR=\""$(gladedir)"\"	\
	-DTASKLIST_GLADEDIR=\""$(gladedir)"\"	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 

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

libgen_util_applet_2_la_LIBADD = \
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

schemadir   = $(sysconfdir)/gconf/schemas
schema_DATA = mailcheck.schemas pager.schemas tasklist.schemas

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
	$(schema_DATA)					\
	$(glade_DATA)
