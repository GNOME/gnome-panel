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

-include $(top_srcdir)/git.mk
