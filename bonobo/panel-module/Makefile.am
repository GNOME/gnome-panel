panelmodule_LTLIBRARIES = libpanel-applets-bonobo.la
panelmoduledir = $(modulesdir)

libpanel_applets_bonobo_la_SOURCES =	\
	$(CORBA_SRCLIST)		\
	panel-applets-bonobo-module.c	\
	panel-applets-manager-bonobo.c	\
	panel-applets-manager-bonobo.h	\
	panel-applet-frame-bonobo.c	\
	panel-applet-frame-bonobo.h

libpanel_applets_bonobo_la_CPPFLAGS =	\
	$(BONOBO_CFLAGS)		\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gnome-panel	\
	-I$(top_builddir)		\
	-DDATADIR=\""$(datadir)"\"

libpanel_applets_bonobo_la_LDFLAGS =				\
	-export_dynamic -avoid-version -module -no-undefined	\
	-export-symbols-regex '^g_io_module_(load|unload|query)'

libpanel_applets_bonobo_la_LIBADD =	\
	$(BONOBO_LIBS)

CORBA_SRCLIST =			\
	GNOME_Panel-stubs.c	\
	GNOME_Panel-skels.c	\
	GNOME_Panel-common.c	\
	GNOME_Panel.h

$(CORBA_SRCLIST): $(top_srcdir)/bonobo/idl/GNOME_Panel.idl $(ORBIT_IDL)
	$(AM_V_GEN)$(ORBIT_IDL) -I $(BONOBO_IDLDIR) -I $(BONOBO_ACT_IDLDIR) $(top_srcdir)/bonobo/idl/GNOME_Panel.idl

xmluidir   = $(datadir)/gnome-2.0/ui
xmlui_DATA = GNOME_Panel_Popup.xml

install-data-hook:
	if test -z "$(DESTDIR)" -a "$(GIO_QUERYMODULES)" != "no" ; then	\
		$(GIO_QUERYMODULES) $(modulesdir) ;			\
	fi
uninstall-local:
	if test "$(GIO_QUERYMODULES)" != "no" ; then	\
		$(GIO_QUERYMODULES) $(modulesdir) ;	\
	fi

EXTRA_DIST = $(xmlui_DATA)

CLEANFILES = $(CORBA_SRCLIST)

-include $(top_srcdir)/git.mk
