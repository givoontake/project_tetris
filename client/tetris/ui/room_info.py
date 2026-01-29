# room_window.py
import pygame
from typing import Optional

from tetris.config.define import *
from tetris.ui.rectangle import Rectangle
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.font_manager import FontManager
from tetris.models.dataclass import RoomData

# 행 하나(방 하나)를 표현하는 뷰-오브젝트
class RoomInfo:
    LOCKED_WIDTH_RATE = 0.15
    TITLE_WIDTH_RATE = 0.35
    CAPACITY_WIDTH_DATE = 0.35
    STATUS_WIDTH_RATE = 0.15
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager, fm: FontManager, 
                 data: RoomData, background_color: tuple[int, int, int], is_reactable: bool = False):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.fm = fm
        self.data = data
        self.background_color = background_color
        self.is_reactable = is_reactable

        self.background = Rectangle(self.screen, self.rect, self.fm, None, "")
        self.info_rects: list[Rectangle] = []

        self.hovered = False
        self.pressed = False
        self.text_color = WHITE

        self.set_layout()

    def set_layout(self):
        # [잠금아이콘, 제목, 인원, 상태]의 상대폭 비율

        locked_rect = pygame.Rect(self.rect.x, self.rect.y, self.rect.w*self.LOCKED_WIDTH_RATE, self.rect.h)       
        self.locked = Rectangle(self.screen, locked_rect, self.fm, None, str(self.data.locked))
        self.info_rects.append(self.locked)

        title_rect = locked_rect.copy()
        title_rect.x += locked_rect.w
        title_rect.w = self.rect.w*self.TITLE_WIDTH_RATE
        self.title = Rectangle(self.screen, title_rect, self.fm, None, str(self.data.title))
        self.info_rects.append(self.title)

        capacity_rect = title_rect.copy()
        capacity_rect.x += title_rect.w
        capacity_rect.w = self.rect.w*self.CAPACITY_WIDTH_DATE
        self.capacity = Rectangle(self.screen, capacity_rect, self.fm, None, f"{str(self.data.cur_user)}/{str(self.data.max_user)}")
        self.info_rects.append(self.capacity)

        status_rect = capacity_rect.copy()
        status_rect.x += capacity_rect.w
        status_rect.w = self.rect.w*self.STATUS_WIDTH_RATE
        self.status = Rectangle(self.screen, status_rect, self.fm, None, f"{str(self.data.status)}")
        self.info_rects.append(self.status)

    def update_info(self, data: RoomData):
        self.room_id = data.room_id
        self.title = data.title
        self.locked = data.locked
        self.cur_user = data.cur_user
        self.max_user = data.max_user
        self.status = data.status
        self.set_layout()

    def handle_event(self, ev: pygame.event.Event) -> Optional[int]:
        if self.is_reactable == False: return None
        clicked = False

        if ev.type == pygame.MOUSEMOTION:
            self.hovered = self.rect.collidepoint(ev.pos)

        elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            if self.rect.collidepoint(ev.pos):
                self.pressed = True
            else:
                self.pressed = False

        elif ev.type == pygame.MOUSEBUTTONUP and ev.button == 1:
            if self.pressed and self.rect.collidepoint(ev.pos):
                clicked = True
            self.pressed = False
 
        return self.room_id if clicked else None
    
    def set_react_color(self):
        if self.is_reactable: return

        color = self.background_color
        if self.pressed:
            color = ORANGE
        elif self.hovered:
            color = GRAY
        self.set_background_color(color)

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
