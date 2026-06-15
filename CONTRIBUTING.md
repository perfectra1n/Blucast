# Contributing to BluCast

Thank you for your interest in contributing to BluCast! This document provides guidelines and information for contributors.

## Code of Conduct

This project adheres to a [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code.

## How to Contribute

### Reporting Bugs

Before creating a bug report, please check the existing issues to avoid duplicates. When filing a bug report, include:

- **Clear title** describing the issue
- **Steps to reproduce** the problem
- **Expected behavior** vs actual behavior
- **System information**: OS, GPU model, driver version
- **Logs** from the terminal output if applicable

### Suggesting Features

Feature requests are welcome! Please:

- Check existing issues and discussions first
- Clearly describe the use case
- Explain why this would benefit other users

### Pull Requests

1. **Fork the repository** and create your branch from `main`
2. **Follow the code style** of the project
3. **Write clear commit messages**
4. **Test your changes** thoroughly
5. **Update documentation** if needed
6. **Submit the PR** with a clear description

## Development Setup

### Prerequisites

- Linux (Fedora or Ubuntu recommended)
- NVIDIA GPU with drivers
- Podman or Docker with NVIDIA Container Toolkit
- Python 3.8+
- CMake 3.10+
- GCC/G++ with C++17 support

### Setting Up for Development

1. Clone your fork:
   ```bash
   git clone https://github.com/yourusername/blucast.git
   cd blucast
   ```

2. Download the NVIDIA Maxine SDK (VideoFX + AudioFX) from [NVIDIA NGC](https://catalog.ngc.nvidia.com/),
   plus TensorRT/cuDNN, and arrange a `sdk/` directory (see the README).

3. Build and run from the source tree (no install needed) by pointing the launcher at the repo:
   ```bash
   BLUCAST_DATADIR="$PWD" packaging/common/blucast --build --sdk=sdk.tar.gz
   BLUCAST_DATADIR="$PWD" packaging/common/blucast
   ```

### Code Structure

```
blucast/
├── app/
│   ├── control_panel.py     # Qt GUI application
│   ├── server.cpp           # C++ video/audio processing server
│   ├── audio_processor.h    # NvAFX (microphone effects) wrapper
│   ├── audio_io.h           # PulseAudio capture / virtual-mic I/O
│   └── CMakeLists.txt       # Build configuration
├── scripts/
│   ├── vcam_watcher.sh      # Virtual camera consumer monitor
│   └── vmic_watcher.sh      # Virtual microphone consumer monitor
├── Containerfile            # Container build definition
└── packaging/               # Native packages (Arch/Debian/Fedora/Nix) + launcher
    ├── common/blucast       # The `blucast` launcher (installed to /usr/bin)
    └── README.md            # Per-distro build/install instructions
```

### Code Style

**Python:**
- Follow PEP 8
- Use type hints where appropriate
- Docstrings for classes and functions

**C++:**
- C++17 standard
- Use meaningful variable names
- Comment complex logic

**Shell scripts:**
- Use `set -e` for error handling
- Quote variables: `"$var"`
- Use `[[ ]]` for conditionals

### Testing

Before submitting a PR:

1. Test with different effect modes
2. Verify virtual camera works in video apps
3. Check for memory leaks with valgrind (if applicable)
4. Test on different GPU generations if possible

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

## Questions?

Feel free to open an issue or discussion for any questions about contributing.
