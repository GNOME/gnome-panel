NULL =

noinst_LTLIBRARIES = libtray.la
noinst_PROGRAMS = testtray

libtray_la_CFLAGS = \
	-DGNOMELOCALEDIR=\""$(localedir)"\" \
	-DG_LOG_DOMAIN=\""notification-area-applet"\" \
	-DNOTIFICATION_AREA_MENU_UI_DIR=\""$(uidir)"\" \
	-I$(srcdir) \
	-I$(srcdir)/../../libpanel-applet \
	-I$(top_builddir)/libpanel-applet \
	$(NOTIFICATION_AREA_CFLAGS) \
	$(LIBPANEL_APPLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libtray_la_SOURCES =		\
	fixedtip.h		\
	fixedtip.c		\
	na-tray.c		\
	na-tray.h		\
	na-tray-child.c		\
	na-tray-child.h		\
	na-tray-manager.c	\
	na-tray-manager.h

libnotification_area_applet_la_CFLAGS = $(libtray_la_CFLAGS)

libnotification_area_applet_la_SOURCES = main.c main.h

libnotification_area_applet_la_LIBADD = \
	libtray.la					\
	../../libpanel-applet/libpanel-applet.la	\
	$(X_LIBS)					\
	$(NOTIFICATION_AREA_LIBS)			\
	$(LIBPANEL_APPLET_LIBS)

libnotification_area_applet_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

testtray_SOURCES = testtray.c
testtray_LDADD =			\
	libtray.la			\
	$(X_LIBS)			\
	$(NOTIFICATION_AREA_LIBS)

testtray_CFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/../../libpanel-applet \
	-I$(top_builddir)/libpanel-applet \
	$(NOTIFICATION_AREA_CFLAGS) \
	$(LIBPANEL_APPLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

testtray_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

APPLET_LOCATION   = $(pkglibdir)/$(LIBPANEL_APPLET_API_VERSION)/libnotification-area-applet.so

notification_area_appletlibdir = $(pkglibdir)/$(LIBPANEL_APPLET_API_VERSION)
notification_area_appletlib_LTLIBRARIES = libnotification-area-applet.la

appletdir       = $(appletsdir)
applet_in_files = org.gnome.panel.NotificationAreaApplet.panel-applet.in
applet_DATA     = $(applet_in_files:.panel-applet.in=.panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		-e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
		$< > $@

@PANEL_INTLTOOL_PANEL_APPLET_RULE@

EXTRA_DIST =								\
	org.gnome.panel.NotificationAreaApplet.panel-applet.in.in	\
	$(ui_DATA)

CLEANFILES =			\
	$(applet_DATA) 		\
	$(applet_DATA).in

-include $(top_srcdir)/git.mk
