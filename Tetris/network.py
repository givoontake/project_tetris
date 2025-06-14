import socket
import threading
from define import SERVER_HOST, SERVER_PORT

class NetworkClient:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.running = False

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
            return False

    def recv_loop(self):
        while self.running:
            data = self.sock.recv(1024)
            if not data:
                break
        self.running = False

    def send_loop(self):
        while self.running:
            pass

    def close(self):
        self.running = False
        self.sock.close()