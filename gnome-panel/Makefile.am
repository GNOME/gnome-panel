test:
	echo $(CXXCOMPILE)

## FIXME: convert all applets to CORBA
#SUBDIRS  = menu mailcheck clock launcher batmon netwatch logout \
#	@PROGRAMS_PANEL_CDPLAYER@

SUBDIRS  = libapplet clock @PROGRAMS_PANEL_CDPLAYER@

INCLUDES =	 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-I$(includedir) $(X_CFLAGS)

bin_PROGRAMS = panel

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
	panel-widget.h
#	$(CORBA_SOURCES)

MICO_VERS = 2.0.3
MICO_LIBS = -lmicocoss$(MICO_VERS) -lmicoaux$(MICO_VERS) -lmico$(MICO_VERS)

panel_LDADD = libapplet/libapplet.a \
              $(GNOME_LIBDIR) $(MICO_LIBS) -lXt \
              $(GNOMEUI_LIBS) $(INTLLIBS)

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
