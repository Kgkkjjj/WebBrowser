import tkinter as tk
from html.parser import HTMLParser
import urllib.error
import urllib.parse
import urllib.request
from typing import Dict, List, Tuple, Callable

try:
    from py_mini_racer import py_mini_racer
    JS_AVAILABLE = True
except Exception:  # pragma: no cover - optional dependency
    py_mini_racer = None
    JS_AVAILABLE = False

try:
    from tkinter import TclError
    from tkinterweb import HtmlFrame, bindings as tkweb_bindings

    _orig_get_prop = tkweb_bindings.TkinterWeb.get_node_property

    def _safe_get_prop(self, node_handle, node_property, *args):
        try:
            return _orig_get_prop(self, node_handle, node_property, *args)
        except TclError:
            return ""

    tkweb_bindings.TkinterWeb.get_node_property = _safe_get_prop
    TKINTERWEB_AVAILABLE = True
except Exception:  # pragma: no cover - optional dependency
    HtmlFrame = None
    TKINTERWEB_AVAILABLE = False

# Approximate set of 130 supported HTML tags and attributes
SUPPORTED_TAGS = [
    "html", "head", "body", "title", "meta", "link", "style", "script",
    "section", "nav", "article", "aside", "h1", "h2", "h3", "h4", "h5", "h6",
    "header", "footer", "address", "p", "hr", "br", "pre", "blockquote",
    "ol", "ul", "li", "dl", "dt", "dd", "figure", "figcaption", "div",
    "main", "span", "a", "em", "strong", "small", "s", "cite", "q", "dfn",
    "abbr", "data", "time", "code", "var", "samp", "kbd", "sub", "sup",
    "i", "b", "u", "mark", "ruby", "rt", "rp", "bdi", "bdo", "table",
    "caption", "thead", "tbody", "tfoot", "tr", "td", "th", "col",
    "colgroup", "form", "label", "input", "button", "select", "datalist",
    "optgroup", "option", "textarea", "output", "progress", "meter",
    "fieldset", "legend", "details", "summary", "dialog", "script",
    "noscript", "template", "canvas", "img", "audio", "video", "source",
    "track", "map", "area", "svg", "iframe", "object", "param", "embed",
    "picture", "portal", "wbr",
]

SUPPORTED_ATTRIBUTES = [
    "id", "class", "style", "href", "src", "alt", "title", "lang",
    "height", "width", "name", "content", "rel", "type", "value",
    "placeholder", "action", "method", "selected", "disabled", "checked",
    "for", "data-*", "aria-*", "role", "tabindex", "align", "cellpadding",
    "cellspacing", "colspan", "rowspan", "target", "download", "maxlength",
    "min", "max", "step", "rows", "cols", "wrap", "async", "defer",
    "crossorigin", "integrity", "media", "charset", "http-equiv",
    "autoplay", "controls", "loop", "muted", "poster", "preload",
]

SUPPORTED_HTML_FEATURES = SUPPORTED_TAGS + SUPPORTED_ATTRIBUTES


