%define gettext_package gnome-panel-2.0

%define glib2_version 2.0.0
%define gtk2_version 2.1.2
%define libglade2_version 2.0.0
%define libgnome_version 2.1.1
%define libgnomeui_version 2.1.2
%define gnome_desktop_version 2.0.3
%define libwnck_version 2.1.0
%define libbonobo_version 2.0.0
%define libbonoboui_version 2.0.0
%define gnome_vfs2_version 2.0.2
%define bonobo_activation_version 1.0.0
%define libxslt_version 1.0.17

Summary: GNOME panel
Name: gnome-panel
Version: 2.1.2
Release: 2
URL: http://www.gnome.org
Source0: ftp://ftp.gnome.org/pub/GNOME/pre-gnome2/sources/gnome-panel/%{name}-%{version}.tar.gz
License: GPL 
Group: User Interface/Desktops
BuildRoot: %{_tmppath}/%{name}-root

Requires: gnome-desktop >= %{gnome_desktop_version}
Prereq: /bin/awk, /bin/cat, /bin/ln, /bin/rm

BuildRequires: glib2-devel >= %{glib2_version}
BuildRequires: gtk2-devel >= %{gtk2_version}
BuildRequires: libglade2-devel >= %{libglade2_version}
BuildRequires:  libgnome-devel >= %{libgnome_version}
BuildRequires: libgnomeui-devel >= %{libgnomeui_version}
BuildRequires: gnome-desktop-devel >= %{gnome_desktop_version}
BuildRequires: libwnck-devel >= %{libwnck_version}
BuildRequires: libbonobo-devel >= %{libbonobo_version}
BuildRequires: libbonoboui-devel >= %{libbonoboui_version}
BuildRequires: gnome-vfs2-devel >= %{gnome_vfs2_version}
BuildRequires: bonobo-activation-devel >= %{bonobo_activation_version}
BuildRequires: libxslt-devel >= %{libxslt_version}
BuildRequires: Xft
BuildRequires: fontconfig

%description

The GNOME panel provides the window list, workspace switcher, menus, and other 
features for the GNOME desktop.

%prep
%setup -q 

%build

%configure
make

%install
rm -rf $RPM_BUILD_ROOT

export GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL=1
%makeinstall
unset GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL

## blow away stuff we don't want
/bin/rm -rf $RPM_BUILD_ROOT/var/scrollkeeper
rmdir --ignore-fail-on-non-empty $RPM_BUILD_ROOT/var
/bin/rm -f $RPM_BUILD_ROOT%{_libdir}/libpanel-applet-2.a
/bin/rm -f $RPM_BUILD_ROOT%{_libdir}/libpanel-applet-2.la
/bin/rm -f $RPM_BUILD_ROOT%{_libdir}/libgen_util_applet-2.a
/bin/rm -f $RPM_BUILD_ROOT%{_libdir}/libgen_util_applet-2.la
/bin/rm -f $RPM_BUILD_ROOT%{_libdir}/gnome-panel/libnotification-area-applet.a
/bin/rm -f $RPM_BUILD_ROOT%{_libdir}/gnome-panel/libnotification-area-applet.la

%find_lang %{gettext_package}
grep -q '/usr/share/locale' %{gettext_package}.lang || exit 1

%clean
rm -rf $RPM_BUILD_ROOT

%post
export GCONF_CONFIG_SOURCE=`gconftool-2 --get-default-source`
# this spits a warning right now, needs fixing in gconf
gconftool-2 --direct --config-source=$GCONF_CONFIG_SOURCE --recursive-unset /schemas/apps/panel
SCHEMAS="panel-global-config.schemas panel-per-panel-config.schemas mailcheck.schemas pager.schemas tasklist.schemas clock.schemas fish.schemas"
for S in $SCHEMAS; do
  gconftool-2 --makefile-install-rule %{_sysconfdir}/gconf/schemas/$S > /dev/null
done
/sbin/ldconfig

%postun
if [ "$1" = "0" ]; then
  rm -f ${_sysconfdir}/gconf/schemas/panel-per-panel-config.schemas
fi
/sbin/ldconfig
  
%files -f %{gettext_package}.lang
%defattr(-,root,root)

%doc AUTHORS COPYING ChangeLog NEWS README

%{_datadir}/pixmaps
%{_datadir}/gnome
%{_datadir}/gnome-panelrc
%{_datadir}/control-center-2.0
%{_datadir}/idl
%{_datadir}/gnome-2.0
%{_datadir}/gnome-panel
%{_datadir}/gen_util
%{_datadir}/gtk-doc
%{_datadir}/omf
%{_datadir}/man/man*/*
%{_bindir}/*
%{_libexecdir}/*
%{_libdir}/bonobo
%{_libdir}/*.so.*
%{_sysconfdir}/gconf/schemas/*.schemas
%{_sysconfdir}/sound

# theoretically a devel package, but just doesn't seem worth it
%{_libdir}/pkgconfig/*
%{_includedir}/panel-2.0
%{_libdir}/*.so
%{_libdir}/gnome-panel/libnotification-area-applet.so


%changelog
* Sun Nov 10 2002 Christian F.K. Schaller <Uraeus@gnome.org>
- Initial update of CVS version based on RH one

