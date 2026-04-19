# tetris/ui/scroll_window_base.py
import pygame
from abc import ABC, abstractmethod

from tetris.resources.define_colors import *

SCROLL_WIDTH = 20


class ScrollWindowBase(ABC):
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, line_height: int):
        self.screen = screen
        self.rect = rect

        self.window_x = rect.x
        self.window_y = rect.y
        self.window_w = rect.w
        self.window_h = rect.h

        self.bg_scroll_x = self.window_x + self.window_w
        self.bg_scroll_y = self.window_y
        self.bg_scroll_w = SCROLL_WIDTH
        self.bg_scroll_h = self.window_h

        self.scroll_x = self.bg_scroll_x
        self.scroll_y = self.bg_scroll_y
        self.scroll_w = self.bg_scroll_w
        self.scroll_h = self.bg_scroll_h

        self.line_height = line_height
        self.show_lines = self.set_show_lines()
        self.min_h = self.line_height

        self.window_rect = pygame.Rect(self.window_x, self.window_y, self.window_w, self.window_h)
        self.bg_scroll_rect = pygame.Rect(self.bg_scroll_x, self.bg_scroll_y, self.bg_scroll_w, self.bg_scroll_h)
        self.scroll_rect = pygame.Rect(self.scroll_x, self.scroll_y, self.scroll_w, self.scroll_h)

        self.max_scrollable_line = 0
        self.scrollable_line = 0
        self.scrollable_px = self.scroll_h - self.min_h
        self.scroll_px = 0

        self.prev_y = 0
        self.can_drag = False
        self.can_scroll = False

        self.show_start = 0
        self.show_end = 0

        self.set_scroll_info()

    @abstractmethod
    def get_items_len(self) -> int:
        pass

    @abstractmethod
    def draw_item(self, i: int, draw_text_y: int):
        pass

    def set_show_lines(self) -> int:
        lines = self.window_h // self.line_height
        if lines < 1:
            lines = 1
        return lines

    def set_scroll_info(self):
        self.max_scrollable_line = self.get_items_len()
        if self.max_scrollable_line - self.show_lines > 0:
            self.scrollable_line = self.max_scrollable_line - self.show_lines
        else:
            self.scrollable_line = 0

        self.scrollable_px = self.bg_scroll_h - self.min_h
        if self.max_scrollable_line > 0:
            self.scroll_px = self.scrollable_px / self.max_scrollable_line
        else:
            self.scroll_px = 0

    def scroll_to_bottom(self):
        self.show_start = self.scrollable_line

        bottom_scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
        self.scroll_y = bottom_scroll_y
        self.scroll_rect.y = int(bottom_scroll_y)

    def set_scroll_len(self):
        if self.scrollable_line <= 0:
            self.scroll_h = self.bg_scroll_h
        else:
            self.scroll_h = self.bg_scroll_h - int(self.scroll_px * self.scrollable_line)
            if self.scroll_h < self.min_h:
                self.scroll_h = self.min_h

        max_scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
        if self.scroll_y > max_scroll_y:
            self.scroll_y = max_scroll_y
        if self.scroll_y < self.bg_scroll_y:
            self.scroll_y = self.bg_scroll_y

        self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, int(self.scroll_h))

    def set_show_start_index(self):
        if self.scrollable_line <= 0:
            self.show_start = 0
            return

        track_top = self.bg_scroll_y
        track_bottom = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
        if track_bottom <= track_top:
            self.show_start = 0
            return

        ratio = (self.scroll_y - track_top) / (track_bottom - track_top)
        if ratio < 0:
            ratio = 0
        elif ratio > 1:
            ratio = 1

        self.show_start = int(ratio * self.scrollable_line)
        if self.show_start < 0:
            self.show_start = 0
        if self.show_start > self.scrollable_line:
            self.show_start = self.scrollable_line

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            window_collode = self.window_rect.collidepoint(ev.pos)
            bg_scroll_collide = self.bg_scroll_rect.collidepoint(ev.pos)
            scroll_collide = self.scroll_rect.collidepoint(ev.pos)
            _, pos_y = ev.pos

            if bg_scroll_collide:
                if scroll_collide:
                    self.prev_y = pos_y
                    self.can_drag = True
                    self.can_scroll = True
                else:
                    self.scroll_y = pos_y - (self.scroll_h / 2)
                    if self.scroll_y <= self.bg_scroll_y:
                        self.scroll_y = self.bg_scroll_y
                    elif self.scroll_y > self.bg_scroll_y and self.scroll_y < self.bg_scroll_y + self.bg_scroll_h - self.scroll_h:
                        pass
                    else:
                        self.scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
                    self.prev_y = pos_y
                    self.can_drag = True
                    self.can_scroll = True

            elif window_collode:
                self.can_scroll = True
                self.can_drag = False

            else:
                self.can_drag = False
                self.can_scroll = False

            self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, int(self.scroll_h))
            self.set_show_start_index()

        elif ev.type == pygame.MOUSEMOTION:
            _, pos_y = ev.pos
            if self.can_drag:
                if pos_y <= self.bg_scroll_y:
                    self.scroll_y = self.bg_scroll_y
                    self.prev_y = self.bg_scroll_y
                elif pos_y > self.bg_scroll_y and pos_y < self.bg_scroll_y + self.bg_scroll_h:
                    px_diff = pos_y - self.prev_y
                    if self.scroll_y + px_diff + self.scroll_h > self.bg_scroll_y + self.bg_scroll_h:
                        self.scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
                    elif self.scroll_y + px_diff < self.bg_scroll_y:
                        self.scroll_y = self.bg_scroll_y
                    else:
                        self.scroll_y += px_diff
                    self.prev_y = pos_y
                else:
                    self.scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h
                    self.prev_y = self.bg_scroll_y + self.bg_scroll_h

            self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, int(self.scroll_h))
            self.set_show_start_index()

        elif ev.type == pygame.MOUSEBUTTONUP:
            self.can_drag = False

        elif ev.type == pygame.MOUSEWHEEL and self.can_scroll:
            max_scroll_y = self.bg_scroll_y + self.bg_scroll_h - self.scroll_h

            if ev.y > 0:
                if self.scroll_y - self.scroll_px > self.bg_scroll_y:
                    self.scroll_y -= self.scroll_px
                else:
                    self.scroll_y = self.bg_scroll_y
            else:
                if self.scroll_y + self.scroll_px < max_scroll_y:
                    self.scroll_y += self.scroll_px
                else:
                    self.scroll_y = max_scroll_y

            self.set_show_start_index()
            self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, int(self.scroll_h))

    def clear(self):
        self.scrollable_line = 0
        self.show_start = 0
        self.show_end = 0
        self.scroll_y = self.bg_scroll_y
        self.scroll_h = self.bg_scroll_h
        self.scroll_rect = pygame.Rect(self.scroll_x, int(self.scroll_y), self.scroll_w, int(self.scroll_h))
        self.set_scroll_info()

    def draw(self):
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
