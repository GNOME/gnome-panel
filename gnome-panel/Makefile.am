SUBDIRS  = menu mailcheck clock launcher batmon

INCLUDES =	 			\
	-I../.. 			\
	-I$(srcdir)/../..	 	\
	-I$(includedir)			\
	$(X_CFLAGS)

bin_PROGRAMS = panel module_test

panel_SOURCES = \
	gdkextra.c	\
	gdkextra.h	\
	applet_files.c	\
	applet_files.h	\
	applet_cmds.h	\
	panel_cmds.h	\
	main.c		\
	panel.c		\
	panel.h

module_test_SOURCES = \
	module_test.c 

MYLIBS = \
	$(top_builddir)/libgnomeui/libgnomeui.la 	\
	$(top_builddir)/libgnome/libgnome.la		\
	$(DL_LIB)					\
        $(X_LIBS)

panel_LDADD = $(MYLIBS) 

module_test_LDADD = $(MYLIBS)
