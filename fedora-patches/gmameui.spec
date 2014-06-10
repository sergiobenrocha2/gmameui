Summary: Frontend for MAME
Name: gmameui
Version: 0.2.13
Release: 0.4.20120704cvs%{?dist}
License: GPLv3+
Group: Applications/Emulators
URL: http://gmameui.sourceforge.net/
#http://gmameui.cvs.sourceforge.net/viewvc/gmameui/gmameui/?view=tar
Source0: gmameui-gmameui20120704cvs.tar.gz
#from gnome-icon-theme-gperfection2-2.3-1.noarch.rpm
Source1: missingicons.tbz
Patch0: gmameui-0.2.12-fix.patch
Patch2: gmameui-fix3.patch
Patch3: gmameui-fix4.patch
#BuildRequires: gtk2-devel
BuildRequires: libgnome-devel
BuildRequires: expat-devel
BuildRequires: libglade2-devel
BuildRequires: gettext, bison
BuildRequires: intltool, perl(XML::Parser)
BuildRequires: libarchive-devel
BuildRequires: libvtemm-devel
BuildRequires: gnome-doc-utils
BuildRequires: rarian-compat
BuildRequires: libzip-devel
BuildRequires: gtkimageview-devel
BuildRequires: vte-devel

Requires: gnome-icon-theme-legacy


%description
GMAMEUI is a front-end program that helps you run MAME (either xmame or
sdlmame), allowing you to run your arcade games quickly and easily.


%prep
%setup -q -n gmameui
%setup -a1 -qn gmameui
%patch0 -p1 -b .fix
%patch2 -p0 -b .fix3
%patch3 -p0 -b .fix4


%build
./autogen.sh
%configure
%{__make} %{?_smp_mflags}


%install
%{__rm} -rf %{buildroot} _docs
%{__make} install DESTDIR=%{buildroot}
%find_lang %{name}

# Put the docs back into place
%{__mkdir} _docs
%{__rm} -f %{buildroot}%{_docdir}/%{name}*/{README,TODO}
%{__mv} %{buildroot}%{_docdir}/%{name}*/* _docs/

# Install missing icons
#%{__cp} emblem-distinguished.png emblem-sound.png stock_unknown.png stock_filter-navigator.png \
#stock_toggle-preview.png stock_list_enum-off.png %{buildroot}%{_datadir}/gmameui/

%{__cp} gmameui-view-list.png gmameui-view-filters.png gmameui-view-screenshot.png \
gmameui-emblem-unknown.png gmameui-emblem-played.png gmameui-emblem-sound.png \
%{buildroot}%{_datadir}/gmameui/

%files -f %{name}.lang
%defattr(-,root,root,-)
%doc _docs/*
%{_bindir}/gmameui
%{_datadir}/applications/gmameui.desktop
%{_datadir}/pixmaps/gmameui.png
%{_datadir}/gmameui/
%{_datadir}/gnome/help/gmameui/
%{_datadir}/omf/gmameui/
%{_mandir}/man6/gmameui.6*


%changelog
* Tue Mar 12 2013 Nicolas Chauvet <kwizart@gmail.com> - 0.2.13-0.4.20120704cvs
- https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Wed Jul 11 2012 Sérgio Basto <sergio@serjux.com> - 0.4.13-0.2.20120704cvs
- add Requires: gnome-icon-theme-legacy, to fix more missing icons.

* Sat Jul 07 2012 Sérgio Basto <sergio@serjux.com> - 0.2.13-0.2.20120704cvs
- add missing icons, extract from gnome-icon-theme-gperfection2_2.3-0ubuntu3_all.deb
  gmamui relies on some old gnome-icons-theme, cleans icons errors. 

* Wed Jul 04 2012 Sérgio Basto <sergio@serjux.com> - 0.2.13-0.1.20120704cvs
- from Julian Sikorski patch for gmameui.spec : 
  - Updated the license tag to GPLv3+
  - Added vte-devel to BuildRequires, removed explicit gtk2-devel
  - Dropped empty README and TODO from %%doc
- add gmameui-fix3.patch, fix3 extends old fix2, which Fix FTBFS with
  libarchive12 
- add gmameui-fix4.patch, which force autotools use autopoint, which make autogen.sh works much more
  smooth. TODO understand why ./setup-gettext --gettext-tool, don't return autopoint in mock build.

* Sun May 13 2012 Nicolas Chauvet <kwizart@gmail.com> - 0.2.12-1
- Update to 0.2.12
- Fix build

* Thu Feb 09 2012 Nicolas Chauvet <kwizart@gmail.com> - 0.2.10-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Sun Jul  5 2009 Matthias Saou <http://freshrpms.net/> 0.2.10-1
- Update to 0.2.10.
- Add new libgnome-devel and libarchive-devel build requirements.
- Re-enable translations, they build/install again.

* Sun Mar 29 2009 Thorsten Leemhuis <fedora [AT] leemhuis [DOT] info> - 0.2.2-3
- rebuild for new F11 features

* Sat Oct 18 2008 Thorsten Leemhuis <fedora [AT] leemhuis [DOT] info - 0.2.2-2
- rebuild for RPM Fusion

* Fri May 16 2008 Matthias Saou <http://freshrpms.net/> 0.2.2-1
- Initial RPM package, based on the gxmame spec file.

