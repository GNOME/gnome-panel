SUBDIRS = 16x16 22x22 24x24 32x32 48x48 scalable

gsettings_applets_schemas_in_in =						\
	org.gnome.gnome-panel.applet.fish.gschema.xml.in.in			\
	org.gnome.gnome-panel.applet.window-list.gschema.xml.in.in		\
	org.gnome.gnome-panel.applet.workspace-switcher.gschema.xml.in.in

gsettings_panel_schemas_in_in =					\
	org.gnome.gnome-panel.gschema.xml.in.in			\
	org.gnome.gnome-panel.launcher.gschema.xml.in.in	\
	org.gnome.gnome-panel.menu-button.gschema.xml.in.in	\
	org.gnome.gnome-panel.object.gschema.xml.in.in		\
	org.gnome.gnome-panel.toplevel.gschema.xml.in.in

gsettings_schemas_in_in =			\
	$(gsettings_panel_schemas_in_in)	\
	$(gsettings_applets_schemas_in_in)

@INTLTOOL_XML_NOMERGE_RULE@

gsettings_ENUM_NAMESPACE = org.gnome.gnome-panel
gsettings_ENUM_FILES = $(top_srcdir)/gnome-panel/panel-enums-gsettings.h

gsettings_schemas_in = $(gsettings_schemas_in_in:.xml.in.in=.xml.in)
gsettings_SCHEMAS = $(gsettings_schemas_in:.xml.in=.xml)

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@datadir\@^$(datadir)^g' -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

@GSETTINGS_RULES@

default_layoutdir   = $(datadir)/gnome-panel
default_layout_DATA = panel-default-layout.layout

EXTRA_DIST =				\
	$(gsettings_schemas_in_in)	\
	$(default_layout_DATA)

CLEANFILES =			\
	$(gsettings__enum_file)	\
	$(gsettings_SCHEMAS_in)	\
	$(gsettings_SCHEMAS)	\
	*.gschema.valid

-include $(top_srcdir)/git.mk
