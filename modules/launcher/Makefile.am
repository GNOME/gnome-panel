NULL =

launcher_libdir = $(libdir)/gnome-panel/modules
launcher_lib_LTLIBRARIES = org.gnome.gnome-panel.launcher.la

org_gnome_gnome_panel_launcher_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DGMENU_I_KNOW_THIS_IS_UNSTABLE \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	-DGRESOURCE_PREFIX=\""/org/gnome/gnome-panel/modules/launcher"\" \
	-DG_LOG_DOMAIN=\""launcher"\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

org_gnome_gnome_panel_launcher_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(LAUNCHER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

org_gnome_gnome_panel_launcher_la_SOURCES = \
	gp-custom-launcher-applet.c \
	gp-custom-launcher-applet.h \
	gp-editor.c \
	gp-editor.h \
	gp-icon-name-chooser.c \
	gp-icon-name-chooser.h \
	gp-launcher-applet.c \
	gp-launcher-applet.h \
	gp-launcher-button.c \
	gp-launcher-button.h \
	gp-launcher-module.c \
	gp-launcher-properties.c \
	gp-launcher-properties.h \
	gp-launcher-utils.c \
	gp-launcher-utils.h \
	$(BUILT_SOURCES) \
	$(NULL)

org_gnome_gnome_panel_launcher_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(LAUNCHER_LIBS) \
	$(NULL)

org_gnome_gnome_panel_launcher_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

gsettings_SCHEMAS = \
	org.gnome.gnome-panel.applet.launcher.gschema.xml \
	$(NULL)

@GSETTINGS_RULES@

ui_FILES = \
	custom-launcher-menu.ui \
	gp-icon-name-chooser.ui \
	launcher-menu.ui \
	$(NULL)

launcher_resources = \
	$(shell $(GLIB_COMPILE_RESOURCES) \
		--sourcedir=$(srcdir) \
		--generate-dependencies \
		$(srcdir)/launcher.gresource.xml)

launcher-resources.c: launcher.gresource.xml $(launcher_resources)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) \
		--target=$@ --sourcedir=$(srcdir) \
		--generate --c-name launcher $<

launcher-resources.h: launcher.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) \
		--target=$@ --sourcedir=$(srcdir) \
		--generate --c-name launcher $<

BUILT_SOURCES = \
	launcher-resources.c \
	launcher-resources.h \
	$(NULL)

EXTRA_DIST = \
	launcher.gresource.xml \
	$(gsettings_SCHEMAS) \
	$(ui_FILES) \
	$(NULL)

CLEANFILES = \
	*.gschema.valid \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
