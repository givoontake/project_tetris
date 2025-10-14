# network.py
import socket
import threading
import queue
from define import SERVER_HOST, SERVER_PORT

class NetworkClient:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.running = False
        self.recv_thread = None
        self.send_thread = None

        # 송신 큐 & 이벤트로 바쁜 대기 방지
        self._send_q = queue.Queue()
        self._send_ev = threading.Event()
        self._lock = threading.Lock()

    def connect(self):
        try:
            self.sock.connect((SERVER_HOST, SERVER_PORT))
            self.running = True
            self.recv_thread = threading.Thread(target=self.recv_loop, daemon=True)
            self.send_thread = threading.Thread(target=self.send_loop, daemon=True)
            self.recv_thread.start()
            self.send_thread.start()
            return True
        except Exception:
            self.running = False
            try:
                self.sock.close()
            except Exception:
                pass
            return False

    def send(self, data: bytes):
        """필요 시 사용할 송신 API (기존 기능 유지 + 확장)."""
        if not self.running:
            return
        self._send_q.put(data)
        self._send_ev.set()

    def recv_loop(self):
        try:
            while self.running:
                data = self.sock.recv(4096)
                if not data:
                    break
                # TODO: 수신 데이터 처리(상태/큐로 전달) — 현재는 자리만 유지
        except Exception:
            # 필요 시 로깅
            pass
        finally:
            self._request_close()

    def send_loop(self):
        try:
            while self.running:
                # 이벤트 대기(큐가 비었으면 슬립)
                self._send_ev.wait(timeout=1.0)
                if not self.running:
                    break
                # 이벤트 리셋 후 배치 송신
                self._send_ev.clear()
                while not self._send_q.empty():
                    try:
                        payload = self._send_q.get_nowait()
                    except queue.Empty:
                        break
                    try:
                        self.sock.sendall(payload)
                    except Exception:
                        # 송신 실패 시 종료
                        self._request_close()
                        break
        except Exception:
            pass
        finally:
            self._request_close()

    def _request_close(self):
        with self._lock:
            if not self.running:
                return
            self.running = False
            try:
                # 반쯤 열린 상태 방지
                self.sock.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass
            try:
                self.sock.close()
            except Exception:
                pass
            # 대기중인 송신 스레드 깨우기
            self._send_ev.set()

    def close(self):
        self._request_close()
        # 스레드가 있다면 조인 시도(데몬이므로 생략 가능)
        if self.recv_thread and self.recv_thread.is_alive():
            try:
                self.recv_thread.join(timeout=0.2)
            except Exception:
                pass
        if self.send_thread and self.send_thread.is_alive():
            try:
                self.send_thread.join(timeout=0.2)
            except Exception:
                pass
