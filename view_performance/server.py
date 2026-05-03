import json
import socket
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse


WEB_HOST = "127.0.0.1"
WEB_PORT = 8080
GAME_SERVER_PORT = 12345
STRESS_VIEW_PORT = 23456

C2S_LOGIN = 2
S2V_SERVER_METRICS = 1
S2V_STRESS_METRICS = 2
V2S_STRESS_CONNECT_CONTROL = 3

MAX_USER_ID = 48
MAX_USER_PASSWORD = 48

BASE_DIR = Path(__file__).resolve().parent
TEMPLATE_DIR = BASE_DIR / "templates"
STATIC_DIR = BASE_DIR / "static"

HEADER_STRUCT = struct.Struct("<HB")
MAX_LOGICAL_PROCESSOR_METRICS = 64
MAX_TICK_WORKER_METRICS = 32
SERVER_METRICS_STRUCT = struct.Struct(
    "<QQQQQQqQQQQQ"
    + ("Q" * MAX_LOGICAL_PROCESSOR_METRICS)
    + ("Q" * MAX_TICK_WORKER_METRICS)
)
STRESS_METRICS_STRUCT = struct.Struct("<QQQQQQQQQ")
STRESS_CONNECT_CONTROL_STRUCT = struct.Struct("<B")

CONTENT_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
}


class MetricsHub:
    def __init__(self):
        self.lock = threading.Lock()
        self.server_socket = None
        self.stress_socket = None
        self.server_connected = False
        self.stress_connected = False
        self.server = {}
        self.stress = {}
        self.server_updated_at = None
        self.stress_updated_at = None

    def close(self):
        with self.lock:
            sockets = [self.server_socket, self.stress_socket]
            self.server_socket = None
            self.stress_socket = None
            self.server_connected = False
            self.stress_connected = False
        for sock in sockets:
            if sock:
                try:
                    sock.close()
                except OSError:
                    pass

    def close_target(self, name):
        with self.lock:
            if name == "server":
                sock = self.server_socket
                self.server_socket = None
                self.server_connected = False
            else:
                sock = self.stress_socket
                self.stress_socket = None
                self.stress_connected = False
        if sock:
            try:
                sock.close()
            except OSError:
                pass

    def snapshot(self):
        with self.lock:
            now = time.monotonic()
            server_fresh = self.server_updated_at is not None and now - self.server_updated_at <= 10
            stress_fresh = self.stress_updated_at is not None and now - self.stress_updated_at <= 10
            return {
                "server_connected": self.server_connected,
                "stress_connected": self.stress_connected,
                "server_fresh": server_fresh,
                "stress_fresh": stress_fresh,
                "server": dict(self.server),
                "stress": dict(self.stress),
            }

    def set_socket(self, name, sock):
        with self.lock:
            if name == "server":
                old_sock = self.server_socket
                self.server_socket = sock
                self.server_connected = True
            else:
                old_sock = self.stress_socket
                self.stress_socket = sock
                self.stress_connected = True
        if old_sock and old_sock is not sock:
            try:
                old_sock.close()
            except OSError:
                pass

    def mark_disconnected(self, name, disconnected_sock):
        with self.lock:
            if name == "server":
                if self.server_socket is not disconnected_sock:
                    return
                self.server_connected = False
                sock = self.server_socket
                self.server_socket = None
            else:
                if self.stress_socket is not disconnected_sock:
                    return
                self.stress_connected = False
                sock = self.stress_socket
                self.stress_socket = None
        if sock:
            try:
                sock.close()
            except OSError:
                pass

    def update_server(self, payload):
        values = SERVER_METRICS_STRUCT.unpack(payload[:SERVER_METRICS_STRUCT.size])
        keys = [
            "current_connected_users", "total_connected_users", "current_latency_ms",
            "average_latency_ms", "max_latency_ms", "completed_pending_total",
            "current_pending_count", "active_room_count", "server_memory_bytes",
            "created_thread_count", "logical_processor_count", "tick_worker_count",
        ]
        server_data = dict(zip(keys, values[:len(keys)]))
        processor_count = min(int(server_data.get("logical_processor_count", 0)), MAX_LOGICAL_PROCESSOR_METRICS)
        processor_start = len(keys)
        processor_usage = list(values[processor_start:processor_start + processor_count])
        tick_worker_count = min(int(server_data.get("tick_worker_count", 0)), MAX_TICK_WORKER_METRICS)
        tick_worker_start = processor_start + MAX_LOGICAL_PROCESSOR_METRICS
        tick_worker_ticks = list(values[tick_worker_start:tick_worker_start + tick_worker_count])
        server_data["logical_processor_usage"] = processor_usage
        server_data["tick_worker_ticks_per_second"] = tick_worker_ticks
        with self.lock:
            self.server = server_data
            self.server_connected = True
            self.server_updated_at = time.monotonic()

    def update_stress(self, payload):
        values = STRESS_METRICS_STRUCT.unpack(payload[:STRESS_METRICS_STRUCT.size])
        keys = [
            "connected_client", "playing_client", "current_latency_ms",
            "average_latency_ms", "max_latency_ms",
            "current_login_latency_ms", "average_login_latency_ms",
            "max_login_latency_ms", "connect_enabled",
        ]
        with self.lock:
            self.stress = dict(zip(keys, values))
            self.stress_connected = True
            self.stress_updated_at = time.monotonic()

    def send_stress_connect_control(self, enabled):
        with self.lock:
            sock = self.stress_socket
        if not sock:
            raise ConnectionError("stress socket is not connected")

        payload = STRESS_CONNECT_CONTROL_STRUCT.pack(1 if enabled else 0)
        packet_size = HEADER_STRUCT.size + len(payload)
        sock.sendall(HEADER_STRUCT.pack(packet_size, V2S_STRESS_CONNECT_CONTROL) + payload)


