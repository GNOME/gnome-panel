NULL =

lib_LTLIBRARIES = libgnome-panel.la

libgnome_panel_la_SOURCES = \
	gp-applet.c \
	gp-applet.h \
	gp-applet-info.c \
	gp-applet-info.h \
	gp-applet-info-private.h \
	gp-applet-private.h \
	gp-module.h \
	gp-module-info.c \
	gp-module-info.h \
	gp-module-info-private.h \
	$(BUILT_SOURCES) \
	$(NULL)

libgnome_panel_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"libgnome-panel\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libgnome_panel_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libgnome_panel_la_LDFLAGS = \
	-version-info $(LIBGNOME_PANEL_LT_VERSION) \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libgnome_panel_la_LIBADD = \
	$(LIBGNOME_PANEL_LIBS) \
	$(NULL)

libgnome_panel_includedir = $(includedir)/gnome-panel/libgnome-panel
libgnome_panel_include_HEADERS = \
	gp-applet.h \
	gp-applet-info.h \
	gp-enum-types.h \
	gp-module.h \
	gp-module-info.h \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/gp-applet.h \
	$(NULL)

gp-enum-types.c: gp-enum-types.c.in gp-enum-types.h $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gp-enum-types.c.in $(ENUM_TYPES) > \
		gp-enum-types.c.tmp && mv gp-enum-types.c.tmp gp-enum-types.c

gp-enum-types.h: gp-enum-types.h.in $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gp-enum-types.h.in $(ENUM_TYPES) > \
		gp-enum-types.h.tmp && mv gp-enum-types.h.tmp gp-enum-types.h

BUILT_SOURCES = \
	gp-enum-types.c \
	gp-enum-types.h \
	$(NULL)

EXTRA_DIST = \
	gp-enum-types.c.in \
	gp-enum-types.h.in \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk