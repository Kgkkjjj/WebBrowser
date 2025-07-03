"""Security helpers for Tk Browser."""

import datetime
import ssl
import socket
from typing import Callable, Dict
from urllib.parse import urlparse
import urllib.request


class SecurityManager:
    """Simple security checks for Tk Browser."""

    def __init__(self, blocklist=None, ip_blocklist=None):
        self.blocklist = set(blocklist or [])
        self.ip_blocklist = set(ip_blocklist or [])
        self.request_times: Dict[str, list[float]] = {}
        self.checks: Dict[str, Callable[[str], bool]] = {
            "https": self.is_https,
            "certificate": self.verify_certificate,
            "hsts": self.hsts_enabled,
            "csp": self.csp_present,
            "x_frame": self.x_frame_options,
            "x_content_type": self.x_content_type_options,
            "referrer": self.referrer_policy,
            "x_xss": self.x_xss_protection,
            "permissions": self.permissions_policy,
            "no_server": self.no_server_header,
            "content_length": self.content_length_ok,
            "cookie_secure": self.cookies_secure,
            "cookie_httponly": self.cookies_http_only,
            "cookie_samesite": self.cookies_same_site,
            "cache_control": self.cache_control_no_store,
            "pragma": self.pragma_no_cache,
            "mixed_content": self.contains_mixed_content,
            "redirect": self.redirects_to_https,
            "content_language": self.content_language_set,
            "expires": self.expires_future,
            "cors": self.cors_restricted,
            "dns": self.dns_resolves,
            "rate_limit": self.rate_limit_check,
            "ip_block": self.ip_blocked,
            "domain": self.domain_length_valid,
        }

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

    def _get_headers(self, url: str) -> Dict[str, str]:
        try:
            req = urllib.request.Request(url, method="HEAD")
            with urllib.request.urlopen(req, timeout=3) as resp:
                return {k.lower(): v for k, v in resp.headers.items()}
        except Exception:
            return {}

    # --- additional security systems ---

    def hsts_enabled(self, url: str) -> bool:
        headers = self._get_headers(url)
        return "strict-transport-security" in headers

    def csp_present(self, url: str) -> bool:
        headers = self._get_headers(url)
        return "content-security-policy" in headers

    def x_frame_options(self, url: str) -> bool:
        headers = self._get_headers(url)
        return "x-frame-options" in headers

    def x_content_type_options(self, url: str) -> bool:
        headers = self._get_headers(url)
        return headers.get("x-content-type-options", "").lower() == "nosniff"

    def referrer_policy(self, url: str) -> bool:
        headers = self._get_headers(url)
        return "referrer-policy" in headers

    def x_xss_protection(self, url: str) -> bool:
        headers = self._get_headers(url)
        val = headers.get("x-xss-protection", "")
        return val.startswith("1")

    def permissions_policy(self, url: str) -> bool:
        headers = self._get_headers(url)
        return "permissions-policy" in headers

    def no_server_header(self, url: str) -> bool:
        headers = self._get_headers(url)
        return "server" not in headers

    def content_length_ok(self, url: str) -> bool:
        headers = self._get_headers(url)
        try:
            length = int(headers.get("content-length", "0"))
        except ValueError:
            return True
        return length < 5_000_000

    def cookies_secure(self, url: str) -> bool:
        headers = self._get_headers(url)
        cookies = headers.get("set-cookie", "")
        if not cookies:
            return True
        return all("secure" in c.lower() for c in cookies.split(","))

    def cookies_http_only(self, url: str) -> bool:
        headers = self._get_headers(url)
        cookies = headers.get("set-cookie", "")
        if not cookies:
            return True
        return all("httponly" in c.lower() for c in cookies.split(","))

    def cookies_same_site(self, url: str) -> bool:
        headers = self._get_headers(url)
        cookies = headers.get("set-cookie", "")
        if not cookies:
            return True
        return all("samesite" in c.lower() for c in cookies.split(","))

    def cache_control_no_store(self, url: str) -> bool:
        headers = self._get_headers(url)
        return "no-store" in headers.get("cache-control", "").lower()

    def pragma_no_cache(self, url: str) -> bool:
        headers = self._get_headers(url)
        return "no-cache" in headers.get("pragma", "").lower()

    def contains_mixed_content(self, url: str) -> bool:
        if not self.is_https(url):
            return False
        try:
            with urllib.request.urlopen(url, timeout=3) as resp:
                data = resp.read(4096).decode("utf-8", "ignore")
            return "src=\"http://" in data.lower()
        except Exception:
            return False

    def redirects_to_https(self, url: str) -> bool:
        parsed = urlparse(url)
        if parsed.scheme != "http":
            return False
        try:
            with urllib.request.urlopen(url, timeout=3) as resp:
                return resp.geturl().startswith("https://")
        except Exception:
            return False

    def content_language_set(self, url: str) -> bool:
        headers = self._get_headers(url)
        return "content-language" in headers

    def expires_future(self, url: str) -> bool:
        headers = self._get_headers(url)
        val = headers.get("expires")
        if not val:
            return True
        try:
            exp = datetime.datetime.strptime(val, "%a, %d %b %Y %H:%M:%S %Z")
        except Exception:
            return True
        return exp > datetime.datetime.utcnow()

    def cors_restricted(self, url: str) -> bool:
        headers = self._get_headers(url)
        origin = headers.get("access-control-allow-origin")
        return origin not in ("*", None)

    def dns_resolves(self, url: str) -> bool:
        host = urlparse(url).hostname
        if not host:
            return False
        try:
            socket.gethostbyname(host)
            return True
        except Exception:
            return False

    def rate_limit_check(self, url: str) -> bool:
        now = datetime.datetime.utcnow().timestamp()
        times = self.request_times.setdefault(url, [])
        times.append(now)
        # keep last minute
        times[:] = [t for t in times if now - t < 60]
        return len(times) < 10

    def ip_blocked(self, url: str) -> bool:
        host = urlparse(url).hostname
        if not host:
            return False
        try:
            ip = socket.gethostbyname(host)
        except Exception:
            return False
        return ip in self.ip_blocklist

    def domain_length_valid(self, url: str) -> bool:
        host = urlparse(url).hostname
        return bool(host and 3 <= len(host) <= 253)

    # convenience
    def run_checks(self, url: str) -> Dict[str, bool]:
        results: Dict[str, bool] = {}
        for name, func in self.checks.items():
            try:
                results[name] = func(url)
            except Exception:
                results[name] = False
        return results
