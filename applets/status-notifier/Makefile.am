NULL =

libsn_applet_la_CPPFLAGS = \
	-DGNOMELOCALEDIR=\""$(localedir)"\" \
	-I$(srcdir) \
	-I$(srcdir)/../../libpanel-applet \
	-I$(top_builddir)/libpanel-applet \
	$(NULL)

libsn_applet_la_CFLAGS = \
	$(LIBPANEL_APPLET_CFLAGS) \
	$(STATUS_NOTIFIER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libsn_applet_la_SOURCES = \
	sn-applet.c \
	sn-applet.h \
	sn-button.c \
	sn-button.h \
	$(NULL)

libsn_applet_la_LIBADD = \
	../../libpanel-applet/libpanel-applet.la \
	$(LIBPANEL_APPLET_LIBS) \
	$(STATUS_NOTIFIER_LIBS) \
	$(NULL)

libsn_applet_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

APPLET_LOCATION = $(pkglibdir)/libsn-applet.so

sn_applet_libdir = $(pkglibdir)
sn_applet_lib_LTLIBRARIES = libsn-applet.la

appletdir = $(appletsdir)
applet_in_files = org.gnome.panel.SnApplet.panel-applet.in
applet_DATA = $(applet_in_files:.panel-applet.in=.panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		-e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
		$< > $@

@PANEL_INTLTOOL_PANEL_APPLET_RULE@

EXTRA_DIST = \
	org.gnome.panel.SnApplet.panel-applet.in.in \
	$(NULL)

CLEANFILES = \
	$(applet_DATA) \
	$(applet_DATA).in \
	$(NULL)

-include $(top_srcdir)/git.mk
