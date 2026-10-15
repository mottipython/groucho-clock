Name:           groucho-clock
Version:        %{version}
Release:        1%{?dist}
Summary:        The Groucho Clock, a cartoon screensaver clock of Groucho Marx
License:        MIT
Source0:        groucho-clock-%{version}.tar.gz
BuildRequires:  gcc, make, pkgconfig(sdl2)
Requires:       swayidle

%description
The Groucho Clock, a cartoon screensaver that tells the time with Groucho
Marx's eyes: the lenses of his glasses are two clock faces, and his pupils are
the hands, the left one on the hour and the right one on the minutes. His
eyebrows waggle every minute, his cigar burns down through the hour, and on the
hour he says something. Now and then his brother Gummo peeks in.

On Wayland desktops it starts by itself after five minutes idle.
groucho-clock-screensaver changes the delay or turns it off.

%prep
%setup -q

%build
make %{?_smp_mflags} CFLAGS="%{optflags}" LDFLAGS="%{?__global_ldflags}"

%install
make install DESTDIR=%{buildroot} PREFIX=%{_prefix} SYSCONFDIR=%{_sysconfdir}
rm -rf %{buildroot}%{_datadir}/doc/groucho-clock

%files
%license LICENSE
%doc README.md
%{_bindir}/groucho-clock
%{_bindir}/groucho-clock-screensaver
%{_datadir}/applications/groucho-clock.desktop
%dir %{_datadir}/screensaver-service
%dir %{_datadir}/screensaver-service/savers
%{_datadir}/screensaver-service/savers/groucho-clock.desktop
%{_datadir}/icons/hicolor/256x256/apps/groucho-clock.png
%config(noreplace) %{_sysconfdir}/xdg/autostart/groucho-clock-screensaver.desktop
