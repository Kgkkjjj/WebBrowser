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
cleaner layout with a larger search box and flexible link tiles linking to
`downloads.html` and `news.html`. When installed via the net installer, the
entire `data` folder is copied to `/usr/local/share/openb` so these pages work
even when you run the browser outside the source tree. Use the address bar to
enter a URL or a search query. The toolbar offers back, forward, reload, stop,
home and info buttons. The **Info** button now opens a full about dialog with
details and a link to the project website. A status bar at the bottom displays
page load progress and link targets.

OpenB stores persistent data in `~/.local/share/openb` and caches pages under
`~/.cache/openb` for faster loading.

## Updating

When running from a git checkout, OpenB can update itself without any external
scripts. Simply click the **Update** button in the toolbar and confirm the
prompt. The browser will pull the latest changes from this repository and
rebuild automatically. If OpenB was installed without the git data available,
an error dialog will inform you that updating is not possible.

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

