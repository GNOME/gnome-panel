NULL =

SUBDIRS = \
	libgnome-panel \
	libpanel-util \
	modules \
	gnome-panel \
	help \
	data \
	man \
	po \
	$(NULL)

EXTRA_DIST = \
	HACKING \
	README.md \
	$(NULL)

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_GETTEXT) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	`find "$(srcdir)/m4" -type f -name "*.m4" -print` \
	$(srcdir)/ABOUT-NLS \
	$(srcdir)/INSTALL \
	$(srcdir)/config.h.in~ \
	$(srcdir)/configure \
	$(srcdir)/po/Makefile.in.in \
	$(srcdir)/po/Makevars.template \
	$(srcdir)/po/Rules-quot \
	$(srcdir)/po/boldquot.sed \
	$(srcdir)/po/en@boldquot.header \
	$(srcdir)/po/en@quot.header \
	$(srcdir)/po/gnome-panel.pot \
	$(srcdir)/po/insert-header.sin \
	$(srcdir)/po/quot.sed \
	$(srcdir)/po/remove-potcdate.sin \
	$(srcdir)/po/stamp-po \
	$(NULL)

GITIGNOREFILES = $(PACKAGE)-\*.tar.{gz,bz2,xz}

-include $(top_srcdir)/git.mk
