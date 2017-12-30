NULL =

noinst_LTLIBRARIES = \
	libpanel-applet-private.la \
	$(NULL)

libpanel_applet_private_la_CFLAGS = \
	-DMODULESDIR=\""$(libdir)/gnome-panel/modules"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DPANEL_APPLETS_DIR=\"$(appletsdir)\" \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/gnome-panel \
	-I$(top_builddir)/gnome-panel \
	-I$(top_builddir)/gnome-panel/libpanel-applet-private \
	-I$(top_builddir)/gnome-panel/libpanel-util \
	$(PANEL_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libpanel_applet_private_la_SOURCES =	\
	gp-applet-frame.c \
	gp-applet-frame.h \
	gp-applet-manager.c \
	gp-applet-manager.h \
	panel-applets-manager-dbus.c	\
	panel-applets-manager-dbus.h	\
	panel-applet-container.c	\
	panel-applet-container.h	\
	panel-applet-frame-dbus.c	\
	panel-applet-frame-dbus.h \
	$(NULL)

libpanel_applet_private_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(NULL)

libpanel_applet_private_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
