NULL =

wncklet_libdir = $(libdir)/gnome-panel/modules
wncklet_lib_LTLIBRARIES = org.gnome.gnome-panel.wncklet.la

org_gnome_gnome_panel_wncklet_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

org_gnome_gnome_panel_wncklet_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(WNCKLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

org_gnome_gnome_panel_wncklet_la_SOURCES = \
	wncklet.c \
	wncklet.h \
	wncklet-module.c \
	window-menu.c \
	window-menu.h \
	window-list.c \
	window-list.h \
	workspace-switcher.c \
	workspace-switcher.h \
	showdesktop.c \
	showdesktop.h \
	$(BUILT_SOURCES) \
	$(NULL)

org_gnome_gnome_panel_wncklet_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(WNCKLET_LIBS) \
	$(NULL)

org_gnome_gnome_panel_wncklet_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

ui_FILES = \
	window-list-menu.ui \
	window-list.ui \
	workspace-switcher-menu.ui \
	workspace-switcher.ui \
	$(NULL)

wncklet-resources.c: wncklet.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/wncklet.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name wncklet $<

wncklet-resources.h: wncklet.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/wncklet.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name wncklet $<

BUILT_SOURCES = \
	wncklet-resources.c \
	wncklet-resources.h \
	$(NULL)

EXTRA_DIST = \
	wncklet.gresource.xml \
	$(ui_FILES) \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
