## Process this file with automake to produce Makefile.in.

## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.2f

CFLAGS += $(ORBIT_CFLAGS)

if ORBIT_INSTALLED
CXX = $(CC)
else
CXX = mico-c++
endif
## We set this to force the values in mico-c++ to be used.  Otherwise,
## on Linux boxes, the compilation will fail with the default
## CXXFLAGS.
CXXFLAGS =

test:
	echo $(CXXCOMPILE)

INCLUDES = -I. -I$(srcdir) 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-I$(includedir) $(GNOME_INCLUDEDIR)

bin_PROGRAMS = panel #launcher_applet

libpanel_appletincludedir = $(includedir)

lib_LTLIBRARIES = libpanel_applet.la

if ORBIT_INSTALLED
CORBA_SRCLIST = gnome-panel-stubs.c gnome-panel-skels.c gnome-panel-common.c gnome-panel.h
else
CORBA_SRCLIST = gnome-panel.cc gnome-panel.h
endif

if ORBIT_INSTALLED
APPLET_LIB=orbit-applet-lib.c
else
APPLET_LIB=applet-lib.cc mico-parse.cc mico-parse.h
endif

libpanel_applet_la_SOURCES = \
	$(APPLET_LIB) \
	panel-util.c  \
	panel-util.h  \
	applet-widget.c \
	$(CORBA_SRCLIST)
#else

libpanel_appletinclude_HEADERS = \
	applet-lib.h \
	applet-widget.h

libpanel_applet_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)
libpanel_applet_la_LIBADD = $(MICO_LIBS) -lm $(DL_LIB)

BUILT_SOURCES = $(CORBA_SRCLIST)

if ORBIT_INSTALLED
$(CORBA_SRCLIST): gnome-panel.idl
	orbit-idl $(srcdir)/gnome-panel.idl
else
gnome-panel.cc gnome-panel.h: gnome-panel.idl
## `idl' lamely fails to strip the path part of the file
## name when computing the name of the output file, so we prod it a
## little.
	idl --name gnome-panel $(srcdir)/gnome-panel.idl
endif

if ORBIT_INSTALLED
PANEL_ORB_SRCLIST= orbit-glue.c \
	mico-glue.h
else
PANEL_ORB_SRCLIST= mico-glue.cc \
	mico-glue.h		\
	mico-parse.cc		\
	mico-parse.h
endif

panel_SOURCES = 		\
	drawer.c  		\
	drawer.h  		\
	gdkextra.c		\
	gdkextra.h		\
	main.c			\
	main.h			\
	menu.c  		\
	cookie.h		\
	cookie.c		\
	menu.h  		\
	swallow.c  		\
	swallow.h  		\
	$(PANEL_ORB_SRCLIST)	\
	panel.c			\
	panel.h         	\
	panel_config.c  	\
	panel_config.h		\
	panel_config_global.c  	\
	panel_config_global.h  	\
	panel-widget.c  	\
	panel-widget.h		\
	snapped-widget.c  	\
	snapped-widget.h	\
	drawer-widget.c  	\
	drawer-widget.h		\
	corner-widget.c  	\
	corner-widget.h		\
	panel-util.c		\
	panel-util.h		\
	mulapp.c		\
	mulapp.h		\
	launcher.c		\
	launcher.h		\
	logout.c		\
	logout.h		\
	$(CORBA_SRCLIST)

if ORBIT_INSTALLED
ORB_LIBS=$(ORBIT_LIBS)
MICO_LIBS=$(ORBIT_LIBS)
else
ORB_LIBS=$(MICO_LIBS)
endif

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

EXTRA_DIST = panelrc.in panel.in gnome-panel.idl appletsConf.sh.in

## Put `exec' in the name because this should be installed by
## `install-exec', not `install-data'.
confexecdir = $(libdir)
confexec_DATA = appletsConf.sh

#
# Create the appletsConf.sh file from the .sh.in file
#
if ORBIT_INSTALLED
appletsConf.sh: appletsConf.sh.in Makefile
	sed -e 's,\@GNOME_LIBDIR\@,$(GNOME_LIBDIR),g' \
	    -e 's,\@GNOMEUI_LIBS\@,$(GNOMEUI_LIBS),g' \
	    -e 's,\@GNOME_INCLUDEDIR\@,$(GNOME_INCLUDEDIR),g' \
	    -e 's,\@ORBIT_LIBS\@,$(ORBIT_LIBS),g' \
	    < $(srcdir)/appletsConf.sh.in > appletsConf.tmp \
	  && mv appletsConf.tmp appletsConf.sh
else
appletsConf.sh: appletsConf.sh.in Makefile
	sed -e 's,\@GNOME_LIBDIR\@,$(GNOME_LIBDIR),g' \
	    -e 's,\@GNOMEUI_LIBS\@,$(GNOMEUI_LIBS),g' \
	    -e 's,\@GNOME_INCLUDEDIR\@,$(GNOME_INCLUDEDIR),g' \
	    -e 's,\@MICO_LIBS\@,$(MICO_LIBS),g' \
	    < $(srcdir)/appletsConf.sh.in > appletsConf.tmp \
	  && mv appletsConf.tmp appletsConf.sh
endif
