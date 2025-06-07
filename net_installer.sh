#!/bin/sh
# Net Installer for OpenB Web Browser
# This script clones the latest source, compiles it, and installs the browser.
# The data directory is copied so the home page works when running the installed binary.

set -e

REPO="https://github.com/Kgkkjjj/WebBrowser.git"
INSTALL_DIR="/usr/local/bin"
SHARE_DIR="/usr/local/share/openb"

for cmd in git make gcc pkg-config curl tar; do
    command -v "$cmd" >/dev/null 2>&1 || {
        echo "$cmd is required but not installed." >&2
        exit 1
    }
done

if command -v apt-get >/dev/null 2>&1; then
    echo "Updating package lists and installing dependencies..."
    sudo apt-get update -y
    sudo apt-get install -y build-essential libgtk-3-dev libwebkit2gtk-4.1-dev \
        python3 python3-venv git
fi

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT
cd "$TMP_DIR"

echo "Cloning OpenB..."
if ! git clone --depth 1 "$REPO" src; then
    echo "Clone failed" >&2
    exit 1
fi

cd src

echo "Building..."
make

if [ $(id -u) -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        echo "Installing to $INSTALL_DIR requires root privileges."
        sudo install -m 755 openb "$INSTALL_DIR/openb" || { echo "Install failed" >&2; exit 1; }
        sudo mkdir -p "$SHARE_DIR"
        sudo cp -r data/* "$SHARE_DIR/" || { echo "Data install failed" >&2; exit 1; }
        sudo mkdir -p "$SHARE_DIR/extensions"
        sudo cp -r extensions/* "$SHARE_DIR/extensions/" 2>/dev/null || true
        sudo cp server.py "$SHARE_DIR/" || true
    else
        echo "Run this script as root to install the binary." >&2
        exit 1
    fi
else
    install -m 755 openb "$INSTALL_DIR/openb" || { echo "Install failed" >&2; exit 1; }
    mkdir -p "$SHARE_DIR"
    cp -r data/* "$SHARE_DIR/" || { echo "Data install failed" >&2; exit 1; }
    mkdir -p "$SHARE_DIR/extensions"
    cp -r extensions/* "$SHARE_DIR/extensions/" 2>/dev/null || true
    cp server.py "$SHARE_DIR/" || true
fi

echo "Installation complete. You can run the browser with 'openb'."
