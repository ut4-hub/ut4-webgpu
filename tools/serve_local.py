#!/usr/bin/env python3
# Local (Mac) static server for the UT WebGPU build — mirrors nixtop's ut-serve/serve.py
# - COOP/COEP/CORP headers on EVERY response (SharedArrayBuffer needs them)
# - Proper HTTP Range (206) for the big .data/.wasm files
# - .data cached immutably; .wasm no-store (reused filename, new bytes per build)
import os, sys, http.server, socketserver
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8091
DIRECTORY = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "ut-serve")
BUF = 1024 * 1024
class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=DIRECTORY, **k)
    def log_message(self, *a):
        pass
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        p = self.path.split("?")[0]
        if p.endswith(".data"):
            self.send_header("Cache-Control", "public, max-age=31536000, immutable")
        elif p.endswith(".wasm"):
            self.send_header("Cache-Control", "no-store")
        elif p.endswith(".js") or p.endswith(".html"):
            # bootstrap/loader JS changes between sessions; never let the browser
            # heuristically cache them (only the big .data should be cached).
            self.send_header("Cache-Control", "no-cache")
        super().end_headers()
    def do_GET(self):
        rng = self.headers.get("Range")
        if not rng:
            return super().do_GET()
        # Minimal single-range support for the big files
        try:
            m = None
            import re
            m = re.match(r"bytes=(\d+)-(\d*)$", rng)
            if not m:
                return super().do_GET()
            start = int(m.group(1)); end = m.group(2)
            path = self.translate_path(self.path)
            size = os.path.getsize(path)
            end = int(end) if end else size - 1
            end = min(end, size - 1)
            length = end - start + 1
            self.send_response(206)
            self.send_header("Content-Type", self.guess_type(path))
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
            self.send_header("Content-Length", str(length))
            self.end_headers()
            with open(path, "rb") as f:
                f.seek(start)
                remaining = length
                while remaining > 0:
                    chunk = f.read(min(BUF, remaining))
                    if not chunk:
                        break
                    try:
                        self.wfile.write(chunk)
                    except (BrokenPipeError, ConnectionResetError):
                        return
                    remaining -= len(chunk)
        except (BrokenPipeError, ConnectionResetError):
            pass
        except Exception as e:
            self.send_error(500, str(e))
class ThreadingHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True
if __name__ == "__main__":
    os.chdir(DIRECTORY)
    with ThreadingHTTPServer(("127.0.0.1", PORT), Handler) as httpd:
        print(f"Serving {DIRECTORY} on http://127.0.0.1:{PORT}/")
        httpd.serve_forever()
