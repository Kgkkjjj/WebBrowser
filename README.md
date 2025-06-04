# Simple GTK Web Browser

This project is a minimal web browser written in C using GTK 3 and WebKit2GTK.
It demonstrates how to build a lightweight browser with a custom home page, an address bar and basic navigation controls.

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
./browser
```

The browser opens on a simple home page. Use the address bar to enter a URL or a search query.

## Updating

An `update` directory contains a simple script to update the browser from GitHub.
Set `REPO_URL` in `update/update.sh` to your repository URL, then run:

```bash
./update/update.sh
```

The script uses `git` to fetch the latest changes from the configured repository.
