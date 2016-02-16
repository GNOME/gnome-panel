dnl **************************************************************************
dnl Define version info
dnl **************************************************************************

m4_define([gp_major_version], [3])
m4_define([gp_minor_version], [18])
m4_define([gp_micro_version], [2])
m4_define([gp_version], [gp_major_version.gp_minor_version.gp_micro_version])

dnl **************************************************************************
dnl Define bugzilla address
dnl **************************************************************************

m4_define([gp_bugzilla_url],
          [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-panel])

dnl **************************************************************************
dnl Initialize autoconf
dnl **************************************************************************

AC_INIT([gnome-panel], [gp_version], [gp_bugzilla_url])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

dnl **************************************************************************
dnl Set release policy
dnl **************************************************************************

AX_IS_RELEASE([git-directory])

dnl **************************************************************************
dnl Enable debug
dnl **************************************************************************

AX_CHECK_ENABLE_DEBUG([yes])

dnl **************************************************************************
dnl Add warning flags
dnl **************************************************************************

AX_COMPILER_FLAGS([WARN_CFLAGS], [WARN_LDFLAGS])

dnl **************************************************************************
dnl Initialize automake
dnl **************************************************************************

AM_INIT_AUTOMAKE([1.13 no-dist-gzip dist-xz tar-ustar -Wno-portability])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

dnl **************************************************************************
dnl Initialize libtool
dnl **************************************************************************

LT_PREREQ([2.2.6])
LT_INIT([dlopen disable-static])
LT_LIB_M

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LIBPANEL_APPLET_API_VERSION=5.0
AC_SUBST([LIBPANEL_APPLET_API_VERSION])

LIBPANEL_APPLET_GIR_VERSION=5_0
AC_SUBST([LIBPANEL_APPLET_GIR_VERSION])

LIB_PANEL_APPLET_LT_VERSION=1:0:1
AC_SUBST(LIB_PANEL_APPLET_LT_VERSION)

AC_ARG_ENABLE([documentation],
	AS_HELP_STRING([--enable-documentation], [enable man pages and HTML]),
	[], [enable_documentation=yes])
AM_CONDITIONAL(ENABLE_DOCUMENTATION, test x$enable_documentation = xyes)

IT_PROG_INTLTOOL([0.40.6])

AC_PROG_CC

AM_PATH_PYTHON
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

# Check for introspection
GOBJECT_INTROSPECTION_CHECK([0.9.5])

GLIB_GSETTINGS

m4_pattern_allow([AM_V_GEN])dnl Make autoconf not complain about the rule below
PANEL_INTLTOOL_PANEL_APPLET_RULE='%.panel-applet: %.panel-applet.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; $(AM_V_GEN) LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST([PANEL_INTLTOOL_PANEL_APPLET_RULE])

AC_CHECK_HEADERS(crt_externs.h)
AC_CHECK_FUNCS(_NSGetEnviron)

LIBGNOME_DESKTOP_REQUIRED=2.91.0
GDK_PIXBUF_REQUIRED=2.25.2
PANGO_REQUIRED=1.15.4
GLIB_REQUIRED=2.45.3
GTK_REQUIRED=3.15.2
LIBWNCK_REQUIRED=3.4.6
LIBGNOME_MENU_REQUIRED=3.7.90
LIBECAL_REQUIRED=3.5.3
LIBEDATASERVER_REQUIRED=3.5.3
CAIRO_REQUIRED=1.0.0
GWEATHER_REQUIRED=3.17.1
DCONF_REQUIRED=0.13.4
LIBRSVG_REQUIRED=2.36.2

dnl pkg-config dependency checks

PKG_CHECK_MODULES(PANEL, gmodule-2.0 >= $GLIB_REQUIRED
			 gdk-pixbuf-2.0 >= $GDK_PIXBUF_REQUIRED
			 pango >= $PANGO_REQUIRED
			 gtk+-3.0 >= $GTK_REQUIRED
			 cairo-xlib glib-2.0 >= $GLIB_REQUIRED
			 gmodule-2.0 >= $GLIB_REQUIRED
			 gio-2.0 >= $GLIB_REQUIRED
			 gio-unix-2.0 >= $GLIB_REQUIRED
			 gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED
			 libgnome-menu-3.0 >= $LIBGNOME_MENU_REQUIRED
			 dconf >= $DCONF_REQUIRED)
