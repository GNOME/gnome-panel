INCLUDES = $(WNCKLET_CFLAGS) -DEEL_OMIT_SELF_CHECK

EELFILES = \
	eel-ellipsizing-label.h \
	eel-ellipsizing-label.c \
	eel-pango-extensions.h \
	eel-pango-extensions.c \
	eel-gtk-macros.h \
	eel-string.h \
	eel-string.c

EELDIR = $(srcdir)/../../../../eel/eel

noinst_LTLIBRARIES = libeelcnp.la

libeelcnp_la_SOURCES = $(EELFILES)

regenerate-built-sources:
	EELFILES="$(EELFILES)" EELDIR="$(EELDIR)" $(srcdir)/update-from-eel.sh
