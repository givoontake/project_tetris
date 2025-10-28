# menu.py
import pygame
from define import *
from define_format import MAX_INPUT_SIZE

ORANGE = (255, 165, 0)
GRAY   = (100, 100, 100)
WHITE  = (255, 255, 255)

class Button:
    def __init__(self, surf, rel_rect, text):
        sw, sh = surf.get_size()
        x, y, w, h = rel_rect
        self.rect = pygame.Rect(int(x*sw), int(y*sh), int(w*sw), int(h*sh))
        fs = max(14, int(self.rect.height * 0.55))
        self.font = pygame.font.Font("resource/dodamdodam.ttf", 28)
        self.text = text
        self._render_text()

    def _render_text(self):
        self.text_surf = self.font.render(self.text, True, WHITE)
        self.text_rect = self.text_surf.get_rect(center=self.rect.center)

    def on_resize(self, rel_rect, surf):
        sw, sh = surf.get_size()
        x, y, w, h = rel_rect
        self.rect = pygame.Rect(int(x*sw), int(y*sh), int(w*sw), int(h*sh))
        fs = max(14, int(self.rect.height * 0.55))
        self.font = pygame.font.Font("resource/dodamdodam.ttf", 28)
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

        self.font = pygame.font.Font("resource/dodamdodam.ttf", 28)
        self.padding = 10

        # 커서 점멸
        self.cursor_visible = True
        self.cursor_timer_ms = 0
        self.cursor_blink_ms = 500

        # 백스페이스 키 반복(typematic) 상태
        self.input_active = False
        self.input_timer_ms = 0
        self.input_delay_ms = 500   # 첫 반복까지 대기(0.5s)
        self.input_repeat_ms = 50  # 이후 반복 간격(0.1s)
        self.input_delay_done = False
        self.repeat_key = None      # 현재 눌려 반복 중인 키 코드
        self.repeat_char = ""       # 문자키의 unicode(반복 시 재사용)

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            self.active = self.rect.collidepoint(ev.pos)
            self.color = self.color_active if self.active else self.color_idle

        if ev.type == pygame.KEYDOWN and self.active:
            if ev.key == pygame.K_BACKSPACE:
                # 누르는 순간 즉시 1글자 삭제
                if self.text:
                    self.text = self.text[:-1]
                # 반복 시작
                self.input_active = True
                self.input_timer_ms = 0
                self.input_delay_done = False
                self.repeat_key = pygame.K_BACKSPACE
                self.repeat_char = ""
            elif ev.key == pygame.K_RETURN:
                # 엔터는 상위에서 처리
                pass
            else:
                if ev.unicode and ev.unicode.isprintable():
                    if len(self.text) < MAX_INPUT_SIZE:
                        self.text += ev.unicode
                    # 반복 시작(문자키)
                    self.input_active = True
                    self.input_timer_ms = 0
                    self.input_delay_done = False
                    self.repeat_key = ev.key
                    self.repeat_char = ev.unicode
                else:
                    # 인쇄 불가 키는 반복 비활성
                    self.input_active = False
                    self.repeat_key = None
                    self.repeat_char = ""

        elif ev.type == pygame.KEYUP:
            # 손을 떼면(현재 반복 중인 키를 뗀 경우) 반복 종료
            if self.repeat_key is not None and ev.key == self.repeat_key:
                self.input_active = False
                self.input_timer_ms = 0
                self.input_delay_done = False
                self.repeat_key = None
                self.repeat_char = ""


    def update(self, dt_ms: int):
    # 커서 점멸
        if self.active:
            self.cursor_timer_ms += dt_ms
            if self.cursor_timer_ms >= self.cursor_blink_ms:
                self.cursor_timer_ms = 0
                self.cursor_visible = not self.cursor_visible
        else:
            self.cursor_visible = False
            self.cursor_timer_ms = 0

        # 키 반복
        if self.active and self.input_active and self.repeat_key is not None:
            self.input_timer_ms += dt_ms

            if not self.input_delay_done:
                # 첫 500ms 지연
                if self.input_timer_ms >= self.input_delay_ms:
                    self.input_timer_ms -= self.input_delay_ms
                    self.input_delay_done = True
            else:
                # 지연 이후 100ms 간격 반복
                while self.input_timer_ms >= self.input_repeat_ms:
                    self.input_timer_ms -= self.input_repeat_ms

                    if self.repeat_key == pygame.K_BACKSPACE:
                        if self.text:
                            self.text = self.text[:-1]
                        else:
                            # 더 지울 게 없으면 타이머만 정리하고 반복 유지
                            self.input_timer_ms = 0
                            break
                    else:
                        # 문자키 반복(인쇄 가능 + 길이 제한)
                        if self.repeat_char and self.repeat_char.isprintable() and len(self.text) < MAX_INPUT_SIZE:
                            self.text += self.repeat_char
                        else:
                            # 반복 불가(공간 부족/인쇄 불가) 시 타이머만 정리
                            self.input_timer_ms = 0
                            break


    def draw(self, surf: pygame.Surface):
        # 배경
        pygame.draw.rect(surf, self.color, self.rect, border_radius=8)

        # 표시 텍스트 (비밀번호면 마스킹)
        show = self.text if not self.is_password else ("●" * len(self.text))
        if not self.text and not self.active:
            txt = self.font.render(self.placeholder, True, (150, 150, 150))
        else:
            txt = self.font.render(show, True, (255, 255, 255))

        text_pos = (self.rect.x + self.padding, self.rect.y + (self.rect.height - txt.get_height()) // 2)
        surf.blit(txt, text_pos)

        # 커서
        if self.active and self.cursor_visible:
            cursor_x = text_pos[0] + txt.get_width()
            cursor_y = self.rect.y + 8
            pygame.draw.rect(surf, (255, 255, 255),
                             (cursor_x, cursor_y, 2, self.rect.height - 16))
