# tetris/ui/chat_window.py
import pygame

from tetris.config.define import *
from tetris.net.packet_structs import MAX_CHAT_INPUT
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define_colors import *
from tetris.ui.inputbox import InputBox
from tetris.ui.scroll_window_base import ScrollWindowBase

CHAT_WINDOW_WIDTH = 900
CHAT_WINDOW_HEIGHT = 300
SCROLL_WIDTH = 20
CHAT_INPUT_HEIGHT = 40
CHAT_PADDING_Y = 10

MAX_MESSAGE_LINES = 100
FONT_SIZE = 20
LINE_PADDING = 5

OPTIMIZED_OFFSET = 10


class ChatWindow(ScrollWindowBase):
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.texts: list[str] = []
        show_rect = rect.copy()
        show_rect.h -= CHAT_INPUT_HEIGHT
        show_rect.h -= CHAT_PADDING_Y
        self.show_rect = show_rect
        self.show_bg_surface = pygame.Surface((show_rect.w, show_rect.h), pygame.SRCALPHA)
        pygame.draw.rect(self.show_bg_surface, (0, 0, 0, 120), self.show_bg_surface.get_rect())
        pygame.draw.rect(self.show_bg_surface, WHITE, self.show_bg_surface.get_rect(), 1)

        input_rect = rect.copy()
        input_rect.y = show_rect.bottom + CHAT_PADDING_Y
        input_rect.h = CHAT_INPUT_HEIGHT
        input_rect.w = rect.w
        self.input_box = InputBox(self.screen, input_rect, self.rm,
                                  "채팅을 입력하세요", MAX_CHAT_INPUT, is_password=False, allow_korean=True,
                                  use_holder=False)
        self.input_rect = input_rect
        self.input_bg_surface = pygame.Surface((input_rect.w, input_rect.h), pygame.SRCALPHA)
        pygame.draw.rect(self.input_bg_surface, (0, 0, 0, 120), self.input_bg_surface.get_rect())
        pygame.draw.rect(self.input_bg_surface, WHITE, self.input_bg_surface.get_rect(), 1)
        self.font = self.input_box.font

        super().__init__(screen, show_rect, FONT_SIZE + LINE_PADDING)
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

    def handle_event(self, ev: pygame.event.Event):
        super().handle_event(ev)
        return self.input_box.handle_event(ev)

    def update(self, dt_ms: int):
        self.input_box.update(dt_ms)

    def draw_item(self, i: int, draw_text_y: int):
        text_surf = self.font.render(self.texts[i], True, WHITE)
        self.screen.blit(text_surf, (self.window_x, draw_text_y))

    def draw(self):
        self.screen.blit(self.show_bg_surface, self.show_rect.topleft)
        bg_surface = pygame.Surface((self.bg_scroll_rect.w, self.bg_scroll_rect.h), pygame.SRCALPHA)
        bg_surface.fill((0, 0, 0, 192))
        self.screen.blit(bg_surface, self.bg_scroll_rect.topleft)
        if self.can_drag:
            pygame.draw.rect(self.screen, ORANGE, self.scroll_rect)
        else:
            pygame.draw.rect(self.screen, WHITE, self.scroll_rect)

        if self.scrollable_line > 0:
            self.show_end = self.show_start + self.show_lines
            if self.show_end > self.get_items_len():
                self.show_end = self.get_items_len()
        else:
            self.show_start = 0
            self.show_end = self.get_items_len()

        draw_text_y = self.window_y
        for i in range(self.show_start, self.show_end):
            self.draw_item(i, draw_text_y)
            draw_text_y += self.line_height
        self.screen.blit(self.input_bg_surface, self.input_rect.topleft)
        self.input_box.draw()
