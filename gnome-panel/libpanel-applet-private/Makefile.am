noinst_LTLIBRARIES =			\
	libpanel-applet-private.la	\
	libpanel-applet-private-mini.la

AM_CPPFLAGS =							\
	$(PANEL_CFLAGS)						\
	-I.							\
	-I$(srcdir)						\
	-I$(top_srcdir)/gnome-panel				\
	-I$(top_builddir)/gnome-panel				\
	-I$(top_builddir)/gnome-panel/libpanel-applets-private	\
	-I$(top_builddir)/gnome-panel/libpanel-util		\
	-DDATADIR=\""$(datadir)"\"				\
	-DPANEL_APPLETS_DIR=\"$(appletsdir)\"			\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

libpanel_applet_private_la_SOURCES =	\
	panel-applets-manager-dbus.c	\
	panel-applets-manager-dbus.h	\
	panel-applet-container.c	\
	panel-applet-container.h	\
	panel-applet-frame-dbus.c	\
	panel-applet-frame-dbus.h

libpanel_applet_private_mini_la_SOURCES =\
	panel-applet-mini.c		\
	panel-applets-manager-dbus.c	\
	panel-applets-manager-dbus.h	\
	panel-applet-container.c	\
	panel-applet-container.h

-include $(top_srcdir)/git.mk
