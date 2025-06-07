# OpenB - GTK Web Browser

OpenB is a small web browser written in C using GTK 3 and WebKit2GTK. It provides a simple interface with keyboard shortcuts and a customizable home page. Pages load quickly thanks to WebKit's disk cache, which is stored under `~/.cache/openb`. The toolbar includes navigation controls, a new window button and an **Info** dialog describing the browser.

## Building

Ensure the GTK 3 and WebKit2GTK development packages are installed.

Python 3 with the `venv` module is required to run the built-in backend server.

On **Debian/Ubuntu** systems:

```bash
sudo apt-get install build-essential libgtk-3-dev libwebkit2gtk-4.1-dev
# for older releases use libwebkit2gtk-4.0-dev instead
```

On **Arch Linux**:

```bash
sudo pacman -S base-devel gtk3 webkit2gtk
```

Build the application from the top level of the source tree with:

```bash
make
```
If you see `make: *** No targets specified and no makefile found.  Stop.`
ensure that you are running the command inside the OpenB source
directory where `Makefile` resides.  Releases installed with
`net_installer.sh` already contain a prebuilt binary so they do not
include the Makefile.

## Running

Run the compiled binary:

```bash
./openb
```

The browser opens on an interactive home page located in `data/home.html`.
This page includes about sixty lines of HTML and JavaScript while its style
rules live in a separate `styles.css` file.
A search box sits above thirty colourful tiles generated with JavaScript that
link to `downloads.html`, `news.html`, `devtools.html` and `extensions.html`.
Each tile logs a short message when clicked so you can confirm the JavaScript
is working. Queries entered in the search box open `search.html`.
Both the home page and search page rely on a small backend server that proxies
requests to the DuckDuckGo API. OpenB automatically creates a Python virtual
environment under `~/.local/share/openb/venv`, installs the `requests`
dependency, and starts `server.py` on `http://localhost:8080/` when the browser
launches. Set `OPENB_PORT` to override this. If the port is already in use the
server prints an error and the browser reuses the existing instance. The server
serves the pages from the `data` directory and exposes a
`/search` endpoint used by the JavaScript code. It also provides `/status` and
`/log` for health checks and reviewing the backend log. The tab count can be
updated through `/update_tabs`. All requests and errors are written to
`~/.cache/openb/server.log`.
When installed via the net installer the entire `data` folder and `server.py`
are copied to `/usr/local/share/openb` so the pages work even when you run the
browser outside the source tree. Use the
address bar to enter a URL or a search query; non-URL text is sent to
`search.html` for results. The toolbar offers back, forward,
reload, stop, home and info buttons as well as **Update**, **DevTools**, **Zoom**
and **View Source** controls. Additional buttons allow you to toggle JavaScript,
clear the cache, capture a screenshot, view downloads, inspect search history and
open the Extensions page. A status bar at the bottom displays page load progress
and hovered links. Every toolbar button shows text alongside its icon so new
users can quickly learn each function.

OpenB automatically sizes the main window to about 90% of the primary monitor so
it fits comfortably on different screens.

OpenB stores persistent data in `~/.local/share/openb` and caches pages under
`~/.cache/openb` for faster loading.
Downloads are saved to your standard `~/Downloads` directory and you can view
active transfers with the **Downloads** button. Search queries entered in the
address bar are kept in memory for the current session and accessible via the
**History** button.
Any loading or network errors are written to `~/.cache/openb/error.log` and
displayed in a dialog. A dedicated **Tasks** window shows active downloads and
operations. Open it with the **Tasks** toolbar button or `Ctrl+Shift+M`.
The manager also keeps an eye on memory usage. If the browser consumes more
than about 200 MB, a warning is written to the log and shown in a dialog so you
can decide whether to continue.
Tracking protection hooks into each network resource. If the browser fails
to connect the filtering signal, a warning is logged so you can investigate.

## Updating

OpenB can update itself by downloading the latest binary release from GitHub.
Click the **Update** button on the toolbar and confirm the prompt. The browser
will fetch the newest archive, install it over the current executable and then
ask you to restart. If the necessary tools (`curl` or `wget` and `tar`) are not
available, an error dialog is shown.

## Net Installer

A script `net_installer.sh` installs OpenB from the internet by cloning the
source, compiling it and copying the files into standard locations.  The
`openb` binary is installed to `/usr/local/bin` and the `data` directory is
copied to `/usr/local/share/openb` so the home page works when running outside
the source tree.  Any JavaScript files from `extensions` are installed to
`/usr/local/share/openb/extensions` alongside `server.py` for the search
backend.

Run it with:

```bash
./net_installer.sh
```

The script requires `git`, `make` and a working C compiler. It will fetch the
latest code, build the browser and install it system wide.

Root privileges are required for the installation step. Ensure the required build
dependencies for GTK and WebKit2GTK are installed on your system.

## Developer Tools

OpenB ships with several tools for debugging web pages:

- **Web Inspector** (F12)
- **View Source** (Ctrl+U)
- **Zoom controls** (Ctrl++ / Ctrl+-)
- **Toggle JavaScript** (Ctrl+J)
- **Clear Cache** (F9)
- **Take Screenshot** (Ctrl+P)
- **Manage Extensions** (Ctrl+E)
- **Reset Zoom** (Ctrl+0)
- **Find in Page** (Ctrl+F)
- **Toggle Images** (Ctrl+I)
- **Clear Cookies** (Ctrl+Shift+Del)
- **Copy URL** (Ctrl+Shift+C)
- **Paste & Go** (Ctrl+Shift+V)
- **Bookmark Page** (Ctrl+D)
- **Show Bookmarks** (Ctrl+B)
- **Open File** (Ctrl+O)
- **Save Page** (Ctrl+S)
- **Print Page** (Ctrl+Shift+P)
- **Toggle Fullscreen** (F11)
- **Toggle Dark Mode** (F2)
- **Reader Mode** (Ctrl+R)
- **Picture in Picture** (Ctrl+Shift+I)
- **Page Info** (F3)
- **Clear History** (Ctrl+Shift+H)
- **Clear Downloads** (Ctrl+Shift+D)
- **Task Manager** (Ctrl+Shift+M)
- **New Tab** (Ctrl+T)
- **Close Tab** (Ctrl+W)
- **Private Window** (Ctrl+Shift+N)

Spell checking can be enabled if dictionaries are installed. OpenB also blocks
common tracking scripts, restores sessions across launches and includes
built-in pop-up blocking.

Extensions are simple JavaScript files placed in `extensions/`. They are
injected into every page when the browser starts. Use the **Extensions** button
to open the info page.

