# SageShell

An interactive fish-like shell written in [SageLang](https://github.com/Night-Traders-Dev/SageLang).

## Installation
`sagepkg install SageShell`

## Features
- **🌿 Prompt**: Modern Sage-themed prompt.
- **⌨️ Key Combos**: Support for `Ctrl+L` (clear), `Ctrl+D` (exit), and `Ctrl+C`.
- **📜 History**: Navigate through previous commands with searching (Up/Down).
- **🎨 Syntax Highlighting**: Real-time coloring for built-ins and external commands.
- **💡 Autosuggestions**: History-based suggestions (Right Arrow or Tab to accept).
- **⌨️ Tab Completion**: Advanced command and path completion.
- **📊 Status Bar**: Real-time bottom bar showing current shell, time (updating every second), and CPU temperature (°F).

## Project Structure
- `universal/`: Source code.
- `aarch64/`: Native binaries for ARM64.
- `x86_64/`: Native binaries for x64.
