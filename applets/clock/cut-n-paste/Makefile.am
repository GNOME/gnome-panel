INCLUDES = $(WARN_CFLAGS) $(CLOCK_CFLAGS) -DEEL_OMIT_SELF_CHECK

noinst_LTLIBRARIES = libclockcnp.la

libclockcnp_la_SOURCES = \
	eggcellrenderertext.h \
	eggcellrenderertext.c \
	$(EELFILES) \
	eggmarshalers.h \
	eggmarshalers.c

eggmarshalers.h: eggmarshalers.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_egg_marshal > $@

eggmarshalers.c: eggmarshalers.list $(GLIB_GENMARSHAL)
	echo "#include \"eggmarshalers.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=_egg_marshal >> $@

BUILT_SOURCES = eggmarshalers.c eggmarshalers.h

EXTRA_DIST = eggmarshalers.list

EELFILES = \
	eel-pango-extensions.h \
	eel-pango-extensions.c

EELDIR = $(srcdir)/../../../../eel/eel

regenerate-built-sources:
	EELFILES="$(EELFILES)" EELDIR="$(EELDIR)" $(srcdir)/update-from-eel.sh
