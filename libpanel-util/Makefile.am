NULL =

noinst_LTLIBRARIES = libpanel-util.la

libpanel_util_la_CFLAGS = \
	-I$(srcdir) \
	-I$(top_builddir)/libpanel-util \
	$(PANEL_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libpanel_util_la_SOURCES =		\
	panel-dconf.c			\
	panel-dconf.h			\
	panel-error.c			\
	panel-error.h			\
	panel-glib.c			\
	panel-glib.h			\
	panel-gsettings.c		\
	panel-gsettings.h		\
	panel-keyfile.c			\
	panel-keyfile.h			\
	panel-show.c			\
	panel-show.h			\
	panel-xdg.c			\
	panel-xdg.h \
	$(NULL)

libpanel_util_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
