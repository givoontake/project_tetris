# session.py
from typing import Optional
import threading
from asset_manager import *

class Session:
    """
    클라이언트 자신의 최소 상태.
    - 현재 요구사항: id만 유지
    - 로그인 완료 판단: id가 None이 아니면 True
    """
    _shared = None
    _lock = threading.Lock()

    def __init__(self, block_asset):
        self.block_asset = block_asset
        self.id: Optional[int] = None
        self.block_texture = {'I': None, 'J': None, 'L': None, 'O': None, 'S': None, 'T': None, 'Z': None}
        self.update_texture({'I': DEFAULT_RED, 'J': DEFAULT_ORANGE, 'L': DEFAULT_YELLOW, 'O': DEFAULT_GREEN, 'S': DEFAULT_BLUE, 'T': DEFAULT_INDIGO, 'Z': DEFAULT_PURPLE})

    @classmethod
    def shared(cls) -> "Session":
        if cls._shared is None:
            with cls._lock:
                if cls._shared is None:
                    cls._shared = Session()
        return cls._shared
    
    def update_texture(self, new_texture: dict):
        for shape, new_type in new_texture.items():
            self.block_texture[shape] = self.block_asset[new_type]

    def reset(self):
        self.id = None

    def set_id(self, new_id: int):
        self.id = int(new_id)
