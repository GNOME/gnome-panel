NULL =

notification_area_libdir = $(libdir)/gnome-panel/modules
notification_area_lib_LTLIBRARIES = org.gnome.gnome-panel.notification-area.la

org_gnome_gnome_panel_notification_area_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DG_LOG_DOMAIN=\""notification-area"\" \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

org_gnome_gnome_panel_notification_area_la_CFLAGS = \
	$(NOTIFICATION_AREA_CFLAGS) \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

org_gnome_gnome_panel_notification_area_la_SOURCES = \
	fixedtip.h \
	fixedtip.c \
	na-applet.c \
	na-applet.h \
	na-module.c \
	na-tray.c \
	na-tray.h \
	na-tray-child.c \
	na-tray-child.h \
	na-tray-manager.c \
	na-tray-manager.h \
	$(NULL)

org_gnome_gnome_panel_notification_area_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(NOTIFICATION_AREA_LIBS) \
	$(LIBGNOME_PANEL_LIBS) \
	$(NULL)

org_gnome_gnome_panel_notification_area_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
