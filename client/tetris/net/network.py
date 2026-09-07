# network.py
import socket
import threading

from tetris.config.define import *
from tetris.net.packet_builder import PacketBuilder
from tetris.net.packet_manager import PacketManager

class NetworkWorker:
    """
    - 연결 후 수신 스레드에서 TCP 스트림을 버퍼링
    - PacketManager: merge → process (완성 패킷은 내부 thread-safe 큐에 적재)
    - 메인 스레드(상태 머신): 큐 드레인 → 필요 시 직접 파싱(S2C_LOGIN 등) → pm.handle_packet() 호출
    """

    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.running = False
        self.recv_thread: threading.Thread | None = None
        self.socket_lock = threading.Lock()

        self._pm = PacketManager()
        self.builder = PacketBuilder()

    # ---- 메인 스레드에서 사용할 접근자 ----


    # ---- 연결/해제 ----
    def connect_to_server(self) -> bool:
        try:
            temp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            temp_sock.connect((SERVER_HOST, SERVER_PORT))
            self.sock = temp_sock
            self.running = True
            self.recv_thread = threading.Thread(target=self.recv_loop, daemon=True)
            self.recv_thread.start()
            return True
        except Exception:
            self.running = False
            try:
                temp_sock.close()
            except Exception:
                pass
            return False

    # ---- 수신 루프 ----
    def recv_loop(self):
        try:
            while self.running:
                recv_packet = self.sock.recv(BUF_SIZE)
                if not recv_packet:
                    break
                # 병합 + 커팅(완성 패킷은 큐 적재)
                self._pm.merge_packet(recv_packet)
                self._pm.process_packet()
        except Exception as e:
            print(f"[recv_loop] 예외 발생: {type(e).__name__} - {e}")
            self.sock.close()
            

    # ---- 즉시 송신(필요 시) ----
    def send_packet(self, data: bytes) -> bool:
        if not self.running:
            return False
        try:
            with self.socket_lock:
                self.sock.sendall(data)
            return True
        except Exception as e:
            print(f"[send_packet] 예외 발생: {type(e).__name__} - {e}")
            return False
