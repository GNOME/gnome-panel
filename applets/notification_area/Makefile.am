INCLUDES =							\
	-I$(srcdir)						\
	-I$(srcdir)/../../libpanel-applet			\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGNOME_ICONDIR=\""$(datadir)/pixmaps\""		\
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

EGGFILES=			\
	eggtraymanager.h	\
	eggtraymanager.c

notification_area_applet_SOURCES =		\
	fixedtip.h				\
	fixedtip.c				\
	main.c					\
	obox.c					\
	obox.h					\
	$(EGGFILES)				\
	eggmarshalers.h				\
	eggmarshalers.c

notification_area_applet_LDADD =			\
	../../libpanel-applet/libpanel-applet-2.la	\
	$(X_LIBS)					\
	$(PANEL_LIBS)

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_NotificationAreaApplet.server.in
server_DATA     = GNOME_NotificationAreaApplet.server

GNOME_NotificationAreaApplet.server.in: $(srcdir)/GNOME_NotificationAreaApplet.server.in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_NotificationAreaApplet.xml

EXTRA_DIST =					\
	GNOME_NotificationAreaApplet.server.in.in	\
	$(ui_DATA)				\
	update-from-egg.sh			\
	eggmarshalers.list

EGGDIR=$(srcdir)/../../../libegg/libegg/tray

BUILT_SOURCES = eggmarshalers.c eggmarshalers.h

CLEANFILES =                                    \
	GNOME_NotificationAreaApplet.server.in        \
	GNOME_NotificationAreaApplet.server

regenerate-built-sources:
	EGGFILES="$(EGGFILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh

eggmarshalers.h: eggmarshalers.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_egg_marshal > $@

eggmarshalers.c: eggmarshalers.list $(GLIB_GENMARSHAL)
	echo "#include \"eggmarshalers.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=_egg_marshal >> $@
