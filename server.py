#!/usr/bin/env python3
"""Simple backend for OpenB home and search pages."""
import http.server
import socketserver
import requests
from urllib.parse import urlparse, parse_qs, quote
import os

PORT = 8080

class Handler(http.server.SimpleHTTPRequestHandler):
    def _cors(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.send_header('Access-Control-Allow-Methods', 'GET, OPTIONS')

    def do_OPTIONS(self):
        self.send_response(200)
        self._cors()
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/search':
            q = parse_qs(parsed.query).get('q', [''])[0]
            api = f"https://api.duckduckgo.com/?q={quote(q)}&format=json&no_redirect=1&no_html=1"
            try:
                resp = requests.get(api, timeout=5)
                data = resp.content
            except Exception as e:
                data = ('{"error": "%s"}' % str(e)).encode()
            self.send_response(200)
            self._cors()
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(data)
        else:
            if parsed.path == '/':
                self.path = '/data/home.html'
            return http.server.SimpleHTTPRequestHandler.do_GET(self)

if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    with socketserver.TCPServer(("", PORT), Handler) as httpd:
        print(f"Serving at http://localhost:{PORT}/")
        httpd.serve_forever()