AC_SUBST(PANEL_CFLAGS)
AC_SUBST(PANEL_LIBS)

PKG_CHECK_MODULES(LIBPANEL_APPLET, gtk+-3.0 >= $GTK_REQUIRED cairo-xlib cairo-gobject gio-2.0 >= $GLIB_REQUIRED gio-unix-2.0 >= $GLIB_REQUIRED)
AC_SUBST(LIBPANEL_APPLET_CFLAGS)
AC_SUBST(LIBPANEL_APPLET_LIBS)

PKG_CHECK_MODULES(FISH, gtk+-3.0 >= $GTK_REQUIRED cairo >= $CAIRO_REQUIRED)
AC_SUBST(FISH_CFLAGS)
AC_SUBST(FISH_LIBS)

PKG_CHECK_MODULES(NOTIFICATION_AREA, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(NOTIFICATION_AREA_CFLAGS)
AC_SUBST(NOTIFICATION_AREA_LIBS)

PKG_CHECK_MODULES(WNCKLET, gtk+-3.0 >= $GTK_REQUIRED libwnck-3.0 >= $LIBWNCK_REQUIRED)
AC_SUBST(WNCKLET_CFLAGS)
AC_SUBST(WNCKLET_LIBS)

AC_CHECK_HEADERS(langinfo.h)
AC_CHECK_FUNCS(nl_langinfo)

PKG_CHECK_MODULES(TZ, gio-2.0 >= $GLIB_REQUIRED)
AC_SUBST(TZ_CFLAGS)
AC_SUBST(TZ_LIBS)

PKG_CHECK_MODULES(CLOCK, pango >= $PANGO_REQUIRED gtk+-3.0 >= $GTK_REQUIRED glib-2.0 >= $GLIB_REQUIRED gio-2.0 >= $GLIB_REQUIRED librsvg-2.0 >= $LIBRSVG_REQUIRED gweather-3.0 >= $GWEATHER_REQUIRED gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED polkit-gobject-1 dconf >= $DCONF_REQUIRED)
AC_SUBST(CLOCK_CFLAGS)
AC_SUBST(CLOCK_LIBS)

# Check whether to enable e-d-s support for clock applet
AC_MSG_CHECKING([whether to enable evolution-data-server support])
AC_ARG_ENABLE([eds],
    AS_HELP_STRING([--enable-eds], [Whether to enable evolution-data-server support @<:@default=auto@:>@]),
    [], [enable_eds=auto])
AC_MSG_RESULT([$enable_eds])
CLOCK_EDS_ICONDIR=
if test "$enable_eds" != "no"; then
    PKG_CHECK_MODULES(CLOCK_EDS,[
        libecal-1.2 >= $LIBECAL_REQUIRED
        libedataserver-1.2 >= $LIBEDATASERVER_REQUIRED
     ], [have_eds=yes], [have_eds=no])

    if test "$enable_eds" = "yes" -a "$have_eds" = "no"; then
        AC_MSG_ERROR([evolution-data-server support was requested but required dependencies were not found: $CLOCK_EDS_PKG_ERRORS])
    elif test "$have_eds" = "yes"; then
        enable_eds=yes
        AC_DEFINE([HAVE_EDS], 1, [Define to enable evolution-data-server support])
        CLOCK_EDS_ICONDIR=`$PKG_CONFIG --variable=datadir evolution-data-server-1.2`/pixmaps/evolution-data-server
    else
        enable_eds=no
    fi
fi

AM_CONDITIONAL(ENABLE_EDS, test "$enable_eds" = "yes")
AC_SUBST(CLOCK_EDS_ICONDIR)

dnl **************************************************************************
dnl Check if struct dirent contains a d_type member, for run dialog
dnl **************************************************************************

AC_STRUCT_DIRENT_D_TYPE

dnl X development libraries check

PKG_CHECK_MODULES(X, x11 xau, :, [
  # pkg-config modules not found (only present since X11R7 aka Xorg); use
  # old-style detection
  AC_PATH_XTRA()
  # X not found
  if test x$no_x = xyes ; then
    AC_MSG_ERROR([X development libraries not found])
  fi

  gp_save_cflags="$CFLAGS"
  gp_save_libs="$LIBS"
  CFLAGS="$X_CFLAGS"
  LIBS="$X_PRE_LIBS $X_LIBS $X_EXTRA_LIBS"
  AC_CHECK_LIB(X11, XFree,, AC_MSG_ERROR([libX11 not found]))
  AC_CHECK_LIB(Xau, XauFileName,, AC_MSG_ERROR([libXau not found]))
  CFLAGS="$gp_save_cflags"
  LIBS="$gp_save_libs"

  X_LIBS="$X_PRE_LIBS $X_LIBS -lX11 -lXau $X_EXTRA_LIBS"
])

AC_SUBST(X_LIBS)

dnl X RANDR extension

PKG_CHECK_MODULES(XRANDR, xrandr >= 1.2.0, have_randr=yes, have_randr=no)
if test "x$have_randr" = "xyes"; then
  AC_DEFINE(HAVE_RANDR, 1, [Have the Xrandr extension library])
fi

dnl Modules dir
AC_SUBST([modulesdir],"\$(libdir)/gnome-panel/modules")

dnl Applets dir
AC_SUBST([appletsdir],"\$(datadir)/gnome-panel/\$(LIBPANEL_APPLET_API_VERSION)/applets")
AC_SUBST([appletsdir_pkgconfig],"\${datadir}/gnome-panel/\${libpanel_applet_api_version}/applets")

dnl  Language Support

GETTEXT_PACKAGE=gnome-panel-3.0
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define the gettext package to be used])
AC_SUBST(GETTEXT_PACKAGE)