HUB = MetricsHub()


def parse_target(value, default_port, label):
    value = (value or "").strip()
    if not value:
        raise ValueError(f"{label} IP를 입력하세요.")
    if ":" in value:
        host, port = value.rsplit(":", 1)
        return host.strip(), int(port)
    return value, default_port


def recv_exact(sock, size):
    chunks = []
    remaining = size
    while remaining > 0:
        chunk = sock.recv(remaining)
        if not chunk:
            raise ConnectionError("socket closed")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def send_server_login(sock):
    login_id = b"server".ljust(MAX_USER_ID, b"\0")
    password = b"1234".ljust(MAX_USER_PASSWORD, b"\0")
    size = HEADER_STRUCT.size + MAX_USER_ID + MAX_USER_PASSWORD
    sock.sendall(HEADER_STRUCT.pack(size, C2S_LOGIN) + login_id + password)


def metric_reader(name, sock):
    try:
        while True:
            header = recv_exact(sock, HEADER_STRUCT.size)
            size, packet_type = HEADER_STRUCT.unpack(header)
            if size < HEADER_STRUCT.size:
                raise ConnectionError("invalid packet size")
            payload = recv_exact(sock, size - HEADER_STRUCT.size)
            if packet_type == S2V_SERVER_METRICS and len(payload) >= SERVER_METRICS_STRUCT.size:
                HUB.update_server(payload)
            elif packet_type == S2V_STRESS_METRICS and len(payload) >= STRESS_METRICS_STRUCT.size:
                HUB.update_stress(payload)
    except OSError:
        pass
    except ConnectionError:
        pass
    finally:
        HUB.mark_disconnected(name, sock)


def connect_target(name, host_value, default_port, label):
    HUB.close_target(name)
    target = parse_target(host_value, default_port, label)
    sock = None
    try:
        sock = socket.create_connection(target, timeout=5)
        sock.settimeout(None)
        if name == "server":
            send_server_login(sock)
        HUB.set_socket(name, sock)
        threading.Thread(target=metric_reader, args=(name, sock), daemon=True).start()
    except Exception:
        if sock:
            try:
                sock.close()
            except OSError:
                pass
        HUB.close_target(name)
        raise


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        return

    def _send_json(self, status, data):
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_file(self, file_path):
        if not file_path.exists() or not file_path.is_file():
            self.send_error(404)
            return

        body = file_path.read_bytes()
        content_type = CONTENT_TYPES.get(file_path.suffix, "application/octet-stream")
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/":
            self._send_file(TEMPLATE_DIR / "index.html")
            return

        if path.startswith("/static/"):
            file_path = (STATIC_DIR / path[len("/static/"):]).resolve()
            if STATIC_DIR.resolve() not in file_path.parents:
                self.send_error(403)
                return
            self._send_file(file_path)
            return

        if path == "/api/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.end_headers()
            try:
                while True:
                    data = json.dumps(HUB.snapshot())
                    self.wfile.write(f"data: {data}\n\n".encode("utf-8"))
                    self.wfile.flush()
                    time.sleep(1)
            except OSError:
                return

        self.send_error(404)

    def do_POST(self):
        path = urlparse(self.path).path
        content_length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(content_length) if content_length else b"{}"

        if path == "/api/connect/server":
            try:
                body = json.loads(raw.decode("utf-8"))
                connect_target("server", body.get("server_host"), GAME_SERVER_PORT, "Server")
                self._send_json(200, {"ok": True, "target": "server"})
            except Exception as exc:
                HUB.close_target("server")
                self._send_json(200, {"ok": False, "error": str(exc)})
            return

        if path == "/api/connect/stress":
            try:
                body = json.loads(raw.decode("utf-8"))
                connect_target("stress", body.get("stress_host"), STRESS_VIEW_PORT, "Stress")
                self._send_json(200, {"ok": True, "target": "stress"})
            except Exception as exc:
                HUB.close_target("stress")
                self._send_json(200, {"ok": False, "error": str(exc)})
            return

        if path == "/api/disconnect":
            HUB.close()
            self._send_json(200, {"ok": True})
            return

        if path == "/api/stress/connect-control":
            try:
                body = json.loads(raw.decode("utf-8"))
                HUB.send_stress_connect_control(bool(body.get("enabled")))
                self._send_json(200, {"ok": True})
            except Exception as exc:
                self._send_json(200, {"ok": False, "error": str(exc)})
            return

        self.send_error(404)


def main():
    print(f"performance view listen: {WEB_HOST}:{WEB_PORT}")
    print(f"local browser: http://{WEB_HOST}:{WEB_PORT}")
    ThreadingHTTPServer((WEB_HOST, WEB_PORT), Handler).serve_forever()


if __name__ == "__main__":
    main()
