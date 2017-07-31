NULL =

separator_libdir = $(libdir)/gnome-panel/modules
separator_lib_LTLIBRARIES = separator.la

separator_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DG_LOG_DOMAIN=\""separator"\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

separator_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(SEPARATOR_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

separator_la_SOURCES = \
	separator-applet.c \
	separator-applet.h \
	separator-module.c \
	$(NULL)

separator_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(SEPARATOR_LIBS) \
	$(NULL)

separator_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
