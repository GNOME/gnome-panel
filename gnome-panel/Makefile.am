SUBDIRS  = menu mailcheck clock launcher batmon netwatch logout

INCLUDES =	 			\
	-I../.. 			\
	-I$(srcdir)/../..	 	\
	-I$(includedir)			\
	$(X_CFLAGS)

bin_PROGRAMS = panel

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

MYLIBS = \
	$(top_builddir)/libgnomeui/libgnomeui.la 	\
	$(top_builddir)/libgnome/libgnome.la		\
	$(DL_LIB)					\
        $(X_LIBS) $(x_libs)

panel_LDADD = $(MYLIBS) 

data_DATA = panelrc

## Why not just make this via configure?  Because @datadir@ will be
## expanded incorrectly.
panelrc: panelrc.in Makefile
## Why do a sed and then an mv?  To avoid problems if the user
## interrupts.
	sed -e 's,\@datadir\@,$(datadir),g' < $(srcdir)/panelrc.in \
	  > panelrc.tmp && mv -f panelrc.tmp panelrc

MOSTLYCLEANFILES = panelrc

EXTRA_DIST = panelrc.in
