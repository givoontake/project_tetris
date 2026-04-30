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
from tetris.resources.define import *
from tetris.models.dataclass import RoomData


class RoomInfo:
    LOCKED_WIDTH_RATE = 0.2
    TITLE_WIDTH_RATE = 0.4
    CAPACITY_WIDTH_RATE = 0.2
    STATUS_WIDTH_RATE = 0.1
    JOIN_WIDTH_RATE = 0.1
    BORDER_RADIUS = 0
    BORDER_WIDTH = 1

    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager,
                 background_color: tuple[int, int, int], data: S2C_ROOM_INFO_PACKET = None):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.data = data
        self.is_reactable = False
        if data != None:
            if data.is_play == False:
                self.is_reactable = True
        self.background_color = background_color

        self.background = Rectangle(self.screen, self.rect, self.rm, False, None, "")
        self.info_rects: list[Rectangle] = []

        self.text_color = WHITE

        self.set_layout()

    def _make_info_rect(self, rect: pygame.Rect, text: str) -> Rectangle:
        info = Rectangle(self.screen, rect, self.rm, False, None, text)
        info.set_background_color((0, 0, 0, 0))
        return info

    def set_layout(self):
        if self.data == None:
            is_private = "공개 여부"
        else:
            if self.data.is_private:
                is_private = "비공개"
            else:
                is_private = "공개"

        locked_rect = pygame.Rect(self.rect.x, self.rect.y, self.rect.w * self.LOCKED_WIDTH_RATE, self.rect.h)
        self.is_private = self._make_info_rect(locked_rect, is_private)
        self.info_rects.append(self.is_private)

        title_rect = locked_rect.copy()
        title_rect.x += locked_rect.w
        title_rect.w = self.rect.w * self.TITLE_WIDTH_RATE
        if self.data == None:
            room_name = "방 제목"
        else:
            room_name = self.data.room_name
        self.title = self._make_info_rect(title_rect, room_name)
        self.info_rects.append(self.title)

        capacity_rect = title_rect.copy()
        capacity_rect.x += title_rect.w
        capacity_rect.w = self.rect.w * self.CAPACITY_WIDTH_RATE
        if self.data == None:
            capacity_text = "인원 수"
        else:
            capacity_text = f"{self.data.cur_user}/{self.data.max_user}"
        self.capacity = self._make_info_rect(capacity_rect, capacity_text)
        self.info_rects.append(self.capacity)

        status_rect = capacity_rect.copy()
        status_rect.x += capacity_rect.w
        status_rect.w = self.rect.w * self.STATUS_WIDTH_RATE
        if self.data == None:
            is_play = "상태"
        else:
            if self.data.is_play:
                is_play = "게임중"
            else:
                is_play = "대기"
        self.is_play = self._make_info_rect(status_rect, is_play)
        self.info_rects.append(self.is_play)

        self.join_button = None
        self.join_red = None
        self.join_rect = status_rect.copy()
        self.join_rect.x += status_rect.w
        self.join_rect.w = self.rect.w * self.JOIN_WIDTH_RATE
        if self.is_reactable and self.data.is_play == False:
            self.join_button = Button(self.screen, self.join_rect, self.rm, "참가", 0)
            self.join_button.set_images(
                self.rm.images.ui_images[UI_BUTTON_GREEN],
                self.rm.images.ui_images[UI_BUTTON_BLUE],
                self.rm.images.ui_images[UI_BUTTON_ORANGE]
            )
            self.join_button.set_text_size(25)
        else:
            self.join_red = Rectangle(
                self.screen,
                self.join_rect,
                self.rm,
                True,
                self.rm.images.ui_images[UI_BUTTON_RED],
                "참가"
            )

    def update_info(self, data: S2C_ROOM_INFO_PACKET):
        self.room_gen = data.room_gen
        self.title = data.room_name
        self.is_private = data.is_private
        self.cur_user = data.cur_user
        self.max_user = data.max_user
        self.is_play = data.is_play
        self.set_layout()

    def handle_event(self, ev: pygame.event.Event) -> bool:
        if self.is_reactable == False:
            return None

        if self.join_button:
            return self.join_button.handle_event(ev)

        return False

    def set_background_color(self, color: tuple[int, int, int]): # r, g, b
        self.background.set_background_color(color)
        for info in self.info_rects:
            info.set_background_color((0, 0, 0, 0))

    def update_info_rects(self, new_rect_y: int):
        self.rect.y = new_rect_y
        self.background.rect.y = new_rect_y
        for info in self.info_rects:
            info.rect.y = new_rect_y
        self.join_rect.y = new_rect_y
        if self.join_button:
            self.join_button.button.rect.y = new_rect_y
        if self.join_red:
            self.join_red.rect.y = new_rect_y

    def draw(self):
        bg_color = self.background_color
        if len(bg_color) == 3:
            bg_color = (bg_color[0], bg_color[1], bg_color[2], 120)
        bg_surface = pygame.Surface((self.rect.w, self.rect.h), pygame.SRCALPHA)
        pygame.draw.rect(
            bg_surface,
            bg_color,
            bg_surface.get_rect(),
            border_radius=self.BORDER_RADIUS
        )
        self.screen.blit(bg_surface, self.rect.topleft)
        pygame.draw.rect(self.screen, WHITE, self.rect, self.BORDER_WIDTH, border_radius=self.BORDER_RADIUS)
        for info in self.info_rects:
            info.draw()

        if self.join_button:
            self.join_button.draw()
        if self.join_red:
            self.join_red.draw()
