#!/bin/bash
set -e

SAGEPKG_HOME="$HOME/.sagepkg"
BIN_DIR="$SAGEPKG_HOME/bin"
PKGS_DIR="$SAGEPKG_HOME/pkgs"

echo "Installing SagePkg..."

# Create directories
mkdir -p "$BIN_DIR"
mkdir -p "$PKGS_DIR"

# Canonical sagepkg script lives under packages/ — no root-level duplicate.
cp packages/sagepkg/universal/sagepkg.sage "$BIN_DIR/sagepkg"
cp lib/json.sage "$BIN_DIR/json.sage"
chmod +x "$BIN_DIR/sagepkg"

echo "SagePkg script installed to $BIN_DIR/sagepkg"

# Add to PATH
SHELL_NAME=$(basename "$SHELL")
CONFIG_FILE=""

case "$SHELL_NAME" in
    bash)
        CONFIG_FILE="$HOME/.bashrc"
        ;;
    zsh)
        CONFIG_FILE="$HOME/.zshrc"
        ;;
    fish)
        CONFIG_FILE="$HOME/.config/fish/config.fish"
        ;;
    SageShell)
        CONFIG_FILE="$HOME/.sageshellrc"
        ;;
    *)
        echo "Unknown shell: $SHELL_NAME. Please add $BIN_DIR to your PATH manually."
        ;;
esac

if [ -n "$CONFIG_FILE" ]; then
    if ! grep -q "$BIN_DIR" "$CONFIG_FILE" 2>/dev/null; then
        echo "Adding $BIN_DIR to PATH in $CONFIG_FILE..."
        if [ "$SHELL_NAME" == "fish" ]; then
            echo -e "\n# SagePkg PATH\nset -gx PATH \"$BIN_DIR\" \$PATH" >> "$CONFIG_FILE"
        else
            echo -e "\n# SagePkg PATH\nexport PATH=\"$BIN_DIR:\$PATH\"" >> "$CONFIG_FILE"
        fi
    fi
fi

# Fallback: always try to update .bashrc and .profile if they exist
for fallback in "$HOME/.bashrc" "$HOME/.profile"; do
    if [ -f "$fallback" ] && ! grep -q "$BIN_DIR" "$fallback"; then
        echo "Adding $BIN_DIR to PATH in $fallback..."
        echo -e "\n# SagePkg PATH\nexport PATH=\"$BIN_DIR:\$PATH\"" >> "$fallback"
    fi
done

echo "Done!"
