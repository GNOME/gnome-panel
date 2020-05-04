NULL =

lib_LTLIBRARIES = libgnome-panel.la

libgnome_panel_la_SOURCES = \
	gp-action-private.h \
	gp-action.c \
	gp-action.h \
	gp-applet-info-private.h \
	gp-applet-info.c \
	gp-applet-info.h \
	gp-applet-private.h \
	gp-applet.c \
	gp-applet.h \
	gp-image-menu-item.c \
	gp-image-menu-item.h \
	gp-initial-setup-dialog-private.h \
	gp-initial-setup-dialog.c \
	gp-initial-setup-dialog.h \
	gp-lockdown.c \
	gp-lockdown.h \
	gp-module-private.h \
	gp-module.c \
	gp-module.h \
	gp-utils.c \
	gp-utils.h \
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
	-version-info $(LT_VERSION_INFO) \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libgnome_panel_la_LIBADD = \
	$(LIBGNOME_PANEL_LIBS) \
	$(NULL)

libgnome_panel_includedir = $(includedir)/gnome-panel/libgnome-panel
libgnome_panel_include_HEADERS = \
	gp-action.h \
	gp-applet.h \
	gp-applet-info.h \
	gp-enum-types.h \
	gp-image-menu-item.h \
	gp-initial-setup-dialog.h \
	gp-lockdown.h \
	gp-module.h \
	gp-utils.h \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/gp-applet.h \
	$(srcdir)/gp-lockdown.h \
	$(NULL)

gp-enum-types.c: gp-enum-types.c.in gp-enum-types.h $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gp-enum-types.c.in $(ENUM_TYPES) > \
		gp-enum-types.c.tmp && mv gp-enum-types.c.tmp gp-enum-types.c

gp-enum-types.h: gp-enum-types.h.in $(ENUM_TYPES) Makefile.am
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
