import pygame
from typing import Optional

from tetris.models.dataclass import *
from tetris.config.define import *
from tetris.net.packet_structs import S2C_ROOM_INFO_PACKET
from tetris.ui.scroll_window_base import ScrollWindowBase
from tetris.ui.rectangle import Rectangle
from tetris.ui.room_info import RoomInfo
from tetris.ui.button import Button
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import Fonts
from tetris.resources.define_colors import *
from tetris.resources.define import *


class RoomList(ScrollWindowBase):
    VISIBLE_ROOM = 6
    BACKGROUND_1 = GRAY
    BACKGROUND_2 = DARK_GRAY
    PAGE_INFO_GAP = 10
    PAGE_INFO_HEIGHT = 50
    BORDER_RADIUS = 8
    BORDER_WIDTH = 1

    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.background = Rectangle(screen, rect, rm, False, None, "")
        self.rooms: list[RoomInfo] = []
        self.show_rooms: list[RoomInfo] = []

        self.set_layout()
        super().__init__(screen, self.background.rect, self.show_room_start_rect.h)
        self.set_scroll_info()
        self.set_scroll_len()
        self.update_show_rooms()

    def set_layout(self):
        background_rect = self.rect.copy()
        background_rect.h -= self.PAGE_INFO_GAP + self.PAGE_INFO_HEIGHT
        self.background.update_rect(background_rect)

        self.show_room_start_rect = background_rect.copy()
        self.show_room_start_rect.h = int(background_rect.h / self.VISIBLE_ROOM)

        refresh_rect = pygame.Rect(0, 0, 100, self.PAGE_INFO_HEIGHT)
        refresh_rect.x = self.rect.x + self.rect.w - refresh_rect.w
        refresh_rect.y = background_rect.y + background_rect.h + self.PAGE_INFO_GAP
        self.btn_refresh = Button(self.screen, refresh_rect, self.rm, "새로고침", 0)

    def get_items_len(self) -> int:
        return len(self.rooms)

    def update_show_rooms(self):
        if self.scrollable_line > 0:
            self.show_end = self.show_start + self.show_lines
            if self.show_end > len(self.rooms):
                self.show_end = len(self.rooms)
        else:
            self.show_start = 0
            self.show_end = len(self.rooms)

        self.show_rooms = self.rooms[self.show_start:self.show_end]
        for index in range(len(self.show_rooms)):
            room = self.show_rooms[index]
            new_rect_y = self.show_room_start_rect.y + index * self.show_room_start_rect.h
            room.update_info_rects(new_rect_y)
            if room.join_button:
                room.join_button.button.rect.y = new_rect_y

    def clear(self):
        self.rooms.clear()
        self.show_rooms.clear()
        super().clear()

    def add_room(self, info: S2C_ROOM_INFO_PACKET):
        added_room_index = len(self.rooms)
        if added_room_index % 2:
            color = GRAY
        else:
            color = DARK_GRAY
        show_index = added_room_index % self.show_lines
        room_rect = self.show_room_start_rect.copy()
        room_rect.y = self.show_room_start_rect.y + show_index * self.show_room_start_rect.h

        room = RoomInfo(self.screen, room_rect, self.rm, color, info)
        self.rooms.append(room)
        self.set_scroll_info()
        self.set_scroll_len()
        self.update_show_rooms()

    def handle_event(self, ev: pygame.event.Event) -> Optional[int | str]:
        super().handle_event(ev)
        self.update_show_rooms()

        if self.btn_refresh.handle_event(ev):
            return "refresh"

        for i in range(len(self.show_rooms)):
            if self.show_rooms[i].handle_event(ev):
                return i
        return None

    def draw_item(self, i: int, draw_text_y: int):
        room = self.rooms[i]
        room.update_info_rects(draw_text_y)
        if room.join_button:
            room.join_button.button.rect.y = draw_text_y
        room.draw()

    def draw(self):
        bg_surface = pygame.Surface((self.background.rect.w, self.background.rect.h), pygame.SRCALPHA)
        pygame.draw.rect(
            bg_surface,
            (0, 0, 0, 120),
            bg_surface.get_rect(),
            border_radius=self.BORDER_RADIUS
        )
        self.screen.blit(bg_surface, self.background.rect.topleft)
        pygame.draw.rect(self.screen, WHITE, self.background.rect, self.BORDER_WIDTH, border_radius=self.BORDER_RADIUS)
        super().draw()
        self.update_show_rooms()
        self.btn_refresh.draw()
