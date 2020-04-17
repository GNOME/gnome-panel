NULL =

action_button_libdir = $(libdir)/gnome-panel/modules
action_button_lib_LTLIBRARIES = org.gnome.gnome-panel.action-button.la

org_gnome_gnome_panel_action_button_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DG_LOG_DOMAIN=\""action-button"\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

org_gnome_gnome_panel_action_button_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(ACTION_BUTTON_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

org_gnome_gnome_panel_action_button_la_SOURCES = \
	gp-action-button-applet.c \
	gp-action-button-applet.h \
	gp-action-button.c \
	gp-action-button.h \
	gp-action-button-module.c \
	gp-force-quit-applet.c \
	gp-force-quit-applet.h \
	gp-lock-screen-applet.c \
	gp-lock-screen-applet.h \
	gp-logout-applet.c \
	gp-logout-applet.h \
	gp-run-applet.c \
	gp-run-applet.h \
	gp-shutdown-applet.c \
	gp-shutdown-applet.h \
	panel-force-quit.c \
	panel-force-quit.h \
	$(BUILT_SOURCES) \
	$(NULL)

org_gnome_gnome_panel_action_button_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(ACTION_BUTTON_LIBS) \
	$(NULL)

org_gnome_gnome_panel_action_button_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

ui_FILES = \
	gp-lock-screen-menu.ui \
	$(NULL)

action_button_resources = \
	$(shell $(GLIB_COMPILE_RESOURCES) \
		--sourcedir=$(srcdir) \
		--generate-dependencies \
		$(srcdir)/action-button.gresource.xml)

action-button-resources.c: action-button.gresource.xml $(action_button_resources)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) \
		--target=$@ --sourcedir=$(srcdir) \
		--generate --c-name action_button $<

action-button-resources.h: action-button.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) \
		--target=$@ --sourcedir=$(srcdir) \
		--generate --c-name action_button $<

gpab-screensaver-gen.h:
gpab-screensaver-gen.c: org.gnome.ScreenSaver.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gpab \
		--generate-c-code gpab-screensaver-gen \
		$(srcdir)/org.gnome.ScreenSaver.xml

gpab-session-manager-gen.h:
gpab-session-manager-gen.c: org.gnome.SessionManager.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gpab \
		--generate-c-code gpab-session-manager-gen \
		$(srcdir)/org.gnome.SessionManager.xml

BUILT_SOURCES = \
	action-button-resources.c \
	action-button-resources.h \
	gpab-screensaver-gen.c \
	gpab-screensaver-gen.h \
	gpab-session-manager-gen.c \
	gpab-session-manager-gen.h \
	$(NULL)

EXTRA_DIST = \
	action-button.gresource.xml \
	org.gnome.ScreenSaver.xml \
	org.gnome.SessionManager.xml \
	$(ui_FILES) \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
