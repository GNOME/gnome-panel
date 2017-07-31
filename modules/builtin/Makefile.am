NULL =

SUBDIRS = \
	$(NULL)

noinst_LTLIBRARIES = libbuiltin.la

libbuiltin_la_SOURCES = \
	gp-builtin.c \
	gp-builtin.h \
	$(NULL)

libbuiltin_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"libbuiltin\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libbuiltin_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(BUILTIN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libbuiltin_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libbuiltin_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(BUILTIN_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
