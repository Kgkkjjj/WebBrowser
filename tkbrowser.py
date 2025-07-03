import json
import os
import tkinter as tk
from tkinter import ttk
from tkinter import messagebox, filedialog
from tkinterweb import HtmlFrame
import urllib.request
import urllib.parse

SETTINGS_FILE = "settings.json"


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
        self._load_settings()
        self._create_widgets()
        self.new_tab()

    def _load_settings(self):
        if os.path.exists(SETTINGS_FILE):
            try:
                with open(SETTINGS_FILE, "r", encoding="utf-8") as f:
                    data = json.load(f)
                self.home_url = data.get("home_url", self.home_url)
                self.search_engine = data.get(
                    "search_engine", self.search_engine
                )
            except Exception:
                pass

    def _save_settings(self):
        data = {"home_url": self.home_url, "search_engine": self.search_engine}
        try:
            with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2)
        except Exception:
            pass

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

        def save():
            self.home_url = home_var.get() or self.home_url
            self.search_engine = engine_var.get() or self.search_engine
            self._save_settings()
            win.destroy()

        ttk.Button(win, text="Save", command=save).grid(
            row=2, column=0, columnspan=2, pady=10
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
        menu_bar.add_cascade(label="Bookmarks", menu=bookmark_menu)

        history_menu = tk.Menu(menu_bar, tearoff=False)
        history_menu.add_command(
            label="Show History",
            accelerator="Ctrl+H",
            command=self.show_history,
        )
        menu_bar.add_cascade(label="History", menu=history_menu)

        tools_menu = tk.Menu(menu_bar, tearoff=False)
        tools_menu.add_command(
            label="View Source", accelerator="Ctrl+U", command=self.view_source
        )
        menu_bar.add_cascade(label="Tools", menu=tools_menu)

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
        self.bind_all("<Control-w>", lambda e: self.close_tab())
        self.bind_all("<Control-u>", lambda e: self.view_source())
        self.bind_all("<Control-d>", lambda e: self.add_bookmark())
        self.bind_all("<Control-b>", lambda e: self.show_bookmarks())
        self.bind_all("<Control-h>", lambda e: self.show_history())
        self.bind_all("<Control-comma>", lambda e: self.open_preferences())

    def new_tab(self, url=None):
        if url is None:
            url = self.home_url
        frame = ttk.Frame(self.notebook)
        html = HtmlFrame(frame, horizontal_scrollbar="auto")
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

    def close_tab(self, event=None):
        current = self.notebook.select()
        if current:
            self.notebook.forget(current)
            if not self.notebook.tabs():
                self.new_tab()

    def view_source(self, event=None):
        url = self.url_var.get()
        try:
            with urllib.request.urlopen(url) as resp:
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

    def go_home(self):
        self.url_var.set(self.home_url)
        self.load_url()


def main():
    TabbedBrowser().mainloop()


if __name__ == "__main__":
    main()
