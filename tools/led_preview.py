#!/usr/bin/env python3
"""LED Preview Server.

Bridges LED frame data from the native firmware build to a browser visualizer.

The native build's NeoPixel stub broadcasts raw RGB frames via UDP to
localhost:9876. This script receives those frames, serves the HTML visualizer,
and streams frames to connected browsers via Server-Sent Events (SSE).

Usage:
    python3 tools/led_preview.py [--no-build] [--no-open]

    By default, builds the native firmware, spawns it, and opens the browser.
"""

import argparse
import base64
import http.server
import os
import signal
import socket
import subprocess
import sys
import threading
import time
import webbrowser

HTTP_PORT = 8080
UDP_PORT = 9876
NUM_LEDS = 18
FRAME_SIZE = NUM_LEDS * 3  # 54 bytes: 18 LEDs × 3 channels (RGB)

# Latest frame data, shared between UDP receiver and SSE handlers.
_frame_lock = threading.Lock()
_frame_data = b"\x00" * FRAME_SIZE
_frame_seq = 0

# Firmware stdin pipe, shared between stdin forwarder and API handler.
_fw_stdin = None
_fw_stdin_lock = threading.Lock()

# --------------------------------------------------------------------------
# UDP receiver: captures frames from the native firmware's NeoPixel stub.
# --------------------------------------------------------------------------

def udp_receiver():
    global _frame_data, _frame_seq
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("127.0.0.1", UDP_PORT))
    sock.settimeout(1.0)
    while True:
        try:
            data, _ = sock.recvfrom(512)
        except socket.timeout:
            continue
        except OSError:
            break
        if len(data) >= FRAME_SIZE:
            with _frame_lock:
                _frame_data = data
                _frame_seq += 1


# --------------------------------------------------------------------------
# HTTP server: serves the visualizer HTML and SSE frame stream.
# --------------------------------------------------------------------------

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
HTML_PATH = os.path.join(SCRIPT_DIR, "led_preview.html")


EDITOR_HTML_PATH = os.path.join(SCRIPT_DIR, "led_editor.html")


class PreviewHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/events":
            self._handle_sse()
        elif self.path == "/" or self.path == "/index.html":
            self._serve_html()
        elif self.path == "/editor":
            self._serve_file(EDITOR_HTML_PATH, "text/html")
        else:
            self.send_error(404)

    def do_POST(self):
        if self.path == "/api/send":
            self._handle_send()
        else:
            self.send_error(404)

    def _handle_send(self):
        """Receive a command and write it to the firmware's stdin."""
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length)
        try:
            data = __import__("json").loads(body)
            cmd = data.get("command", "")
        except Exception:
            self.send_error(400, "Invalid JSON")
            return

        if not cmd:
            self.send_error(400, "Missing 'command' field")
            return

        with _fw_stdin_lock:
            if _fw_stdin is None or _fw_stdin.closed:
                self.send_error(503, "Firmware not running")
                return
            try:
                _fw_stdin.write((cmd + "\n").encode())
                _fw_stdin.flush()
            except OSError as e:
                self.send_error(500, str(e))
                return

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(b'{"ok":true}')

    def do_OPTIONS(self):
        """Handle CORS preflight for /api/send."""
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def _serve_file(self, path, content_type):
        try:
            with open(path, "rb") as f:
                content = f.read()
        except FileNotFoundError:
            self.send_error(404, f"{os.path.basename(path)} not found")
            return
        self.send_response(200)
        self.send_header("Content-Type", f"{content_type}; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def _serve_html(self):
        self._serve_file(HTML_PATH, "text/html")

    def _handle_sse(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()

        last_seq = 0
        try:
            while True:
                with _frame_lock:
                    seq = _frame_seq
                    data = _frame_data
                if seq != last_seq:
                    last_seq = seq
                    # Send as base64-encoded binary for compact SSE payload.
                    payload = base64.b64encode(data).decode("ascii")
                    self.wfile.write(f"data: {payload}\n\n".encode())
                    self.wfile.flush()
                else:
                    time.sleep(0.005)  # ~200 Hz poll, well above 60fps
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, format, *args):
        # Silence per-request logging.
        pass


def http_server():
    server = http.server.HTTPServer(("127.0.0.1", HTTP_PORT), PreviewHandler)
    server.serve_forever()


# --------------------------------------------------------------------------
# Main: build, spawn firmware, start servers, open browser.
# --------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="LED animation preview server")
    parser.add_argument("--no-build", action="store_true", help="skip PlatformIO build")
    parser.add_argument("--no-open", action="store_true", help="don't open browser automatically")
    args = parser.parse_args()

    project_root = os.path.dirname(SCRIPT_DIR)
    native_bin = os.path.join(project_root, ".pio", "build", "native", "program")

    # Build native firmware.
    if not args.no_build:
        print("Building native firmware...")
        result = subprocess.run(
            ["pio", "run", "-e", "native"],
            cwd=project_root,
        )
        if result.returncode != 0:
            print("Build failed.", file=sys.stderr)
            sys.exit(1)

    if not os.path.exists(native_bin):
        print(f"Native binary not found: {native_bin}", file=sys.stderr)
        sys.exit(1)

    # Start UDP receiver.
    udp_thread = threading.Thread(target=udp_receiver, daemon=True)
    udp_thread.start()

    # Start HTTP server.
    http_thread = threading.Thread(target=http_server, daemon=True)
    http_thread.start()
    print(f"Visualizer at http://localhost:{HTTP_PORT}")
    print(f"Editor    at http://localhost:{HTTP_PORT}/editor")

    # Open browser.
    if not args.no_open:
        webbrowser.open(f"http://localhost:{HTTP_PORT}")

    # Spawn native firmware with piped stdin so the API can also write to it.
    print(f"Starting firmware... (Ctrl+C to quit)\n")
    firmware = subprocess.Popen(
        [native_bin],
        stdin=subprocess.PIPE,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )

    global _fw_stdin
    with _fw_stdin_lock:
        _fw_stdin = firmware.stdin

    # Forward terminal stdin to firmware stdin in a background thread.
    def stdin_forwarder():
        try:
            while True:
                line = sys.stdin.readline()
                if not line:
                    break
                with _fw_stdin_lock:
                    if _fw_stdin and not _fw_stdin.closed:
                        _fw_stdin.write(line.encode() if isinstance(line, str) else line)
                        _fw_stdin.flush()
        except (OSError, ValueError):
            pass

    fwd_thread = threading.Thread(target=stdin_forwarder, daemon=True)
    fwd_thread.start()

    def cleanup(sig, frame):
        firmware.terminate()
        sys.exit(0)

    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)

    firmware.wait()


if __name__ == "__main__":
    main()
