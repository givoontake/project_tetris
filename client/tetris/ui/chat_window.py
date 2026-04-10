# tetris/ui/chat_window.py
import pygame

from tetris.config.define import *
from tetris.resources.define_colors import *
from tetris.ui.scroll_window_base import ScrollWindowBase

CHAT_WINDOW_WIDTH = 1000
CHAT_WINDOW_HEIGHT = 250
SCROLL_WIDTH = 20

MAX_MESSAGE_LINES = 100
FONT_SIZE = 20
LINE_PADDING = 5

OPTIMIZED_OFFSET = 10


class ChatWindow(ScrollWindowBase):
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, font: pygame.font.Font):
        self.screen = screen
        self.rect = rect
        self.font = font
        self.texts: list[str] = []

        super().__init__(screen, rect, FONT_SIZE + LINE_PADDING)
        self.max_scrollable_line = MAX_MESSAGE_LINES - self.show_lines
        self.set_scroll_info()

    def get_items_len(self) -> int:
        return len(self.texts)

    def get_text_px(self, text: str) -> int:
        width, _ = self.font.size(text)
        return width

    def add_new_message(self, user_name: str, new_message: str):
        full_text = f"[{user_name}]: {new_message}"
        remain = full_text
        remain_len = len(remain)

        while remain_len > 0:
            if self.get_text_px(remain) <= self.window_w:
                self.texts.append(remain)
                break

            offset = 0
            prev_offset = 0

            while True:
                if offset + OPTIMIZED_OFFSET >= remain_len:
                    break

                prev_offset = offset
                offset += OPTIMIZED_OFFSET

                part_px = self.get_text_px(remain[:offset])
                if part_px > self.window_w:
                    offset = prev_offset
                    break

            while offset < remain_len:
                part_px = self.get_text_px(remain[:offset + 1])
                if part_px > self.window_w:
                    break
                offset += 1

            if offset == 0:
                offset = 1

            self.texts.append(remain[:offset])

            remain = remain[offset:]
            remain_len = len(remain)

        if len(self.texts) > MAX_MESSAGE_LINES:
            over_lines = len(self.texts) - MAX_MESSAGE_LINES
            del self.texts[:over_lines]

        self.set_scroll_info()
        self.set_scroll_len()
        if self.can_drag:
            self.set_show_start_index()
        else:
            self.scroll_to_bottom()

    def clear(self):
        self.texts.clear()
        super().clear()

    def draw_item(self, i: int, draw_text_y: int):
        text_surf = self.font.render(self.texts[i], True, WHITE)
        self.screen.blit(text_surf, (self.window_x, draw_text_y))