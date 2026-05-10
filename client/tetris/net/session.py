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
        self.block_texture_keys = DEFAULT_BLOCK_IMAGE_KEYS.copy()
        
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

    def set_block_texture(self, shape_key: str, block_image_key: int):
        if shape_key not in self.block_textures:
            return
        if block_image_key not in self.rm.images.block_images:
            return

        self.block_texture_keys[shape_key] = block_image_key
        self.block_textures[shape_key] = self.rm.images.block_images[block_image_key]
    
    def load_default_textures(self):
        self.block_texture_keys = DEFAULT_BLOCK_IMAGE_KEYS.copy()
        self.block_textures = {
            shape_key: self.rm.images.block_images[block_image_key]
            for shape_key, block_image_key in self.block_texture_keys.items()
        }

    # def set_id(self, new_id: int):
    #     self.id = int(new_id)
