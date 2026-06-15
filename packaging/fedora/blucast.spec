Name:           blucast
Version:        0.1.0
Release:        1%{?dist}
Summary:        AI-powered virtual camera and microphone (NVIDIA Maxine)

License:        MIT
URL:            https://github.com/Andrei9383/BluCast
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildArch:      noarch
BuildRequires:  desktop-file-utils

# akmod-v4l2loopback ships in RPM Fusion (free); podman runs the GPU container.
Requires:       akmod-v4l2loopback
Requires:       podman
Requires:       pipewire-pulseaudio
Recommends:     nvidia-container-toolkit

%description
BluCast provides NVIDIA Broadcast-style real-time effects on Linux: background
blur/replacement on a virtual camera and noise/echo removal on a virtual
microphone, powered by the NVIDIA Maxine SDK inside a GPU container.

This package installs the launcher and system integration. Build the GPU
container image once after installation with:
  blucast --build --sdk=/path/to/sdk.tar.gz

%prep
%autosetup -n BluCast-%{version}

%build
# Nothing to compile here; the GPU container is built at runtime.

%install
install -Dm0755 packaging/common/blucast %{buildroot}%{_bindir}/blucast
install -Dm0644 packaging/common/modules-load.d/blucast.conf \
    %{buildroot}%{_prefix}/lib/modules-load.d/blucast.conf
install -Dm0644 packaging/common/modprobe.d/blucast-v4l2loopback.conf \
    %{buildroot}%{_prefix}/lib/modprobe.d/blucast-v4l2loopback.conf
install -Dm0644 packaging/common/udev/83-blucast-vcam.rules \
    %{buildroot}%{_prefix}/lib/udev/rules.d/83-blucast-vcam.rules
install -Dm0644 packaging/common/pipewire/blucast-vmic.conf \
    %{buildroot}%{_datadir}/pipewire/pipewire-pulse.conf.d/blucast-vmic.conf
install -Dm0644 packaging/common/blucast.desktop \
    %{buildroot}%{_datadir}/applications/blucast.desktop
install -Dm0644 assets/logo.svg \
    %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/blucast.svg
install -Dm0644 Containerfile %{buildroot}%{_datadir}/blucast/Containerfile
cp -r app    %{buildroot}%{_datadir}/blucast/app
cp -r assets %{buildroot}%{_datadir}/blucast/assets
install -Dm0755 scripts/vcam_watcher.sh %{buildroot}%{_datadir}/blucast/scripts/vcam_watcher.sh
install -Dm0755 scripts/vmic_watcher.sh %{buildroot}%{_datadir}/blucast/scripts/vmic_watcher.sh

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/blucast.desktop

%post
udevadm control --reload-rules 2>/dev/null || true
udevadm trigger 2>/dev/null || true
modprobe v4l2loopback 2>/dev/null || true

%postun
udevadm control --reload-rules 2>/dev/null || true

%files
%license LICENSE
%{_bindir}/blucast
%{_prefix}/lib/modules-load.d/blucast.conf
%{_prefix}/lib/modprobe.d/blucast-v4l2loopback.conf
%{_prefix}/lib/udev/rules.d/83-blucast-vcam.rules
%{_datadir}/pipewire/pipewire-pulse.conf.d/blucast-vmic.conf
%{_datadir}/applications/blucast.desktop
%{_datadir}/icons/hicolor/scalable/apps/blucast.svg
%{_datadir}/blucast/

%changelog
* Mon Jun 15 2026 BluCast contributors <noreply@example.com> - 0.1.0-1
- Initial native packaging replacing install.sh/run.sh.
