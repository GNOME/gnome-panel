fish_helpdir = $(datadir)/gnome/help/fish_applet/no
fish_help_DATA = \
	index.html	\
	topic.dat

EXTRA_DIST = fish.sgml topic.dat index.html

all: index.html

index.html: fish/index.html
	-cp fish/index.html .

# the wierd srcdir trick is because the db2html from the Cygnus RPMs
# cannot handle relative filenames
fish/index.html: $(srcdir)/fish.sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2html $$srcdir/fish.sgml

dist-hook: index.html
	mkdir $(distdir)/fish
	-cp fish/*.html $(distdir)/fish
	-cp fish/*.css $(distdir)/fish

install-data-local: index.html
	-for file in $(srcdir)/fish/*.html $(srcdir)/fish/*.css; do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(fish_helpdir)/$$basefile; \
	done

fish.ps: fish.sgml
	-db2ps $<

fish.rtf: fish.sgml
	-db2rtf $<

