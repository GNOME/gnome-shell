Summary: Metacity window manager
Name: metacity
Version: 2.3.34
Release: 1
URL: http://people.redhat.com/~hp/metacity/
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: User Interface/Desktops
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: gtk2-devel >= 1.3.10

%description

Metacity is a simple window manager that integrates nicely with 
GNOME 2.

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc README AUTHORS COPYING NEWS
%{_bindir}/metacity
%{_bindir}/metacity-restart
%{_datadir}/gnome/wm-properties/metacity.desktop

%changelog
* Tue Oct 30 2001 Havoc Pennington <hp@redhat.com>
- 2.3.34

* Fri Oct 13 2001 Havoc Pennington <hp@redhat.com>
- 2.3.21 

* Mon Sep 17 2001 Havoc Pennington <hp@redhat.com>
- 2.3.8
- 2.3.13

* Wed Sep  5 2001 Havoc Pennington <hp@redhat.com>
- Initial build.


