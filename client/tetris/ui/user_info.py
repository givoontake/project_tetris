# tetris/ui/user_info.py
import pygame
from typing import Optional

from tetris.models.dataclass import EventFriend
from tetris.ui.rectangle import Rectangle
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define_colors import *

NICKNAME_WIDTH_RATE = 0.7
STATE_WIDTH_RATE = 0.3


class UserInfo:
    BORDER_RADIUS = 8
    BORDER_WIDTH = 1

    def __init__(self,
        screen: pygame.Surface,
        rect: pygame.Rect,
        rm: ResourceManager,
        id: int,
        nickname: str,
        state: bool = False,
        border_width: int = 1
    ):
        self.screen = screen
        self.rect = rect
        self.rm = rm

        self.id = id
        self.nickname = nickname
        self.state = state
        self.border_width = border_width
        self.background_color = BLACK

        self.info_rects: list[Rectangle] = []

        self.set_layout()
        self.update_info()

    def set_layout(self):
        nickname_rect = self.rect.copy()
        nickname_rect.w = int(self.rect.w * NICKNAME_WIDTH_RATE)

        state_rect = self.rect.copy()
        state_rect.x = nickname_rect.x + nickname_rect.w
        state_rect.w = self.rect.w - nickname_rect.w

        self.nickname_rect = Rectangle(self.screen, nickname_rect, self.rm, False, None, "", 0)
        self.state_rect = Rectangle(self.screen, state_rect, self.rm, False, None, "", 0)

        self.info_rects = [self.nickname_rect, self.state_rect]

    def update_info(self):
        self.nickname_rect.set_text(self.nickname)
        if self.state:
            self.state_rect.set_text("접속중")
        else:
            self.state_rect.set_text("미접속")

    def set_state(self, new_state: bool = False):
        self.state = new_state
        self.update_info()

    def set_nickname(self, new_nickname: str):
        self.nickname = new_nickname
        self.update_info()

    def set_background_color(self, color: tuple[int, int, int]):
        self.background_color = color
        for info in self.info_rects:
            info.set_background_color(color)

    def set_text_color(self, color: tuple[int, int, int]):
        for info in self.info_rects:
            info.set_text_color(color)

    def update_info_rects(self, new_rect_y: int):
        self.rect.y = new_rect_y
        for info in self.info_rects:
            info.rect.y = new_rect_y

    def set_rect(self, new_rect: pygame.Rect):
        self.rect = new_rect

        nickname_rect = new_rect.copy()
        nickname_rect.w = int(new_rect.w * NICKNAME_WIDTH_RATE)

        state_rect = new_rect.copy()
        state_rect.x = nickname_rect.x + nickname_rect.w
        state_rect.w = new_rect.w - nickname_rect.w

        self.nickname_rect.update_rect(nickname_rect)
        self.state_rect.update_rect(state_rect)

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 3:
            if self.rect.collidepoint(ev.pos):
                return EventFriend("", self.id, ev.pos)
        return None

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
            font_rect = info.font_surface.get_rect()
            center_x = info.rect.x + info.rect.w // 2
            center_y = info.rect.y + info.rect.h // 2
            font_rect.x = center_x - font_rect.w / 2
            font_rect.y = center_y - font_rect.h / 2
            self.screen.blit(info.font_surface, font_rect)
