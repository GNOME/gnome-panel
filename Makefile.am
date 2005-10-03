SUBDIRS =			\
	po 			\
	idl 			\
	icons			\
	gnome-panel		\
	libpanel-applet		\
	applets			\
	help			\
	doc			\
	man

EXTRA_DIST = \
	gnome-panel.spec.in	\
	gnome-panel.spec	\
	COPYING-DOCS		\
	HACKING			\
	omf.make		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --disable-scrollkeeper
DISTCLEANFILES =		\
        intltool-extract	\
        intltool-merge		\
        intltool-update		\
	gnome-doc-utils.make
