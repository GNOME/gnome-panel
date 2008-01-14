schemasdir      = $(GCONF_SCHEMA_FILE_DIR)
schemas_DATA_in = intlclock.schemas.in
schemas_DATA    = $(schemas_DATA_in:.schemas.in=.schemas)

serverdir      = $(libdir)/bonobo/servers
server_DATA_in = GNOME_IntlClockApplet_Factory.server.in
server_DATA    = $(server_DATA_in:.server.in=.server)

uidir 	= $(datadir)/gnome-2.0/ui
ui_DATA	= GNOME_IntlClockApplet.xml	

pkgdata_DATA = intlclock.glade

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$$p ; \
		done \
	fi
else
install-data-local:
endif

@INTLTOOL_SERVER_RULE@

$(server_DATA_in): $(server_DATA_in:.server.in=.server.in.in)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

CLEANFILES =						\
	GNOME_IntlClockApplet_Factory.server		\
	GNOME_IntlClockApplet_Factory.server.in		\
	$(schemas_DATA)					\
	$(server_DATA)					\
	$(dbus_services_DATA)

EXTRA_DIST =						\
	GNOME_IntlClockApplet_Factory.server.in.in	\
	$(pkgdata_DATA)					\
	$(schemas_DATA_in)				\
	$(ui_DATA)					\
	$(dbus_services_DATA_in)			\
	$(dbus_conf_DATA)				\
	$(polkit_DATA)					\
	gnome-clock-applet-mechanism.xml

MAINTAINERCLEANFILES = Makefile.in
