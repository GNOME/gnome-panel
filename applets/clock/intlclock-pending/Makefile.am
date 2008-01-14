## We put the libpanel-applet dir here for when we build the applet
## inside the gnome-panel source tree
INCLUDES =							\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	$(INTLCLOCK_CFLAGS)					\
	-DGNOMELOCALEDIR=\"$(prefix)/$(DATADIRNAME)/locale\"	\
	-DINTLCLOCK_TEXTDOMAIN=\"gnome-panel-2.0\"		\
	-DINTLCLOCK_ICONDIR=\"$(pkgdatadir)\"			\
	-DINTLCLOCK_DATADIR=\"$(pkgdatadir)\"

if HAVE_LIBECAL
CALENDAR_SOURCES =		\
	calendar-client.c	\
	calendar-client.h	\
	calendar-sources.c	\
	calendar-sources.h	\
	calendar-debug.h
endif

noinst_PROGRAMS  = intlclock-standalone set-timezone-demo
libexec_PROGRAMS = intlclock-applet gnome-clock-applet-mechanism

COMMON_SOURCES =				\
	$(CALENDAR_SOURCES)			\
	double-click-detector.c			\
	double-click-detector.h			\
	intlclock.c				\
	intlclock.h				\
	intlclock-applet.c			\
	intlclock-applet.h			\
	intlclock-country.c			\
	intlclock-country.h			\
	intlclock-events-popup.c		\
	intlclock-events-popup.h		\
	intlclock-face.c			\
	intlclock-face.h			\
	intlclock-location.c			\
	intlclock-location.h			\
	intlclock-location-tile.c		\
	intlclock-location-tile.h		\
	intlclock-marshallers.c			\
	intlclock-marshallers.h			\
	intlclock-map.c				\
	intlclock-map.h				\
	intlclock-sunpos.c			\
	intlclock-sunpos.h			\
	intlclock-ui.c				\
	intlclock-ui.h				\
	intlclock-zoneinfo.c			\
	intlclock-zoneinfo.h			\
	intlclock-zonetable.c			\
	intlclock-zonetable.h			\
	set-timezone.c				\
	set-timezone.h				\
	gweather-xml.c				\
	gweather-xml.h


intlclock_applet_SOURCES =			\
	$(COMMON_SOURCES)			\
	main.c

intlclock_standalone_SOURCES =			\
	$(COMMON_SOURCES)			\
	standalone.c

intlclock_applet_LDADD = 				\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(INTLCLOCK_LIBS)				\
	-lgweather

intlclock_standalone_LDADD =				\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(INTLCLOCK_LIBS)				\
	-lgweather

set_timezone_demo_SOURCES =	\
	set-timezone-demo.c

set_timezone_demo_INCLUDES =	\
	$(SET_TIMEZONE_DEMO_CFLAGS)

set_timezone_demo_LDADD =	\
	$(SET_TIMEZONE_DEMO_LIBS)

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in
