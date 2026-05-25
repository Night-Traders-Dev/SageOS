# SagePkg

The official package repository for [SageLang](https://github.com/Night-Traders-Dev/SageLang).

## Installation

To install the `sagepkg` tool itself:

```bash
# Clone the repository
git clone https://github.com/Night-Traders-Dev/SagePkg
cd SagePkg

# Install using Makefile
make install
```

This will install `sagepkg` to `~/.sagepkg/bin` and update your shell's `PATH`.

## How to use SagePkg

### 1. Sync and Initialize
```bash
sagepkg update
```
This downloads the latest package list and ensures your environment is ready.

### 2. Install a package
```bash
sagepkg install SageFetch
```
`sagepkg` will prefer a pre-compiled binary for your architecture. If one is not available, it will automatically download the source and build it for you.

### 3. Build from source
To force building a package from source:
```bash
sagepkg build SageShell
```

### 4. Remove a package
To uninstall a package:
```bash
sagepkg remove SageShell
```

### 5. Run by name
Once installed, you can call packages directly:
```bash
SageFetch
```

## Available Packages
- **SageFetch**: A high-performance Neofetch clone with robust CPU and shell detection.
- **SageShell**: A modern fish-like shell featuring syntax highlighting, autosuggestions, tab completion, and a real-time status bar.
- **SageNano**: A nano-inspired terminal text editor with syntax highlighting, search, and page navigation.
- **SageUtils**: Common terminal utilities implemented in SageLang.

## Development

This repository includes [SageLang](https://github.com/Night-Traders-Dev/SageLang) as a submodule to facilitate cross-development and debugging.

```bash
# Clone with submodules
git clone --recursive https://github.com/Night-Traders-Dev/SagePkg

# Or update existing clone
git submodule update --init --recursive
```

---
© 2026 Night Traders Dev
