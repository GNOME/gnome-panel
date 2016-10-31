NULL =

status_notifier_libdir = $(libdir)/gnome-panel/applets
status_notifier_lib_LTLIBRARIES = status-notifier.la

status_notifier_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

status_notifier_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(STATUS_NOTIFIER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

status_notifier_la_SOURCES = \
	sn-applet.c \
	sn-applet.h \
	sn-module.c \
	$(NULL)

status_notifier_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(STATUS_NOTIFIER_LIBS) \
	$(NULL)

status_notifier_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
