NULL =

noinst_LTLIBRARIES = libseparator.la

libseparator_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DG_LOG_DOMAIN=\""separator"\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libseparator_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(SEPARATOR_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libseparator_la_SOURCES = \
	separator-applet.c \
	separator-applet.h \
	separator-module.c \
	separator-module.h \
	$(NULL)

libseparator_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(SEPARATOR_LIBS) \
	$(NULL)

libseparator_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
