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
	-DLIBDIR=\"$(libdir)\"					\
	-DNOTIFICATION_AREA_MENU_UI_DIR=\""$(uidir)"\"

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
APPLET_IN_PROCESS = true
APPLET_LOCATION   = $(pkglibdir)/libnotification-area-applet.so

notification_area_appletlibdir = $(pkglibdir)
notification_area_appletlib_LTLIBRARIES = libnotification-area-applet.la
libnotification_area_applet_la_SOURCES = $(NOTIFICATION_AREA_SOURCES)
libnotification_area_applet_la_LIBADD = $(NOTIFICATION_AREA_LDADD)
libnotification_area_applet_la_LDFLAGS = -module -avoid-version
libnotification_area_applet_la_CFLAGS =
else
APPLET_IN_PROCESS = false
APPLET_LOCATION   = $(libexecdir)/notification-area-applet

libexec_PROGRAMS = notification-area-applet
notification_area_applet_SOURCES = $(NOTIFICATION_AREA_SOURCES)
notification_area_applet_LDADD = $(NOTIFICATION_AREA_LDADD)
notification_area_applet_CFLAGS =
endif

appletdir       = $(datadir)/gnome-panel/applets
applet_in_files = GNOME_NotificationAreaApplet.panel-applet.in
applet_DATA     = $(applet_in_files:.panel-applet.in=.panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed \
            -e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
            -e "s|\@IN_PROCESS\@|$(APPLET_IN_PROCESS)|" \
            -e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
            $< > $@

@PANEL_INTLTOOL_PANEL_APPLET_RULE@

if !NOTIFICATION_AREA_INPROCESS
servicedir       = $(datadir)/dbus-1/services
service_in_files = org.gnome.panel.applet.NotificationAreaAppletFactory.service.in
service_DATA     = $(service_in_files:.service.in=.service)

org.gnome.panel.applet.NotificationAreaAppletFactory.service: $(service_in_files)
	$(AM_V_GEN)sed \
            -e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
            $< > $@
endif

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_NotificationAreaApplet.xml

EXTRA_DIST =					\
	GNOME_NotificationAreaApplet.panel-applet.in.in	\
	$(ui_DATA)				\
	$(service_in_files)			\
	na-marshal.list

BUILT_SOURCES = na-marshal.c na-marshal.h

CLEANFILES =                                    \
	$(applet_DATA) 				\
	$(applet_DATA).in			\
	$(service_DATA)

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
