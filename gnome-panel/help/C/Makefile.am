# it does things aphabetically, we need these dirs there
a_newfigsdir = $(panel_helpdir)/figures/newtest/
a_newfigs_DATA =

a_oldfigsdir = $(panel_helpdir)/figures/old/
a_oldfigs_DATA =

figs = \
	figures/create_launcher.png	\
	figures/drawer_open.png		\
	figures/drawer_properties.png	\
	figures/example_menu.png	\
	figures/example_panel.png	\
	figures/glob_pref_anim.png	\
	figures/hide-button.png		\
	figures/launcher_advprops.png	\
	figures/launcher_props.png	\
	figures/logout-screen.png	\
	figures/menu_properties.png	\
	figures/mm-button.png		\
	figures/panel_applets.png	\
	figures/panel_lockbut.png	\
	figures/panel_logoutbut.png	\
	figures/panel_props_back.png	\
	figures/panel_props_edge.png	\
	figures/panel_swallow.png	\
	figures/status_dock.png



panel_helpdir = $(datadir)/gnome/help/panel/C
panel_help_DATA = \
	index.html	\
	topic.dat	\
	$(figs)

EXTRA_DIST = panel.sgml topic.dat index.html $(figs)

all: index.html

#panel.sgml: panel.sgml.header panel.sgml.chapter panel.sgml.footer
#	cd $(srcdir)
#	cp panel.sgml.header panel.sgml
#	cat panel.sgml.chapter >> panel.sgml
#	cat panel.sgml.footer >> panel.sgml

index.html: panel/index.html
	-cp panel/index.html .

# the wierd srcdir trick is because the db2html from the Cygnus RPMs
# cannot handle relative filenames
panel/index.html: $(srcdir)/panel.sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2html $$srcdir/panel.sgml

dist-hook: index.html
	mkdir $(distdir)/panel
	-cp panel/*.html $(distdir)/panel
	-cp panel/*.css $(distdir)/panel

install-data-local: index.html
	-for file in $(srcdir)/panel/*.html $(srcdir)/panel/*.css; do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(panel_helpdir)/$$basefile; \
	done

panel.ps: panel.sgml
	-db2ps $<

panel.rtf: panel.sgml
	-db2rtf $<

