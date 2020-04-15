NULL =

SUBDIRS = \
	16x16 22x22 24x24 32x32 48x48 scalable \
	pkgconfig \
	theme \
	$(NULL)

gsettings_ENUM_NAMESPACE = org.gnome.gnome-panel
gsettings_ENUM_FILES = $(top_srcdir)/gnome-panel/panel-enums-gsettings.h

gsettings_SCHEMAS = \
	org.gnome.gnome-panel.applet.fish.gschema.xml \
	org.gnome.gnome-panel.applet.window-list.gschema.xml \
	org.gnome.gnome-panel.applet.workspace-switcher.gschema.xml \
	org.gnome.gnome-panel.applet.clock.gschema.xml \
	org.gnome.gnome-panel.applet.initial-settings.gschema.xml \
	org.gnome.gnome-panel.gschema.xml \
	org.gnome.gnome-panel.object.gschema.xml \
	org.gnome.gnome-panel.toplevel.gschema.xml \
	$(NULL)

@GSETTINGS_RULES@

default_layoutdir   = $(datadir)/gnome-panel/layouts
default_layout_DATA = default.layout

EXTRA_DIST = \
	$(gsettings_SCHEMAS) \
	$(default_layout_DATA) \
	$(NULL)

CLEANFILES = \
	*.gschema.valid \
	$(gsettings__enum_file) \
	$(NULL)

-include $(top_srcdir)/git.mk
