# session.py
from typing import Optional
import threading
from tetris.resources.resource_manager import *

class Session:
    def __init__(self, rm: ResourceManager):
        self.rm = rm
        self.id = -1
        self.is_self = False
        self.nickname = ""
        self.win = 0
        self.lose = 0
        self.max_score = 0
        self.block_textures = {'I': None, 'J': None, 'L': None, 'O': None, 'S': None, 'T': None, 'Z': None, 'G': None}
        
        self.load_default_textures()
    
    def set_my_session(self):
        self.is_self = True

    def set_is_not_my_session(self, id: int, nickname: str):
        self.id = id
        self.nickname = nickname

    def set_new_textures(self, new_textures: dict):
        if set(self.block_textures.keys()) != new_textures: # 키가 전부 일치하는지
            return 
        
        self.block_textures = new_textures # 텍스쳐 가져올 때 실제로 있는건지 검증하는 코드가 필요할 것 같기는 한데.. 일단 패스

    def load_default_textures(self):
       self.block_textures = {
        'Z': self.rm.images.block_images[DEFAULT_RED],      
        'L': self.rm.images.block_images[DEFAULT_ORANGE],  
        'O': self.rm.images.block_images[DEFAULT_YELLOW],
        'S': self.rm.images.block_images[DEFAULT_GREEN], 
        'J': self.rm.images.block_images[DEFAULT_BLUE],   
        'I': self.rm.images.block_images[DEFAULT_INDIGO],  
        'T': self.rm.images.block_images[DEFAULT_PURPLE],
        'G': self.rm.images.block_images[DEFAULT_GRAY],
    }

    # def set_id(self, new_id: int):
    #     self.id = int(new_id)
