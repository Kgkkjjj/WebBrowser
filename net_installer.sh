#!/bin/sh
# Net Installer for OpenB Web Browser
# This script downloads the latest binary release and
# installs the browser. The data directory is copied so
# the home page works when running the installed binary.

set -e

ARCHIVE_URL="https://github.com/Kgkkjjj/WebBrowser/releases/latest/download/openb.tar.gz"
INSTALL_DIR="/usr/local/bin"
SHARE_DIR="/usr/local/share/openb"

for cmd in curl tar; do
    command -v "$cmd" >/dev/null 2>&1 || {
        echo "$cmd is required but not installed." >&2
        exit 1
    }
done

if command -v apt-get >/dev/null 2>&1; then
    echo "Updating package lists and installing dependencies..."
    sudo apt-get update -y
    sudo apt-get install -y build-essential libgtk-3-dev libwebkit2gtk-4.1-dev
fi

if command -v pkg-config >/dev/null 2>&1; then
    pkg-config --exists gtk+-3.0 webkit2gtk-4.1 || echo "GTK or WebKit2GTK development packages missing."
fi

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$TMP_DIR"
echo "Downloading OpenB..."
if ! curl -L "$ARCHIVE_URL" -o openb.tar.gz; then
    echo "Download failed" >&2
    exit 1
fi

echo "Extracting..."
if ! tar -xzf openb.tar.gz; then
    echo "Extraction failed" >&2
    exit 1
fi

if [ $(id -u) -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        echo "Installing to $INSTALL_DIR requires root privileges."
        sudo cp openb "$INSTALL_DIR/" || { echo "Install failed" >&2; exit 1; }
        sudo mkdir -p "$SHARE_DIR"
        sudo cp -r data/* "$SHARE_DIR/" || { echo "Data install failed" >&2; exit 1; }
        sudo mkdir -p "$SHARE_DIR/extensions"
        sudo cp -r extensions/* "$SHARE_DIR/extensions/" 2>/dev/null || true
    else
        echo "Run this script as root to install the binary." >&2
        exit 1
    fi
else
    cp openb "$INSTALL_DIR/" || { echo "Install failed" >&2; exit 1; }
    mkdir -p "$SHARE_DIR"
    cp -r data/* "$SHARE_DIR/" || { echo "Data install failed" >&2; exit 1; }
    mkdir -p "$SHARE_DIR/extensions"
    cp -r extensions/* "$SHARE_DIR/extensions/" 2>/dev/null || true
fi

echo "Installation complete. You can run the browser with 'openb'."
