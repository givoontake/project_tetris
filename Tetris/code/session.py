# session.py
from typing import Optional
import threading
from resource_manager import *

class Session:
    """
    클라이언트 자신의 최소 상태.
    - 현재 요구사항: id만 유지
    - 로그인 완료 판단: id가 None이 아니면 True
    """
    _shared = None
    _lock = threading.Lock()

    def __init__(self):
        self.id = -1
        self.nickname = ""
        self.win = 0
        self.lose = 0
        self.max_score = 0
        self.block_texture = {'I': None, 'J': None, 'L': None, 'O': None, 'S': None, 'T': None, 'Z': None, 'G': None}
        # self.update_texture({'I': DEFAULT_RED, 'J': DEFAULT_ORANGE, 'L': DEFAULT_YELLOW, 'O': DEFAULT_GREEN, 'S': DEFAULT_BLUE, 'T': DEFAULT_INDIGO, 'Z': DEFAULT_PURPLE})

    @classmethod
    def shared(cls) -> "Session":
        if cls._shared is None:
            with cls._lock:
                if cls._shared is None:
                    cls._shared = Session()
        return cls._shared

    def load_texture(self, rm: ResourceManager):
       self.block_texture = {
        'Z': rm.block_images[DEFAULT_RED],      
        'L': rm.block_images[DEFAULT_ORANGE],  
        'O': rm.block_images[DEFAULT_YELLOW],
        'S': rm.block_images[DEFAULT_GREEN], 
        'J': rm.block_images[DEFAULT_BLUE],   
        'I': rm.block_images[DEFAULT_INDIGO],  
        'T': rm.block_images[DEFAULT_PURPLE],
        'G': rm.block_images[DEFAULT_GRAY],
    }
       
    def set_block_scale(self, rm: ResourceManager, block_length: int):
        for key, value in self.block_texture.items():
            self.block_texture[key] = rm.scale_image(value, block_length, block_length)

    # def set_id(self, new_id: int):
    #     self.id = int(new_id)
