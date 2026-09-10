#!/usr/bin/env python3
"""Static no-cache server for web/deploy/ on :8000.

python http.server serves files live from disk, but browsers cache .js/.wasm
aggressively and keep showing a stale poster. This server forces re-validation so
the freshly built WASM is always served.
"""
import http.server
import os
import socketserver

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'deploy')


class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        super().end_headers()

    def log_message(self, fmt, *args):
        pass


os.chdir(ROOT)
with socketserver.TCPServer(('', 8000), NoCacheHandler) as httpd:
    print(f'serving no-cache on :8000 from {ROOT}')
    httpd.serve_forever()
