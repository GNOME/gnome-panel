INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../panel				\
	-I$(top_builddir)/panel				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR)

bin_PROGRAMS = gen_util_applet

gen_util_applet_SOURCES = \
	main.c		\
	mailcheck.c	\
	mailcheck.h	\
	popcheck.c	\
	popcheck.h	\
	printer.c	\
	printer.h	\
	clock.c		\
	clock.h


gen_util_applet_LDADD = ../../panel/libpanel_applet.la \
		$(GNOME_LIBDIR) $(GNOMEGNORBA_LIBS) $(INTLLIBS) 

PRINTER_BITMAPS = print.xpm
MAILCHECK_BITMAPS = email.png email-e.png tux-anim.png email-eclouds.png

EXTRA_DIST = clock_applet.desktop	\
             mailcheck_applet.desktop	\
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
