dnl **************************************************************************
dnl Define version info
dnl **************************************************************************

m4_define([gp_major_version], [3])
m4_define([gp_minor_version], [59])
m4_define([gp_micro_version], [1])
m4_define([gp_version], [gp_major_version.gp_minor_version.gp_micro_version])

dnl **************************************************************************
dnl Define bugzilla address
dnl **************************************************************************

m4_define([gp_issue_url],
          [https://gitlab.gnome.org/GNOME/gnome-panel/-/issues/])

dnl **************************************************************************
dnl Initialize autoconf
dnl **************************************************************************

AC_INIT([gnome-panel], gp_version, [gp_issue_url])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

dnl **************************************************************************
dnl Set release policy
dnl **************************************************************************

m4_ifdef([AX_IS_RELEASE], [
  AX_IS_RELEASE([git-directory])
], [
  AC_MSG_ERROR([m4_normalize([
    could not find AX_IS_RELEASE macro. Please install the
    'autoconf-archive' package.
  ])])
])

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

AM_INIT_AUTOMAKE([1.16.4 no-dist-gzip dist-xz tar-ustar -Wno-portability])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

dnl **************************************************************************
dnl Initialize libtool
dnl **************************************************************************

LT_PREREQ([2.2.6])
LT_INIT([dlopen disable-static])
LT_LIB_M

dnl **************************************************************************
dnl Check for required programs
dnl **************************************************************************

AC_PROG_CC
AC_PATH_PROG([GDBUS_CODEGEN], [gdbus-codegen])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES], [glib-compile-resources])
AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])

dnl **************************************************************************
dnl Internationalization
dnl **************************************************************************

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])

GETTEXT_PACKAGE=gnome-panel
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"],
                   [Name of default gettext domain])

dnl **************************************************************************
dnl Desktop settings support
dnl **************************************************************************

GLIB_GSETTINGS

dnl **************************************************************************
dnl Library version information
dnl **************************************************************************

dnl Before making a release, the library version should be modified. Follow
dnl these instructions sequentially:
dnl 1. If the library source code has changed at all since the last update,
dnl    then increment lt_revision.
dnl 2. If any interfaces have been added, removed, or changed since the last
dnl    update, increment lt_current, and set lt_revision to 0.
dnl 3. If any interfaces have been added since the last public release, then
dnl    increment lt_age.
dnl 4. If any interfaces have been removed or changed since the last public
dnl    release, then set lt_age to 0.

m4_define([lt_current], [3])
m4_define([lt_revision], [0])
m4_define([lt_age], [0])
m4_define([lt_version_info], [lt_current:lt_revision:lt_age])

AC_SUBST([LT_VERSION_INFO], [lt_version_info])

dnl **************************************************************************

AC_ARG_ENABLE([documentation],
	AS_HELP_STRING([--enable-documentation], [enable man pages and HTML]),
	[], [enable_documentation=yes])
AM_CONDITIONAL(ENABLE_DOCUMENTATION, test x$enable_documentation = xyes)

LIBGNOME_DESKTOP_REQUIRED=3.35.3
GDK_PIXBUF_REQUIRED=2.26.0
PANGO_REQUIRED=1.15.4
GLIB_REQUIRED=2.67.1
GTK_REQUIRED=3.22.0
LIBWNCK_REQUIRED=43.1
LIBGNOME_MENU_REQUIRED=3.7.90
LIBECAL_REQUIRED=3.33.2
LIBEDATASERVER_REQUIRED=3.5.3
LIBSYSTEMD_REQUIRED=230
CAIRO_REQUIRED=1.0.0
GWEATHER_REQUIRED=3.91.0
DCONF_REQUIRED=0.13.4
XRANDR_REQUIRED=1.3.0
GSETTINGS_DESKTOP_SCHEMAS_REQUIRED=42.0

dnl pkg-config dependency checks

PKG_CHECK_MODULES([PANEL], [
  gmodule-2.0 >= $GLIB_REQUIRED
  gdk-pixbuf-2.0 >= $GDK_PIXBUF_REQUIRED
  pango >= $PANGO_REQUIRED
  gtk+-3.0 >= $GTK_REQUIRED
  cairo-xlib
  gdm
  glib-2.0 >= $GLIB_REQUIRED
  gmodule-2.0 >= $GLIB_REQUIRED
  gio-2.0 >= $GLIB_REQUIRED
  gio-unix-2.0 >= $GLIB_REQUIRED
  gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED
  gsettings-desktop-schemas >= $GSETTINGS_DESKTOP_SCHEMAS_REQUIRED
  dconf >= $DCONF_REQUIRED
  x11
  xi
  xrandr >= $XRANDR_REQUIRED
])

PKG_CHECK_MODULES([LIBGNOME_PANEL], [
  gio-2.0 >= $GLIB_REQUIRED
  gmodule-2.0 >= $GLIB_REQUIRED
  gtk+-3.0 >= $GTK_REQUIRED
])

