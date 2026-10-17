# Built in place from a full checkout (with submodules):
#   rpmbuild -bb --build-in-place --define "ver 6.2.0" --define "rel keymap.1" packaging/rpm/moonlight-qt-keymap.spec
%global debug_package %{nil}

Name:           moonlight-qt-keymap
Version:        %{ver}
Release:        %{rel}%{?dist}
Summary:        Moonlight game streaming client (fork with custom key combos and multi-display switching)
License:        GPL-3.0-or-later
URL:            https://github.com/viteck1048/moonlight-qt

BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtbase-private-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  openssl-devel
BuildRequires:  sdl2-compat-devel
BuildRequires:  SDL2_ttf-devel
BuildRequires:  ffmpeg-free-devel
BuildRequires:  opus-devel
BuildRequires:  libva-devel
BuildRequires:  libvdpau-devel
BuildRequires:  libdrm-devel
BuildRequires:  wayland-devel
BuildRequires:  libX11-devel
BuildRequires:  libplacebo-devel
BuildRequires:  vulkan-headers

# Same binary and desktop file names as the upstream package
Conflicts:      moonlight-qt

%description
Moonlight is an open source client for NVIDIA GameStream and Sunshine.
This build comes from the viteck1048 fork and adds user-defined key
combos, host display cycling and Cyrillic text in the streaming overlay.

%prep

%build
qmake6 PREFIX=%{_prefix} CONFIG+=release moonlight-qt.pro
%make_build

%install
make install INSTALL_ROOT=%{buildroot}

%files
%license LICENSE
%{_bindir}/moonlight
%{_datadir}/applications/com.moonlight_stream.Moonlight.desktop
%{_datadir}/icons/hicolor/scalable/apps/moonlight.svg
%{_metainfodir}/com.moonlight_stream.Moonlight.appdata.xml
