SUBDIRS = help

INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../panel				\
	-I$(top_builddir)/panel				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(GNOME_INCLUDEDIR) $(PIXBUF_CFLAGS) -I$(includedir)

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

EXTRA_DIST = clock_applet.desktop.in	\
             mailcheck_applet.desktop.in	\
             mailcheck.soundlist.in	\
             printer.desktop.in		\
             $(PRINTER_BITMAPS)		\
             $(MAILCHECK_BITMAPS)	\
	     gen_util.gnorba

mailcheckbitmapsdir = $(datadir)/pixmaps/mailcheck
mailcheckbitmaps_DATA = $(MAILCHECK_BITMAPS)

clocksysdir = $(datadir)/applets/Clocks
clocksys_in_files = clock_applet.desktop.in
clocksys_DATA = $(clocksys_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

malchecksysdir = $(datadir)/applets/Network
malchecksys_in_files = mailcheck_applet.desktop.in
malchecksys_DATA = $(malchecksys_in_files:.desktop.in=.desktop)

printersysdir = $(datadir)/applets/Utility
printersys_in_files = printer.desktop.in
printersys_DATA = $(printersys_in_files:.desktop.in=.desktop)

genutilgnorbadir  = $(sysconfdir)/CORBA/servers
genutilgnorba_DATA = gen_util.gnorba

soundlistdir = $(sysconfdir)/sound/events
soundlist_in_files = mailcheck.soundlist.in
soundlist_DATA = $(soundlist_in_files:.soundlist.in=.soundlist)
@XML_I18N_MERGE_SOUNDLIST_RULE@

