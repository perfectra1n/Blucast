<div align="center">


<img src="assets/logo.svg" alt="BluCast Logo" width="64" /> <h1 align="center">BluCast</h1>

<p align="center">
  Real-time AI-powered camera <em>and microphone</em> effects using the NVIDIA Maxine SDK.<br>
  Basically NVIDIA Broadcast, but for Linux.
</p>

</div>

<p align="center">
  <img src="assets/preview.png" alt="BluCast preview" width="300" />
</p>


<!-- omit from toc -->
## Table of Contents
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
  - [Arch](#arch)
  - [Fedora](#fedora)
  - [Debian / Ubuntu](#debian--ubuntu)
  - [NixOS](#nixos)
  - [Build the GPU image (all distros, once)](#build-the-gpu-image-all-distros-once)
- [Usage](#usage)
- [Configuration](#configuration)
- [Obtaining the NVIDIA Maxine SDK](#obtaining-the-nvidia-maxine-sdk)
- [How install works (no more sudo hacks)](#how-install-works-no-more-sudo-hacks)
- [Uninstalling](#uninstalling)
- [Troubleshooting](#troubleshooting)
- [License](#license)
- [Contributing](#contributing)
- [Acknowledgments](#acknowledgments)


## Features

**Camera**
- **Background Removal**
- **Background Replacement** — use any image as your background
- **Background Blur**

**Microphone**
- **Noise Removal** — background noise suppression
- **Room Echo Removal** — de-reverberation
- **Studio Voice** — voice enhancement
- Exposed as a **"BluCast Virtual Microphone"** any app can select

**General**
- **On-demand usage** — camera/GPU only active when something is consuming the feed
- **Native Wayland and X11 support**

## Prerequisites

- **NVIDIA GPU** with drivers + CUDA — verify with `nvidia-smi`
- **Podman or Docker**
- **[NVIDIA Container Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html)** — GPU passthrough into the container
- **(GNOME only):** [AppIndicator extension](https://extensions.gnome.org/extension/615/appindicator-support/) for the tray icon

The `v4l2loopback` kernel module and the virtual microphone are set up **for you** by the
package (it depends on your distro's `v4l2loopback-dkms`/`akmod-v4l2loopback`).

## Installation

BluCast now ships as a native package per distro instead of a `curl | bash` script. The package
installs the launcher and all system integration; you then build the GPU image once (below).

> The packages do **not** redistribute the proprietary NVIDIA SDK — you supply it at image-build
> time. See [Obtaining the NVIDIA Maxine SDK](#obtaining-the-nvidia-maxine-sdk).

### Arch
```bash
cd packaging/arch && makepkg -si
```

### Fedora
```bash
# Enable RPM Fusion (provides akmod-v4l2loopback), then build & install:
sudo dnf install https://mirrors.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
# (build steps in packaging/README.md)
sudo dnf install ~/rpmbuild/RPMS/noarch/blucast-*.rpm
```

### Debian / Ubuntu
```bash
cp -r packaging/debian debian && dpkg-buildpackage -b -us -uc
sudo apt install ../blucast_*.deb
```

### NixOS
```nix
# add the flake input, then in your system modules:
imports = [ blucast.nixosModules.blucast ];
programs.blucast.enable = true;
```
`nixos-rebuild switch` wires v4l2loopback, the virtual mic, udev and podman declaratively.

Full per-distro build instructions: [`packaging/README.md`](packaging/README.md).

### Build the GPU image (all distros, once)
```bash
blucast --build --sdk=/path/to/sdk.tar.gz      # or an extracted sdk/ directory
systemctl --user restart pipewire pipewire-pulse wireplumber   # expose the virtual mic
```

> [!TIP]
> **Firefox users:** if the virtual camera doesn't appear, set
> `media.webrtc.camera.allow-pipewire` to `false` in `about:config`.

## Usage

1. Launch `blucast` from a terminal or your application menu.
2. **Camera:** pick an effect (blur / replace / remove); for replace, click **Browse**.
3. **Microphone:** toggle the mic effects on, pick noise/echo/studio, set strength, choose your input mic.
4. In your conferencing app select **"BluCast Virtual Camera"** and **"BluCast Virtual Microphone"**.

## Configuration

Settings live in `~/.config/blucast/settings.json` and persist between sessions:

```json
{
  "effect_mode"     : "blur",     // "blur" | "replace" | "remove" | "none"
  "background_image": "",
  "blur_strength"   : 50,         // 0-100
  "resolution"      : "1280x720",
  "fps"             : 30,
  "input_device"    : "/dev/video0",
  "audio_enabled"   : false,
  "audio_effect"    : "denoise",  // "none" | "denoise" | "dereverb" | "studio"
  "audio_intensity" : 100,        // 0-100
  "audio_device"    : ""          // pulse source name; "" = default mic
}
```

## Obtaining the NVIDIA Maxine SDK

> [!NOTE]
> The Maxine SDK (v0.7.2.0) requires specific versions: **CUDA 11.8.0, cuDNN 8.6.0.163,
> TensorRT 8.5.1.7**.

From the [NVIDIA NGC Catalog](https://catalog.ngc.nvidia.com/): download the **Video Effects SDK**,
the **Audio Effects SDK**, **TensorRT 8.5.x**, and **cuDNN 8.x** for Linux, then arrange a `sdk/`
directory:

```
sdk/
├── VideoFX/
├── AudioFX/
├── TensorRT-8.5.1.7/
└── cudnn/
```

Pack it (`tar -czf sdk.tar.gz sdk/`) and pass it to `blucast --build --sdk=sdk.tar.gz`.

## How install works (no more sudo hacks)

The old `install.sh` mutated the host imperatively (`sudo modprobe`, writing `/etc/modprobe.d`,
`/etc/udev`, a NOPASSWD `sudoers` entry, runtime `pactl load-module`). The packages replace all of
that with the Linux norm:

| Concern | How it's handled now |
|---|---|
| Load `v4l2loopback` | `/usr/lib/modules-load.d/blucast.conf` (boot) + dependency on `*-dkms`/`akmod` |
| Module options | `/usr/lib/modprobe.d/blucast-v4l2loopback.conf` |
| Camera permissions | `udev` `uaccess` rule — no `chmod 666`, no `sudoers` |
| Virtual microphone | declarative PipeWire drop-in — no runtime `pactl` |
| Launch | `/usr/bin/blucast` (a normal program, no privilege escalation) |

## Uninstalling

Use your package manager — it reverses everything the package installed:

```bash
sudo pacman -R blucast          # Arch
sudo apt purge blucast          # Debian/Ubuntu
sudo dnf remove blucast         # Fedora
# NixOS: set programs.blucast.enable = false; and rebuild
podman rmi localhost/blucast:latest   # remove the built image (optional)
```

## Troubleshooting

**No camera detected** — `ls /dev/video*`; confirm you're in the `video` group.

**Virtual camera missing** — it loads at boot; if absent right after install, reboot or
`sudo modprobe v4l2loopback`.

**Virtual mic missing** — restart PipeWire: `systemctl --user restart pipewire pipewire-pulse wireplumber`,
then check `pactl list short sources | grep BluCast`.

**GPU errors** — `nvidia-smi`; test passthrough:
`podman run --rm --device nvidia.com/gpu=all nvidia/cuda:11.8.0-base-ubuntu20.04 nvidia-smi`.

**Error setting up CDI** (`unresolvable CDI devices nvidia.com/gpu=all`) — generate the spec:
`sudo nvidia-ctk cdi generate --output=/etc/cdi/nvidia.yaml`.

## License

MIT — see [LICENSE](LICENSE).

**Third-party:** NVIDIA Maxine SDK · OpenCV · PySide6 · TensorRT.

## Contributing

Contributions welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).

## Acknowledgments

- NVIDIA Maxine team for the VideoFX/AudioFX SDKs
- OpenCV community
- Qt/PySide6 project
