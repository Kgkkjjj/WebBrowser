import json
import os
import threading
import tkinter as tk
from tkinter import ttk
from tkinter import messagebox, filedialog, simpledialog
from tkinterweb import HtmlFrame
import tkinterweb.utilities as tku
import urllib.request
import urllib.parse
import nethelper

SETTINGS_FILE = "settings.json"
SESSION_FILE = "session.json"


class TabbedBrowser(tk.Tk):
    """A tabbed web browser using Tkinter and tkinterweb."""

    def __init__(self):
        super().__init__()
        self.title("Tk Browser")
        self.geometry("1024x768")
        self.search_engines = {
            "DuckDuckGo": "https://duckduckgo.com/?q={}",
            "Google": "https://www.google.com/search?q={}",
        }
        self.home_url = "https://www.python.org"
        self.search_engine = "DuckDuckGo"
        self.bookmarks = []
        self.history = []
        self.user_agent = "TkBrowser"
        self.proxy = ""
        self.zoom = 100
        self.dark_mode = False
        self.js_window = None
        self.js_mode = False
        self._load_settings()
        self._load_session()
        self._create_widgets()
        self.new_tab()
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def _load_settings(self):
        if os.path.exists(SETTINGS_FILE):
            try:
                with open(SETTINGS_FILE, "r", encoding="utf-8") as f:
                    data = json.load(f)
                self.home_url = data.get("home_url", self.home_url)
                self.search_engine = data.get(
                    "search_engine", self.search_engine
                )
                self.user_agent = data.get("user_agent", self.user_agent)
                self.proxy = data.get("proxy", self.proxy)
                self.js_mode = data.get("js_mode", self.js_mode)
                if self.proxy:
                    os.environ["http_proxy"] = self.proxy
                    os.environ["https_proxy"] = self.proxy
            except Exception:
                pass

    def _save_settings(self):
        data = {
            "home_url": self.home_url,
            "search_engine": self.search_engine,
            "user_agent": self.user_agent,
            "proxy": self.proxy,
            "js_mode": self.js_mode,
        }
        try:
            with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2)
        except Exception:
            pass

    def _load_session(self):
        if os.path.exists(SESSION_FILE):
            try:
                with open(SESSION_FILE, "r", encoding="utf-8") as f:
                    urls = json.load(f)
                if isinstance(urls, list):
                    for url in urls:
                        self.new_tab(url)
            except Exception:
                pass

    def _save_session(self):
        urls = []
        for tab in self.notebook.tabs():
            frame = self.notebook.nametowidget(tab)
            html = getattr(frame, "html", None)
            if html:
                urls.append(html.current_url)
        try:
            with open(SESSION_FILE, "w", encoding="utf-8") as f:
                json.dump(urls, f, indent=2)
        except Exception:
            pass

    def on_close(self):
        self._save_session()
        self._save_settings()
        self.destroy()

    def open_preferences(self, event=None):
        win = tk.Toplevel(self)
        win.title("Preferences")
        tk.Label(win, text="Home URL:").grid(
            row=0, column=0, sticky=tk.W, padx=5, pady=5
        )
        home_var = tk.StringVar(value=self.home_url)
        ttk.Entry(win, textvariable=home_var, width=40).grid(
            row=0, column=1, padx=5, pady=5
        )

        tk.Label(win, text="Search Engine:").grid(
            row=1, column=0, sticky=tk.W, padx=5, pady=5
        )
        engine_var = tk.StringVar(value=self.search_engine)
        ttk.Combobox(
            win,
            textvariable=engine_var,
            values=list(self.search_engines.keys()),
            state="readonly",
        ).grid(row=1, column=1, padx=5, pady=5)

        tk.Label(win, text="User Agent:").grid(
            row=2, column=0, sticky=tk.W, padx=5, pady=5
        )
        ua_var = tk.StringVar(value=self.user_agent)
        ttk.Entry(win, textvariable=ua_var, width=40).grid(
            row=2, column=1, padx=5, pady=5
        )

        tk.Label(win, text="Proxy URL:").grid(
            row=3, column=0, sticky=tk.W, padx=5, pady=5
        )
        proxy_var = tk.StringVar(value=self.proxy)
        ttk.Entry(win, textvariable=proxy_var, width=40).grid(
            row=3, column=1, padx=5, pady=5
        )

        def save():
            self.home_url = home_var.get() or self.home_url
            self.search_engine = engine_var.get() or self.search_engine
            self.user_agent = ua_var.get() or self.user_agent
            self.proxy = proxy_var.get() or ""
            self._save_settings()
            if self.proxy:
                os.environ["http_proxy"] = self.proxy
                os.environ["https_proxy"] = self.proxy
            else:
                os.environ.pop("http_proxy", None)
                os.environ.pop("https_proxy", None)
            win.destroy()

        ttk.Button(win, text="Save", command=save).grid(
            row=4, column=0, columnspan=2, pady=10
        )
        win.grab_set()

    def _create_widgets(self):
        self._create_menu()
        toolbar = ttk.Frame(self)
        toolbar.pack(side=tk.TOP, fill=tk.X)

        self.url_var = tk.StringVar()
        url_entry = ttk.Entry(toolbar, textvariable=self.url_var)
        url_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5, pady=5)
        url_entry.bind("<Return>", self.load_url)

        go_btn = ttk.Button(toolbar, text="Go", command=self.load_url)
        go_btn.pack(side=tk.LEFT, padx=5)

        back_btn = ttk.Button(toolbar, text="Back", command=self.go_back)
        back_btn.pack(side=tk.LEFT)

        forward_btn = ttk.Button(
            toolbar, text="Forward", command=self.go_forward
        )
        forward_btn.pack(side=tk.LEFT)

        reload_btn = ttk.Button(
            toolbar, text="Reload", command=self.reload_page
        )
        reload_btn.pack(side=tk.LEFT)

        home_btn = ttk.Button(
            toolbar, text="Home", command=self.go_home
        )
        home_btn.pack(side=tk.LEFT)

        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        self.status_var = tk.StringVar()
        status = ttk.Label(
            self, textvariable=self.status_var, relief=tk.SUNKEN, anchor=tk.W
        )
        status.pack(side=tk.BOTTOM, fill=tk.X)

    def _create_menu(self):
        menu_bar = tk.Menu(self)
        file_menu = tk.Menu(menu_bar, tearoff=False)
        file_menu.add_command(
            label="New Tab", accelerator="Ctrl+T", command=self.new_tab
        )
        file_menu.add_command(
            label="Open File", accelerator="Ctrl+O", command=self.open_file
        )
        file_menu.add_command(
            label="Save Page As", accelerator="Ctrl+S", command=self.save_page
        )
        file_menu.add_command(
            label="Print", accelerator="Ctrl+P", command=self.print_page
        )
        file_menu.add_command(
            label="Close Tab", accelerator="Ctrl+W", command=self.close_tab
        )
        file_menu.add_separator()
        file_menu.add_command(label="Quit", command=self.quit)
        menu_bar.add_cascade(label="File", menu=file_menu)

        bookmark_menu = tk.Menu(menu_bar, tearoff=False)
        bookmark_menu.add_command(
            label="Add Bookmark",
            accelerator="Ctrl+D",
            command=self.add_bookmark,
        )
        bookmark_menu.add_command(
            label="Show Bookmarks",
            accelerator="Ctrl+B",
            command=self.show_bookmarks,
        )
        bookmark_menu.add_command(
            label="Export Bookmarks", command=self.export_bookmarks
        )
        menu_bar.add_cascade(label="Bookmarks", menu=bookmark_menu)

        history_menu = tk.Menu(menu_bar, tearoff=False)
        history_menu.add_command(
            label="Show History",
            accelerator="Ctrl+H",
            command=self.show_history,
        )
        history_menu.add_command(
            label="Clear History", command=self.clear_history
        )
        menu_bar.add_cascade(label="History", menu=history_menu)

        tools_menu = tk.Menu(menu_bar, tearoff=False)
        tools_menu.add_command(
            label="View Source", accelerator="Ctrl+U", command=self.view_source
        )
        tools_menu.add_command(label="Zoom In", command=self.zoom_in)
        tools_menu.add_command(label="Zoom Out", command=self.zoom_out)
        tools_menu.add_command(label="Reset Zoom", command=self.reset_zoom)
        tools_menu.add_command(
            label="Toggle Dark Mode", command=self.toggle_dark_mode
        )
        menu_bar.add_cascade(label="Tools", menu=tools_menu)

        self.js_var = tk.BooleanVar(value=self.js_mode)
        js_menu = tk.Menu(menu_bar, tearoff=False)
        js_menu.add_command(
            label="Open JS Window", command=self.open_js_window
        )
        js_menu.add_command(
            label="Run JavaScript...", command=self.run_js
        )
        js_menu.add_separator()
        js_menu.add_checkbutton(
            label="Use JS Mode", command=self.toggle_js_mode,
            variable=self.js_var
        )
        menu_bar.add_cascade(label="JS Tools", menu=js_menu)

        settings_menu = tk.Menu(menu_bar, tearoff=False)
        settings_menu.add_command(
            label="Preferences",
            accelerator="Ctrl+,",
            command=self.open_preferences,
        )
        menu_bar.add_cascade(label="Settings", menu=settings_menu)

        self.config(menu=menu_bar)
        self.bind_all("<Control-t>", lambda e: self.new_tab())
        self.bind_all("<Control-o>", lambda e: self.open_file())
        self.bind_all("<Control-s>", lambda e: self.save_page())
        self.bind_all("<Control-p>", lambda e: self.print_page())
        self.bind_all("<Control-w>", lambda e: self.close_tab())
        self.bind_all("<Control-u>", lambda e: self.view_source())
        self.bind_all("<Control-d>", lambda e: self.add_bookmark())
        self.bind_all("<Control-b>", lambda e: self.show_bookmarks())
        self.bind_all("<Control-h>", lambda e: self.show_history())
        self.bind_all("<Control-plus>", lambda e: self.zoom_in())
        self.bind_all("<Control-minus>", lambda e: self.zoom_out())
        self.bind_all("<Control-comma>", lambda e: self.open_preferences())
        self.bind_all("<Control-j>", lambda e: self.open_js_window())
        self.bind_all("<Control-Shift-J>", lambda e: self.toggle_js_mode())

    def new_tab(self, url=None):
        if url is None:
            url = self.home_url
        frame = ttk.Frame(self.notebook)
        html = HtmlFrame(frame, horizontal_scrollbar="auto", experimental=True)
        html.pack(fill=tk.BOTH, expand=True)
        frame.html = html
        self.notebook.add(frame, text="New Tab")
        self.notebook.select(frame)
        self.url_var.set(url)
        self.status_var.set("Loading...")
        try:
            html.load_website(url)
            self.notebook.tab(frame, text=url)
            self.history.append(url)
        except Exception:
            pass
        finally:
            self.status_var.set(url)

    def current_html(self):
        current = self.notebook.select()
        if not current:
            return None
        frame = self.notebook.nametowidget(current)
        return getattr(frame, "html", None)

    def load_url(self, event=None):
        url = self.url_var.get().strip()
        if not url.startswith("http://") and not url.startswith("https://"):
            query = urllib.parse.quote_plus(url)
            engine = self.search_engines.get(self.search_engine, self.home_url)
            url = engine.format(query)
            self.url_var.set(url)
        html = self.current_html()
        if not html:
            return
        tku.HEADERS["User-Agent"] = self.user_agent
        if self.js_mode:
            self._ensure_js_window(url)
        self.status_var.set("Loading...")
        try:
            html.load_website(url)
            self.notebook.tab(self.notebook.select(), text=url)
            self.history.append(url)
        except Exception as exc:
            messagebox.showerror("Error", str(exc))
        finally:
            self.status_var.set(url)

    def go_back(self):
        html = self.current_html()
        if html:
            try:
                html.go_back()
            except Exception:
                pass

    def go_forward(self):
        html = self.current_html()
        if html:
            try:
                html.go_forward()
            except Exception:
                pass

    def reload_page(self):
        html = self.current_html()
        if html:
            try:
                html.reload()
            except Exception:
                pass

    def apply_zoom(self):
        html = self.current_html()
        if html:
            html.add_css(f"body {{zoom: {self.zoom}%}}")

    def zoom_in(self):
        self.zoom += 10
        self.apply_zoom()

    def zoom_out(self):
        self.zoom = max(10, self.zoom - 10)
        self.apply_zoom()

    def reset_zoom(self):
        self.zoom = 100
        self.apply_zoom()

    def toggle_dark_mode(self, event=None):
        self.dark_mode = not self.dark_mode
        html = self.current_html()
        if html:
            if self.dark_mode:
                html.add_css(
                    "body { background-color: #222; color: #eee; } "
                    "a { color: #8bf; }"
                )
            else:
                html.add_css(
                    "body { background-color: white; color: black; } "
                    "a { color: blue; }"
                )

    def _ensure_js_window(self, url):
        try:
            import webview
        except Exception:
            messagebox.showerror(
                "Error", "pywebview is required for JS support"
            )
            return None
        if self.js_window:
            try:
                self.js_window.load_url(url)
                return self.js_window
            except Exception:
                self.js_window = None
        self.js_window = webview.create_window("JS Mode", url)
        threading.Thread(target=webview.start, daemon=True).start()
        return self.js_window

    def open_js_window(self, event=None):
        url = self.url_var.get() or self.home_url
        self._ensure_js_window(url)

    def run_js(self, event=None):
        if not self.js_window:
            messagebox.showinfo("JS", "Open a JS window first")
            return
        code = simpledialog.askstring("Run JavaScript", "Enter script:")
        if code:
            try:
                result = self.js_window.evaluate_js(code)
                messagebox.showinfo("Result", str(result))
            except Exception as exc:
                messagebox.showerror("Error", str(exc))

    def toggle_js_mode(self, event=None):
        self.js_mode = not self.js_mode
        self.js_var.set(self.js_mode)
        status = "enabled" if self.js_mode else "disabled"
        messagebox.showinfo("JavaScript Mode", f"JS mode {status}.")

    def close_tab(self, event=None):
        current = self.notebook.select()
        if current:
            self.notebook.forget(current)
            if not self.notebook.tabs():
                self.new_tab()

    def view_source(self, event=None):
        url = self.url_var.get()
        try:
            req = urllib.request.Request(
                url, headers={"User-Agent": self.user_agent}
            )
            with urllib.request.urlopen(req) as resp:
                source = resp.read().decode("utf-8", "replace")
        except Exception as exc:
            messagebox.showerror("Error", str(exc))
            return
        win = tk.Toplevel(self)
        win.title(f"Source: {url}")
        text = tk.Text(win, wrap="none")
        text.insert("1.0", source)
        text.configure(state="disabled")
        text.pack(fill=tk.BOTH, expand=True)

    def open_file(self, event=None):
        path = filedialog.askopenfilename(
            filetypes=[("HTML files", "*.html *.htm"), ("All files", "*.*")]
        )
        if path:
            html = self.current_html()
            if html:
                try:
                    html.load_file(path)
                    self.notebook.tab(self.notebook.select(), text=path)
                    self.url_var.set(path)
                except Exception as exc:
                    messagebox.showerror("Error", str(exc))

    def save_page(self, event=None):
        html = self.current_html()
        if not html:
            return
        path = filedialog.asksaveasfilename(
            defaultextension=".html",
            filetypes=[("HTML files", "*.html"), ("All files", "*.*")],
        )
        if path:
            try:
                html.save_page(path)
                self.status_var.set(f"Saved {path}")
            except Exception as exc:
                messagebox.showerror("Error", str(exc))

    def print_page(self, event=None):
        html = self.current_html()
        if not html:
            return
        path = filedialog.asksaveasfilename(
            defaultextension=".ps",
            filetypes=[("PostScript", "*.ps"), ("All files", "*.*")],
        )
        if path:
            try:
                html.print_page(path, pagesize="A4")
                self.status_var.set(f"Printed to {path}")
            except Exception as exc:
                messagebox.showerror("Error", str(exc))

    def add_bookmark(self, event=None):
        url = self.url_var.get()
        if url and url not in self.bookmarks:
            self.bookmarks.append(url)
            self.status_var.set(f"Bookmarked {url}")

    def show_bookmarks(self, event=None):
        if not self.bookmarks:
            messagebox.showinfo("Bookmarks", "No bookmarks added.")
            return
        win = tk.Toplevel(self)
        win.title("Bookmarks")
        lb = tk.Listbox(win)
        for bm in self.bookmarks:
            lb.insert(tk.END, bm)
        lb.pack(fill=tk.BOTH, expand=True)

        def open_sel(event=None):
            sel = lb.curselection()
            if sel:
                self.url_var.set(lb.get(sel[0]))
                self.load_url()
                win.destroy()

        lb.bind("<Double-1>", open_sel)
        open_btn = ttk.Button(win, text="Open", command=open_sel)
        open_btn.pack(pady=5)
        ttk.Button(
            win,
            text="Export",
            command=lambda: self.export_bookmarks(),
        ).pack(pady=5)

    def export_bookmarks(self, event=None):
        path = filedialog.asksaveasfilename(
            defaultextension=".json",
            filetypes=[("JSON", "*.json"), ("All files", "*.*")],
        )
        if path:
            try:
                with open(path, "w", encoding="utf-8") as f:
                    json.dump(self.bookmarks, f, indent=2)
                self.status_var.set(f"Exported bookmarks to {path}")
            except Exception as exc:
                messagebox.showerror("Error", str(exc))

    def show_history(self, event=None):
        if not self.history:
            messagebox.showinfo("History", "No pages visited yet.")
            return
        win = tk.Toplevel(self)
        win.title("History")
        lb = tk.Listbox(win)
        for url in self.history:
            lb.insert(tk.END, url)
        lb.pack(fill=tk.BOTH, expand=True)
        lb.bind(
            "<Double-1>",
            lambda e: (
                self.url_var.set(lb.get(lb.curselection()[0])),
                self.load_url(),
                win.destroy(),
            ),
        )
        ttk.Button(win, text="Clear", command=self.clear_history).pack(pady=5)

    def clear_history(self, event=None):
        self.history.clear()
        self.status_var.set("History cleared")

    def go_home(self):
        self.url_var.set(self.home_url)
        self.load_url()


def main():
    threading.Thread(target=nethelper.run_server, daemon=True).start()
    TabbedBrowser().mainloop()


if __name__ == "__main__":
    main()
