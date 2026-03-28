#!/usr/bin/env python3
"""Dev server for testing the badge WiFi portal on a phone.

Serves portal.html at / and returns static JSON for the API endpoints.
Run and open http://<your-lan-ip>:8080 on your phone.
"""

import json
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

PORTAL_HTML = Path(__file__).resolve().parent.parent / "lib" / "core" / "network" / "portal.html"

CONFIG = {
    "name": "", "r": 128, "g": 0, "b": 255, "brightness": 200,
    "share": False, "pronouns": "", "affiliation": "",
    "contact": "",
}
SOCIAL = {"social": 42, "sponsor": 180, "light": 5, "attraction": 255}


class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/" or self.path == "/generate_204" or self.path == "/connecttest.txt":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(PORTAL_HTML.read_bytes())
        elif self.path == "/api/config":
            self._json(CONFIG)
        elif self.path == "/api/social":
            self._json(SOCIAL)
        else:
            self.send_error(404)

    def do_POST(self):
        if self.path == "/api/config":
            length = int(self.headers.get("Content-Length", 0))
            body = json.loads(self.rfile.read(length)) if length else {}
            print(f"POST /api/config: {json.dumps(body, indent=2)}")
            CONFIG.update(body)
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"ok":true}')
        else:
            self.send_error(404)

    def _json(self, obj):
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(obj).encode())


if __name__ == "__main__":
    port = 8080
    server = HTTPServer(("0.0.0.0", port), Handler)
    print(f"Serving portal at http://0.0.0.0:{port}")
    server.serve_forever()
