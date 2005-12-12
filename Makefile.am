#
# FIXME:
#  currently scrollkeeper-update does not seem to
#  remove its database when there is no useful
#  information contained there. Therefore, with
#  "make uninstall" you're left with loads of
#  stuff in /var/scrollkeeper and distcheck
#  fails.
#
#  http://mail.gnome.org/archives/gnome-doc-list/2003-May/msg00008.html
#
#  This overrides the uninstall check so distcheck
#  passes
#
# distuninstallcheck_listfiles = find . -type f -print | grep -v 'scrollkeeper' | grep -v 'omf' | grep -v 'figures'
distuninstallcheck:
	@:

distcleancheck_listfiles = find . -type f -print | grep -v 'omf\.out' | grep -v 'legal.xml'

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
