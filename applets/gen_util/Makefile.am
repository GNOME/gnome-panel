INCLUDES = -I. -I$(srcdir) 				\
	-I$(srcdir)/../../panel				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR)

BUILT_SOURCES = dummy.cc

dummy.cc:
	@touch dummy.cc

bin_PROGRAMS = gen_util_applet

gen_util_applet_SOURCES = \
	main.c		\
	mailcheck.c	\
	printer.c	\
	clock.c

MICO_VERS = 2.0.5
MICO_LIBS = -lmicocoss$(MICO_VERS) -lmicoaux$(MICO_VERS) -lmico$(MICO_VERS)

gen_util_applet_LDADD = ../../panel/libpanel_applet.la \
		     $(GNOME_LIBDIR) $(MICO_LIBS) -lXt \
		     $(GNOMEUI_LIBS) $(INTLLIBS) 

gen_util_applet_LINK = $(CXXLINK)

PRINTER_BITMAPS = print.xpm
MAILCHECK_BITMAPS = email.xpm email-e.xpm

EXTRA_DIST = clock_applet.desktop	\
             mailcheck_applet.desktop	\
             printer.desktop		\
             $(PRINTER_BITMAPS)		\
             $(MAILCHECK_BITMAPS)

mailcheckbitmapsdir = $(datadir)/pixmaps/mailcheck
mailcheckbitmaps_DATA = $(MAILCHECK_BITMAPS)

clocksysdir = $(datadir)/applets/Utility
clocksys_DATA = clock_applet.desktop

malchecksysdir = $(datadir)/applets/Network
malchecksys_DATA = mailcheck_applet.desktop

printersysdir = $(datadir)/applets/Utility
printersys_DATA = printer.desktop
