# BluCast packaging

Native packages for Arch, NixOS, Fedora, and Debian/Ubuntu. They replace the old
`install.sh`/`run.sh` with proper system integration:

- the `blucast` launcher on `$PATH` (no privileged host mutation at runtime),
- `v4l2loopback` loaded at boot via `modules-load.d` + configured via `modprobe.d`,
- the virtual camera made user-accessible via a `udev` `uaccess` rule,
- the **virtual microphone** created declaratively by a PipeWire drop-in,
- a `.desktop` entry + themed icon.

The packages **do not** contain the proprietary NVIDIA Maxine SDK. After installing,
build the GPU container image once from your own SDK:

```bash
blucast --build --sdk=/path/to/sdk.tar.gz   # or --sdk=/path/to/extracted/sdk/
```

`sdk.tar.gz` must expand to a `sdk/` directory containing `VideoFX/`, `AudioFX/`,
`TensorRT-8.5.1.7/`, and `cudnn/` (see the top-level README for how to obtain these).

Then restart PipeWire so the virtual mic appears, and launch:

```bash
systemctl --user restart pipewire pipewire-pulse wireplumber
blucast
```

## Layout

```
packaging/
  common/        files shared by every package (installed to /usr/...)
    blucast                          → /usr/bin/blucast (launcher)
    modules-load.d/blucast.conf      → /usr/lib/modules-load.d/
    modprobe.d/blucast-v4l2loopback.conf → /usr/lib/modprobe.d/
    udev/83-blucast-vcam.rules       → /usr/lib/udev/rules.d/
    pipewire/blucast-vmic.conf       → /usr/share/pipewire/pipewire-pulse.conf.d/
    blucast.desktop                  → /usr/share/applications/
  arch/      PKGBUILD + blucast.install
  debian/    debhelper packaging (control, rules, postinst, ...)
  fedora/    blucast.spec
  nix/       package.nix + module.nix   (flake.nix is at the repo root)
```

## Build / install per distro

### Arch (AUR-style)
```bash
cd packaging/arch
makepkg -si            # builds + installs; pulls v4l2loopback-dkms, podman
```

### Debian / Ubuntu
```bash
cp -r packaging/debian debian
dpkg-buildpackage -b -us -uc
sudo apt install ../blucast_*.deb    # pulls v4l2loopback-dkms, podman|docker.io
```

### Fedora (requires RPM Fusion for akmod-v4l2loopback)
```bash
sudo dnf install https://mirrors.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
rpmdev-setuptree
ver=$(rpmspec -q --qf '%{version}\n' packaging/fedora/blucast.spec | head -1)
tar --transform "s,^,BluCast-${ver}/," -czf ~/rpmbuild/SOURCES/blucast-${ver}.tar.gz \
    packaging app assets scripts Containerfile LICENSE
rpmbuild -bb packaging/fedora/blucast.spec
sudo dnf install ~/rpmbuild/RPMS/noarch/blucast-*.rpm
```

### NixOS (flake + module)
```nix
# flake.nix
{
  inputs.blucast.url = "github:Andrei9383/BluCast";
  outputs = { nixpkgs, blucast, ... }: {
    nixosConfigurations.myhost = nixpkgs.lib.nixosSystem {
      modules = [ blucast.nixosModules.blucast { programs.blucast.enable = true; } ];
    };
  };
}
```
`nixos-rebuild switch` wires v4l2loopback, the PipeWire virtual mic, udev, podman,
and installs the app declaratively. Then `blucast --build --sdk=...` once.

## CI

`.github/workflows/packaging.yml` lints all of the above and **builds** the `.deb`,
the RPM, and the Nix package on every change (none of those need the proprietary SDK).
The container image is not built in CI because it bakes in the non-redistributable SDK.
