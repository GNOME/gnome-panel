test:
	echo $(CXXCOMPILE)

## FIXME: convert all applets to CORBA
#SUBDIRS  = menu mailcheck clock launcher batmon netwatch logout \
#	@PROGRAMS_PANEL_CDPLAYER@

SUBDIRS  = clock @PROGRAMS_PANEL_CDPLAYER@

INCLUDES =	 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-I$(includedir)

bin_PROGRAMS = panel clock_applet
lib_LTLIBRARIES = libpanel_applet.la

CORBA_SOURCES = \
	gnome-panel.cc \
	gnome-panel.h

MICO_VERS = 2.0.3
MICO_LIBS = -lmicocoss$(MICO_VERS) -lmicoaux$(MICO_VERS) -lmico$(MICO_VERS)

libpanel_applet_la_SOURCES = \
	applet-lib.cc \
	applet-widget.c \
	$(CORBA_SOURCES)

libpanel_appletinclude_HEADERS = \
	applet-lib.h \
	applet-widget.h

libpanel_applet_la_LDFLAGS = -version-info 0:0:0 -rpath $(libdir)
libpanel_applet_la_LIBADD = $(MICO_LIBS) -lm $(DL_LIB)

BUILT_SOURCES = $(CORBA_SOURCES)

gnome-panel.cc: gnome-panel.idl
## `idl' version 2.0.3 lamely fails to strip the path part of the file
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
	menu.h  		\
	mico-glue.cc		\
	mico-glue.h		\
	panel.c			\
	panel.h         	\
	panel_config.c  	\
	panel_config.h		\
	panel_config_global.c  	\
	panel_config_global.h  	\
	panel-widget.c  	\
	panel-widget.h		\
	$(CORBA_SOURCES)

panel_LDADD = $(GNOME_LIBDIR) $(MICO_LIBS) -lXt $(GNOMEUI_LIBS) $(INTLLIBS)

## FIXME: move clock applet into clock
clock_applet_SOURCES = \
	clock.c

clock_applet_LDADD = -lpanel_applet $(GNOME_LIBDIR) $(MICO_LIBS) -lXt $(GNOMEUI_LIBS) $(INTLLIBS)

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
