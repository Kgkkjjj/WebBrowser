import http.server
import urllib.parse
import urllib.request


class FetchHandler(http.server.BaseHTTPRequestHandler):
    """HTTP handler that proxies GET requests via /fetch?url=..."""

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path != "/fetch":
            self.send_error(404, "not found")
            return
        query = urllib.parse.parse_qs(parsed.query)
        url = query.get("url", [None])[0]
        if not url:
            self.send_error(400, "missing url")
            return
        try:
            with urllib.request.urlopen(url) as resp:
                self.send_response(resp.getcode())
                for k, v in resp.headers.items():
                    self.send_header(k, v)
                self.end_headers()
                self.wfile.write(resp.read())
        except Exception as exc:
            self.send_error(500, str(exc))


def run_server(port: int = 8090):
    """Start the helper server on the given port."""

    server = http.server.HTTPServer(("", port), FetchHandler)
    print(f"Python helper listening on :{port}")
    server.serve_forever()


if __name__ == "__main__":
    run_server()
