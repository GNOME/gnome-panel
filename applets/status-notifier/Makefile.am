NULL =

status_notifier_libdir = $(libdir)/gnome-panel/applets
status_notifier_lib_LTLIBRARIES = status-notifier.la

status_notifier_la_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DG_LOG_DOMAIN=\""status-notifier"\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

status_notifier_la_CFLAGS = \
	$(LIBGNOME_PANEL_CFLAGS) \
	$(STATUS_NOTIFIER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

status_notifier_la_SOURCES = \
	sn-applet.c \
	sn-applet.h \
	sn-host.c \
	sn-host.h \
	sn-host-v0.c \
	sn-host-v0.h \
	sn-item.c \
	sn-item.h \
	sn-item-v0.c \
	sn-item-v0.h \
	sn-module.c \
	$(BUILT_SOURCES) \
	$(NULL)

status_notifier_la_LIBADD = \
	$(top_builddir)/libgnome-panel/libgnome-panel.la \
	$(LIBGNOME_PANEL_LIBS) \
	$(STATUS_NOTIFIER_LIBS) \
	$(NULL)

status_notifier_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

sn-dbus-menu-gen.h:
sn-dbus-menu-gen.c: com.canonical.dbusmenu.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Sn \
		--generate-c-code sn-dbus-menu-gen \
		$(srcdir)/com.canonical.dbusmenu.xml

sn-host-v0-gen.h:
sn-host-v0-gen.c: org.kde.StatusNotifierHost.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Sn \
		--generate-c-code sn-host-v0-gen \
		$(srcdir)/org.kde.StatusNotifierHost.xml

sn-item-v0-gen.h:
sn-item-v0-gen.c: org.kde.StatusNotifierItem.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Sn \
		--generate-c-code sn-item-v0-gen \
		$(srcdir)/org.kde.StatusNotifierItem.xml

sn-watcher-v0-gen.h:
sn-watcher-v0-gen.c: org.kde.StatusNotifierWatcher.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Sn \
		--generate-c-code sn-watcher-v0-gen \
		$(srcdir)/org.kde.StatusNotifierWatcher.xml

BUILT_SOURCES = \
	sn-dbus-menu-gen.c \
	sn-dbus-menu-gen.h \
	sn-host-v0-gen.c \
	sn-host-v0-gen.h \
	sn-item-v0-gen.c \
	sn-item-v0-gen.h \
	sn-watcher-v0-gen.c \
	sn-watcher-v0-gen.h \
	$(NULL)

EXTRA_DIST = \
	com.canonical.dbusmenu.xml \
	org.kde.StatusNotifierHost.xml \
	org.kde.StatusNotifierItem.xml \
	org.kde.StatusNotifierWatcher.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