AM_GLIB_GNU_GETTEXT

dnl gtk-doc stuff

GTK_DOC_CHECK([1.24.1], [--flavour no-tmpl])

dnl yelp-tools stuff

YELP_HELP_INIT

dnl platform test
case $host in
*-*-solaris*)
	ostype=solaris
	AC_DEFINE(HAVE_SOLARIS, 1, [Define to 1])
	;;
esac

dnl **************************************************************************
dnl Process .in files
dnl **************************************************************************

AC_CONFIG_FILES([
  Makefile

  data/Makefile
  data/16x16/Makefile
  data/22x22/Makefile
  data/24x24/Makefile
  data/32x32/Makefile
  data/48x48/Makefile
  data/scalable/Makefile

  gnome-panel/Makefile
  gnome-panel/libpanel-applet-private/Makefile
  gnome-panel/libpanel-util/Makefile

  libpanel-applet/Makefile
  libpanel-applet/libpanel-applet.pc
  libpanel-applet/libpanel-applet-uninstalled.pc

  po/Makefile.in

  applets/Makefile
  applets/clock/Makefile
  applets/clock/pixmaps/Makefile
  applets/fish/Makefile
  applets/notification_area/Makefile
  applets/wncklet/Makefile

  doc/Makefile
  doc/reference/Makefile
  doc/reference/libpanel-applet/Makefile

  help/Makefile
  help/clock/Makefile
  help/fish/Makefile

  man/Makefile
])

AC_OUTPUT

dnl **************************************************************************
dnl Show summary
dnl **************************************************************************

echo ""
echo "  GNOME Panel $VERSION"
echo "  `echo GNOME Panel $VERSION | sed "s/./=/g"`"
echo ""
echo "  srcdir ..........................: ${srcdir}"
echo ""
echo "  compiler ........................: ${CC}"
echo "  cflags ..........................: ${CFLAGS}"
echo ""
echo "  prefix ..........................: ${prefix}"
echo "  exec_prefix .....................: ${exec_prefix}"
echo "  bindir ..........................: ${bindir}"
echo "  datadir .........................: ${datadir}"
echo "  libdir ..........................: ${libdir}"
echo "  libexecdir ......................: ${libexecdir}"
echo "  localstatedir ...................: ${localstatedir}"
echo "  sbindir .........................: ${sbindir}"
echo "  sysconfdir ......................: ${sysconfdir}"
echo ""
echo "  Maintainer mode .................: ${USE_MAINTAINER_MODE}"
echo ""
echo "  Evolution-Data-Server support ...: ${enable_eds}"
echo "  XRandr support ..................: ${have_randr}"
echo "  Build introspection support .....: ${found_introspection}"
echo "  Build gtk-doc documentation .....: ${enable_gtk_doc}"
echo ""
