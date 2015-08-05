noinst_LTLIBRARIES = libpanel-util.la

AM_CPPFLAGS =							\
	$(PANEL_CFLAGS)						\
	-I.							\
	-I$(srcdir)						\
	-I$(top_builddir)/gnome-panel/libpanel-util		\
	-DDATADIR=\""$(datadir)"\"				\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

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
	panel-color.c			\
	panel-color.h			\
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
	panel-list.c			\
	panel-list.h			\
	panel-screensaver.c		\
	panel-screensaver.h		\
	panel-session-manager.c		\
	panel-session-manager.h		\
	panel-show.c			\
	panel-show.h			\
	panel-xdg.c			\
	panel-xdg.h

EXTRA_DIST = \
	org.freedesktop.login1.Manager.xml

-include $(top_srcdir)/git.mk
