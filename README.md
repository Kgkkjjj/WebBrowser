# OpenB - GTK Web Browser

OpenB is a small web browser written in C using GTK 3 and WebKit2GTK. It provides a simple interface with keyboard shortcuts and a customizable home page. Pages load quickly thanks to WebKit's disk cache, which is stored under `~/.cache/openb`. The toolbar includes navigation controls, a new window button and an **Info** dialog describing the browser.

## Building

Ensure the GTK 3 and WebKit2GTK development packages are installed.

On **Debian/Ubuntu** systems:

```bash
sudo apt-get install build-essential libgtk-3-dev libwebkit2gtk-4.1-dev
# for older releases use libwebkit2gtk-4.0-dev instead
```

On **Arch Linux**:

```bash
sudo pacman -S base-devel gtk3 webkit2gtk
```

Build the application with:

```bash
make
```

## Running

Run the compiled binary:

```bash
./openb
```

The browser opens on a home page defined in `data/home.html`. It now sports a
cleaner layout with a larger search box and flexible link tiles. Feel free to
customize this file to suit your preferences. When installed via the net
installer, this home page is copied to `/usr/local/share/openb` so the browser
can find it from anywhere. Use the address bar to enter a URL or a search query.
The toolbar offers back, forward, reload, stop, home and info buttons. Click the
**Info** button to see details about the browser. A status bar at the bottom
displays page load progress and link targets.

OpenB stores persistent data in `~/.local/share/openb` and caches pages under
`~/.cache/openb` for faster loading.

## Updating

An `update` directory contains a simple script to pull the latest changes from
[this repository](https://github.com/Kgkkjjj/WebBrowser/tree/codex/build-web-browser-with-gtk-3-gui).
Run the update script from the project root with:

```bash
./update/update.sh
```

If the current directory is not a git repository and already contains files,
the script will abort instead of cloning. Make sure to run it inside the project
directory or an empty folder.

The script uses `git` to fetch the latest changes and then rebuilds the
browser. You can change the repository URL by editing the `REPO_URL`
variable in `update/update.sh`.

## Net Installer

A script `net_installer.sh` is provided for installing OpenB directly from the
internet. It performs a shallow clone of this repository, builds the browser and
installs the `openb` binary to `/usr/local/bin`. The entire `data`
directory is copied to `/usr/local/share/openb` so the browser can load
the start page even when run outside the source tree.

Run it with:

```bash
./net_installer.sh
```

Root privileges are required for the installation step. Ensure the required build
dependencies for GTK and WebKit2GTK are installed on your system.

