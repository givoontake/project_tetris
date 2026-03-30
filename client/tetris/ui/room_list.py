import pygame
from typing import Optional

from tetris.models.dataclass import *
from tetris.config.define import *
from tetris.net.packet_structs import S2C_ROOM_INFO_PACKET
from tetris.ui.rectangle import Rectangle
from tetris.ui.room_info import RoomInfo
from tetris.ui.button import Button
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import Fonts
from tetris.resources.define_colors import *
class RoomList:
    VISIBLE_ROOM = 6
    BACKGROUND_1 = GRAY
    BACKGROUND_2 = DARK_GRAY

    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.background = Rectangle(screen, rect, rm, None, "")
        self.rooms: list[RoomInfo] = []
        self.show_rooms: list[RoomInfo] = []

        # 데이터
        self.rooms: list[RoomInfo] = []
        self.cur_index: int = 0
        self.max_index: int = 0

        self.set_layout()

    def set_layout(self):
        header_rect = self.rect.copy()
        header_rect.h = self.rect.h / (self.VISIBLE_ROOM + 2) # 헤더, 버튼
        self.header = RoomInfo(self.screen, header_rect, self.rm, BLACK)
        self.show_room_start_rect = header_rect.copy()
        self.show_room_start_rect.y += header_rect.h

        self.btn_prev_page = None
        self.btn_next_page = None

        page_info_rect_h = header_rect.h
        page_info_rect_w = self.rect.w*0.1
        page_info_rect_x = self.rect.x + self.rect.w / 2 - page_info_rect_w / 2
        page_info_rect_y = self.rect.y + self.rect.h - header_rect.h # 버튼 높이는 헤더와 같다.
        page_info_rect = pygame.Rect(page_info_rect_x, page_info_rect_y, page_info_rect_w, page_info_rect_h)
        self.page_info_text = f"{self.cur_index + 1}/{self.max_index + 1}"
        self.page_info = Rectangle(self.screen, page_info_rect, self.rm, None, self.page_info_text)

        btn_prev_page_rect = page_info_rect.copy()
        btn_prev_page_rect.w = page_info_rect.h # 가로 세로를 h에 맞춤
        btn_prev_page_rect.x = page_info_rect.x - btn_prev_page_rect.w
        self.btn_prev_page = Button(self.screen, btn_prev_page_rect, self.rm, None, "<")

        btn_next_page_rect = btn_prev_page_rect.copy()
        btn_next_page_rect.x = page_info_rect_x + page_info_rect_w
        self.btn_next_page = Button(self.screen, btn_next_page_rect, self.rm, None, ">")

    def update_layout(self):
        if (len(self.rooms) % self.VISIBLE_ROOM) == 0:
            self.max_index = (len(self.rooms) // self.VISIBLE_ROOM) - 1 # 인덱스 기반이라 하나 빼야함
        else:
            self.max_index = (len(self.rooms) // self.VISIBLE_ROOM) # 마지막 페이지는 나머지로 나오므로 +1

        self.page_info_text = f"{self.cur_index + 1}/{self.max_index + 1}"
        self.page_info.set_text(self.page_info_text)
        start = self.cur_index*self.VISIBLE_ROOM
        end = start + self.VISIBLE_ROOM
        if end > len(self.rooms) - 1: # 마지막 페이지 예외 처리
            end = len(self.rooms)

        self.show_rooms = self.rooms[start:end]

    def clear(self):
        self.rooms.clear()
        self.show_rooms.clear()
        self.cur_index = 0
        self.max_index = 0

    def add_room(self, info: S2C_ROOM_INFO_PACKET):
        added_room_index = len(self.rooms) # 추가되는 방의 개별 인덱스는 길이와 같다.
        if added_room_index % 2: color = GRAY
        else: color = DARK_GRAY
        show_index = added_room_index % self.VISIBLE_ROOM
        room_rect = self.show_room_start_rect.copy()
        room_rect.y = self.show_room_start_rect.y + show_index*self.show_room_start_rect.h

        room = RoomInfo(self.screen, room_rect, self.rm, color, info)
        self.rooms.append(room)
        self.update_layout()

    def handle_event(self, ev: pygame.event.Event) -> Optional[int]: # 인덱스 반환(참가 버튼)
        if self.btn_prev_page.handle_event(ev):
            if self.cur_index > 0:
                self.cur_index -= 1
                self.page_info.set_text(self.page_info_text)
            self.update_layout()

        elif self.btn_next_page.handle_event(ev):
            if self.cur_index < self.max_index:
                self.cur_index += 1
                self.page_info.set_text(self.page_info_text)
            self.update_layout()

        rooms = self.show_rooms
        for i in range(len(rooms)):
            if rooms[i].handle_event(ev):
                return i
        return None

    def draw(self):
        self.background.draw()
        self.header.draw()
        for room in self.show_rooms:
            room.draw()

        self.page_info.draw()
        self.btn_prev_page.draw()
        self.btn_next_page.draw()
