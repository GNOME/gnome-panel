INCLUDES =							\
	-I$(srcdir)						\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	$(GNOME_INCLUDEDIR)					\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(PANEL_CFLAGS)						\
	$(APPLET_CFLAGS)					\
	-DG_LOG_DOMAIN=\""notification-area-applet"\"		\
	-DPREFIX=\"$(prefix)\"					\
	-DSYSCONFDIR=\"$(sysconfdir)\"				\
	-DDATADIR=\"$(datadir)\"				\
	-DLIBDIR=\"$(libdir)\"

libexec_PROGRAMS = notification-area-applet

notification_area_applet_SOURCES =		\
	fixedtip.h				\
	fixedtip.c				\
	main.c					\
	obox.c					\
	obox.h					\
	na-tray-manager.c			\
	na-tray-manager.h			\
	na-marshal.c				\
	na-marshal.h

notification_area_applet_LDADD =			\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(X_LIBS)					\
	$(PANEL_LIBS)

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_NotificationAreaApplet.server.in
server_DATA     = GNOME_NotificationAreaApplet.server

$(server_in_files): $(server_in_files).in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" \
	    -e "s|\@VERSION\@|$(PACKAGE_VERSION)|" \
	    $< > $@

@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_NotificationAreaApplet.xml

EXTRA_DIST =					\
	GNOME_NotificationAreaApplet.server.in.in	\
	$(ui_DATA)				\
	na-marshal.list

BUILT_SOURCES = na-marshal.c na-marshal.h

CLEANFILES =                                    \
	GNOME_NotificationAreaApplet.server.in        \
	GNOME_NotificationAreaApplet.server

na-marshal.h: na-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_na_marshal > $@

na-marshal.c: na-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"na-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=_na_marshal >> $@
