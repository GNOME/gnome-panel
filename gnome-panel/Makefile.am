SUBDIRS  = menu mailcheck clock launcher batmon netwatch logout \
	@PROGRAMS_PANEL_CDPLAYER@

INCLUDES =	 			\
	-I$(includedir) $(X_CFLAGS)

bin_PROGRAMS = panel clock_applet

panel_SOURCES = \
	gdkextra.c	\
	gdkextra.h	\
	main.c		\
	panel.c		\
	panel.h         \
	menu.c  	\
	menu.h  	\
	panel-widget.c  \
	panel-widget.h  \
	panel_config_global.c  \
	panel_config_global.h  \
	panel_config.c  \
	panel_config.h

panel_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS)


## FIXME: move clock applet into clock
clock_applet_SOURCES = \
	clock.c

clock_applet_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS)

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
