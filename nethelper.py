"""Networking helper with multiple demo endpoints."""

from __future__ import annotations

import http.server
import json
import os
import random
import socket
import subprocess
import threading
import time
import urllib.parse
import urllib.request
import webbrowser
from typing import Dict


START_TIME = time.time()
REQUEST_COUNT = 0
CACHE: Dict[str, bytes] = {}
BLACKLIST = {"https://example.com/blocked"}
RANDOM_URLS = [
    "https://www.python.org",
    "https://httpbin.org/html",
]


class FetchHandler(http.server.BaseHTTPRequestHandler):
    """HTTP handler providing many small networking utilities."""

    server_version = "NetHelper/1.0"

    def do_GET(self):
        """Route the GET request to the appropriate handler."""
        global REQUEST_COUNT
        REQUEST_COUNT += 1
        parsed = urllib.parse.urlparse(self.path)
        query = urllib.parse.parse_qs(parsed.query)
        path = parsed.path

        if path == "/fetch":
            self.handle_fetch(query)
        elif path == "/headers":
            self.handle_headers(query)
        elif path == "/download":
            self.handle_download(query)
        elif path == "/ping":
            self.handle_ping(query)
        elif path == "/dns":
            self.handle_dns(query)
        elif path == "/delay":
            self.handle_delay(query)
        elif path == "/random":
            self.handle_random()
        elif path == "/status":
            self.handle_status()
        elif path == "/redirect":
            self.handle_redirect(query)
        elif path == "/echo":
            self.handle_echo(query)
        elif path == "/post":
            self.handle_post(query)
        elif path == "/cookie":
            self.handle_cookie(query)
        elif path == "/auth":
            self.handle_auth(query)
        elif path == "/metrics":
            self.handle_status()
        elif path == "/cache":
            self.handle_cache(query)
        elif path == "/blacklist":
            self.handle_blacklist(query)
        elif path == "/open":
            self.handle_open(query)
        elif path == "/ip":
            self.handle_ip()
        elif path == "/json":
            self.handle_json(query)
        elif path == "/shutdown":
            self.handle_shutdown()
        else:
            self.send_error(404, "not found")

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/echo":
            length = int(self.headers.get("Content-Length", 0))
            data = self.rfile.read(length)
            self.respond(200, data)
        elif parsed.path == "/upload":
            self.handle_upload()
        else:
            self.send_error(404, "not found")

    # Handlers for individual endpoints

    def handle_fetch(self, query):
        url = query.get("url", [None])[0]
        if not url:
            self.send_error(400, "missing url")
            return
        if url in BLACKLIST:
            self.send_error(403, "blocked")
            return
        try:
            with urllib.request.urlopen(url) as resp:
                data = resp.read()
                self.send_response(resp.getcode())
                for k, v in resp.headers.items():
                    self.send_header(k, v)
                self.end_headers()
                self.wfile.write(data)
        except Exception as exc:
            self.send_error(500, str(exc))

    def handle_headers(self, query):
        url = query.get("url", [None])[0]
        if not url:
            self.send_error(400, "missing url")
            return
        try:
            with urllib.request.urlopen(url) as resp:
                headers = dict(resp.headers)
            self.respond_json(headers)
        except Exception as exc:
            self.send_error(500, str(exc))

    def handle_download(self, query):
        url = query.get("url", [None])[0]
        if not url:
            self.send_error(400, "missing url")
            return
        try:
            with urllib.request.urlopen(url) as resp:
                data = resp.read()
            name = os.path.basename(urllib.parse.urlparse(url).path) or "file"
            self.send_response(200)
            self.send_header(
                "Content-Disposition", f"attachment; filename={name}"
            )
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        except Exception as exc:
            self.send_error(500, str(exc))

    def handle_ping(self, query):
        host = query.get("host", [None])[0]
        if not host:
            self.send_error(400, "missing host")
            return
        try:
            out = subprocess.check_output(
                ["ping", "-c", "1", host], stderr=subprocess.STDOUT
            )
            self.respond(200, out)
        except subprocess.CalledProcessError as exc:
            self.respond(500, exc.output)

    def handle_dns(self, query):
        host = query.get("host", [None])[0]
        if not host:
            self.send_error(400, "missing host")
            return
        try:
            ip = socket.gethostbyname(host)
            self.respond(200, ip)
        except Exception as exc:
            self.send_error(500, str(exc))

    def handle_delay(self, query):
        secs = float(query.get("secs", ["0"])[0])
        url = query.get("url", [None])[0]
        time.sleep(max(0.0, secs))
        if url:
            self.handle_fetch({"url": [url]})
        else:
            self.respond(200, "done")

    def handle_random(self):
        url = random.choice(RANDOM_URLS)
        self.handle_redirect({"url": [url]})

    def handle_status(self):
        data = {
            "uptime": time.time() - START_TIME,
            "requests": REQUEST_COUNT,
        }
        self.respond_json(data)

    def handle_redirect(self, query):
        url = query.get("url", [None])[0]
        if not url:
            self.send_error(400, "missing url")
            return
        self.send_response(302)
        self.send_header("Location", url)
        self.end_headers()

    def handle_echo(self, query):
        msg = query.get("msg", [""])[0]
        self.respond(200, msg)

    def handle_post(self, query):
        url = query.get("url", [None])[0]
        data = query.get("data", [""])[0].encode()
        if not url:
            self.send_error(400, "missing url")
            return
        try:
            req = urllib.request.Request(url, data=data)
            with urllib.request.urlopen(req) as resp:
                body = resp.read()
            self.respond(200, body)
        except Exception as exc:
            self.send_error(500, str(exc))

    def handle_cookie(self, query):
        url = query.get("url", [None])[0]
        if not url:
            self.send_error(400, "missing url")
            return
        try:
            opener = urllib.request.build_opener()
            opener.addheaders = [("Cookie", "test=1")]
            with opener.open(url) as resp:
                cookies = resp.headers.get("Set-Cookie", "")
            self.respond(200, cookies)
        except Exception as exc:
            self.send_error(500, str(exc))

    def handle_auth(self, query):
        user = query.get("user", [""])[0]
        pw = query.get("pass", [""])[0]
        if user == "admin" and pw == "secret":
            self.respond(200, "ok")
        else:
            self.send_error(401, "unauthorized")

    def handle_cache(self, query):
        url = query.get("url", [None])[0]
        if not url:
            self.send_error(400, "missing url")
            return
        if url in CACHE:
            data = CACHE[url]
        else:
            try:
                with urllib.request.urlopen(url) as resp:
                    data = resp.read()
                CACHE[url] = data
            except Exception as exc:
                self.send_error(500, str(exc))
                return
        self.respond(200, data)

    def handle_blacklist(self, query):
        url = query.get("url", [None])[0]
        if not url:
            self.send_error(400, "missing url")
            return
        if url in BLACKLIST:
            self.respond(200, "blocked")
        else:
            self.respond(200, "ok")

    def handle_open(self, query):
        url = query.get("url", [None])[0]
        if not url:
            self.send_error(400, "missing url")
            return
        webbrowser.open(url)
        self.respond(200, "opened")

    def handle_ip(self):
        ip = self.client_address[0]
        self.respond(200, ip)

    def handle_json(self, query):
        self.respond_json(
            {k: v[0] if len(v) == 1 else v for k, v in query.items()}
        )

    def handle_shutdown(self):
        self.respond(200, "shutting down")
        threading.Thread(target=self.server.shutdown).start()

    def handle_upload(self):
        length = int(self.headers.get("Content-Length", 0))
        data = self.rfile.read(length)
        self.respond(200, f"received {len(data)} bytes")

    # Utility helpers

    def respond(
        self,
        code: int,
        data: bytes | str,
        content_type: str = "text/plain",
    ):
        if isinstance(data, str):
            data = data.encode()
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def respond_json(self, obj):
        data = json.dumps(obj).encode()
        self.respond(200, data, "application/json")


def run_server(port: int = 8090):
    """Start the helper server on the given port."""

    server = http.server.HTTPServer(("", port), FetchHandler)
    print(f"Python helper listening on :{port}")
    server.serve_forever()


if __name__ == "__main__":
    run_server()
