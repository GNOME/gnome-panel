#SUBDIRS = help

INCLUDES = -I. -I$(srcdir) 				\
	-I$(srcdir)/../../libpanel-applet		\
	-I$(top_builddir)/libpanel-applet		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir)                                 \
	$(GNOME_INCLUDEDIR)				\
	$(WARN_CFLAGS)					\
	$(WNCKLET_CFLAGS)


libgen_util_applet_2_la_SOURCES = \
	main.c		\
	clock.c		\
	clock.h		\
	pager.c		\
	pager.h		\
	tasklist.c	\
	tasklist.h

#	mailcheck.c	\
#	mailcheck.h	\
#	popcheck.c	\
#	popcheck.h	\
#	printer.c	\
#	printer.h	\
#	remote-helper.c	\
#	remote-helper.h \

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

EXTRA_DIST = GNOME_GenUtilApplet_Factory.server.in.in		\
             $(PRINTER_BITMAPS)					\
             $(MAILCHECK_BITMAPS)

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_GenUtilApplet_Factory.server.in
server_DATA     = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

mailcheckbitmapsdir = $(datadir)/pixmaps/mailcheck
mailcheckbitmaps_DATA = $(MAILCHECK_BITMAPS)
