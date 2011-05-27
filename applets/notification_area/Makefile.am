noinst_LTLIBRARIES = libtray.la
noinst_PROGRAMS = testtray

AM_CPPFLAGS =							\
	$(NOTIFICATION_AREA_CFLAGS)				\
	$(LIBPANEL_APPLET_CFLAGS)				\
	-I$(srcdir)						\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"		\
	-DG_LOG_DOMAIN=\""notification-area-applet"\"		\
	-DNOTIFICATION_AREA_MENU_UI_DIR=\""$(uidir)"\"		\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

libtray_la_SOURCES =		\
	fixedtip.h		\
	fixedtip.c		\
	na-marshal.c		\
	na-marshal.h		\
	na-tray.c		\
	na-tray.h		\
	na-tray-child.c		\
	na-tray-child.h		\
	na-tray-manager.c	\
	na-tray-manager.h

NOTIFICATION_AREA_SOURCES = main.c main.h

NOTIFICATION_AREA_LDADD =				\
	libtray.la					\
	../../libpanel-applet/libpanel-applet-4.la	\
	$(X_LIBS)					\
	$(NOTIFICATION_AREA_LIBS)			\
	$(LIBPANEL_APPLET_LIBS)

testtray_SOURCES = testtray.c
testtray_LDADD =			\
	libtray.la			\
	$(X_LIBS)			\
	$(NOTIFICATION_AREA_LIBS)

if NOTIFICATION_AREA_INPROCESS
APPLET_IN_PROCESS = true
APPLET_LOCATION   = $(pkglibdir)/$(LIBPANEL_APPLET_API_VERSION)/libnotification-area-applet.so

notification_area_appletlibdir = $(pkglibdir)/$(LIBPANEL_APPLET_API_VERSION)
notification_area_appletlib_LTLIBRARIES = libnotification-area-applet.la
libnotification_area_applet_la_SOURCES = $(NOTIFICATION_AREA_SOURCES)
libnotification_area_applet_la_LIBADD = $(NOTIFICATION_AREA_LDADD)
libnotification_area_applet_la_LDFLAGS = -module -avoid-version
libnotification_area_applet_la_CFLAGS = $(AM_CFLAGS)
else
APPLET_IN_PROCESS = false
APPLET_LOCATION   = $(libexecdir)/notification-area-applet

libexec_PROGRAMS = notification-area-applet
notification_area_applet_SOURCES = $(NOTIFICATION_AREA_SOURCES)
notification_area_applet_LDADD = $(NOTIFICATION_AREA_LDADD)
notification_area_applet_CFLAGS = $(AM_CFLAGS)
endif

na-marshal.h: na-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --header --prefix=_na_marshal > $@

na-marshal.c: na-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)echo "#include \"na-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=_na_marshal >> $@

BUILT_SOURCES = na-marshal.c na-marshal.h

appletdir       = $(appletsdir)
applet_in_files = org.gnome.panel.NotificationAreaApplet.panel-applet.in
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

EXTRA_DIST =								\
	org.gnome.panel.NotificationAreaApplet.panel-applet.in.in	\
	$(ui_DATA)							\
	$(service_in_files)						\
	na-marshal.list

CLEANFILES =			\
	$(applet_DATA) 		\
	$(applet_DATA).in	\
	$(service_DATA)

-include $(top_srcdir)/git.mk
