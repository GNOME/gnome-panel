NULL =

SUBDIRS = \
	libgnome-panel \
	gnome-panel \
	libpanel-applet \
	applets \
	docs \
	help \
	data \
	man \
	po \
	$(NULL)

AM_DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	$(NULL)

EXTRA_DIST = \
	COPYING-DOCS \
	HACKING \
	$(NULL)

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	`find "$(srcdir)/m4" -type f -name "*.m4" -print` \
	$(srcdir)/INSTALL \
	$(srcdir)/config.h.in~ \
	$(srcdir)/configure \
	$(srcdir)/gtk-doc.make \
	$(NULL)

GITIGNOREFILES = $(PACKAGE)-\*.tar.{gz,bz2,xz}

-include $(top_srcdir)/git.mk
