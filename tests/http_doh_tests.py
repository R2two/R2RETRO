#!/usr/bin/env python3
"""Offline DoH: real libcurl DNS wire requests, independent TLS endpoints."""
import http.server
import os
from pathlib import Path
import ssl
import struct
import subprocess
import sys
import tempfile
import threading
import time


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def send_body(self, code, body, content_type):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", f"https://second.invalid:{self.server.server_port}/ok")
            self.send_header("Content-Length", "0")
            self.end_headers()
        else:
            self.send_body(404 if self.path == "/missing" else 200, b"fixture", "text/plain")

    def do_POST(self):
        try:
            self.server.queries += 1
            query = self.rfile.read(int(self.headers["Content-Length"]))
            assert self.headers["Content-Type"] == "application/dns-message"
            if self.path == "/stall":
                time.sleep(6)
            if self.path == "/bad":
                self.send_body(200, b"bad DNS", "application/dns-message")
                return
            end = 12
            while query[end]:
                end += query[end] + 1
            end += 1
            qtype, qclass = struct.unpack("!HH", query[end:end+4])
            assert qclass == 1
            answer = qtype == 1
            body = query[:2] + struct.pack("!5H", 0x8180, 1, int(answer), 0, 0) + query[12:end+4]
            if answer:
                body += b"\xc0\x0c" + struct.pack("!HHIH", 1, 1, 60, 4) + b"\x7f\x00\x00\x01"
            self.send_body(200, body, "application/dns-message")
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass


def main():
    binary, public_ca = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
    servers = []
    with tempfile.TemporaryDirectory(prefix="r2retro-doh-") as temporary:
        root = Path(temporary)
        for name, san in (("web", "DNS:game.invalid,DNS:second.invalid"), ("dns", "DNS:resolver.invalid")):
            pem, key = root / f"{name}.pem", root / f"{name}.key"
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                            "-subj", f"/CN={name}", "-addext", "subjectAltName="+san,
                            "-keyout", str(key), "-out", str(pem)], check=True, capture_output=True)
            server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
            server.daemon_threads = True
            server.queries = 0
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.minimum_version = ssl.TLSVersion.TLSv1_2
            context.load_cert_chain(pem, key)
            server.socket = context.wrap_socket(server.socket, server_side=True)
            threading.Thread(target=server.serve_forever, daemon=True).start()
            servers.append(server)
        ca = root / "both.pem"
        ca.write_bytes((root/"web.pem").read_bytes()+(root/"dns.pem").read_bytes())
        base = f"https://game.invalid:{servers[0].server_port}"
        doh = f"https://resolver.invalid:{servers[1].server_port}"
        bootstrap = f"resolver.invalid:{servers[1].server_port}:127.0.0.1"
        env = {k: v for k, v in os.environ.items() if not k.lower().endswith("proxy")}
        try:
            cases = [
                (base+"/ok", ca, doh+"/dns-query", "ok"),
                (base+"/redirect", ca, doh+"/dns-query", "ok"),
                (base+"/missing", ca, doh+"/dns-query", "404"),
                (base+"/ok", public_ca, doh+"/dns-query", "fail"),
                (base+"/ok", root/"web.pem", doh+"/dns-query", "fail"),
                (base+"/ok", root/"dns.pem", doh+"/dns-query", "fail"),
                (base+"/ok", ca, doh.replace("resolver.invalid", "localhost")+"/dns-query", "fail"),
                (base+"/ok", ca, doh+"/bad", "fail"),
                (base+"/ok", ca, doh+"/stall", "cancel"),
                (base+"/ok", ca, doh.replace("https:", "http:")+"/dns-query", "fail"),
            ]
            for url, trust, endpoint, expected in cases:
                subprocess.run([binary, "--doh-smoke", url, str(trust), endpoint, expected, bootstrap],
                               env=env, check=True, timeout=18)
            assert servers[1].queries >= 6, "Did not exercise real DNS wire requests"
            print(f"PASS: {len(cases)} offline DoH cases; {servers[1].queries} DNS requests")
        finally:
            for server in servers:
                server.shutdown()
                server.server_close()


if __name__ == "__main__":
    main()
