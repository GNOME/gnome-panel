panel_figsdir = $(panel_helpdir)/figs
panel_figs_DATA =
figs = \
	figs/clock.png		\
	figs/drawer.png		\
	figs/global-config.png	\
	figs/hide-button.png	\
	figs/launcher-props.png	\
	figs/logout-screen.png	\
	figs/mm-button.png	\
	figs/thispanel.png

panel_helpdir = $(datadir)/gnome/help/panel/C
panel_help_DATA = \
	index.html	\
	topic.dat	\
	$(figs)

EXTRA_DIST = panel.sgml topic.dat $(figs)

all: index.html

panel.sgml: panel.sgml.header panel.sgml.chapter panel.sgml.footer
	cd $(srcdir)
	cp panel.sgml.header panel.sgml
	cat panel.sgml.chapter >> panel.sgml
	cat panel.sgml.footer >> panel.sgml

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

