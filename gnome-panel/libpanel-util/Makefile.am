NULL =

noinst_LTLIBRARIES = libpanel-util.la

libpanel_util_la_CFLAGS = \
	-DDATADIR=\""$(datadir)"\" \
	-I$(srcdir) \
	-I$(top_builddir)/gnome-panel/libpanel-util \
	$(PANEL_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

BUILT_SOURCES =			\
	dbus-login1-manager.c \
	dbus-login1-manager.h

dbus-login1-manager.h:
dbus-login1-manager.c: org.freedesktop.login1.Manager.xml Makefile.am
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix=org.freedesktop \
		--generate-c-code dbus-login1-manager \
		$(srcdir)/org.freedesktop.login1.Manager.xml

libpanel_util_la_SOURCES =		\
	$(BUILT_SOURCES)		\
	panel-cleanup.c			\
	panel-cleanup.h			\
	panel-dconf.c			\
	panel-dconf.h			\
	panel-end-session-dialog.c \
	panel-end-session-dialog.h \
	panel-error.c			\
	panel-error.h			\
	panel-glib.c			\
	panel-glib.h			\
	panel-gsettings.c		\
	panel-gsettings.h		\
	panel-gtk.c			\
	panel-gtk.h			\
	panel-icon-chooser.c		\
	panel-icon-chooser.h		\
	panel-keyfile.c			\
	panel-keyfile.h			\
	panel-launch.c			\
	panel-launch.h			\
	panel-screensaver.c		\
	panel-screensaver.h		\
	panel-session-manager.c		\
	panel-session-manager.h		\
	panel-show.c			\
	panel-show.h			\
	panel-xdg.c			\
	panel-xdg.h \
	$(NULL)

libpanel_util_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

EXTRA_DIST = \
	org.freedesktop.login1.Manager.xml \
	$(NULL)

-include $(top_srcdir)/git.mk
