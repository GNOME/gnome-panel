SUBDIRS =		\
	clock		\
	fish		\
	notification_area \
	wncklet


# Remove any old gen_util .server files lying around.
gen_util_server = $(libdir)/bonobo/servers/GNOME_GenUtilApplet_Factory.server
install-data-local:
	if test -f $(DESTDIR)$(gen_util_server); then \
		rm -f $(DESTDIR)$(gen_util_server); \
	fi

-include $(top_srcdir)/git.mk
