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

An `update` directory contains a simple script to pull the latest changes from
[this repository](https://github.com/Kgkkjjj/WebBrowser/tree/codex/build-web-browser-with-gtk-3-gui).
To update the browser, run:

```bash
./update/update.sh
```

The script uses `git` to fetch the latest changes from the configured repository.
The repository URL can be changed by editing the `REPO_URL` variable in
`update/update.sh`.
