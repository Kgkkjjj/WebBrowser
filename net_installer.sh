#!/bin/sh
# Net Installer for OpenB Web Browser
# This script clones the OpenB source from the internet,
# builds it and installs the binary to /usr/local/bin.

set -e

REPO_URL="https://github.com/Kgkkjjj/WebBrowser.git"
BRANCH="codex/build-web-browser-with-gtk-3-gui"
INSTALL_DIR="/usr/local/bin"

command -v git >/dev/null 2>&1 || {
    echo "git is required but not installed." >&2
    exit 1
}

command -v make >/dev/null 2>&1 || {
    echo "make is required but not installed." >&2
    exit 1
}

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$TMP_DIR"
echo "Cloning source..."
# clone only the specified branch
if ! git clone --branch "$BRANCH" --depth 1 "$REPO_URL" openb-source; then
    echo "Failed to clone repository" >&2
    exit 1
fi

cd openb-source

echo "Building..."
if ! make; then
    echo "Build failed" >&2
    exit 1
fi

if [ $(id -u) -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        echo "Installing to $INSTALL_DIR requires root privileges."
        sudo cp openb "$INSTALL_DIR/" || { echo "Install failed" >&2; exit 1; }
    else
        echo "Run this script as root to install the binary." >&2
        exit 1
    fi
else
    cp openb "$INSTALL_DIR/" || { echo "Install failed" >&2; exit 1; }
fi

echo "Installation complete. You can run the browser with 'openb'."
