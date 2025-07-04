import tkinter as tk
from html.parser import HTMLParser
import urllib.request
from typing import List, Tuple, Callable


class _Parser(HTMLParser):
    def __init__(self, text: tk.Text):
        super().__init__()
        self.text = text
        self.links: List[Tuple[str, str]] = []
        self._current: Tuple[str, str, str] | None = None

    def handle_starttag(self, tag, attrs):
        if tag == 'br':
            self.text.insert(tk.END, '\n')
        elif tag in ('p', 'div'):
            self.text.insert(tk.END, '\n\n')
        elif tag == 'a':
            href = dict(attrs).get('href')
            if href:
                tag_name = f"link{len(self.links)}"
                start = self.text.index(tk.END)
                self._current = (tag_name, start, href)

    def handle_endtag(self, tag):
        if tag == 'a' and self._current:
            tag_name, start, href = self._current
            end = self.text.index(tk.END)
            self.text.tag_add(tag_name, start, end)
            self.links.append((tag_name, href))
            self._current = None

    def handle_data(self, data):
        self.text.insert(tk.END, data)


class SimpleHtmlReader(tk.Frame):
    """Very small HTML viewer built for Tk Browser."""

    def __init__(
        self,
        master,
        link_callback: Callable[[str], None] | None = None,
        user_agent: str = "TkBrowser",
    ):
        super().__init__(master)
        self.text = tk.Text(self, wrap='word')
        self.text.pack(fill=tk.BOTH, expand=True)
        self.link_callback = link_callback
        self.links: dict[str, str] = {}
        self.history: List[str] = []
        self.history_index = -1
        self.current_url = ''
        self.page_source = ''
        self.user_agent = user_agent

    def _display_html(self, html: str):
        self.text.delete('1.0', tk.END)
        parser = _Parser(self.text)
        parser.feed(html)
        parser.close()
        self.links = {}
        for tag, href in parser.links:
            self.text.tag_config(tag, foreground='blue', underline=True)
            self.text.tag_bind(
                tag, '<Button-1>', lambda e, u=href: self._follow(u)
            )
            self.links[tag] = href

    def _follow(self, url: str):
        if self.link_callback:
            self.link_callback(url)

    def _push_history(self, url: str):
        if self.history_index < len(self.history) - 1:
            self.history = self.history[: self.history_index + 1]
        self.history.append(url)
        self.history_index = len(self.history) - 1

    def _open_history(self):
        url = self.history[self.history_index]
        if url.startswith('http://') or url.startswith('https://'):
            self.load_website(url, add_history=False)
        else:
            self.load_file(url, add_history=False)

    def load_website(self, url: str, add_history: bool = True):
        req = urllib.request.Request(
            url, headers={"User-Agent": self.user_agent}
        )
        with urllib.request.urlopen(req) as resp:
            html = resp.read().decode('utf-8', 'replace')
        self.page_source = html
        self.current_url = url
        self._display_html(html)
        if add_history:
            self._push_history(url)

    def load_file(self, path: str, add_history: bool = True):
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            html = f.read()
        self.page_source = html
        self.current_url = path
        self._display_html(html)
        if add_history:
            self._push_history(path)

    def reload(self):
        if not self.current_url:
            return
        if (
            self.current_url.startswith("http://")
            or self.current_url.startswith("https://")
        ):
            self.load_website(self.current_url, add_history=False)
        else:
            self.load_file(self.current_url, add_history=False)

    def go_back(self):
        if self.history_index > 0:
            self.history_index -= 1
            self._open_history()

    def go_forward(self):
        if self.history_index + 1 < len(self.history):
            self.history_index += 1
            self._open_history()

    def save_page(self, path: str):
        with open(path, 'w', encoding='utf-8') as f:
            f.write(self.page_source)

    def print_page(self, path: str, pagesize: str = 'A4'):
        self.save_page(path)

    def add_css(self, css: str):
        # CSS styling is not supported in this simple reader
        pass
