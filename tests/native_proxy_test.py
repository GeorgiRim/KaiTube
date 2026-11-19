"""Verify that a locally saved HLS master fetches playlists and segments via the configured proxy."""
import base64
import json
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from urllib.parse import urlsplit

player = Path(sys.argv[1]).resolve()
fixtures = Path(sys.argv[2]).resolve()
requests = []
user, password = "player@example", "p@ss:% /+#"
basic = "Basic " + base64.b64encode(f"{user}:{password}".encode()).decode()


class FixtureProxy(BaseHTTPRequestHandler):
    def do_GET(self):
        url = urlsplit(self.path)
        if url.hostname != "kaitube.invalid" or url.path not in ("/local.m3u8", "/local0.ts"):
            self.send_error(404)
            return
        if self.headers.get("Proxy-Authorization") != self.server.required_auth:
            self.send_response(407)
            self.send_header("Proxy-Authenticate", 'Basic realm="KaiTube test"')
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        requests.append(url.path)
        data = (fixtures / url.path[1:]).read_bytes()
        self.send_response(200)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


with tempfile.TemporaryDirectory() as tmp, ThreadingHTTPServer(("127.0.0.1", 0), FixtureProxy) as proxy:
    threading.Thread(target=proxy.serve_forever, daemon=True).start()
    master = Path(tmp, "selected.m3u8")
    master.write_text("#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=512000\nhttp://kaitube.invalid/local.m3u8\n")
    settings = {"http_proxy": f"http://127.0.0.1:{proxy.server_port}"}
    for required_auth in (None, basic):
        requests.clear()
        proxy.required_auth = required_auth
        if required_auth:
            settings.update(http_proxy_username=user, http_proxy_password=password)
        Path(tmp, "switch_newpipe_settings.json").write_text(json.dumps(settings))
        subprocess.run([player, f"playlist:{master}"], cwd=tmp, check=True, timeout=15,
                       env={**os.environ, "http_proxy": "", "no_proxy": ""})
        assert {"/local.m3u8", "/local0.ts"}.issubset(requests), requests
