# OpenB - GTK Web Browser

OpenB is a small web browser written in C using GTK 3 and WebKit2GTK. It provides a simple interface with fast loading through WebKit's browser cache, keyboard shortcuts, and a customizable home page. The toolbar includes navigation controls, a new window button and an **Info** dialog describing the browser.

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

The browser opens on a simple home page defined in `data/home.html`. This page
provides a search form and a link to your downloads directory. Feel free to edit
the file to customize the welcome screen. Use the address bar to enter a URL or
a search query. The toolbar offers back, forward, reload, stop, home and info
buttons. Click the **Info** button to see details about the browser.

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
