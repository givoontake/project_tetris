# tetris/ui/user_list.py
import pygame

from tetris.ui.scroll_window_base import ScrollWindowBase
from tetris.ui.user_info import UserInfo, NICKNAME_WIDTH_RATE
from tetris.ui.rectangle import Rectangle
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define_colors import *

HEADER_HEIGHT = 25


class UserList(ScrollWindowBase):
    BACKGROUND_1 = GRAY
    BACKGROUND_2 = DARK_GRAY
    BORDER_RADIUS = 0
    BORDER_WIDTH = 1

    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager):
        self.screen = screen
        self.rect = rect
        self.rm = rm

        self.background = Rectangle(screen, rect, rm, False, None, "", 0)
        self.header_rects: list[Rectangle] = []

        header_height = HEADER_HEIGHT
        header_rect = rect.copy()
        header_rect.h = header_height
        self.header_rect = header_rect.copy()

        nickname_rect = header_rect.copy()
        nickname_rect.w = int(header_rect.w * NICKNAME_WIDTH_RATE)

        state_rect = header_rect.copy()
        state_rect.x = nickname_rect.x + nickname_rect.w
        state_rect.w = header_rect.w - nickname_rect.w

        self.nickname_header = Rectangle(screen, nickname_rect, rm, False, None, "닉네임", 0)
        self.nickname_header.set_background_color((0, 0, 0, 0))
        self.nickname_header.set_text_size(15)
        self.nickname_header.set_border(0)
        self.state_header = Rectangle(screen, state_rect, rm, False, None, "상태", 0)
        self.state_header.set_background_color((0, 0, 0, 0))
        self.state_header.set_text_size(15)
        self.state_header.set_border(0)
        self.header_rects = [self.nickname_header, self.state_header]

        self.list_rect = rect.copy()
        self.list_rect.y += header_height
        self.list_rect.h -= header_height

        self.base_user_rect = self.list_rect.copy()
        self.base_user_rect.h = header_height

        self.user_infos: list[UserInfo] = []
        self.show_user_infos: list[UserInfo] = []

        super().__init__(screen, self.list_rect, self.base_user_rect.h)

        self.set_scroll_info()
        self.set_scroll_len()

    def get_items_len(self) -> int:
        return len(self.user_infos)

    def update_show_user_infos(self):
        if self.scrollable_line > 0:
            self.show_end = self.show_start + self.show_lines
            if self.show_end > len(self.user_infos):
                self.show_end = len(self.user_infos)
        else:
            self.show_start = 0
            self.show_end = len(self.user_infos)

        self.show_user_infos = self.user_infos[self.show_start:self.show_end]

    def add_user(self, id: int, nickname: str, state: bool = False):
        for user_info in self.user_infos:
            if user_info.id == id:
                user_info.set_nickname(nickname)
                user_info.set_state(state)
                return

        add_index = len(self.user_infos)
        if add_index % 2:
            color = self.BACKGROUND_1
        else:
            color = self.BACKGROUND_2

        rect_index = add_index % self.show_lines
        user_rect = self.base_user_rect.copy()
        user_rect.y = self.base_user_rect.y + rect_index * self.base_user_rect.h

        user_info = UserInfo(self.screen, user_rect, self.rm, id, nickname, state)
        user_info.set_background_color(color)
        self.user_infos.append(user_info)

        self.set_scroll_info()
        self.set_scroll_len()
        self.update_show_user_infos()

    def delete_user(self, delete_user_id: int):
        for index in range(len(self.user_infos)):
            if self.user_infos[index].id == delete_user_id:
                del self.user_infos[index]
                self.update_user_infos(index)
                break

        self.set_scroll_info()
        self.set_scroll_len()
        self.set_show_start_index()
        self.update_show_user_infos()

    def update_user(self, id: int, nickname: str, state: bool = False):
        for user_info in self.user_infos:
            if user_info.id == id:
                user_info.set_nickname(nickname)
                user_info.set_state(state)
                break

    def update_user_state(self, id: int, state: bool = False):
        for user_info in self.user_infos:
            if user_info.id == id:
                user_info.set_state(state)
                break

    def update_user_infos(self, start_index: int):
        for index in range(start_index, len(self.user_infos)):
            if index % 2:
                color = self.BACKGROUND_1
            else:
                color = self.BACKGROUND_2

            rect_index = index % self.show_lines
            user_rect = self.base_user_rect.copy()
            user_rect.y = self.base_user_rect.y + rect_index * self.base_user_rect.h

            self.user_infos[index].set_rect(user_rect)
            self.user_infos[index].set_background_color(color)

    def clear(self):
        self.user_infos.clear()
        self.show_user_infos.clear()
        super().clear()

    def handle_event(self, ev: pygame.event.Event):
        super().handle_event(ev)
        self.update_show_user_infos()

        for user_info in self.show_user_infos:
            result = user_info.handle_event(ev)
            if result is not None:
                return result
        return None

    def draw_item(self, i: int, draw_text_y: int):
        user_info = self.user_infos[i]
        user_rect = self.base_user_rect.copy()
        user_rect.y = draw_text_y
        user_info.set_rect(user_rect)
        user_info.draw()

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
        pygame.draw.rect(self.screen, WHITE, self.header_rect, self.BORDER_WIDTH, border_radius=self.BORDER_RADIUS)
        for header in self.header_rects:
            header.draw()

        super().draw()
        self.update_show_user_infos()
