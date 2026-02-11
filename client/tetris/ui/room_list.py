import pygame
from typing import Optional

from tetris.models.dataclass import *
from tetris.config.define import *
from tetris.ui.rectangle import Rectangle
from tetris.ui.room_info import RoomInfo
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.font_manager import FontManager
from tetris.resources.define_colors import *
class RoomList:
    VISIBLE_ROOM = 8
    BACKGROUND_1 = GRAY
    BACKGROUND_2 = DARK_GRAY

    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager, fm: FontManager):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.fm = fm
        self.background = Rectangle(screen, rect, fm, None, "")
        header_rect = rect.copy()
        header_rect.h = rect.h / 8
        self.header = RoomInfo(screen, header_rect, rm, fm, RoomData(), BLACK, False)
        self.base_room_rect = header_rect.copy()
        self.base_room_rect.y += header_rect.h
        self.rooms: list[RoomInfo] = []
        self.show_rooms: list[RoomInfo] = []

        # 데이터
        self.rooms: list[RoomInfo] = []
        self.page_index: int = 0

    def set_layout(self):
        pass

    def update_show_rooms(self):
        start = self.page_index * self.VISIBLE_ROOM
        end = start + self.VISIBLE_ROOM
        if end > len(self.rooms) - 1:
            end = len(self.rooms)

        self.show_rooms = self.rooms[start:end]

    # ---------------- 외부 API ---------------- #
    def clear(self):
        self.rooms.clear()
        self.page_index = 0

    def update_room(self, info: RoomData | RoomData): # 기존 방에 대한 업데이트만 수행
        for room in self.rooms:
            if room.room_id == info.room_id:
                room.update_info(info)
                break

    def update_rooms(self, start_index: int):
        for index in range(start_index, len(self.rooms)):
            if index % 2: color = GRAY
            else: color = DARK_GRAY
            rect_index = index % self.VISIBLE_ROOM
            room_rect = self.base_room_rect.copy()
            room_rect.y = self.base_room_rect.y + rect_index*self.base_room_rect.h
            self.rooms[index].update_info_rects(room_rect.y)
            self.rooms[index].set_background_color(color)

    def add_room(self, info: RoomData | RoomData):
        add_index = len(self.rooms) # 추가되는 부분의 인덱스는 길이와 같다.
        if add_index % 2: color = GRAY
        else: color = DARK_GRAY
        rect_index = add_index % self.VISIBLE_ROOM
        room_rect = self.base_room_rect.copy()
        room_rect.y = self.base_room_rect.y + rect_index*self.base_room_rect.h
        room = RoomInfo(self.screen, room_rect, self.rm, self.fm, info, color, True)
        self.rooms.append(room)
        self.update_show_rooms()

    def delete_room(self, delete_room_id: int):
        for index in range(len(self.rooms)):
            if self.rooms[index].room_id == delete_room_id:
                del self.rooms[index]
                self.update_rooms(index)
                break

        self.update_show_rooms()

    def handle_event(self, ev: pygame.event.Event) -> Optional[int]:
        for room in self.show_rooms:
            result = room.handle_event(ev)
            if result is not None:
                return result
        return None

    def draw(self):
        self.background.draw()
        self.header.draw()
        for room in self.show_rooms:
            room.draw()
