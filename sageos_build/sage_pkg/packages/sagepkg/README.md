# sagepkg

The SageLang Package Manager.

## Installation
`sagepkg` is usually installed during the SageLang setup or by cloning the `SagePkg` repository.

## Commands
- `sagepkg list`: List available packages.
- `sagepkg install <name>`: Install a package.
- `sagepkg remove <name>`: Remove a package.
- `sagepkg build <name>`: Build a package from source.
- `sagepkg update`: Sync the package repository and self-update.

## Features
- **📦 Binary & Source Support**: Prefers binaries but falls back to building from source if needed.
- **🔄 Auto-update**: Can update itself and its package list.
- **🚀 Simple Integration**: Easily add and run new SageLang tools.

## Project Structure
- `universal/`: Source code.
- `metadata.json`: Package metadata.
