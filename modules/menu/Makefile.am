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
	gp-menu-module.c \
	gp-menu.c \
	gp-menu.h \
	gp-user-menu-applet.c \
	gp-user-menu-applet.h \
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

-include $(top_srcdir)/git.mk
