## Process this file with automake to produce Makefile.in.

## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.2f

if ORBIT_INSTALLED
panel_LINK = $(LINK)
else
CXX = mico-c++
## We set this to force the values in mico-c++ to be used.  Otherwise,
## on Linux boxes, the compilation will fail with the default
## CXXFLAGS.
CXXFLAGS =
panel_LINK = $(CXXLINK)
endif

INCLUDES = -I. -I$(srcdir) 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-I$(includedir) $(GNOME_INCLUDEDIR) $(ORB_CFLAGS)

bin_PROGRAMS = panel #launcher_applet

libpanel_appletincludedir = $(includedir)

lib_LTLIBRARIES = libpanel_applet.la

if ORBIT_INSTALLED

CORBA_SRCLIST = gnome-panel-stubs.c gnome-panel-skels.c gnome-panel-common.c gnome-panel.h

$(CORBA_SRCLIST): gnome-panel.idl
	orbit-idl $(srcdir)/gnome-panel.idl

APPLET_LIB = orbit-applet-lib.c

PANEL_ORB_SRCLIST= orbit-glue.c

# We need explicit dependencies here for people building out
# of the tarballs.

orbit-applet-lib.c: gnome-panel.h

orbit-glue.c: gnome-panel.h

else

CORBA_SRCLIST = gnome-panel.cc gnome-panel.h

gnome-panel.cc gnome-panel.h: gnome-panel.idl
## `idl' lamely fails to strip the path part of the file
## name when computing the name of the output file, so we prod it a
## little.
	idl --name gnome-panel $(srcdir)/gnome-panel.idl

APPLET_LIB=applet-lib.cc mico-parse.cc mico-parse.h

PANEL_ORB_SRCLIST= mico-glue.cc mico-parse.cc mico-parse.h

endif

libpanel_applet_la_SOURCES = \
	$(APPLET_LIB) \
	panel-util.c  \
	panel-util.h  \
	applet-widget.c \
	$(CORBA_SRCLIST)

EXTRA_libpanel_applet_la_SOURCES = \
	orbit-applet-lib.c \
	applet-lib.cc mico-parse.cc mico-parse.h

libpanel_appletinclude_HEADERS = \
	applet-lib.h \
	applet-widget.h

libpanel_applet_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)
libpanel_applet_la_LIBADD = $(ORB_LIBS) -lm $(DL_LIB)

BUILT_SOURCES = $(CORBA_SRCLIST)

panel_SOURCES = 		\
	panel-include.h		\
	main.c			\
	main.h			\
	applet.c		\
	applet.h         	\
	panel.c			\
	panel.h         	\
	session.c		\
	session.h		\
	panel_config.c  	\
	panel_config.h		\
	panel_config_global.c  	\
	panel_config_global.h  	\
	menu.c  		\
	menu.h  		\
	launcher.c		\
	launcher.h		\
	logout.c		\
	logout.h		\
	extern.c  		\
	extern.h  		\
	swallow.c  		\
	swallow.h  		\
	drawer.c  		\
	drawer.h  		\
	panel-widget.c  	\
	panel-widget.h		\
	snapped-widget.c  	\
	snapped-widget.h	\
	drawer-widget.c  	\
	drawer-widget.h		\
	corner-widget.c  	\
	corner-widget.h		\
	exec-queue.c		\
	exec-queue.h		\
	mulapp.c		\
	mulapp.h		\
	panel-util.c		\
	panel-util.h		\
	gdkextra.c		\
	gdkextra.h		\
	cookie.h		\
	cookie.c		\
	mico-glue.h		\
	$(PANEL_ORB_SRCLIST)	\
	$(CORBA_SRCLIST)

EXTRA_panel_SOURCES = \
	orbit-glue.c \
	mico-glue.cc mico-parse.cc mico-parse.h

panel_LDADD = $(GNOME_LIBDIR) $(ORB_LIBS) $(GNOMEUI_LIBS) $(INTLLIBS)

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
MAINTAINERCLEANFILES = $(CORBA_SRCLIST)

EXTRA_DIST = panelrc.in panel.in gnome-panel.idl appletsConf.sh.in

## Put `exec' in the name because this should be installed by
## `install-exec', not `install-data'.
confexecdir = $(libdir)
confexec_DATA = appletsConf.sh

#
# Create the appletsConf.sh file from the .sh.in file
#
appletsConf.sh: appletsConf.sh.in Makefile
	sed -e 's,\@GNOME_LIBDIR\@,$(GNOME_LIBDIR),g' \
	    -e 's,\@GNOMEUI_LIBS\@,$(GNOMEUI_LIBS),g' \
	    -e 's,\@GNOME_INCLUDEDIR\@,$(GNOME_INCLUDEDIR),g' \
	    -e 's,\@ORB_LIBS\@,$(ORB_LIBS),g' \
	    < $(srcdir)/appletsConf.sh.in > appletsConf.tmp \
	  && mv appletsConf.tmp appletsConf.sh

