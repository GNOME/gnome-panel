## Process this file with automake to produce Makefile.in.

## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.2f

CFLAGS += -DUSE_INTERNAL_LAUNCHER -DDRAWER_AND_SWALLOW

CXX = mico-c++
## We set this to force the values in mico-c++ to be used.  Otherwise,
## on Linux boxes, the compilation will fail with the default
## CXXFLAGS.
CXXFLAGS =

test:
	echo $(CXXCOMPILE)

INCLUDES = -I. -I$(srcdir) 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-I$(includedir) $(GNOME_INCLUDEDIR)

## FIXME: logout should really be an internal applets since it is just
## too trivial 
bin_PROGRAMS = panel logout_applet launcher_applet

bin_SCRIPTS = panel.sh

CORBA_SOURCES = \
	gnome-panel.cc \
	gnome-panel.h

MICO_VERS = 2.0.5
MICO_LIBS = -lmicocoss$(MICO_VERS) -lmicoaux$(MICO_VERS) -lmico$(MICO_VERS)

lib_LTLIBRARIES = libpanel_applet.la

libpanel_applet_la_SOURCES = \
	applet-lib.cc \
	mico-parse.cc \
	panel-util.c  \
	$(CORBA_SOURCES)

#libpanel_appletinclude_HEADERS = applet-lib.h \
#	applet-widget.h

libpanel_applet_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)
libpanel_applet_la_LIBADD = $(MICO_LIBS) -lm $(DL_LIB)

BUILT_SOURCES = $(CORBA_SOURCES)

gnome-panel.cc gnome-panel.h: gnome-panel.idl
## `idl' lamely fails to strip the path part of the file
## name when computing the name of the output file, so we prod it a
## little.
	idl --name gnome-panel $(srcdir)/gnome-panel.idl

panel_SOURCES = 		\
	drawer.c  		\
	drawer.h  		\
	gdkextra.c		\
	gdkextra.h		\
	main.c			\
	menu.c  		\
	cookie.c		\
	menu.h  		\
	swallow.c  		\
	swallow.h  		\
	mico-glue.cc		\
	mico-glue.h		\
	mico-parse.cc		\
	mico-parse.h		\
	panel.c			\
	panel.h         	\
	panel_config.c  	\
	panel_config.h		\
	panel_config_global.c  	\
	panel_config_global.h  	\
	panel-widget.c  	\
	panel-widget.h		\
	panel-util.c		\
	$(CORBA_SOURCES)

panel_LDADD = $(GNOME_LIBDIR) $(MICO_LIBS) -lXt $(GNOMEUI_LIBS) $(INTLLIBS)

launcher_applet_SOURCES = \
	launcher-lib.cc   \
	launcher-lib.h    \
	launcher.c

launcher_applet_LDADD = libpanel_applet.la $(GNOME_LIBDIR) $(MICO_LIBS) -lXt $(GNOMEUI_LIBS) $(INTLLIBS)

logout_applet_SOURCES = \
	logout.c
## Must define linker for logout_applet -- it needs a C++ linker, but
## automake doesn't discover that automatically.
logout_applet_LINK = $(CXXLINK)

logout_applet_LDADD = libpanel_applet.la $(GNOME_LIBDIR) $(MICO_LIBS) -lXt $(GNOMEUI_LIBS) $(INTLLIBS)

data_DATA = panelrc

## Why not just make this via configure?  Because @datadir@ will be
## expanded incorrectly.

xpanel: xpanel.in Makefile
## Why do a sed and then an mv?  To avoid problems if the user
## interrupts.
	sed -e 's,\@bindir\@,$(bindir),g' < $(srcdir)/xpanel.in \
	> xpanel.tmp && chmod +x xpanel.tmp && mv -f xpanel.tmp xpanel

panelrc: panelrc.in Makefile
## Why do a sed and then an mv?  To avoid problems if the user
## interrupts.
	sed -e 's,\@datadir\@,$(datadir),g' < $(srcdir)/panelrc.in \
	  > panelrc.tmp && mv -f panelrc.tmp panelrc

MOSTLYCLEANFILES = panelrc

EXTRA_DIST = panelrc.in panel.in
