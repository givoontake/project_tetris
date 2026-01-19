import pygame
from typing import Optional

from font_manager import FontManager
from define import *

PLACEHOLDER = (100, 100, 100)

class InputBox:
    def __init__(
        self,
        screen: pygame.Surface,
        rect: pygame.Rect,
        fm: FontManager,
        placeholder: str = "",
        max_input_len: int | None = None,  # optional과 같음
        is_password: bool = False,
        allow_korean: bool = True,   # ✅ 한글 허용 여부
    ):
        self.screen = screen
        self.rect = rect
        self.fm = fm
        self.placeholder = placeholder
        self.text_h = int(rect.h*0.8)
        self.text = ""
        self.editing_text = ""
        self.active = False
        self.is_password = is_password
        self.allow_korean = allow_korean
        self.max_input_len = max_input_len
        self.padding = int(self.text_h / 2)
        self.color = GRAY
        self.font = self.fm.get_font(self.text_h)

        # 커서 점멸
        self.cursor_visible = True
        self.cursor_timer_ms = 0
        self.cursor_blink_ms = 500

        # 키 입력은 윈도우 입력기를 통해 처리하지만 완성된 글자만 반환하므로 지우기는 따로 처리해야함
        self.backspace_pressed = False
        #self.backspace_repeat_active = False
        self.backspace_repeat_timer = 0
        self.backspace_repeat_time1 = 500
        self.backspace_repeat_time1_active = True
        self.backspace_repeat_time2 = 50
        self.backspace_repeat_time2_active = False

    def get_inputbox_rect(self) -> pygame.Rect:
        return self.rect

    def add_char(self, ch: str):
        if not self.active:
            return
        # 빈 문자, 제어문자 필터링
        if not ch or not ch.isprintable():
            return
        # 한글 비허용이면 ASCII만 받음
        if not self.allow_korean and not ch.isascii():
            return
        # 길이 초과 방지
        if self.max_input_len is not None and len(self.text) >= self.max_input_len:
            return
        
        if not ch.isascii():
            self.backspace_repeat_time1_active = False
            self.backspace_repeat_time2_active = True
        # 통과했으면 추가
        self.text += ch

    def delete_char(self):
        if not self.active:
            return
        if self.text:
            self.text = self.text[:-1]

    def get_text_width(self, text: str) -> int:
        width, _ = self.font.size(text)
        return width
    
    def get_total_text(self) -> str:
        total_text = ""
        if len(self.text) < self.max_input_len:
            total_text = self.text + self.editing_text
        else:
            total_text = self.text
        
        if self.is_password:
            total_text = "●" * len(total_text)

        return total_text

    def get_render_text(self) -> str:
        text_px_len = None
        render_text = ""
        if len(self.text) < self.max_input_len:
            render_text = self.text + self.editing_text
            text_px_len = self.get_text_width(self.text) + self.get_text_width(self.editing_text)
        else:
            render_text = self.text
            text_px_len = self.get_text_width(self.text)

        box_px_len = self.rect.w - self.padding * 2# 좌우 기본 패딩
        offset = 0
        
        if self.is_password:
            render_text = "●" * (len(self.text) + len(self.editing_text))
                                      
        while True:            
            if text_px_len <= box_px_len:
                return render_text
            
            else:
                if self.get_text_width(render_text[offset:]) <= box_px_len:
                    return render_text[offset:]
                else:
                    offset += 1
                    if offset >= len(self.text):
                        return ""


    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        # 마우스 클릭으로 포커스 on/off
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            self.active = self.rect.collidepoint(ev.pos)
            if self.active:
                self.color = BLUE
            else:
                self.color = GRAY

        # ===== 한글/영문 조합 중 문자열 (프리뷰) =====
        elif ev.type == pygame.TEXTEDITING:
            # 한글 허용인 필드만 조합 상태 표시
            if self.active and self.allow_korean:
                self.editing_text = ev.text

        # ===== 최종 확정된 문자 입력 =====
        elif ev.type == pygame.TEXTINPUT:
            self.add_char(ev.text)

        # ===== 제어키 처리 (백스페이스 + 엔터) =====
        elif ev.type == pygame.KEYDOWN:
            if ev.key == pygame.K_BACKSPACE:  # TEXTINPUT으로는 문자 아니면 처리 안됨, 따로 처리 필요
                self.delete_char()
                self.backspace_pressed = True

            elif ev.key == pygame.K_RETURN:
                input_text = self.get_total_text()
                self.text = ""
                self.editing_text = ""
                self.backspace_repeat_timer = 0
                self.backspace_repeat_time1_active = True
                self.backspace_repeat_time2_active = False
                self.backspace_pressed = False
                return input_text
                
        elif ev.type == pygame.KEYUP:
            if ev.key == pygame.K_BACKSPACE:
                self.backspace_repeat_timer = 0
                self.backspace_repeat_time1_active = True
                self.backspace_repeat_time2_active = False
                self.backspace_pressed = False

        return None

    def update(self, dt_ms: int):
        # 커서 점멸만 유지
        if self.active:
            self.cursor_timer_ms += dt_ms
            if self.cursor_timer_ms >= self.cursor_blink_ms:
                self.cursor_timer_ms -= self.cursor_blink_ms
                self.cursor_visible = not self.cursor_visible

            if self.backspace_repeat_time1_active and self.backspace_pressed:
                self.backspace_repeat_timer += dt_ms
                if self.backspace_repeat_timer >= self.backspace_repeat_time1:
                    self.backspace_repeat_timer -= self.backspace_repeat_time1
                    self.backspace_repeat_time1_active = False
                    self.backspace_repeat_time2_active = True
                    self.delete_char()

            elif self.backspace_repeat_time2_active and self.backspace_pressed:
                self.backspace_repeat_timer += dt_ms
                if self.backspace_repeat_timer >= self.backspace_repeat_time2:
                    self.backspace_repeat_timer -= self.backspace_repeat_time2
                    self.delete_char()
                
        else:
            self.cursor_visible = False
            self.cursor_timer_ms = 0
            self.backspace_repeat_timer = 0
            self.backspace_repeat_time1_active = True
            self.backspace_repeat_time2_active = False


    def draw(self):
        # 배경
        #pygame.draw.rect(self.screen, self.color, self.rect)

        # 보여줄 텍스트 (비밀번호면 ●로 마스킹)
        show_text = self.get_render_text()
        if not show_text and not self.active:
            txt = self.font.render(self.placeholder, True, PLACEHOLDER)
        else:
            txt = self.font.render(show_text, True, WHITE)

        text_x = self.rect.x + self.padding
        text_y = self.rect.y + (self.rect.height - txt.get_height()) // 2
        text_pos = (text_x, text_y)
        self.screen.blit(txt, text_pos)

        # 커서
        if self.active and self.cursor_visible:
            cursor_x = text_x + txt.get_width()
            cursor_y = text_y
            cursor_h = self.text_h
            pygame.draw.rect(self.screen, WHITE,(cursor_x, cursor_y, 2, cursor_h))