#!/usr/bin/env python3
"""Offline HTTPS regression. Temporary self-signed fixture keys never enter assets."""
import argparse
import http.server
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
import time


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def do_GET(self):
        try:
            self.respond()
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass  # Expected when exercising abort and byte/timeout limits.

    def respond(self):
        if self.path in ("/redirect", "/downgrade", "/loop"):
            target = {"/redirect": "/ok", "/downgrade": f"http://localhost:{self.server.server_port}/ok", "/loop": "/loop"}[self.path]
            self.send_response(302)
            self.send_header("Location", target)
            self.send_header("Content-Length", "4")
            self.end_headers()
            self.wfile.write(b"skip")
            return
        if self.path.startswith("/status/"):
            code = int(self.path.rsplit("/", 1)[1])
            self.send_response(code)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path == "/truncated":
            self.send_response(200)
            self.send_header("Content-Length", "128")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(b"partial")
            self.close_connection = True
            return
        if self.path in ("/chunked", "/slow", "/stall"):
            self.send_response(200)
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            if self.path == "/stall":
                time.sleep(12)
            else:
                for _ in range(64):
                    self.wfile.write(b"400\r\n" + b"x" * 1024 + b"\r\n")
                    self.wfile.flush()
                    if self.path == "/slow":
                        time.sleep(0.05)
            self.wfile.write(b"0\r\n\r\n")
            return
        data = b"" if self.path == "/empty" else bytes((0, 1, 2, 3, 4, 5, 6, 255))
        self.send_response(200)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("public_ca", type=Path)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    if args.work_dir:
        args.work_dir.mkdir(parents=True, exist_ok=True)
    # WSL /mnt/c does not support FIFOs. Keep POSIX CA fixtures on its native
    # temporary filesystem even when certificate evidence uses --work-dir.
    with tempfile.TemporaryDirectory(prefix="r2n64-https-", dir=args.work_dir) as temporary, \
            tempfile.TemporaryDirectory(prefix="r2n64-ca ") as native_ca:
        directory = Path(temporary)
        certificate, key = directory / "localhost.pem", directory / "localhost.key"
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                        "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
                        "-keyout", str(key), "-out", str(certificate)], check=True, capture_output=True)
        ca_fixtures = Path(native_ca) / "CA fixtures with spaces"
        ca_fixtures.mkdir()
        pem = certificate.read_bytes()
        (ca_fixtures / "valid.pem").write_bytes(pem)
        (ca_fixtures / "empty.pem").touch()
        (ca_fixtures / "directory.pem").mkdir()
        (ca_fixtures / "link.pem").symlink_to(certificate.resolve())
        os.mkfifo(ca_fixtures / "fifo.pem", 0o600)
        (ca_fixtures / "malformed.pem").write_bytes(
            b"-----BEGIN CERTIFICATE-----\nnot-valid-base64\n-----END CERTIFICATE-----\n")
        ca_limit = 2 * 1024 * 1024
        (ca_fixtures / "boundary.pem").write_bytes(pem + b"\n" * (ca_limit - len(pem)))
        (ca_fixtures / "oversized.pem").write_bytes(pem + b"\n" * (ca_limit + 1 - len(pem)))
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        server.daemon_threads = True
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(certificate, key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            subprocess.run([str(args.binary.resolve()), f"https://localhost:{server.server_port}",
                            str(certificate), str(args.public_ca.resolve()), str(ca_fixtures)], check=True, timeout=35)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == "__main__":
    main()
