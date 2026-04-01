# room_window.py
import pygame
from typing import Optional

from tetris.config.define import *
from tetris.net.packet_structs import S2C_ROOM_INFO_PACKET
from tetris.ui.rectangle import Rectangle
from tetris.ui.button import Button
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import Fonts
from tetris.resources.define_colors import *
from tetris.models.dataclass import RoomData

# 행 하나(방 하나)를 표현하는 뷰-오브젝트
class RoomInfo:
    LOCKED_WIDTH_RATE = 0.1
    TITLE_WIDTH_RATE = 0.4
    CAPACITY_WIDTH_RATE = 0.3
    STATUS_WIDTH_RATE = 0.1
    JOIN_WIDTH_RATE = 0.1
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager,
                 background_color: tuple[int, int, int], data: S2C_ROOM_INFO_PACKET = None):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.data = data
        self.is_reactable = False
        if data != None:
            if data.is_play == False: self.is_reactable = True
        self.background_color = background_color
            
        self.background = Rectangle(self.screen, self.rect, self.rm, None, "")
        self.info_rects: list[Rectangle] = []

        self.text_color = WHITE

        self.set_layout()

    def set_layout(self):
        # [잠금아이콘, 제목, 인원, 상태]의 상대폭 비율

        if self.data == None: is_private = "공개여부"
        else: 
            if self.data.is_private: is_private = "비공개"
            else: is_private = "공개"
            
        locked_rect = pygame.Rect(self.rect.x, self.rect.y, self.rect.w*self.LOCKED_WIDTH_RATE, self.rect.h)       
        self.is_private = Rectangle(self.screen, locked_rect, self.rm, None, is_private)
        self.info_rects.append(self.is_private)

        title_rect = locked_rect.copy()
        title_rect.x += locked_rect.w
        title_rect.w = self.rect.w*self.TITLE_WIDTH_RATE
        if self.data == None: room_name = "방 이름"
        else: room_name = self.data.room_name
        self.title = Rectangle(self.screen, title_rect, self.rm, None, room_name)
        self.info_rects.append(self.title)

        capacity_rect = title_rect.copy()
        capacity_rect.x += title_rect.w
        capacity_rect.w = self.rect.w*self.CAPACITY_WIDTH_RATE
        if self.data == None: 
            cur_user = "현재인원"
            max_user = "최대인원"
        else: 
            cur_user = str(self.data.cur_user)
            max_user = str(self.data.max_user)
        self.capacity = Rectangle(self.screen, capacity_rect, self.rm, None, f"{cur_user}/{max_user}")
        self.info_rects.append(self.capacity)

        status_rect = capacity_rect.copy()
        status_rect.x += capacity_rect.w
        status_rect.w = self.rect.w*self.STATUS_WIDTH_RATE
        if self.data == None: is_play = "방 상태"
        else: 
            if self.data.is_play: is_play = "게임중"
            else: is_play = "대기"
        self.is_play = Rectangle(self.screen, status_rect, self.rm, None, f"{is_play}")
        self.info_rects.append(self.is_play)

        self.join_button = None
        if self.is_reactable and self.data.is_play == False: 
            join_rect = status_rect.copy()
            join_rect.x += status_rect.w
            join_rect.w = self.rect.w*self.JOIN_WIDTH_RATE
            self.join_button = Button(self.screen, join_rect, self.rm, None, "참가")

    def update_info(self, data: S2C_ROOM_INFO_PACKET):
        self.room_id = data.room_id
        self.title = data.room_name
        self.is_private = data.is_private
        self.cur_user = data.cur_user
        self.max_user = data.max_user
        self.is_play = data.is_play
        self.set_layout()

    def handle_event(self, ev: pygame.event.Event) -> bool:
        if self.is_reactable == False: return None
        
        if self.join_button:
            return self.join_button.handle_event(ev)
        
        return False

    def set_background_color(self, color: tuple[int, int, int]): # r, g, b
        self.background.set_background_color(color)
        for info in self.info_rects:
            info.set_background_color(color)

    def update_info_rects(self, new_rect_y: int):
        for info in self.info_rects:
            info.rect.y = new_rect_y

    def draw(self):
        for info in self.info_rects:
            info.draw()

        if self.join_button: self.join_button.draw()
