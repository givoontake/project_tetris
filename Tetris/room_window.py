# room_window.py
import pygame
from typing import Optional, Iterable

# 기존 버튼 색/폰트 규칙을 그대로 활용
from menu import Button, ORANGE, GRAY, WHITE, BLACK

# 행 하나(방 하나)를 표현하는 뷰-오브젝트
class RoomInfo:
    """
    하나의 방 행. Button의 상호작용 규칙(hover/press/색)을 그대로 따름.
    클릭 성립 시 room_id를 반환하고, 아니면 None.
    """
    def __init__(self, rect: pygame.Rect, room_id: int,
                 locked: bool, title: str, cur_user: int, max_user: int, status: str, is_header: bool = False):
        
        self.rect = rect
        self.room_id = room_id
        self.title = title
        self.locked = locked
        self.cur_user = cur_user
        self.max_user = max_user
        self.status = status
        self.is_header = is_header

        if is_header == True:
            self.room_id = -1
            self.title = "방 제목"
            self.locked = "공개"
            self.cur_user = "현재 인원"
            self.max_user = "최대 인원"
            self.status = "방 상태"

        # 버튼과 동일한 상호작용 상태
        self.hovered = False
        self.pressed = False

        # 공용 폰트 사용(없으면 Button에서 생성해 둠)
        if Button.shared_font is None:
            Button.shared_font = pygame.font.Font("resource/dodamdodam.ttf", 28)
        self.font = Button.shared_font

        # 열 배치(비율 기반) — 필요하면 RoomWindow에서 설정을 주입하여 덮어씀
        # [잠금아이콘, 제목, 인원, 상태]의 상대폭 비율
        self.info_width_rate = (0.1, 0.4, 0.4, 0.1)

        # 텍스트 컬러는 버튼 규칙(항상 흰색) 유지
        self.text_color = WHITE
    
    def updata_info(self, info: "RoomInfo"):
        self.room_id = info.room_id
        self.title = info.title
        self.locked = info.locked
        self.cur_user = info.cur_user
        self.max_user = info.max_user
        self.status = info.status

    def handle_event(self, ev: pygame.event.Event) -> Optional[int]:
        if self.is_header: return None
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

    def _columns(self) -> list[pygame.Rect]:
        """현재 rect 기준으로 열(Rect)들을 계산."""
        x, y, w, h = self.rect
        w_lock = int(w * self.info_width_rate[0])
        w_title = int(w * self.info_width_rate[1])
        w_count = int(w * self.info_width_rate[2])
        w_state = w - (w_lock + w_title + w_count)
        cols = [
            pygame.Rect(x, y, w_lock, h),                      # 잠금
            pygame.Rect(x + w_lock, y, w_title, h),            # 제목
            pygame.Rect(x + w_lock + w_title, y, w_count, h),  # 인원
            pygame.Rect(x + w_lock + w_title + w_count, y, w_state, h)  # 상태
        ]
        return cols

    def draw(self, surface: pygame.Surface, background_color):
        # 배경
        bg_color = background_color
        if self.pressed:
            bg_color = ORANGE
        elif self.hovered:
             bg_color = GRAY
        pygame.draw.rect(surface, bg_color, self.rect, border_radius=6)

        # 구분선(하단 1px)
        pygame.draw.line(surface, (40, 40, 40),
                         (self.rect.left, self.rect.bottom-1),
                         (self.rect.right, self.rect.bottom-1))

        # 각 열에 텍스트 배치
        col_lock, col_title, col_count, col_state = self._columns()

        # 1) 잠금 표시
        lock_text = "공개" if self.locked else "비공개"
        lock_surf = self.font.render(lock_text, True, self.text_color)
        surface.blit(lock_surf, lock_surf.get_rect(center=col_lock.center))

        # 2) 제목
        title_surf = self.font.render(self.title, True, self.text_color)
        title_pos = (col_title.x + 12, col_title.y + (col_title.h - title_surf.get_height()) // 2)
        surface.blit(title_surf, title_pos)

        # 3) 인원 (cur/max)
        count_text = f"{self.cur_user}/{self.max_user}"
        count_surf = self.font.render(count_text, True, self.text_color)
        surface.blit(count_surf, count_surf.get_rect(center=col_count.center))

        # 4) 상태
        state_surf = self.font.render(self.status, True, self.text_color)
        surface.blit(state_surf, state_surf.get_rect(center=col_state.center))


class RoomWindow:
    
    VISIBLE_ROOM = 8
    BACKGROUND_1 = (10,10,10)
    BACKGROUND_2 = (50,50,50)

    def __init__(self, rect: pygame.Rect):
        self.rect = rect
        self.room_h = int(rect.h / self.VISIBLE_ROOM) # 기본값, _recalc_layout에서 다시 계산

        # 스타일
        self.bg_color = (22, 22, 22)
        self.border_color = (60, 60, 60)

        # 데이터
        self.header_info = RoomInfo(self.get_room_rect(0, True), 0, 0, 0, 0, 0, 0, True)
        self.rooms: list["RoomInfo"] = []
        self.page_index: int = 0

    def get_rect_index(self, index):
        return index % self.VISIBLE_ROOM

    def get_room_rect(self, room_index: int, is_header: bool = False) -> pygame.Rect:
        """0~N 슬롯의 rect 계산 (간격 없이 위에서부터 바로 쌓기)."""
        index = self.get_rect_index(room_index)
        x = self.rect.x
        y = self.rect.y + index * self.room_h
        if is_header == True:
            y = self.rect.y - self.room_h
        w = self.rect.w
        h = self.room_h
        return pygame.Rect(x, y, w, h)
    
    def set_room_rect(self, delete_index):
        for i in range(delete_index, len(self.rooms)):
            self.rooms[i].rect = self.get_room_rect(i)

    def get_show_rooms(self) -> list["RoomInfo"]:
        start = self.page_index * self.VISIBLE_ROOM
        end = start + self.VISIBLE_ROOM
        if end > len(self.rooms) -1:
            end = len(self.rooms)

        return self.rooms[start:end]

    # ---------------- 외부 API ---------------- #
    def clear(self):
        self.rooms.clear()
        self.page_index = 0

    def update_room(self, info: RoomInfo | RoomInfo): # 기존 방에 대한 업데이트만 수행
        for room in self.rooms:
            if room.room_id == info.room_id:
                room.updata_info(info)
                break

    def add_room(self, info: RoomInfo | RoomInfo):
        add_index = len(self.rooms) # 추가되는 부분의 인덱스는 길이와 같다.
        info.rect = self.get_room_rect(add_index)
        self.rooms.append(info)

    def delete_room(self, delete_room_id: int):
        for i in range(len(self.rooms)):
            if self.rooms[i].room_id == delete_room_id:
                del self.rooms[i]
                # self.rooms.remove(self.rooms[i])
                self.set_room_rect(i)
                break

    #def on_resize(self, rect: pygame.Rect):

    def handle_event(self, ev: pygame.event.Event) -> Optional[int]:
        for room in self.get_show_rooms():
            result = room.handle_event(ev)
            if result is not None:
                return result
        return None

    def draw(self, surface: pygame.Surface):

        pygame.draw.rect(surface, BLACK, self.rect)

        show_rooms = self.get_show_rooms()
        color = None

        self.header_info.draw(surface, BLACK)
        for i in range(len(show_rooms)):
            if i % 2 == 0:
                color = self.BACKGROUND_1
            else:
                color = self.BACKGROUND_2
            
            show_rooms[i].draw(surface, color)
