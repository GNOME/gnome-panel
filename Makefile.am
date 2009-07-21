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

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	MAINTAINERS		\
	COPYING-DOCS		\
	HACKING			\
	ChangeLog.pre-git	\
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --disable-scrollkeeper
DISTCLEANFILES =		\
	gnome-doc-utils.make

MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	`find "$(srcdir)" -type f -name Makefile.in -print` \
	$(srcdir)/configure \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/m4/gnome-doc-utils.m4 \
	$(srcdir)/m4/gtk-doc.m4 \
	$(srcdir)/m4/intltool.m4

-include $(top_srcdir)/git.mk
