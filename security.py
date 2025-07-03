import ssl
import socket
from urllib.parse import urlparse


class SecurityManager:
    """Simple security checks for Tk Browser."""

    def __init__(self, blocklist=None):
        self.blocklist = set(blocklist or [])

    def is_blocked(self, url: str) -> bool:
        return any(url.startswith(b) for b in self.blocklist)

    def is_https(self, url: str) -> bool:
        return urlparse(url).scheme == "https"

    def verify_certificate(self, url: str, timeout: int = 3) -> bool:
        """Try to verify the SSL certificate for the URL."""
        parsed = urlparse(url)
        if parsed.scheme != "https" or not parsed.hostname:
            return False
        host = parsed.hostname
        port = parsed.port or 443
        ctx = ssl.create_default_context()
        try:
            sock = socket.socket()
            with ctx.wrap_socket(sock, server_hostname=host) as conn:
                conn.settimeout(timeout)
                conn.connect((host, port))
                conn.getpeercert()
            return True
        except Exception:
            return False
