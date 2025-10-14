# network.py
import socket
import threading
import queue
import struct
from define import SERVER_HOST, SERVER_PORT
from session import Session
from packet_type import *           # C++과 이름 완전 일치
from define_packet import *

class NetworkClient:
    """
    - 연결 즉시 수신 스레드에서 패킷 스트림을 병합/파싱
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
            try: self.sock.close()
            except Exception: pass
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
                self._drain_packets()
        except Exception:
            pass
        finally:
            self._request_close()

    def _drain_packets(self):
        """
        C 패킷 레이아웃:
          short size; char type; ...  (pack(1), little-endian 가정)
        HEADER_FMT_LE = '<Hb' (size(2), type(1))
        """
        view = memoryview(self._rx_buffer)
        offset = 0
        buflen = len(view)
        while True:
            if buflen - offset < HEADER_SIZE:
                break
            size, ptype = struct.unpack_from(HEADER_FMT_LE, view, offset)
            if buflen - offset < size:
                break  # payload 아직 덜 옴
            packet = view[offset: offset + size].tobytes()

            # ---- handle packet by type ----
            if ptype == S2C_LOGIN:
                pkt = unpack_S2C_LOGIN_PACKET(packet)
                # 5) S2C_LOGIN 수신 → 내 id 설정
                self.session.set_id(pkt["id"])

            # (추가 타입은 이후 확장)
            offset += size
        # 남은 바이트 보관
        del self._rx_buffer[:offset]

    # ---- send loop ----
    def send_loop(self):
        try:
            while self.running:
                self._send_ev.wait(timeout=1.0)
                if not self.running: break
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
            if not self.running: return
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
            try: self.recv_thread.join(timeout=0.2)
            except Exception: pass
        if self.send_thread and self.send_thread.is_alive():
            try: self.send_thread.join(timeout=0.2)
            except Exception: pass
