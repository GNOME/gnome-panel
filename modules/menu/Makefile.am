NULL =

menu_libdir = $(libdir)/gnome-panel/modules
menu_lib_LTLIBRARIES = menu.la

menu_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DGMENU_I_KNOW_THIS_IS_UNSTABLE \
	-DG_LOG_DOMAIN=\""menu"\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

menu_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(MENU_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

menu_la_SOURCES = \
	gp-main-menu-applet.c \
	gp-main-menu-applet.h \
	gp-menu-bar-applet.c \
	gp-menu-bar-applet.h \
	gp-menu-bar.c \
	gp-menu-bar.h \
	gp-menu-module.c \
	gp-menu-utils.c \
	gp-menu-utils.h \
	gp-menu.c \
	gp-menu.h \
	gp-user-menu-applet.c \
	gp-user-menu-applet.h \
	$(BUILT_SOURCES) \
	$(NULL)

menu_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(MENU_LIBS) \
	$(NULL)

menu_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

menu_resources := $(shell $(GLIB_COMPILE_RESOURCES) \
	--sourcedir=$(srcdir) --generate-dependencies \
	$(srcdir)/menu.gresource.xml)

menu-resources.c: $(srcdir)/menu.gresource.xml $(menu_resources)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ \
		--sourcedir=$(srcdir) --generate-source --c-name menu \
		$(srcdir)/menu.gresource.xml

menu-resources.h: $(srcdir)/menu.gresource.xml $(menu_resources)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ \
		--sourcedir=$(srcdir) --generate-header --c-name menu \
		$(srcdir)/menu.gresource.xml

BUILT_SOURCES = \
	menu-resources.c \
	menu-resources.h \
	$(NULL)

EXTRA_DIST = \
	menu.gresource.xml \
	$(menu_resources) \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
