#!/usr/bin/env python3
"""
Minimal HTTP server for nRF7002DK.
Listens on 0.0.0.0:8080, prints POST /data bodies.

Run:
    python3 server.py
"""
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import sys

PORT = 8080


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length).decode("utf-8", errors="replace")
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            data = raw

        print(f"[{self.client_address[0]}] POST {self.path} -> {data}", flush=True)

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", "11")
        self.end_headers()
        self.wfile.write(b'{"ok":true}')

    def log_message(self, *_args):
        pass


def main():
    server = HTTPServer(("0.0.0.0", PORT), Handler)
    print(f"Listening on 0.0.0.0:{PORT}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nbye", flush=True)
        sys.exit(0)


if __name__ == "__main__":
    main()
