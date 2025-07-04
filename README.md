# Tk Browser

Tk Browser is a cross-platform web browser built with Python's
`tkinter` GUI toolkit. It now uses the
[`tkinterweb`](https://github.com/Andereoo/TkinterWeb) widget by default so
around **130** HTML tags and common attributes work out of the box. The built-in
reader also understands a small subset of CSS for colours and text styles and
executes simple JavaScript using `py_mini_racer`. Pages are cached locally and
helpful error messages are shown when a fetch fails.
With JavaScript mode enabled, complex sites such as
[python.org](https://www.python.org) display correctly.
It works on Windows and Linux as long as Python and the required packages are
installed. The interface supports multiple tabs and developer tools like
viewing page source. Recent updates add bookmarks, a simple history viewer,
the ability to open local HTML files, and a preferences dialog for setting the
home page and default search engine. The address bar also doubles as a search
box when you enter text that isn't a full URL. New tools include zoom controls,
dark mode, saving pages, printing and exporting bookmarks. Sessions are
restored on startup and the User-Agent and proxy can be configured. URLs can
also be blocked from the **Tools** menu (Ctrl+K) and managed via *Show
Blocklist*.

## Requirements

- Python 3.8+
- `tkinter` (usually bundled with Python on Windows and most Linux distros)
- `tkinterweb` for full HTML/CSS support
- Optional: `pywebview` for JavaScript windows
- Optional: `py_mini_racer` to execute inline JavaScript in the built-in reader

Install the dependencies with pip:

```bash
pip install tkinterweb pywebview py_mini_racer
```

## Usage

Run the browser from the command line:

```bash
python tkbrowser.py
```

A window will open with a basic address bar, navigation buttons and a Home
button. Enter a URL in the address bar and press `Go` or the Enter key to load
the page. Use the Back and Forward buttons or `Reload` to refresh the current
page. Open new tabs from the **File** menu (Ctrl+T) or open local files with
**Ctrl+O**. Add bookmarks with **Ctrl+D** and browse them with **Ctrl+B**.
The **History** menu (Ctrl+H) lists recently visited pages. The **Tools** menu
still provides an option to view the page source (Ctrl+U). Open the
**Preferences** dialog with **Ctrl+,** to choose a start page and search engine.
If the address you type isn't a full URL, it will be sent to the selected
search engine automatically.

### Security

The browser performs security checks using `security.py`. URLs added to the
blocklist are blocked from loading. In addition to verifying HTTPS and the
server certificate, the browser now includes over twenty additional checks for
common headers like HSTS, CSP and CORS. Each request is analysed by a
`SecurityManager` which can also perform rate-limiting and IP blocking. If a
check fails you'll be asked whether to proceed.

### JavaScript support

Install the optional [`pywebview`](https://pywebview.flowrl.com/) package to
enable JavaScript-capable windows. Use **JS Tools → Open JS Window** (Ctrl+J)
to open the current page in a separate WebView process. JavaScript windows are
opened externally, so running custom scripts inside them isn't supported. Enable
"Use JS Mode" from the same menu (Ctrl+Shift+J) to automatically open all pages
in an external WebView so complex sites work with JavaScript by default.

### Optional Python helper

Running `python nethelper.py` starts a small server on port `8090` that can
fetch remote pages via `/fetch?url=...`. The browser automatically starts this
server in the background when launched so you can load these URLs to experiment
with the networking helper.

## License

This project is released under the terms of the MIT License. See
[LICENSE](LICENSE) for details.
