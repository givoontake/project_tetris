# network.py
import socket
import threading
import queue

from define import SERVER_HOST, SERVER_PORT
from session import Session
from packet_type import *              # S2C_LOGIN 등 타입 상수
from handle_packet import HandlePacket  # 공통 직렬화/파싱기

class NetworkClient:
    """
    - 연결 후 수신 스레드에서 TCP 스트림을 버퍼링
    - HandlePacket.parse_stream()으로 패킷 경계 파싱
    - S2C_LOGIN 수신 시 Session.id 설정 → 로그인 완료
    """
    def __init__(self, session: Session):
        self.session = session
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.running = False
        self.recv_thread = None
        self.send_thread = None

        self._send_q = queue.Queue()
        self._send_ev = threading.Event()
        self._lock = threading.Lock()

        self._rx_buffer = bytearray()
        self._pkt = HandlePacket()  # ✅ 포맷/언팩 담당자

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
        if not self.running:
            return
        self._send_q.put(data)
        self._send_ev.set()

    # ---- receive / parse ----
    def recv_loop(self):
        try:
            while self.running:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                self._rx_buffer.extend(chunk)
                self._drain_packets()   # ✅ 이름 유지 (내부 구현만 변경)
        except Exception:
            pass
        finally:
            self._request_close()

    def _drain_packets(self):
        """
        HandlePacket을 이용해 _rx_buffer에서 가능한 모든 패킷을 꺼내 처리.
        (이 함수명은 기존 호출부 호환을 위해 유지)
        """
        for ptype, pkt in self._pkt.parse_stream(self._rx_buffer):
            if ptype == S2C_LOGIN:
                # 로그인 성공 → 내 ID 설정
                self.session.set_id(pkt.id)
            # TODO: 다른 타입도 여기서 분기 추가

    # ---- send loop ----
    def send_loop(self):
        try:
            while self.running:
                self._send_ev.wait(timeout=1.0)
                if not self.running:
                    break
                self._send_ev.clear()
                while not self._send_q.empty():
                    try:
                        payload = self._send_q.get_nowait()
                    except queue.Empty:
                        break
                    try:
                        self.sock.sendall(payload)
                    except Exception:
                        self._request_close()
                        break
        except Exception:
            pass
        finally:
            self._request_close()

    # ---- close ----
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
            self._send_ev.set()

    def close(self):
        self._request_close()
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
