## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module, e.g. 'glib'.
DOC_MODULE=panel-applet
DOC_MODULE_VERSION=4.0

# The top-level SGML file. Change it if you want.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting functions and macros.
DOC_SOURCE_DIR=../../../libpanel-applet

# Extra options to supply to gtkdoc-scan.
SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS=--xml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref.
FIXXREF_OPTIONS=

# Used for dependencies.
HFILE_GLOB=$(top_srcdir)/libpanel-applet/*.h
CFILE_GLOB=$(top_srcdir)/libpanel-applet/*.c

# Header files to ignore when scanning.
IGNORE_HFILES =					\
	panel-applet-bindings.h			\
	panel-applet-enums.h			\
	panel-applet-factory.h			\
	panel-applet-marshal.h

# Images to copy into HTML directory.
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files =

# Other files to distribute.
extra_files =

# CFLAGS and LDFLAGS for compiling scan program. Only needed if your app/lib
# contains GtkObjects/GObjects and you want to document signals and properties.
GTKDOC_CFLAGS =					\
	$(CFLAGS)				\
	$(CPPFLAGS)				\
	$(PANEL_CFLAGS)				\
	-I$(srcdir)/../../../libpanel-applet    \
	-I$(top_builddir)/libpanel-applet

GTKDOC_LIBS =						\
	$(LDFLAGS)					\
	$(PANEL_LIBS)					\
	../../../libpanel-applet/libpanel-applet-4.la

include $(top_srcdir)/gtk-doc.make

dist-hook-local:
#	mkdir $(distdir)/TEXT;          \
#	for f in $(srcdir)/TEXT/* ; do  \
#	  test -f $$f && cp -p $$f $(distdir)/TEXT; \
#	done

-include $(top_srcdir)/git.mk
