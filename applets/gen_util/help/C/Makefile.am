mailcheck_helpdir = $(datadir)/gnome/help/mailcheck/C
mailcheck_help_DATA = \
	index.html	\
	topic.dat

EXTRA_DIST = mailcheck.sgml topic.dat index.html

all: index.html

index.html: mailcheck/index.html
	-cp mailcheck/index.html .

# the wierd srcdir trick is because the db2html from the Cygnus RPMs
# cannot handle relative filenames
mailcheck/index.html: $(srcdir)/mailcheck.sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2html $$srcdir/mailcheck.sgml

dist-hook: index.html
	mkdir $(distdir)/mailcheck
	-cp mailcheck/*.html $(distdir)/mailcheck
	-cp mailcheck/*.css $(distdir)/mailcheck

install-data-local: index.html
	-for file in $(srcdir)/mailcheck/*.html $(srcdir)/mailcheck/*.css; do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(mailcheck_helpdir)/$$basefile; \
	done

mailcheck.ps: mailcheck.sgml
	-db2ps $<

mailcheck.rtf: mailcheck.sgml
	-db2rtf $<

