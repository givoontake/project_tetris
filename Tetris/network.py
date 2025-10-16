# network.py
import socket
import threading

from define import SERVER_HOST, SERVER_PORT
from session import Session
from packet_manager import PacketManager


class NetworkClient:
    """
    - 연결 후 수신 스레드에서 TCP 스트림을 버퍼링
    - PacketManager: merge → process (완성 패킷은 내부 thread-safe 큐에 적재)
    - 메인 스레드(상태 머신): 큐 드레인 → 필요 시 직접 파싱(S2C_LOGIN 등) → pm.handle_packet() 호출
    """

    def __init__(self, session: Session):
        self.session = session
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.running = False
        self.recv_thread: threading.Thread | None = None
        self._lock = threading.Lock()

        self._pm = PacketManager()  # 핸들러 등록/콜백 사용하지 않음(로그인은 메인 스레드에서 직접 처리)

    # ---- 메인 스레드에서 사용할 접근자 ----
    def get_packet_manager(self) -> PacketManager:
        return self._pm

    def get_packet_queue(self):
        return self._pm.queue

    # ---- 연결/해제 ----
    def connect(self) -> bool:
        try:
            self.sock.connect((SERVER_HOST, SERVER_PORT))
            self.running = True
            self.recv_thread = threading.Thread(target=self.recv_loop, daemon=True)
            self.recv_thread.start()
            return True
        except Exception:
            self.running = False
            try:
                self.sock.close()
            except Exception:
                pass
            return False

    def _request_close(self):
        with self._lock:
            if not self.running:
                return
            self.running = False
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass
            try:
                self.sock.close()
            except Exception:
                pass

    def close(self):
        self._request_close()
        if self.recv_thread and self.recv_thread.is_alive():
            try:
                self.recv_thread.join(timeout=0.2)
            except Exception:
                pass

    # ---- 수신 루프 ----
    def recv_loop(self):
        try:
            while self.running:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                # 병합 + 커팅(완성 패킷은 큐 적재)
                self._pm.merge_packet(chunk)
                self._pm.process_packet()
        except Exception:
            pass
        finally:
            self._request_close()

    # ---- 즉시 송신(필요 시) ----
    def send_packet(self, data: bytes) -> bool:
        if not self.running:
            return False
        try:
            with self._lock:
                self.sock.sendall(data)
            return True
        except Exception:
            self._request_close()
            return False
