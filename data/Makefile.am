SUBDIRS = 16x16 22x22 24x24 32x32 48x48 scalable

gsettings_applets_schemas_in_in =						\
	org.gnome.gnome-panel.applet.fish.gschema.xml.in.in			\
	org.gnome.gnome-panel.applet.window-list.gschema.xml.in.in		\
	org.gnome.gnome-panel.applet.workspace-switcher.gschema.xml.in.in 	\
	org.gnome.gnome-panel.applet.clock.gschema.xml.in.in

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

sessiondir = $(datadir)/gnome-session/sessions
session_in_files = gnome-flashback.session.desktop.in gnome-flashback-compiz.session.desktop.in
session_DATA = gnome-flashback.session

xsessiondir = $(datadir)/xsessions
xsession_in_in_files = gnome-fallback.desktop.in.in gnome-flashback-compiz.desktop.in.in
xsession_in_files = gnome-fallback.desktop.in gnome-flashback-compiz.desktop.in
xsession_DATA = $(xsession_in_files:.desktop.in=.desktop)

%.session: %.session.desktop.in
	$(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

%.desktop.in: %.desktop.in.in
	$(AM_V_GEN)sed -e 's,[@]libexecdir[@],$(libexecdir),g' <$< >$@

@INTLTOOL_DESKTOP_RULE@

default_layoutdir   = $(datadir)/gnome-panel
default_layout_DATA = panel-default-layout.layout

EXTRA_DIST =				\
	$(gsettings_schemas_in_in)	\
	$(session_in_files)		\
	$(xsession_in_in_files)		\
	$(default_layout_DATA)

CLEANFILES =			\
	$(gsettings__enum_file)	\
	$(gsettings_SCHEMAS_in)	\
	$(gsettings_SCHEMAS)	\
	*.gschema.valid		\
	$(session_DATA)		\
	$(xsession_DATA)	\
	$(xsession_in_files)

-include $(top_srcdir)/git.mk
