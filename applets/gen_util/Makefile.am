SUBDIRS = help

INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../panel				\
	-I$(top_builddir)/panel				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR)

lib_LTLIBRARIES = libgen_util_applet.la

libgen_util_applet_la_SOURCES = \
	main.c		\
	mailcheck.c	\
	mailcheck.h	\
	popcheck.c	\
	popcheck.h	\
	printer.c	\
	printer.h	\
	clock.c		\
	clock.h		\
	remote-helper.c	\
	remote-helper.h

PRINTER_BITMAPS = print.xpm
MAILCHECK_BITMAPS = email.png email-e.png tux-anim.png email-eclouds.png \
                    email-tiny.png eclouds.png

EXTRA_DIST = clock_applet.desktop	\
             mailcheck_applet.desktop	\
             mailcheck.soundlist	\
             printer.desktop		\
             $(PRINTER_BITMAPS)		\
             $(MAILCHECK_BITMAPS)	\
	     gen_util.gnorba

mailcheckbitmapsdir = $(datadir)/pixmaps/mailcheck
mailcheckbitmaps_DATA = $(MAILCHECK_BITMAPS)

clocksysdir = $(datadir)/applets/Clocks
clocksys_DATA = clock_applet.desktop

malchecksysdir = $(datadir)/applets/Network
malchecksys_DATA = mailcheck_applet.desktop

printersysdir = $(datadir)/applets/Utility
printersys_DATA = printer.desktop

genutilgnorbadir  = $(sysconfdir)/CORBA/servers
genutilgnorba_DATA = gen_util.gnorba

soundlistdir = $(sysconfdir)/sound/events
soundlist_DATA = mailcheck.soundlist

