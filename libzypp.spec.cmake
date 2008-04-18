#
# spec file for package libzypp
#
# Copyright (c) 2007 SUSE LINUX Products GmbH, Nuernberg, Germany.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

# norootforbuild

Name:           @PACKAGE@
License:        GPL v2 or later
Group:          System/Packages
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Autoreqprov:    on
Summary:        Package, Patch, Pattern, and Product Management
Version:        @VERSION@
Release:        0
Source:         @PACKAGE@-@VERSION@.tar.bz2
Source1:        @PACKAGE@-rpmlintrc
Prefix:         /usr
Provides:       yast2-packagemanager
Obsoletes:      yast2-packagemanager
BuildRequires:  cmake
BuildRequires:  libsatsolver-devel >= 0.0.26 openssl-devel
BuildRequires:  boost-devel curl-devel dejagnu doxygen gcc-c++ gettext-devel graphviz hal-devel libxml2-devel

%if 0%{?suse_version}
BuildRequires:  hicolor-icon-theme update-desktop-files rpm-devel
%endif

%if 0%{?fedora_version}
BuildRequires: glib2-devel popt-devel dbus-glib-devel rpm-devel
%endif

%if 0%{?mandriva_version}
BuildRequires:  sqlite3-devel
BuildRequires:  glib2-devel
BuildRequires:  librpm-devel
%else
BuildRequires:  sqlite-devel
%endif



%if 0%{?suse_version}
Requires:       gpg2
%else
Requires:       gnupg
%endif

Requires:       satsolver-tools == %( echo `rpm -q --queryformat '%{VERSION}' satsolver-tools`)

%description
Package, Patch, Pattern, and Product Management

Authors:
--------
    Michael Andres <ma@suse.de>
    Jiri Srain <jsrain@suse.cz>
    Stefan Schubert <schubi@suse.de>
    Duncan Mac-Vicar <dmacvicar@suse.de>
    Klaus Kaempf <kkaempf@suse.de>
    Marius Tomaschewski <mt@suse.de>
    Stanislav Visnovsky <visnov@suse.cz>
    Ladislav Slezak <lslezak@suse.cz>

%package devel
Requires:       libzypp == @VERSION@
Requires:       libxml2-devel curl-devel openssl-devel rpm-devel glibc-devel zlib-devel
Requires:       bzip2 popt-devel dbus-1-devel glib2-devel hal-devel boost-devel libstdc++-devel
Requires:       cmake libsatsolver-devel
Summary:        Package, Patch, Pattern, and Product Management - developers files
Group:          System/Packages
Provides:       yast2-packagemanager-devel
Obsoletes:      yast2-packagemanager-devel

%description -n libzypp-devel
Package, Patch, Pattern, and Product Management - developers files

Authors:
--------
    Michael Andres <ma@suse.de>
    Jiri Srain <jsrain@suse.cz>
    Stefan Schubert <schubi@suse.de>
    Duncan Mac-Vicar <dmacvicar@suse.de>
    Klaus Kaempf <kkaempf@suse.de>
    Marius Tomaschewski <mt@suse.de>
    Stanislav Visnovsky <visnov@suse.cz>
    Ladislav Slezak <lslezak@suse.cz>

%prep
%setup -q

%build
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=%{prefix} \
      -DDOC_INSTALL_DIR=%{_docdir} \
      -DLIB=%{_lib} \
      -DCMAKE_C_FLAGS_RELEASE:STRING="%{optflags}" \
      -DCMAKE_CXX_FLAGS_RELEASE:STRING="%{optflags}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SKIP_RPATH=1 \
      ..
make %{?jobs:-j %jobs} VERBOSE=1
make -C doc/autodoc %{?jobs:-j %jobs}
make -C po %{?jobs:-j %jobs} translations

#make check

%install
rm -rf "$RPM_BUILD_ROOT"
cd build
make install DESTDIR=$RPM_BUILD_ROOT
make -C doc/autodoc install DESTDIR=$RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/etc/zypp/repos.d

%if 0%{?suse_version}
%suse_update_desktop_file -G "" -C "" package-manager
%endif

make -C po install DESTDIR=$RPM_BUILD_ROOT
# Create filelist with translations
cd ..
%{find_lang} zypp


%post
%run_ldconfig
if [ -f /var/cache/zypp/zypp.db ]; then rm /var/cache/zypp/zypp.db; fi
#%{prefix}/lib/zypp/zypp-migrate-sources

%postun
%run_ldconfig

%clean
rm -rf "$RPM_BUILD_ROOT"

%files -f zypp.lang
%defattr(-,root,root)
%dir               /etc/zypp
%dir               /etc/zypp/repos.d
%config(noreplace) /etc/zypp/zypp.conf
%{prefix}/lib/zypp
%{prefix}/share/zypp
%{prefix}/share/applications/package-manager.desktop
%{prefix}/share/icons/hicolor/scalable/apps/package-manager-icon.svg
%{prefix}/share/icons/hicolor/16x16/apps/package-manager-icon.png
%{prefix}/share/icons/hicolor/32x32/apps/package-manager-icon.png
%{prefix}/share/icons/hicolor/48x48/apps/package-manager-icon.png
%{prefix}/bin/package-manager
%{prefix}/bin/package-manager-su
%{_libdir}/libzypp*so.*

%files devel
%defattr(-,root,root)
%{_libdir}/libzypp.so
#%dir %{_libdir}/libzypp.la
%{_docdir}/%{name}
%dir %{prefix}/include/zypp
%{prefix}/include/zypp/*
%{prefix}/share/cmake/Modules/*
%{_libdir}/pkgconfig/libzypp.pc

%changelog