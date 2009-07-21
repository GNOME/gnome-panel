INCLUDES =							\
	-I$(srcdir)						\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"		\
	$(GNOME_INCLUDEDIR)					\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(NOTIFICATION_AREA_CFLAGS)				\
	$(LIBPANEL_APPLET_CFLAGS)				\
	-DG_LOG_DOMAIN=\""notification-area-applet"\"		\
	-DPREFIX=\"$(prefix)\"					\
	-DSYSCONFDIR=\"$(sysconfdir)\"				\
	-DDATADIR=\"$(datadir)\"				\
	-DLIBDIR=\"$(libdir)\"

NOTIFICATION_AREA_SOURCES =		\
	fixedtip.h				\
	fixedtip.c				\
	main.c					\
	na-tray.c				\
	na-tray.h				\
	na-tray-child.c				\
	na-tray-child.h				\
	na-tray-manager.c			\
	na-tray-manager.h			\
	na-marshal.c				\
	na-marshal.h

NOTIFICATION_AREA_LDADD =			\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(X_LIBS)					\
	$(NOTIFICATION_AREA_LIBS)			\
	$(LIBPANEL_APPLET_LIBS)

if NOTIFICATION_AREA_INPROCESS
APPLET_TYPE     = shlib
APPLET_LOCATION = $(pkglibdir)/libnotification-area-applet.so

notification_area_appletlibdir = $(pkglibdir)
notification_area_appletlib_LTLIBRARIES = libnotification-area-applet.la
libnotification_area_applet_la_SOURCES = $(NOTIFICATION_AREA_SOURCES)
libnotification_area_applet_la_LIBADD = $(NOTIFICATION_AREA_LDADD)
libnotification_area_applet_la_LDFLAGS = -module -avoid-version
libnotification_area_applet_la_CFLAGS =
else
APPLET_TYPE     = exe
APPLET_LOCATION = $(libexecdir)/notification-area-applet

libexec_PROGRAMS = notification-area-applet
notification_area_applet_SOURCES = $(NOTIFICATION_AREA_SOURCES)
notification_area_applet_LDADD = $(NOTIFICATION_AREA_LDADD)
notification_area_applet_CFLAGS =
endif

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_NotificationAreaApplet.server.in
server_DATA     = GNOME_NotificationAreaApplet.server

$(server_in_files): $(server_in_files).in Makefile
	$(AM_V_GEN)sed \
	    -e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
	    -e "s|\@TYPE\@|$(APPLET_TYPE)|" \
	    -e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
	    $< > $@

@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_NotificationAreaApplet.xml

EXTRA_DIST =					\
	GNOME_NotificationAreaApplet.server.in.in	\
	$(ui_DATA)				\
	na-marshal.list

BUILT_SOURCES = na-marshal.c na-marshal.h

CLEANFILES =                                    \
	GNOME_NotificationAreaApplet.server.in        \
	GNOME_NotificationAreaApplet.server

na-marshal.h: na-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --header --prefix=_na_marshal > $@

na-marshal.c: na-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)echo "#include \"na-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=_na_marshal >> $@

noinst_PROGRAMS = testtray

testtray_SOURCES = \
	fixedtip.h		\
	fixedtip.c		\
	na-marshal.c		\
	na-marshal.h		\
	na-tray.c		\
	na-tray.h		\
	na-tray-child.c		\
	na-tray-child.h		\
	na-tray-manager.c	\
	na-tray-manager.h	\
	testtray.c

testtray_LDADD = \
	$(X_LIBS)					\
	$(PANEL_LIBS)

-include $(top_srcdir)/git.mk
