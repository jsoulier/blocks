import http.server
import os

class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

os.chdir("build/bin")
print("http://localhost:8000/blocks.html")
http.server.ThreadingHTTPServer(("127.0.0.1", 8000), Handler).serve_forever()
