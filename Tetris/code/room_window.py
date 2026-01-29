# room_window.py
import pygame
from define import *
from typing import Optional
from rectangle import Rectangle
from resource_manager import ResourceManager
from font_manager import FontManager

from dataclass import RoomData

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
