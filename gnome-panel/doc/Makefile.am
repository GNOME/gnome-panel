## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=panel

# The top-level SGML file.
DOC_MAIN_SGML_FILE=panel.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(top_srcdir)/panel

CFLAGS="`gnome-config --cflags gnomeui applets docklets`"
LDFLAGS="`gnome-config --libs gnomeui applets docklets`"

HTML_DIR=$(datadir)/gnome/html

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

tmpl_sources =

panel_docdir = $(HTML_DIR)
panel_doc_DATA = \
	panel.sgml		\
	panel.html		\
	panel.hierarchy		\
	panel.signals		\
	panel.types		\
	panel-decl.txt		\
	panel-sections.txt

EXTRA_DIST = $(panel_doc_DATA)

if ENABLE_GTK_DOC
panel.html: html/index.html
	-cp $(srcdir)/html/index.html $(srcdir)/panel.html
else
panel.html:
endif

html/index.html: sgml/panel-doc.bottom
	$(MAKE) html

sgml/panel-doc.bottom: $(tmpl_sources)
	$(MAKE) sgml

scan:
	-(cd $(srcdir) \
	&& env CFLAGS=$(CFLAGS) LDFLAGS=$(LDFLAGS) \
		 gtkdoc-scanobj --module=$(DOC_MODULE) \
	&& gtkdoc-scan --module=$(DOC_MODULE) --source-dir=.. \
		--ignore-headers="aligned-widget.h   menu-rh.h	\
			menu-util.h	   sliding-widget.h	\
			applet.h           menu.h		\
			basep-widget.h     panel-include.h	\
			border-widget.h    panel-types.h	\
			button-widget.h    panel-util.h		\
			drawer-widget.h    panel-widget.h	\
			drawer.h           panel.h		\
			edge-widget.h      panel_config.h	\
			extern.h           panel_config_global.h \
			floating-widget.h  rgb-stuff.h		\
			global-keys.h      session.h		\
			gnome-run.h        menu-properties.h	\
			launcher.h         status.h		\
			logout.h           swallow.h		\
			main.h             tearoffitem.h	\
			menu-fentry.h      xstuff.h" )

templates: scan
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=..

html:
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	-cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)

clean-local:
	rm -f *~ *.bak *-unused.txt

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*.html`; \
	if test "$$installfiles" = '$(srcdir)/html/*.html'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \
	  echo '-- Fixing Crossreferences' ; \
	  gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(HTML_DIR)|| true; \
	fi)

dist-hook:
	mkdir $(distdir)/html
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY : html sgml templates scan
