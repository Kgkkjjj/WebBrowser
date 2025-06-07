#!/usr/bin/env python3
"""Simple backend for OpenB home and search pages."""
import http.server
import socketserver
import requests
from urllib.parse import urlparse, parse_qs, quote
import os
import json
import logging
from logging.handlers import RotatingFileHandler

PORT = int(os.environ.get('OPENB_PORT', '8080'))
LOG_DIR = os.path.join(os.path.expanduser('~'), '.cache', 'openb')
os.makedirs(LOG_DIR, exist_ok=True)
LOG_FILE = os.path.join(LOG_DIR, 'server.log')

logger = logging.getLogger('openb.server')
handler = RotatingFileHandler(LOG_FILE, maxBytes=1024*1024, backupCount=2)
handler.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
logger.addHandler(handler)
logger.setLevel(logging.INFO)

tab_count = 1

class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        logger.info("%s - %s" % (self.address_string(), fmt % args))

    def _cors(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.send_header('Access-Control-Allow-Methods', 'GET, OPTIONS')

    def do_OPTIONS(self):
        self.send_response(200)
        self._cors()
        self.end_headers()

    def do_GET(self):
        global tab_count
        parsed = urlparse(self.path)
        try:
            if parsed.path == '/search':
                q = parse_qs(parsed.query).get('q', [''])[0]
                api = f"https://api.duckduckgo.com/?q={quote(q)}&format=json&no_redirect=1&no_html=1"
                try:
                    resp = requests.get(api, timeout=5)
                    resp.raise_for_status()
                    data = resp.content
                    self.send_response(200)
                except Exception as err:
                    logger.error('search request failed: %s', err)
                    data = json.dumps({'error': 'search-failed'}).encode()
                    self.send_response(502)
                self._cors()
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(data)
            elif parsed.path == '/status':
                self.send_response(200)
                self._cors()
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({'tabs': tab_count, 'status': 'ok'}).encode())
            elif parsed.path == '/update_tabs':
                try:
                    tab_count = int(parse_qs(parsed.query).get('count',[tab_count])[0])
                except ValueError:
                    tab_count = 1
                self.send_response(200)
                self._cors()
                self.end_headers()
            elif parsed.path == '/log':
                self.send_response(200)
                self._cors()
                self.send_header('Content-Type', 'text/plain')
                self.end_headers()
                try:
                    with open(LOG_FILE, 'r') as f:
                        self.wfile.write(f.read().encode())
                except FileNotFoundError:
                    self.wfile.write(b'')
            else:
                if parsed.path == '/':
                    self.path = '/data/home.html'
                return http.server.SimpleHTTPRequestHandler.do_GET(self)
        except Exception as e:
            logger.exception('handler error')
            self.send_response(500)
            self._cors()
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({'error': str(e)}).encode())

if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    try:
        with socketserver.ThreadingTCPServer(("", PORT), Handler) as httpd:
            logger.info("Serving at http://localhost:%d/", PORT)
            try:
                httpd.serve_forever()
            except KeyboardInterrupt:
                pass
            logger.info("Server stopped")
    except OSError as e:
        logger.error("Failed to bind to port %d: %s", PORT, e)
        print(f"Server error: {e}")
        raise SystemExit(1)