class _Parser(HTMLParser):
    def __init__(self, text: tk.Text):
        super().__init__()
        self.text = text
        self.links: List[Tuple[str, str]] = []
        self._stack: List[Tuple[str, str, Dict[str, str]]] = []
        self.scripts: List[str] = []
        self.styles: Dict[str, Dict[str, str]] = {}
        self._in_script = False
        self._script_buf: List[str] = []
        self._in_style = False
        self._style_buf: List[str] = []

    def _parse_style(self, style: str) -> Dict[str, str]:
        out: Dict[str, str] = {}
        for part in style.split(";"):
            if ":" in part:
                k, v = part.split(":", 1)
                out[k.strip().lower()] = v.strip()
        return out

    def _apply_tag(self, tag_name: str, start: str, style: Dict[str, str]):
        cfg: Dict[str, str] = {}
        if "color" in style:
            cfg["foreground"] = style["color"]
        if "background-color" in style:
            cfg["background"] = style["background-color"]
        if style.get("font-weight") == "bold":
            cfg["font"] = ("TkDefaultFont", 10, "bold")
        if style.get("font-style") == "italic":
            cfg["font"] = ("TkDefaultFont", 10, "italic")
        if style.get("text-decoration") == "underline":
            cfg["underline"] = True
        end = self.text.index(tk.END)
        self.text.tag_add(tag_name, start, end)
        if cfg:
            self.text.tag_config(tag_name, **cfg)

    def handle_starttag(self, tag, attrs):
        if tag == 'br':
            self.text.insert(tk.END, '\n')
        elif tag in ('p', 'div', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6'):
            self.text.insert(tk.END, '\n\n')
        attrs_dict = dict(attrs)
        if tag == 'script':
            self._in_script = True
            self._script_buf = []
            return
        if tag == 'style':
            self._in_style = True
            self._style_buf = []
            return
        style = self.styles.get(tag, {}).copy()
        style.update(self._parse_style(attrs_dict.get('style', '')))
        if tag in ('b', 'strong'):
            style.setdefault('font-weight', 'bold')
        if tag in ('i', 'em'):
            style.setdefault('font-style', 'italic')
        if tag == 'u':
            style.setdefault('text-decoration', 'underline')
        start = self.text.index(tk.END)
        if tag == 'a':
            href = attrs_dict.get('href')
            if href:
                tag_name = f"link{len(self.links)}"
                self._stack.append((tag_name, start, style | {'href': href}))
                return
        self._stack.append((tag, start, style))

    def handle_endtag(self, tag):
        if self._in_script and tag != 'script':
            return
        if tag == 'script' and self._in_script:
            self._in_script = False
            self.scripts.append("".join(self._script_buf))
            self._script_buf = []
            return
        if tag == 'style' and self._in_style:
            self._in_style = False
            css = "".join(self._style_buf)
            for rule in css.split('}'):
                if '{' not in rule:
                    continue
                selector, props = rule.split('{', 1)
                selector = selector.strip()
                style = self._parse_style(props)
                if selector and style:
                    self.styles[selector] = style
            self._style_buf = []
            return
        for i in range(len(self._stack) - 1, -1, -1):
            name, start, style = self._stack[i]
            href = style.pop('href', None)
            if name == tag or (name.startswith('link') and tag == 'a'):
                self._stack = self._stack[:i]
                tag_name = name if href else f"tag{len(self.links)}"
                self._apply_tag(tag_name, start, style)
                if href:
                    self.links.append((tag_name, href))
                break

    def handle_data(self, data):
        if self._in_script:
            self._script_buf.append(data)
        elif self._in_style:
            self._style_buf.append(data)
        else:
            self.text.insert(tk.END, data)


class SimpleHtmlReader(tk.Frame):
    """Very small HTML viewer built for Tk Browser."""

    def __init__(
        self,
        master,
        link_callback: Callable[[str], None] | None = None,
        user_agent: str = "TkBrowser",
        error_callback: Callable[[str], None] | None = None,
    ):
        super().__init__(master)
        self.text = tk.Text(self, wrap='word')
        self.text.pack(fill=tk.BOTH, expand=True)
        self.link_callback = link_callback
        self.error_callback = error_callback
        self.links: dict[str, str] = {}
        self.history: List[str] = []
        self.history_index = -1
        self.current_url = ''
        self.page_source = ''
        self.user_agent = user_agent
        self.cache: Dict[str, str] = {}
        self.error_message = ""

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
        if JS_AVAILABLE and parser.scripts:
            ctx = py_mini_racer.MiniRacer()
            for script in parser.scripts:
                try:
                    ctx.eval(script)
                except Exception:
                    pass
        self.error_message = ""

    def _display_error(self, msg: str):
        self.text.delete('1.0', tk.END)
        self.text.insert(tk.END, f"Error: {msg}\n")
        self.error_message = msg
        if self.error_callback:
            try:
                self.error_callback(msg)
            except Exception:
                pass

    def clear_cache(self):
        self.cache.clear()

    def _fetch_url(self, url: str, timeout: int = 10) -> str:
        if url in self.cache:
            return self.cache[url]
        req = urllib.request.Request(
            url, headers={"User-Agent": self.user_agent}
        )
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            html = resp.read().decode("utf-8", "replace")
        self.cache[url] = html
        return html

    def _follow(self, url: str):
        if self.link_callback:
            if self.current_url.startswith("http"):
                url = urllib.parse.urljoin(self.current_url, url)
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
        try:
            html = self._fetch_url(url)
        except urllib.error.URLError as exc:
            self._display_error(str(exc))
            return
        self.page_source = html
        self.current_url = url
        self._display_html(html)
        if add_history:
            self._push_history(url)

    def load_file(self, path: str, add_history: bool = True):
        try:
            with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                html = f.read()
        except OSError as exc:
            self._display_error(str(exc))
            return
        self.page_source = html
        self.current_url = path
        self._display_html(html)
        if add_history:
            self._push_history(path)

    def reload(self):
        if not self.current_url:
            return
        self.error_message = ""
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


if TKINTERWEB_AVAILABLE:

    class TkHtmlReader(tk.Frame):
        """HTML viewer based on tkinterweb with history and CSS support."""

        def __init__(
            self,
            master,
            link_callback: Callable[[str], None] | None = None,
            user_agent: str = "TkBrowser",
            error_callback: Callable[[str], None] | None = None,
        ):
            super().__init__(master)
            self.frame = HtmlFrame(
                self,
                messages_enabled=False,
                on_link_click=self._follow,
                on_navigate_fail=self._handle_error,
            )
            self.frame.pack(fill=tk.BOTH, expand=True)
            self.link_callback = link_callback
            self.error_callback = error_callback
            self.history: List[str] = []
            self.history_index = -1
            self.current_url = ""
            self.page_source = ""
            self.user_agent = user_agent

        def _handle_error(self, url, error=None, code=None):
            msg = error or str(code) or "Error"
            if self.error_callback:
                try:
                    self.error_callback(msg)
                except Exception:
                    pass

        def _follow(self, url: str):
            if self.link_callback:
                self.link_callback(url)
                return "break"
            self.load_website(url)

        def _push_history(self, url: str):
            if self.history_index < len(self.history) - 1:
                self.history = self.history[: self.history_index + 1]
            self.history.append(url)
            self.history_index = len(self.history) - 1

        def _open_history(self):
            url = self.history[self.history_index]
            if url.startswith("http://") or url.startswith("https://"):
                self.load_website(url, add_history=False)
            else:
                self.load_file(url, add_history=False)

        def load_website(self, url: str, add_history: bool = True):
            try:
                self.frame.load_website(url, useragent=self.user_agent)
                self.page_source = self.frame.save_page()
                self.current_url = url
                if add_history:
                    self._push_history(url)
            except Exception as exc:
                self._handle_error(url, str(exc))

        def load_file(self, path: str, add_history: bool = True):
            try:
                self.frame.load_file(path)
                self.page_source = self.frame.save_page()
                self.current_url = path
                if add_history:
                    self._push_history(path)
            except Exception as exc:
                self._handle_error(path, str(exc))

        def reload(self):
            if not self.current_url:
                return
            is_web = (
                self.current_url.startswith("http://")
                or self.current_url.startswith("https://")
            )
            if is_web:
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
            self.frame.save_page(path)

        def print_page(self, path: str, pagesize: str = "A4"):
            self.frame.print_page(path)

        def add_css(self, css: str):
            self.frame.add_css(css)
