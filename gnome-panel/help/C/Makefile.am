# it does things aphabetically, we need these dirs there
a_newfigsdir = $(panel_helpdir)/figures/newtest/
a_newfigs_DATA =

a_oldfigsdir = $(panel_helpdir)/figures/old/
a_oldfigs_DATA =

figs = \
	figures/newtest/glob_pref_anim.png	\
	figures/newtest/glob_pref_applet.png	\
	figures/newtest/glob_pref_button.png	\
	figures/newtest/glob_pref_menu.png	\
	figures/newtest/glob_pref_misc.png	\
	figures/newtest/panel_ex.png		\
	figures/newtest/panel_lockbut.png	\
	figures/newtest/panel_logoutbut.png	\
	figures/newtest/panel_menu.png		\
	figures/newtest/panel_props_back.png	\
	figures/newtest/panel_props_edge.png	\
	figures/newtest/panel_swallow.png	\
	figures/old/clock.png			\
	figures/old/drawer.png			\
	figures/old/global-config.png		\
	figures/old/hide-button.png		\
	figures/old/launcher-props.png		\
	figures/old/logout-screen.png		\
	figures/old/mm-button.png		\
	figures/old/thispanel.png



panel_helpdir = $(datadir)/gnome/help/panel/C
panel_help_DATA = \
	index.html	\
	topic.dat	\
	$(figs)

EXTRA_DIST = panel.sgml topic.dat $(figs)

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

