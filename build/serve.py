#!/usr/bin/env python3
# Robust static server for the UT WebGPU build.
# - Cross-origin isolation headers (COOP/COEP/CORP) on EVERY response via an
#   end_headers() override — the document itself needs them or self.crossOriginIsolated
#   is false and SharedArrayBuffer can't transfer to the wasm pthread workers
#   (DataCloneError → game loads content but never starts → heartbeats forever).
#   NOTE: the root URL "/" is a directory served by the BASE handler, so the headers
#   MUST live in end_headers(), not in a do_GET-only helper.
# - PROPER HTTP Range (206) for the big .data (1.5GB) / .wasm (329MB); the base
#   handler answers 200+full-length to a Range request → ERR_CONTENT_LENGTH_MISMATCH
#   → the game hangs on "waiting on run dependencies".
# - .data cached immutably (stable across engine rebuilds); .wasm no-store (its
#   filename is reused every build with new bytes). Client aborts handled quietly.
import os, sys, http.server, socketserver

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8091
DIRECTORY = sys.argv[2] if len(sys.argv) > 2 else "."
BUF = 1024 * 1024

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=DIRECTORY, **k)

    def log_message(self, *a):
        pass

    def end_headers(self):
        # Applies to ALL responses (base-handler index/404s AND our Range do_GET).
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        self.send_header("Accept-Ranges", "bytes")
        p = self.path.split("?", 1)[0]
        if p.endswith(".data"):
            self.send_header("Cache-Control", "public, max-age=31536000, immutable")
        else:
            self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_GET(self):
        f = self.translate_path(self.path)
        if not os.path.isfile(f):
            return super().do_GET()  # dirs (→ index.html), 404s
        try:
            size = os.path.getsize(f)
            rng = self.headers.get("Range")
            start, end, partial = 0, size - 1, False
            if rng and rng.startswith("bytes="):
                spec = rng[6:].split(",")[0].strip()
                a, _, b = spec.partition("-")
                if a == "":
                    start = max(0, size - int(b)); end = size - 1
                else:
                    start = int(a); end = int(b) if b else size - 1
                if start > end or start >= size:
                    self.send_response(416)
                    self.send_header("Content-Range", f"bytes */{size}")
                    self.end_headers(); return
                end = min(end, size - 1); partial = True
            self.send_response(206 if partial else 200)
            self.send_header("Content-Type", self.guess_type(f))
            self.send_header("Content-Length", str(end - start + 1))
            if partial:
                self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
            self.end_headers()  # adds COOP/COEP/CORP/Accept-Ranges/Cache-Control
            if self.command == "HEAD":
                return
            remaining = end - start + 1
            with open(f, "rb") as fh:
                fh.seek(start)
                while remaining > 0:
                    chunk = fh.read(min(BUF, remaining))
                    if not chunk:
                        break
                    self.wfile.write(chunk)
                    remaining -= len(chunk)
        except (BrokenPipeError, ConnectionResetError):
            pass

    do_HEAD = do_GET

class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True

if __name__ == "__main__":
    with Server(("127.0.0.1", PORT), Handler) as httpd:
        print(f"serving {DIRECTORY} on 127.0.0.1:{PORT} (COISO+Range; .data cached, .wasm no-store)")
        httpd.serve_forever()
