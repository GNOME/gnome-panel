SUBDIRS = figures

# it does things aphabetically, we need these dirs there

panel_helpdir = $(datadir)/gnome/help/panel/C
panel_help_DATA = \
	index.html	\
	topic.dat

EXTRA_DIST = panel.sgml $(panel_help_DATA)

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
	mkdir -p $(distdir)/panel/stylesheet-images
	-cp panel/*.html $(distdir)/panel
	-cp panel/*.css $(distdir)/panel
	-cp panel/stylesheet-images/*.gif $(distdir)/panel/stylesheet-images

install-data-local: index.html
	-for file in $(srcdir)/panel/*.html $(srcdir)/panel/*.css; do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(panel_helpdir)/$$basefile; \
	done
	-mkdir -p $(DESTDIR)$(panel_helpdir)/stylesheet-images
	-for file in $(srcdir)/panel/stylesheet-images/*.gif; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(panel_helpdir)/stylesheet-images/$$basefile; \
	done

panel.ps: panel.sgml
	-db2ps $<

panel.rtf: panel.sgml
	-db2rtf $<

