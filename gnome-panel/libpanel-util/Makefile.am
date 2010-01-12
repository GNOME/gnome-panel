NULL =

INCLUDES =							\
	-I.							\
	-I$(srcdir)						\
	-I$(top_builddir)/gnome-panel/libpanel-util		\
	-DDATADIR=\""$(datadir)"\"				\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(PANEL_CFLAGS)						\
	$(WARN_CFLAGS)						\
	$(NULL)

noinst_LTLIBRARIES=libpanel-util.la

libpanel_util_la_SOURCES=	\
	panel-cleanup.c			\
	panel-cleanup.h			\
	panel-dbus-service.c		\
	panel-dbus-service.h		\
	panel-error.c			\
	panel-error.h			\
	panel-glib.c			\
	panel-glib.h			\
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
	panel-session-manager.c		\
	panel-session-manager.h		\
	panel-show.c			\
	panel-show.h			\
	panel-xdg.c			\
	panel-xdg.h			\
	$(NULL)

-include $(top_srcdir)/git.mk
