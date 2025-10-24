# menu.py
import pygame
from define import *

ORANGE = (255, 165, 0)
GRAY   = (100, 100, 100)
WHITE  = (255, 255, 255)

class Button:
    def __init__(self, text, rel_rect, surf):
        sw, sh = surf.get_size()
        x, y, w, h = rel_rect
        self.rect = pygame.Rect(int(x*sw), int(y*sh), int(w*sw), int(h*sh))
        fs = max(14, int(self.rect.height * 0.55))
        self.font = pygame.font.SysFont(None, fs)
        self.text = text
        self._render_text()

    def _render_text(self):
        self.text_surf = self.font.render(self.text, True, WHITE)
        self.text_rect = self.text_surf.get_rect(center=self.rect.center)

    def on_resize(self, surf, rel_rect):
        sw, sh = surf.get_size()
        x, y, w, h = rel_rect
        self.rect = pygame.Rect(int(x*sw), int(y*sh), int(w*sw), int(h*sh))
        fs = max(14, int(self.rect.height * 0.55))
        self.font = pygame.font.SysFont(None, fs)
        self._render_text()

    def draw(self, surf):
        bg = ORANGE if self.rect.collidepoint(pygame.mouse.get_pos()) else GRAY
        pygame.draw.rect(surf, bg, self.rect, border_radius=8)
        surf.blit(self.text_surf, self.text_rect)

    def clicked(self, event):
        return (event.type == pygame.MOUSEBUTTONDOWN and
                event.button == 1 and
                self.rect.collidepoint(event.pos))

class InputBox:
    def __init__(self, x: int, y: int, w: int, h: int, placeholder: str = "", is_password: bool = False):
        self.rect = pygame.Rect(x, y, w, h)
        self.placeholder = placeholder
        self.text = ""
        self.active = False
        self.is_password = is_password

        self.color_idle = (40, 40, 40)
        self.color_active = (60, 140, 255)
        self.color = self.color_idle

        self.font = pygame.font.SysFont(None, 28)
        self.padding = 10
        self.cursor_visible = True
        self.cursor_timer_ms = 0
        self.cursor_blink_ms = 500

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            self.active = self.rect.collidepoint(ev.pos)
            self.color = self.color_active if self.active else self.color_idle

        if ev.type == pygame.KEYDOWN and self.active:
            if ev.key == pygame.K_BACKSPACE:
                self.text = self.text[:-1]
            elif ev.key == pygame.K_RETURN:
                # 엔터는 상위(LoginState)에서 처리
                pass
            else:
                if ev.unicode and ev.unicode.isprintable():
                    self.text += ev.unicode

    def update(self, dt_ms: int):
        if self.active:
            self.cursor_timer_ms += dt_ms
            if self.cursor_timer_ms >= self.cursor_blink_ms:
                self.cursor_timer_ms = 0
                self.cursor_visible = not self.cursor_visible
        else:
            self.cursor_visible = False
            self.cursor_timer_ms = 0

    def draw(self, surf: pygame.Surface):
        pygame.draw.rect(surf, self.color, self.rect, border_radius=8)

        # 표시 텍스트 (비밀번호면 마스킹)
        show = self.text if not self.is_password else ("●" * len(self.text))
        if not self.text and not self.active:
            txt = self.font.render(self.placeholder, True, (150, 150, 150))
        else:
            txt = self.font.render(show, True, (255, 255, 255))

        surf.blit(txt, (self.rect.x + self.padding,
                        self.rect.y + (self.rect.height - txt.get_height()) // 2))

        # 커서
        if self.active and self.cursor_visible:
            cursor_x = self.rect.x + self.padding + txt.get_width()
            cursor_y = self.rect.y + 8
            pygame.draw.rect(surf, (255, 255, 255),
                             (cursor_x, cursor_y, 2, self.rect.height - 16))