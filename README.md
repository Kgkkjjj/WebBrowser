# Simple GTK Web Browser

This project is a minimal web browser written in C using GTK 3 and WebKit2GTK.
It demonstrates how to build a lightweight browser with a custom home page and a search entry.

## Building

Ensure the GTK 3 and WebKit2GTK development packages are installed. On Ubuntu:

```bash
sudo apt-get install build-essential libgtk-3-dev libwebkit2gtk-4.1-dev
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

The browser starts on a basic home page with a search bar. Enter a query and press `Enter` to search using DuckDuckGo.
