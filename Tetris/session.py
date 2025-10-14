# session.py
from typing import Optional
import threading

class Session:
    """
    클라이언트 자신의 최소 상태.
    - 현재 요구사항: id만 유지
    - 로그인 완료 판단: id가 None이 아니면 True
    """
    _shared = None
    _lock = threading.Lock()

    def __init__(self):
        self.id: Optional[int] = None

    @classmethod
    def shared(cls) -> "Session":
        if cls._shared is None:
            with cls._lock:
                if cls._shared is None:
                    cls._shared = Session()
        return cls._shared

    def reset(self):
        self.id = None

    def set_id(self, new_id: int):
        self.id = int(new_id)
