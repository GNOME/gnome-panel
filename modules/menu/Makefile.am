NULL =

menu_libdir = $(libdir)/gnome-panel/modules
menu_lib_LTLIBRARIES = org.gnome.gnome-panel.menu.la

org_gnome_gnome_panel_menu_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DGMENU_I_KNOW_THIS_IS_UNSTABLE \
	-DG_LOG_DOMAIN=\""menu"\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

org_gnome_gnome_panel_menu_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(MENU_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

org_gnome_gnome_panel_menu_la_SOURCES = \
	gp-bookmarks.c \
	gp-bookmarks.h \
	gp-lock-logout.c \
	gp-lock-logout.h \
	gp-main-menu-applet.c \
	gp-main-menu-applet.h \
	gp-menu-bar-applet.c \
	gp-menu-bar-applet.h \
	gp-menu-bar.c \
	gp-menu-bar.h \
	gp-menu-button-applet.c \
	gp-menu-button-applet.h \
	gp-menu-button.c \
	gp-menu-button.h \
	gp-menu-module.c \
	gp-menu-utils.c \
	gp-menu-utils.h \
	gp-menu.c \
	gp-menu.h \
	gp-places-menu.c \
	gp-places-menu.h \
	gp-recent-menu.c \
	gp-recent-menu.h \
	gp-user-menu-applet.c \
	gp-user-menu-applet.h \
	gp-user-menu.c \
	gp-user-menu.h \
	gp-volumes.c \
	gp-volumes.h \
	$(BUILT_SOURCES) \
	$(NULL)

org_gnome_gnome_panel_menu_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(MENU_LIBS) \
	$(NULL)

org_gnome_gnome_panel_menu_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

gsettings_SCHEMAS = \
	org.gnome.gnome-panel.applet.menu-button.gschema.xml \
	$(NULL)

@GSETTINGS_RULES@

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

gp-dm-seat-gen.h:
gp-dm-seat-gen.c: org.freedesktop.DisplayManager.Seat.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gp \
		--generate-c-code gp-dm-seat-gen \
		$(srcdir)/org.freedesktop.DisplayManager.Seat.xml

gp-login1-manager-gen.h:
gp-login1-manager-gen.c: org.freedesktop.login1.Manager.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gp \
		--generate-c-code gp-login1-manager-gen \
		$(srcdir)/org.freedesktop.login1.Manager.xml

gp-screensaver-gen.h:
gp-screensaver-gen.c: org.gnome.ScreenSaver.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gp \
		--generate-c-code gp-screensaver-gen \
		$(srcdir)/org.gnome.ScreenSaver.xml

gp-session-manager-gen.h:
gp-session-manager-gen.c: org.gnome.SessionManager.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gp \
		--generate-c-code gp-session-manager-gen \
		$(srcdir)/org.gnome.SessionManager.xml

BUILT_SOURCES = \
	menu-resources.c \
	menu-resources.h \
	gp-dm-seat-gen.c \
	gp-dm-seat-gen.h \
	gp-login1-manager-gen.c \
	gp-login1-manager-gen.h \
	gp-screensaver-gen.c \
	gp-screensaver-gen.h \
	gp-session-manager-gen.c \
	gp-session-manager-gen.h \
	$(NULL)

EXTRA_DIST = \
	menu.gresource.xml \
	org.freedesktop.DisplayManager.Seat.xml \
	org.freedesktop.login1.Manager.xml \
	org.gnome.ScreenSaver.xml \
	org.gnome.SessionManager.xml \
	$(gsettings_SCHEMAS) \
	$(menu_resources) \
	$(NULL)

CLEANFILES = \
	*.gschema.valid \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