PKG_CHECK_MODULES([ACTION_BUTTON], [
  gio-unix-2.0 >= $GLIB_REQUIRED
  gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED
  gtk+-3.0 >= $GTK_REQUIRED
  libgnome-menu-3.0 >= $LIBGNOME_MENU_REQUIRED
  libsystemd >= $LIBSYSTEMD_REQUIRED
  x11
])

PKG_CHECK_MODULES(FISH, gtk+-3.0 >= $GTK_REQUIRED cairo >= $CAIRO_REQUIRED)
AC_SUBST(FISH_CFLAGS)
AC_SUBST(FISH_LIBS)

PKG_CHECK_MODULES([LAUNCHER], [
  gio-unix-2.0 >= $GLIB_REQUIRED
  gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED
  gtk+-3.0 >= $GTK_REQUIRED
  libgnome-menu-3.0 >= $LIBGNOME_MENU_REQUIRED
  libsystemd >= $LIBSYSTEMD_REQUIRED
])

PKG_CHECK_MODULES([MENU], [
  gdm
  gio-unix-2.0 >= $GLIB_REQUIRED
  gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED
  gtk+-3.0 >= $GTK_REQUIRED
  libgnome-menu-3.0 >= $LIBGNOME_MENU_REQUIRED
  libsystemd >= $LIBSYSTEMD_REQUIRED
])

PKG_CHECK_MODULES([NOTIFICATION_AREA], [
  gtk+-3.0 >= $GTK_REQUIRED
  x11
])

PKG_CHECK_MODULES([SEPARATOR], [
  gtk+-3.0 >= $GTK_REQUIRED
])

PKG_CHECK_MODULES([STATUS_NOTIFIER], [
  gio-unix-2.0 >= $GLIB_REQUIRED
  gtk+-3.0 >= $GTK_REQUIRED
])

PKG_CHECK_MODULES(WNCKLET, gtk+-3.0 >= $GTK_REQUIRED libwnck-4.0 >= $LIBWNCK_REQUIRED)
AC_SUBST(WNCKLET_CFLAGS)
AC_SUBST(WNCKLET_LIBS)

AC_CHECK_HEADERS(langinfo.h)
AC_CHECK_FUNCS(nl_langinfo)

PKG_CHECK_MODULES([CLOCK], [
  dconf >= $DCONF_REQUIRED
  geocode-glib-2.0
  gio-2.0 >= $GLIB_REQUIRED
  gio-unix-2.0 >= $GLIB_REQUIRED
  glib-2.0 >= $GLIB_REQUIRED
  gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED
  gtk+-3.0 >= $GTK_REQUIRED
  gweather4 >= $GWEATHER_REQUIRED
  pango >= $PANGO_REQUIRED
  polkit-gobject-1
])

# Check whether to enable e-d-s support for clock applet
AC_MSG_CHECKING([whether to enable evolution-data-server support])
AC_ARG_ENABLE([eds],
    AS_HELP_STRING([--enable-eds], [Whether to enable evolution-data-server support @<:@default=auto@:>@]),
    [], [enable_eds=auto])
AC_MSG_RESULT([$enable_eds])
CLOCK_EDS_ICONDIR=
if test "$enable_eds" != "no"; then
    PKG_CHECK_MODULES(CLOCK_EDS,[
        libecal-2.0 >= $LIBECAL_REQUIRED
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

dnl **************************************************************************
dnl systemd user unit dir
dnl **************************************************************************

AS_IF([$PKG_CONFIG systemd], [
  SYSTEMD_USER_UNIT_DIR=$($PKG_CONFIG \
    --variable=systemduserunitdir \
    --define-variable=prefix=${prefix} \
    systemd)
], [
  SYSTEMD_USER_UNIT_DIR="${prefix}/lib/systemd/user"
])

AC_SUBST([SYSTEMD_USER_UNIT_DIR], [$SYSTEMD_USER_UNIT_DIR])

dnl yelp-tools stuff

YELP_HELP_INIT

dnl **************************************************************************
dnl Process .in files
dnl **************************************************************************

AC_CONFIG_FILES([
  Makefile

  data/Makefile

  data/icons/Makefile
  data/icons/16x16/Makefile
  data/icons/22x22/Makefile
  data/icons/24x24/Makefile
  data/icons/32x32/Makefile
  data/icons/48x48/Makefile
  data/icons/scalable/Makefile

  data/pkgconfig/Makefile
  data/pkgconfig/libgnome-panel.pc

  data/theme/Makefile
  data/theme/Adwaita/Makefile
  data/theme/HighContrast/Makefile
  data/theme/Yaru/Makefile

  gnome-panel/Makefile

  libgnome-panel/Makefile
  libpanel-util/Makefile

  help/Makefile
  help/clock/Makefile
  help/fish/Makefile

  man/Makefile

  modules/Makefile
  modules/action-button/Makefile
  modules/clock/Makefile
  modules/clock/pixmaps/Makefile
  modules/fish/Makefile
  modules/launcher/Makefile
  modules/menu/Makefile
  modules/notification-area/Makefile
  modules/separator/Makefile
  modules/status-notifier/Makefile
  modules/wncklet/Makefile

  po/Makefile.in
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
echo ""
