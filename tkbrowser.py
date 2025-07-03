import tkinter as tk
from tkinter import ttk
from tkinter import messagebox
from tkinterweb import HtmlFrame
import urllib.request


class TabbedBrowser(tk.Tk):
    """A tabbed web browser using Tkinter and tkinterweb."""

    def __init__(self):
        super().__init__()
        self.title("Tk Browser")
        self.geometry("1024x768")
        self._create_widgets()
        self.new_tab()

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
            label="Close Tab", accelerator="Ctrl+W", command=self.close_tab
        )
        file_menu.add_separator()
        file_menu.add_command(label="Quit", command=self.quit)
        menu_bar.add_cascade(label="File", menu=file_menu)

        tools_menu = tk.Menu(menu_bar, tearoff=False)
        tools_menu.add_command(
            label="View Source", accelerator="Ctrl+U", command=self.view_source
        )
        menu_bar.add_cascade(label="Tools", menu=tools_menu)

        self.config(menu=menu_bar)
        self.bind_all("<Control-t>", lambda e: self.new_tab())
        self.bind_all("<Control-w>", lambda e: self.close_tab())
        self.bind_all("<Control-u>", lambda e: self.view_source())

    def new_tab(self, url="https://www.python.org"):
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
        url = self.url_var.get()
        if not url.startswith("http://") and not url.startswith("https://"):
            messagebox.showerror("Invalid URL", f"Cannot load URL: {url}")
            return
        html = self.current_html()
        if not html:
            return
        self.status_var.set("Loading...")
        try:
            html.load_website(url)
            self.notebook.tab(self.notebook.select(), text=url)
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


def main():
    TabbedBrowser().mainloop()


if __name__ == "__main__":
    main()
