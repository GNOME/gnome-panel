SUBDIRS = 16x16 22x22 24x24 32x32 48x48 scalable

gsettings_ENUM_NAMESPACE = org.gnome.gnome-panel
gsettings_ENUM_FILES = $(top_srcdir)/gnome-panel/panel-enums-gsettings.h

gsettings_SCHEMAS =					\
	org.gnome.gnome-panel.gschema.xml		\
	org.gnome.gnome-panel.launcher.gschema.xml	\
	org.gnome.gnome-panel.menu-button.gschema.xml	\
	org.gnome.gnome-panel.object.gschema.xml	\
	org.gnome.gnome-panel.toplevel.gschema.xml

@GSETTINGS_RULES@

default_layoutdir   = $(datadir)/gnome-panel
default_layout_DATA = panel-default-layout.layout

EXTRA_DIST =			\
	$(gsettings_SCHEMAS)	\
	$(default_layout_DATA)

CLEANFILES =			\
	$(gsettings__enum_file)	\
	*.gschema.valid

-include $(top_srcdir)/git.mk
