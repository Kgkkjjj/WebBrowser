# Tk Browser

Tk Browser is a cross-platform web browser built with Python's
`tkinter` GUI toolkit and [`tkinterweb`](https://github.com/Andereoo/TkinterWeb).
It works on Windows and Linux as long as Python and the required packages are
installed. The interface supports multiple tabs and developer tools like
viewing page source. Recent updates add bookmarks, a simple history viewer,
the ability to open local HTML files, and a preferences dialog for setting the
home page and default search engine. The address bar also doubles as a search
box when you enter text that isn't a full URL.

## Requirements

- Python 3.8+
- `tkinter` (usually bundled with Python on Windows and most Linux distros)
- `tkinterweb` package for rendering HTML

Install the dependency via pip:

```bash
pip install tkinterweb
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

## License

This project is released under the terms of the MIT License. See
[LICENSE](LICENSE) for details.
