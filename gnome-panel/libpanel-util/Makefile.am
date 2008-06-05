NULL =

INCLUDES =							\
	-I.							\
	-I$(srcdir)						\
	-I$(top_builddir)/gnome-panel				\
	-I$(top_builddir)/gnome-panel/libpanel-util		\
	-DPANEL_DEBUG						\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGLADEDIR=\""$(gladedir)"\"				\
	-DICONDIR=\""$(datadir)/gnome-panel/pixmaps"\"		\
	-DGMENU_I_KNOW_THIS_IS_UNSTABLE				\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(STANDARD_PROPERTIES_CFLAGS)				\
	$(PANEL_CFLAGS)						\
	$(PANELCONFIG_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(NULL)

noinst_LTLIBRARIES=libpanel-util.la

libpanel_util_la_SOURCES=	\
	panel-cleanup.c			\
	panel-cleanup.h			\
	panel-dbus-service.c		\
	panel-dbus-service.h		\
	panel-keyfile.c			\
	panel-keyfile.h			\
	$(NULL)
